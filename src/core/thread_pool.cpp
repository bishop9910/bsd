#include "thread_pool.hpp"

namespace BSD_CORE {

  void ThreadPool::shutdown(){
    if(!initialized){
      return;
    }
    for(auto& t : threads_group){
      t.request_stop();
    }
    cv.notify_all();
    threads_group.clear();
    initialized = false;
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

  ThreadPool::~ThreadPool(){
    shutdown();
  }

  explicit ThreadPool::ThreadPool(const std::size_t thread_num = std::thread::hardware_concurrency())
    : initialized(false)
  {
    if (thread_num == 0 || thread_num < 0) {
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