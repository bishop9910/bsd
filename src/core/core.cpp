#include "core.hpp"
#include "thread_pool.hpp"

namespace BSD_CORE {
  const std::array<int, 3> version = {0, 0, 1};

  void initDownloadCore(){
    ThreadPool::getInstance().init(20);
    ThreadPool& tp = ThreadPool::getInstance();

    std::list<std::future<int>> future_list;
    for(int i=0; i<20; ++i){
      std::future<int> f = tp.submit([](int a, int b){return a + b;}, i, i+1);
      future_list.emplace_front(std::move(f));
    }
    for(auto& f : future_list){
      std::cout << f.get() << " ";
    }
  }
}