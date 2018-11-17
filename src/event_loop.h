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

#ifndef ROMKATV_HCPROXY_EVENT_LOOP_H_
#define ROMKATV_HCPROXY_EVENT_LOOP_H_

#include <cstdint>
#include <functional>
#include <thread>

#include "epoll.h"
#include "check.h"
#include "list.h"
#include "time.h"

namespace hcproxy {

class EventLoop;

class EventHandler : private Node {
 public:
  explicit EventHandler(int fd) : fd_(fd) { CHECK(fd_ >= 0); }
  EventHandler(EventHandler&&) = delete;

  int fd() const { return fd_; }

  // Not thread-safe.
  void IncRef() {
    CHECK(ref_count_ >= 0);
    ++ref_count_;
  }

  // Not thread-safe.
  void DecRef() {
    CHECK(ref_count_ > 0);
    if (--ref_count_ == 0) delete this;
  }

  // Called from the EventLoop thread. The caller holds a ref count during the call.
  virtual void OnEvent(EventLoop* loop, int events) = 0;
  // Called from the EventLoop thread. The caller holds a ref count during the call.
  virtual void OnTimeout(EventLoop* loop) = 0;

 protected:
  virtual ~EventHandler() {
    CHECK(event_loop_ == nullptr);
    CHECK(ref_count_ == 0);
  }

 private:
  friend class EventLoop;

  const int fd_;
  Time deadline_;
  // Null iff this event handler isn't registered in an event loop.
  const EventLoop* event_loop_ = nullptr;
  int64_t ref_count_ = 0;
};

// A wrapper around a thread + epoll.
class EventLoop {
 public:
  explicit EventLoop(Duration timeout);
  EventLoop(EventLoop&&) = delete;
  ~EventLoop() = delete;

  // Can be called only from the Loop() thread.
  void Add(EventHandler* eh, int events);

  // Can be called only from the Loop() thread.
  // OnEvent() and OnTimeout() won't fire until Add() is called again.
  void Remove(EventHandler* eh);

  // Can be called only from the Loop() thread.
  void Modify(EventHandler* eh, int events);

  // Cannot be called from the Loop() thread. Can be called concurrently.
  void Schedule(std::function<void()> f);

  // When called from the Loop() thread, invokes `f` synchronously.
  // Otherwise calls Schedule(f).
  void ScheduleOrRun(std::function<void()> f);

 private:
  void Loop();

  void Refresh(EventHandler* eh);

  int pipe_[2];
  EPoll epoll_;
  // Event handlers sorted by expiration time. The head is the first to expire.
  List expire_;
  Duration timeout_;
  std::thread loop_;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_EVENT_LOOP_H_
