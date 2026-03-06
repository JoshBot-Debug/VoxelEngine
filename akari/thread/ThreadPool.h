#pragma once

#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace akari::thread {

class ThreadPool {

public:
  class Group {
  private:
    std::atomic<size_t>   m_Count {0};
    std::function<void()> m_OnComplete {nullptr};

  public:
    Group() = default;

    void OnComplete(std::function<void()> fn) {
      m_OnComplete = std::move(fn);
    }

    void Add(size_t n = 1) {
      m_Count.fetch_add(n, std::memory_order::relaxed);
    }

    void Done() {
      if (m_Count.fetch_sub(1, std::memory_order::acq_rel) == 1)
        if (m_OnComplete)
          m_OnComplete();
    }
  };

private:
  std::vector<std::thread>          m_Workers {};
  std::queue<std::function<void()>> m_Tasks {};

  std::mutex              m_Mutex;
  std::condition_variable m_CV;
  bool                    m_Stop {false};

private:
  ThreadPool();
  ~ThreadPool();

  ThreadPool(const ThreadPool&)            = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

public:
  static ThreadPool& Instance();

  static std::shared_ptr<ThreadPool::Group> CreateGroup(const std::function<void()>& fn);

  template <typename F>
  static void Dispatch(F&& task);

  template <typename F>
  static void Dispatch(std::shared_ptr<ThreadPool::Group> group, F&& task);

  template <typename F, typename... Args>
  static void Dispatch(F&& task, Args&... args);

  template <typename F, typename... Args>
  static void Dispatch(std::shared_ptr<ThreadPool::Group> group, F&& task, Args&... args);

  template <typename T, typename F>
  static void ForEach(std::vector<T> data, F&& task);

  template <typename T, typename F>
  static void ForEach(std::shared_ptr<ThreadPool::Group> group, std::vector<T> data, F&& task);
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

template <typename F>
inline void ThreadPool::Dispatch(std::shared_ptr<ThreadPool::Group> group, F&& task) {
  group->Add();
  Dispatch([group, f = std::forward<F>(task)]() mutable {
    f();
    group->Done();
  });
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

template <typename F, typename... Args>
inline void ThreadPool::Dispatch(std::shared_ptr<ThreadPool::Group> group, F&& task, Args&... args) {
  group->Add();
  Dispatch(
      [group, f = std::forward<F>(task), a = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        std::apply(f, a);
        group->Done();
      });
}

template <typename T, typename F>
inline void ThreadPool::ForEach(std::vector<T> data, F&& task) {
  static_assert(std::is_trivially_copyable<T>());

  ThreadPool& pool = ThreadPool::Instance();

  {
    std::lock_guard lock(pool.m_Mutex);

    for (size_t i = 0; i < data.size(); ++i)
      pool.m_Tasks.emplace(
          [i, item = data[i], f = std::forward<F>(task)]() mutable {
            f(i, item);
          });
  }
  pool.m_CV.notify_all();
}

template <typename T, typename F>
inline void ThreadPool::ForEach(std::shared_ptr<ThreadPool::Group> group, std::vector<T> data, F&& task) {
  static_assert(std::is_trivially_copyable<T>());

  ThreadPool& pool = ThreadPool::Instance();

  group->Add(data.size());

  {
    std::lock_guard lock(pool.m_Mutex);

    for (size_t i = 0; i < data.size(); ++i)
      pool.m_Tasks.emplace(
          [group, i, item = data[i], f = std::forward<F>(task)]() mutable {
            f(i, item);
            group->Done();
          });
  }
  pool.m_CV.notify_all();
}

} // namespace akari::thread