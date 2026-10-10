#include <iostream>
#include <mutex>
#include <stop_token>
#include <queue>
#include <thread>
#include <list>
#include <condition_variable>
#include <functional>
#include <future>

#ifndef BSD_THREADPOOL_
#define BSD_THREADPOOL_

namespace BSD_CORE {
  class ThreadPool {
    public:
      ThreadPool(const std::size_t thread_num = 16);
      ~ThreadPool();

      ThreadPool(const ThreadPool&)            = delete;
      ThreadPool& operator=(const ThreadPool&) = delete;
      ThreadPool(ThreadPool&&)                 = delete;
      ThreadPool& operator=(ThreadPool&&)      = delete;

      void shutdown();

      template <class F, class... Args>
      auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using Return = std::invoke_result_t<F, Args...>;
        auto bound = [f = std::forward<F>(f), ...a = std::forward<Args>(args)]() mutable -> Return {
          return std::invoke(std::move(f), std::move(a)...);
        };

        auto task = std::make_shared<std::packaged_task<Return()>>(std::move(bound));
        std::future<Return> fut = task->get_future();

        {
          std::lock_guard<std::mutex> lock(mutex);
          if (!initialized) {
            throw std::runtime_error("[Error] ThreadPool not initialized");
          }
          works.emplace([task](std::stop_token) { (*task)(); });
        }

        cv.notify_one();

        return fut;
      }

    private:
      bool initialized = false;
      std::queue<std::function<void(std::stop_token)>> works;
      std::mutex mutex;
      std::condition_variable_any cv;
      std::list<std::jthread> threads_group;

      void worker_loop(std::stop_token stop_token);
  };
} // namespace BSD_CORE

#endif