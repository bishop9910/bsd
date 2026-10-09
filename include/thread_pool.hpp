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
      static ThreadPool& getInstance() {
        static ThreadPool instance;
        return instance;
      }
      ThreadPool(const ThreadPool&) = delete;
      ThreadPool& operator=(const ThreadPool&) = delete;
      void init(const int thread_num = 16){
        if(initialized) {
          return;
        }

        for (int i = 0; i < thread_num; ++i) {
          threads_group.emplace_back([this](std::stop_token st) {
            worker_loop(st);
          });
        }

        initialized = true;
      }
      void shutdown(){
        if(!initialized){
          return;
        }
        for(auto& t : threads_group){
          t.request_stop();
        }
        cv.notify_all();
        threads_group.clear();
        initialized = false;
      };
      template <class F, class... Args>
      auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using Return = std::invoke_result_t<F, Args...>;
        auto bound = [f = std::forward<F>(f), ...a = std::forward<Args>(args)]()mutable -> Return {
          return std::invoke(std::move(f), std::move(a)...);
        };

        auto task = std::make_shared<std::packaged_task<Return()>>(std::move(bound));
        std::future<Return> fut = task->get_future();

        {
          std::lock_guard<std::mutex> lock(mutex);
          if(!initialized){
            throw std::runtime_error("[Error] ThreadPool not initialized");
          }
          works.emplace([task](std::stop_token) { (*task)(); });
        }

        cv.notify_one();

        return fut;
      }
    private:
      ThreadPool() = default;
      ~ThreadPool() {
        shutdown();
      }
      bool initialized = false;
      std::queue<std::function<void(std::stop_token)>> works;
      std::mutex mutex;
      std::condition_variable_any cv;
      std::list<std::jthread> threads_group;
      void worker_loop(std::stop_token stop_token){
        while(true){
          std::function<void(std::stop_token)> task;
          {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, stop_token, [this, &stop_token]{
              return stop_token.stop_requested() || !works.empty();
            });

            if(stop_token.stop_requested() && works.empty()){
              return;
            }

            if(works.empty()){
              continue;
            }

            task = std::move(works.front());
            works.pop();
          }

          try{
            task(stop_token);
          }catch (const std::exception& e) {
            std::cerr << "[Error] Task exception: " << e.what() << '\n';
          }catch(...){}
        }
      }
  };
} // namespace BSD_CORE

#endif