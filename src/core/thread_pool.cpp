#include "thread_pool.hpp"
#include "log.hpp"

namespace BSD_CORE {

  void ThreadPool::shutdown(){
    {
      std::lock_guard<std::mutex> lock(mutex);
      if(!initialized){
        return;
      }
      initialized = false;
    }
    for(auto& t : threads_group){
      t.request_stop();
    }
    cv.notify_all();
    threads_group.clear();
  }

  void ThreadPool::worker_loop(std::stop_token stop_token){
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

        task = std::move(works.front());
        works.pop();
      }

      try{
        task(stop_token);
      }catch (const std::exception& e) {
        Log::error() << "[Error] Task exception: " << e.what();
      }catch(...){}
    }
  }

  ThreadPool::~ThreadPool(){
    shutdown();
  }

  ThreadPool::ThreadPool(const std::size_t thread_num)
    : initialized(false)
  {
    if (thread_num == 0) {
      throw std::invalid_argument("[Error] ThreadPool thread_num must be > 0");
    }
    for (std::size_t i = 0; i < thread_num; ++i) {
      threads_group.emplace_back(
        [this](std::stop_token st) {
          worker_loop(st); 
        }
      );
    }
    initialized = true;
  }
}