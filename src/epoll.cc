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

#include "epoll.h"

#include <unistd.h>
#include <algorithm>
#include <cassert>

#include "error.h"

namespace hcproxy {

using ::std::chrono::ceil;
using ::std::chrono::milliseconds;

EPoll::EPoll() { HCP_CHECK((epoll_ = epoll_create1(0)) >= 0); }

EPoll::~EPoll() { HCP_CHECK(close(epoll_) == 0); }

void EPoll::Add(int fd, int events, void* data) {
  epoll_event event = {};
  event.events = events;
  event.data.ptr = data;
  HCP_CHECK(epoll_ctl(epoll_, EPOLL_CTL_ADD, fd, &event) == 0);
  ++total_events_;
}

void EPoll::Remove(int fd) {
  assert(total_events_ > 0);
  epoll_event event;
  HCP_CHECK(epoll_ctl(epoll_, EPOLL_CTL_DEL, fd, &event) == 0);
  --total_events_;
}

void EPoll::Modify(int fd, int events, void* data) {
  assert(total_events_ > 0);
  epoll_event event = {};
  event.events = events;
  event.data.ptr = data;
  HCP_CHECK(epoll_ctl(epoll_, EPOLL_CTL_MOD, fd, &event) == 0);
}

void EPoll::Wait(std::optional<Duration> timeout) {
  if (total_events_ > events_.size()) {
    events_.resize(total_events_);
    events_.resize(events_.capacity());
  }
  int ms = timeout ? std::max<int>(0, ceil<milliseconds>(*timeout).count()) : -1;
  HCP_CHECK((ready_events_ = epoll_wait(epoll_, events_.data(), events_.size(), ms)) >= 0);
}

}  // namespace hcproxy
