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

#include "thread_pool.h"

#include <cassert>
#include <optional>
#include <utility>

namespace hcproxy {

ThreadPool::ThreadPool(size_t num_threads) {
  assert(num_threads >= 0);
  for (size_t i = 0; i != num_threads; ++i) {
    threads_.emplace_back([=]() { Loop(i + 1); });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    exit_ = true;
  }
  wake_.notify_all();
  for (std::thread& t : threads_) t.join();
}

void ThreadPool::Schedule(Time t, std::function<void()> f) {
  std::unique_lock<std::mutex> lock(mutex_);
  work_.push(Work{std::move(t), next_, std::move(f)});
  if (work_.top().idx == next_) {
    sleeper_tid_ = 0;
    wake_.notify_one();
  }
  ++next_;
}

void ThreadPool::Loop(size_t tid) {
  auto Next = [&]() -> std::function<void()> {
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
      if (exit_) return nullptr;
      if (work_.empty()) {
        wake_.wait(lock);
        continue;
      }
      Time now = Clock::now();
      const Work& top = work_.top();
      if (top.t <= now) {
        std::function<void()> res = std::move(top.f);
        work_.pop();
        sleeper_tid_ = 0;
        if (!work_.empty()) wake_.notify_one();
        return res;
      }
      if (sleeper_tid_ != 0) {
        wake_.wait(lock);
        continue;
      }
      sleeper_tid_ = tid;
      wake_.wait_until(lock, top.t);
      if (sleeper_tid_ == tid) sleeper_tid_ = 0;
    }
  };
  while (std::function<void()> f = Next()) f();
}

}  // namespace hcproxy
