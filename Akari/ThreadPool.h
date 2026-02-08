#pragma once

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Akari {
class ThreadPool {
public:
  using TaskId = uint32_t;

private:
  std::array<std::atomic<uint32_t>, 64> m_TasksInQueue      = {};
  std::atomic<size_t>                   m_TasksInQueueCount = 0;

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

  static TaskId GenerateId();

  template <typename F>
  static void Dispatch(F&& task);

  template <typename F, typename... Args>
  static void Dispatch(F&& task, Args&... args);

  template <typename T, typename F, typename C>
  static void ForEach(std::vector<T> data, F&& task, C&& onComplete);

  template <typename T, typename F, typename C>
  static void ForEach(TaskId id, std::vector<T> data, F&& task, C&& onComplete);
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
          [item = std::move(data[i]), f = std::forward<F>(task), remaining, completion]() mutable {
            f(std::move(item));
            if (remaining->fetch_sub(1, std::memory_order_acq_rel) == 1)
              (*completion)();
          });
    }
  }
  pool.m_CV.notify_one();
}

template <typename T, typename F, typename C>
inline void ThreadPool::ForEach(TaskId         id,
                                std::vector<T> data,
                                F&&            task,
                                C&&            onComplete) {
  ThreadPool& pool = ThreadPool::Instance();

  uint32_t expected = 0;
  if (!pool.m_TasksInQueue[id].compare_exchange_strong(
          expected,
          (uint32_t)data.size(),
          std::memory_order_acq_rel)) {
    return;
  }

  if (data.empty()) {
    onComplete();
    return;
  }

  auto completion = std::make_shared<std::decay_t<C>>(std::forward<C>(onComplete));
  auto func       = std::forward<F>(task);

  {
    std::lock_guard lock(pool.m_Mutex);

    for (auto& item : data) {
      pool.m_Tasks.emplace(
          [item       = std::move(item),
           f          = func,
           &remaining = pool.m_TasksInQueue[id],
           completion]() mutable {
            f(std::move(item));

            if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
              (*completion)();
          });
    }
  }

  pool.m_CV.notify_all();
}

} // namespace Akari