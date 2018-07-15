#ifndef ROMKATV_HCPROXY_THREAD_POOL_H_
#define ROMKATV_HCPROXY_THREAD_POOL_H_

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>

#include "time.h"

namespace hcproxy {

class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads);
  ThreadPool(ThreadPool&&) = delete;

  // Waits for the currently running functions to finish.
  // Does NOT wait for the queue of functions to drain.
  ~ThreadPool();

  // Runs `f` on one of the threads at or after time `t`. Can be called
  // from any thread. Can be called concurrently.
  //
  // Does not block.
  void Schedule(Time t, std::function<void()> f);

 private:
  struct Work {
    bool operator<(const Work& w) const { return std::tie(w.t, w.idx) < std::tie(t, idx); }
    Time t;
    int64_t idx;
    mutable std::function<void()> f;
  };

  void Loop(size_t tid);

  int64_t next_ = 0;
  bool exit_ = false;
  size_t sleeper_tid_ = 0;
  std::mutex mutex_;
  std::condition_variable wake_;
  std::priority_queue<Work> work_;
  std::vector<std::thread> threads_;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_THREAD_POOL_H_
