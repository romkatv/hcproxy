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
#include <optional>
#include <utility>

#include "addr.h"
#include "bits.h"
#include "check.h"
#include "event_loop.h"
#include "logging.h"
#include "sock.h"

namespace hcproxy {

namespace {

int ConnectAsync(const addrinfo& addr) {
  int fd = socket(addr.ai_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (fd < 0) {
    LOG(ERROR) << "socket() failed: " << Errno();
    CHECK(errno == EMFILE || errno == ENFILE || errno == ENOBUFS || errno == ENOMEM) << Errno();
    return -1;
  }
  LOG(INFO) << "[" << fd << "] connecting to " << IpPort(addr);
  int one = 1;
  CHECK(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) == 0) << Errno();
  if (connect(fd, addr.ai_addr, addr.ai_addrlen) != 0 && errno != EINPROGRESS) {
    LOG(WARN) << "[" << fd << "] connect() failed: " << Errno();
    CHECK(close(fd) == 0) << Errno();
    return -1;
  }
  return fd;
}

class ConnectEventHandler : public EventHandler {
 public:
  ConnectEventHandler(int fd, Connector::Callback cb) : EventHandler(fd), cb_(std::move(cb)) {}

  void OnEvent(EventLoop* loop, int events) override {
    if (HasBits(events, EPOLLERR) || HasBits(events, EPOLLOUT)) Finish(loop, SockError(fd()));
  }

  void OnTimeout(EventLoop* loop) override { Finish(loop, ETIME); }

 private:
  void Finish(EventLoop* loop, int err) {
    loop->Remove(this);
    if (err == 0) {
      LOG(INFO) << "[" << fd() << "] connected";
      cb_(fd());
    } else {
      LOG(WARN) << "[" << fd() << "] unable to connect: " << Errno(err);
      CHECK(close(fd()) == 0) << Errno();
      cb_(-1);
    }
  }

  const Connector::Callback cb_;
};

}  // namespace

Connector::Connector(const Options& opt) : event_loop_(*new EventLoop(opt.connect_timeout)) {}

void Connector::Connect(const addrinfo& addr, Callback cb) {
  CHECK(cb);
  int fd = ConnectAsync(addr);
  if (fd < 0) {
    cb(fd);
    return;
  }
  auto* eh = new ConnectEventHandler(fd, std::move(cb));
  event_loop_.ScheduleOrRun([this, eh]() { event_loop_.Add(eh, EPOLLOUT); });
}

}  // namespace hcproxy
