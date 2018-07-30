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

#include "connector.h"

#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cassert>
#include <optional>
#include <utility>

#include "bits.h"
#include "error.h"
#include "event_loop.h"

namespace hcproxy {

namespace {

int ConnectAsync(const addrinfo& addr) {
  int fd = socket(addr.ai_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (fd < 0) {
    HCP_CHECK(errno == EMFILE || errno == ENFILE || errno == ENOBUFS || errno == ENOMEM);
    return -1;
  }
  int one = 1;
  HCP_CHECK(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) == 0);
  if (connect(fd, addr.ai_addr, addr.ai_addrlen) != 0 && errno != EINPROGRESS) {
    HCP_CHECK(close(fd) == 0);
    return -1;
  }
  return fd;
}

class ConnectEventHandler : public EventHandler {
 public:
  ConnectEventHandler(int fd, Connector::Callback cb) : EventHandler(fd), cb_(std::move(cb)) {}

  void OnEvent(EventLoop* loop, int events) override {
    if (HasBits(events, EPOLLERR)) {
      Finish(loop, false);
    } else if (HasBits(events, EPOLLOUT)) {
      int error = 0;
      socklen_t len = sizeof(error);
      HCP_CHECK(getsockopt(fd(), SOL_SOCKET, SO_ERROR, &error, &len) == 0 && len == sizeof(error));
      Finish(loop, error == 0);
    }
  }

  void OnTimeout(EventLoop* loop) override { Finish(loop, false); }

 private:
  void Finish(EventLoop* loop, bool connected) {
    loop->Remove(this);
    if (connected) {
      cb_(fd());
    } else {
      HCP_CHECK(close(fd()) == 0);
      cb_(-1);
    }
  }

  const Connector::Callback cb_;
};

}  // namespace

Connector::Connector(const Options& opt) : event_loop_(*new EventLoop(opt.connect_timeout)) {}

void Connector::Connect(const addrinfo& addr, Callback cb) {
  assert(cb);
  int fd = ConnectAsync(addr);
  if (fd < 0) {
    cb(fd);
    return;
  }
  auto* eh = new ConnectEventHandler(fd, std::move(cb));
  event_loop_.ScheduleOrRun([this, eh]() { event_loop_.Add(eh, EPOLLOUT); });
}

}  // namespace hcproxy
