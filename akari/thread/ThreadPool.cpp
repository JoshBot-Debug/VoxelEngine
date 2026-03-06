#include "ThreadPool.h"

namespace akari::thread {
ThreadPool::ThreadPool() {
  const uint32_t SIZE = std::thread::hardware_concurrency();

  for (size_t i = 0; i < SIZE; i++) {
    m_Workers.emplace_back([this] {
      while (true) {
        std::function<void()> task;

        {
          std::unique_lock lock(m_Mutex);

          m_CV.wait(lock, [this] {
            return m_Stop || !m_Tasks.empty();
          });

          if (m_Stop && m_Tasks.empty())
            return;

          task = std::move(m_Tasks.front());
          m_Tasks.pop();
        }

        task();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard lock(m_Mutex);
    m_Stop = true;
  }
  m_CV.notify_all();

  for (auto& t : m_Workers)
    t.join();
}

ThreadPool& ThreadPool::Instance() {
  static ThreadPool instance;
  return instance;
}

std::shared_ptr<ThreadPool::Group> ThreadPool::CreateGroup(const std::function<void()>& fn) {
  auto group = std::make_shared<ThreadPool::Group>();
  group->OnComplete(fn);
  return group;
}

} // namespace akari::thread
