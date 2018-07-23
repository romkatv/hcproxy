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
  cv_.notify_all();
  sleeper_cv_.notify_one();
  for (std::thread& t : threads_) t.join();
}

void ThreadPool::Schedule(Time t, std::function<void()> f) {
  std::condition_variable* wake = nullptr;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    work_.push(Work{std::move(t), ++last_idx_, std::move(f)});
    if (work_.top().idx == last_idx_) wake = have_sleeper_ ? &sleeper_cv_ : &cv_;
  }
  if (wake) wake->notify_one();
}

void ThreadPool::Loop(size_t tid) {
  auto Next = [&]() -> std::function<void()> {
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
      if (exit_) return nullptr;
      if (work_.empty()) {
        cv_.wait(lock);
        continue;
      }
      Time now = Clock::now();
      const Work& top = work_.top();
      if (top.t <= now) {
        std::function<void()> res = std::move(top.f);
        work_.pop();
        bool notify = !work_.empty() && !have_sleeper_;
        lock.unlock();
        if (notify) cv_.notify_one();
        return res;
      }
      if (have_sleeper_) {
        cv_.wait(lock);
        continue;
      }
      have_sleeper_ = true;
      sleeper_cv_.wait_until(lock, top.t);
      assert(have_sleeper_);
      have_sleeper_ = false;
    }
  };
  while (std::function<void()> f = Next()) f();
}

}  // namespace hcproxy
