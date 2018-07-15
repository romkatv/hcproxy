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
  int fd;
  // Note: This will crash if we reach the open file descriptor limit.
  HCP_CHECK((fd = socket(addr.ai_family, SOCK_STREAM | SOCK_NONBLOCK, 0)) >= 0);
  int one = 1;
  HCP_CHECK(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) == 0);
  if (fd >= 0) {
    if (connect(fd, addr.ai_addr, addr.ai_addrlen) != 0 && errno != EINPROGRESS) {
      HCP_CHECK(close(fd) == 0);
      return -1;
    }
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
