// Copyright 2018 Roman Perepelitsa
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

  int64_t last_idx_ = 0;
  bool exit_ = false;
  // Do we have a thread waiting on sleeper_cv_?
  bool have_sleeper_ = false;
  std::mutex mutex_;
  // Any number of threads can wait on this condvar. Always without a timeout.
  std::condition_variable cv_;
  // At most one thread can wait on this condvar at a time. Always with a timeout.
  std::condition_variable sleeper_cv_;
  std::priority_queue<Work> work_;
  std::vector<std::thread> threads_;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_THREAD_POOL_H_
