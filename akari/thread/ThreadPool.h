#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace akari::thread {
class ThreadPool {
private:
  std::vector<std::thread>          m_Workers = {};
  std::queue<std::function<void()>> m_Tasks   = {};

  std::mutex              m_Mutex;
  std::condition_variable m_CV;
  bool                    m_Stop = false;

private:
  ThreadPool();
  ~ThreadPool();

  ThreadPool(const ThreadPool&)            = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

public:
  static ThreadPool& Instance();

  template <typename F>
  static void Dispatch(F&& task);

  template <typename F, typename... Args>
  static void Dispatch(F&& task, Args&... args);

  template <typename T, typename F, typename C>
  static void ForEach(std::vector<T> data, F&& task, C&& onComplete);
};

template <typename F>
inline void ThreadPool::Dispatch(F&& task) {
  ThreadPool& pool = Instance();

  {
    std::lock_guard lock(pool.m_Mutex);
    pool.m_Tasks.push(std::move(task));
  }
  pool.m_CV.notify_one();
}

template <typename F, typename... Args>
inline void ThreadPool::Dispatch(F&& task, Args&... args) {
  ThreadPool& pool = Instance();

  {
    std::lock_guard lock(pool.m_Mutex);
    pool.m_Tasks.push([f = std::forward<F>(task), a = std::make_tuple(std::forward<Args>(args)...)] mutable {
      std::apply(f, a);
    });
  }
  pool.m_CV.notify_one();
}

template <typename T, typename F, typename C>
inline void ThreadPool::ForEach(std::vector<T> data, F&& task, C&& onComplete) {
  static_assert(std::is_trivially_copyable<T>());

  ThreadPool& pool = ThreadPool::Instance();

  if (data.empty()) {
    onComplete();
    return;
  }

  auto remaining  = std::make_shared<std::atomic<size_t>>(data.size());
  auto completion = std::make_shared<std::decay_t<C>>(std::forward<C>(onComplete));

  {
    std::lock_guard lock(pool.m_Mutex);

    for (size_t i = 0; i < data.size(); ++i) {
      pool.m_Tasks.emplace(
          [i, item = data[i], f = std::forward<F>(task), remaining, completion]() mutable {
            f(i, item);
            if (remaining->fetch_sub(1, std::memory_order_acq_rel) == 1)
              (*completion)();
          });
    }
  }
  pool.m_CV.notify_one();
}

} // namespace akari::thread