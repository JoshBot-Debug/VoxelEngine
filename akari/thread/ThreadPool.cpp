#include "ThreadPool.h"

#include <iostream>

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

ThreadPool::TaskId ThreadPool::GenerateId() {
  ThreadPool& pool = Instance();

  uint32_t id = pool.m_TasksInQueueCount.fetch_add(1, std::memory_order::relaxed);

  if (id > pool.m_TasksInQueue.size())
    throw std::runtime_error("Too many thread job ids. Maximum is 64");

  return id;
}
} // namespace akari::thread
