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

#include "event_loop.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <utility>

#include "error.h"

namespace hcproxy {

namespace {

void PopFunc(int pipe) {
  std::function<void()>* f;
  int n;
  HCP_CHECK((n = read(pipe, &f, sizeof(f))) == sizeof(f));
  (*f)();
  delete f;
}

void AddFlags(int fd, int flags) {
  flags |= fcntl(fd, F_GETFL, 0);
  HCP_CHECK(fcntl(fd, F_SETFL, flags) == 0);
}

}  // namespace

EventLoop::EventLoop(Duration timeout) : timeout_(std::move(timeout)) {
  assert(timeout_ > Duration::zero());
  // Passing O_DIRECT to pipe2() doesn't work on Windows.
  // Setting it via fcntl() doesn't work on AWS.
  if (pipe2(pipe_, O_DIRECT) != 0) {
    HCP_CHECK(pipe(pipe_) == 0);
    for (int fd : pipe_) AddFlags(fd, O_DIRECT);
  }
  epoll_.Add(pipe_[0], EPOLLIN, nullptr);
  loop_ = std::thread(&EventLoop::Loop, this);
}

void EventLoop::Add(EventHandler* eh, int events) {
  assert(eh);
  assert(!eh->event_loop_);
  assert(std::this_thread::get_id() == loop_.get_id());
  eh->IncRef();
  eh->event_loop_ = this;
  eh->deadline_ = Clock::now() + timeout_;
  expire_.AddTail(eh);
  epoll_.Add(eh->fd_, events, eh);
}

void EventLoop::Remove(EventHandler* eh) {
  assert(eh);
  assert(eh->event_loop_ == this);
  assert(std::this_thread::get_id() == loop_.get_id());
  expire_.Erase(eh);
  epoll_.Remove(eh->fd_);
  eh->event_loop_ = nullptr;
  eh->DecRef();
}

void EventLoop::Modify(EventHandler* eh, int events) {
  assert(eh);
  assert(eh->event_loop_ == this);
  assert(std::this_thread::get_id() == loop_.get_id());
  epoll_.Modify(eh->fd_, events, eh);
  Refresh(eh);
}

void EventLoop::Schedule(std::function<void()> f) {
  assert(f);
  assert(std::this_thread::get_id() != loop_.get_id());
  auto* p = new std::function<void()>(std::move(f));
  HCP_CHECK(write(pipe_[1], &p, sizeof(p)) == sizeof(p));
}

void EventLoop::ScheduleOrRun(std::function<void()> f) {
  assert(f);
  if (std::this_thread::get_id() == loop_.get_id()) {
    f();
  } else {
    Schedule(std::move(f));
  }
}

void EventLoop::Loop() {
  while (true) {
    epoll_.Wait(timeout_);
    for (const epoll_event& ev : epoll_) {
      if (ev.data.ptr != nullptr) {
        static_cast<EventHandler*>(ev.data.ptr)->IncRef();
      }
    }
    for (const epoll_event& ev : epoll_) {
      if (ev.data.ptr == nullptr) {
        PopFunc(pipe_[0]);
      } else {
        auto* eh = static_cast<EventHandler*>(ev.data.ptr);
        if (eh->event_loop_ == this) {
          eh->OnEvent(this, ev.events);
          if (eh->event_loop_ == this) Refresh(eh);
        }
        eh->DecRef();
      }
    }
    while (true) {
      auto* eh = static_cast<EventHandler*>(expire_.head());
      if (!eh || eh->deadline_ > Clock::now()) break;
      assert(eh->event_loop_ == this);
      eh->IncRef();
      eh->OnTimeout(this);
      if (eh->event_loop_ == this) Refresh(eh);
      eh->DecRef();
    }
  }
}

void EventLoop::Refresh(EventHandler* eh) {
  assert(eh);
  assert(eh->event_loop_ == this);
  eh->deadline_ = Clock::now() + timeout_;
  if (auto* tail = static_cast<EventHandler*>(expire_.tail())) {
    assert(eh->deadline_ >= tail->deadline_);
  }
  expire_.Erase(eh);
  expire_.AddTail(eh);
}

}  // namespace hcproxy
