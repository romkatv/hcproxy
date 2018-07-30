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

#ifndef ROMKATV_HCPROXY_EPOLL_H_
#define ROMKATV_HCPROXY_EPOLL_H_

#include <sys/epoll.h>
#include <cstddef>
#include <optional>
#include <vector>

#include "time.h"

namespace hcproxy {

// Thread-compatible. NOT thread-safe.
class EPoll {
 public:
  EPoll();
  EPoll(EPoll&&) = delete;
  ~EPoll();

  void Add(int fd, int events, void* data);
  void Remove(int fd);
  void Modify(int fd, int events, void* data);
  void Wait(std::optional<Duration> timeout);

  // The events are invalidated by Wait() and nothing else.
  const epoll_event* begin() const { return events_.data(); }
  const epoll_event* end() const { return begin() + ready_events_; }

 private:
  int epoll_;
  std::vector<epoll_event> events_;
  size_t total_events_ = 0;
  int ready_events_ = 0;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_EPOLL_H_
