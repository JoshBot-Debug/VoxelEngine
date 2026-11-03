#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

namespace akari::thread {

class ThreadPool {
private:
  struct Task {
    std::stop_token                 StopToken;
    std::move_only_function<void()> Callback;
  };

private:
  std::vector<std::jthread> m_Workers {};
  std::queue<Task>          m_Tasks {};

  std::mutex              m_Mutex;
  std::condition_variable m_CV;

private:
  ThreadPool();
  ~ThreadPool() = default;

  ThreadPool(const ThreadPool&)            = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

public:
  static ThreadPool& Instance();

  template <typename F>
  [[nodiscard]] static std::stop_source Dispatch(F&& task);

  template <typename F>
  static void Dispatch(std::stop_source& stopSource, F&& task);
};

inline ThreadPool::ThreadPool() {
  const uint32_t SIZE = std::thread::hardware_concurrency();

  for (size_t i = 0; i < SIZE; i++) {
    m_Workers.emplace_back([this](std::stop_token stopToken) {
      while (true) {
        Task task;

        {
          std::unique_lock lock(m_Mutex);

          m_CV.wait(lock, [this, &stopToken] {
            return !m_Tasks.empty() || stopToken.stop_requested();
          });

          if (stopToken.stop_requested() && m_Tasks.empty())
            return;

          task = std::move(m_Tasks.front());
          m_Tasks.pop();
        }

        if (task.StopToken.stop_requested())
          continue;

        if (task.Callback)
          task.Callback();
      }
    });
  }
}

inline ThreadPool& ThreadPool::Instance() {
  static ThreadPool instance;
  return instance;
}

template <typename F>
[[nodiscard]] inline std::stop_source ThreadPool::Dispatch(F&& task) {
  ThreadPool&      pool = Instance();
  std::stop_source stopSource;

  {
    std::lock_guard lock(pool.m_Mutex);
    pool.m_Tasks.push({stopSource.get_token(), std::forward<F>(task)});
  }

  pool.m_CV.notify_one();
  return stopSource;
}

template <typename F>
inline void ThreadPool::Dispatch(std::stop_source& stopSource, F&& task) {
  ThreadPool& pool = Instance();

  {
    std::lock_guard lock(pool.m_Mutex);
    pool.m_Tasks.push({stopSource.get_token(), std::forward<F>(task)});
  }

  pool.m_CV.notify_one();
}

} // namespace akari::thread