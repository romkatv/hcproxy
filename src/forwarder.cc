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

#include "forwarder.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits>
#include <string>
#include <utility>

#include "bits.h"
#include "check.h"
#include "event_loop.h"
#include "logging.h"
#include "sock.h"

namespace hcproxy {

namespace {

constexpr std::string_view kResponse = "HTTP/1.1 200 OK\r\n\r\n";

enum class IoStatus {
  kData,
  kEof,
  kError,
  kNoOp,
};

class Buffer {
 public:
  Buffer() {}
  Buffer(Buffer&&) = delete;

  bool Init(size_t size_bytes) {
    CHECK(size_bytes > 0 && size_bytes < std::numeric_limits<int>::max());
    if (pipe(pipe_) != 0) {
      LOG(ERROR) << "pipe() failed: " << Errno();
      CHECK(errno == EMFILE || errno == ENFILE) << Errno();
      for (int& fd : pipe_) fd = -1;
      return false;
    }
    CHECK((capacity_ = fcntl(pipe_[0], F_SETPIPE_SZ, size_bytes)) > 0) << Errno();
    for (int fd : pipe_) {
      CHECK(fd >= 0);
      CHECK(fcntl(fd, F_GETPIPE_SZ) == capacity_);
    }
    return true;
  }

  ~Buffer() {
    for (int fd : pipe_) {
      if (fd >= 0) CHECK(close(fd) == 0) << Errno();
    }
  }

  void Write(std::string_view data) {
    CHECK(size_ >= 0);
    CHECK(size_ <= capacity_);
    CHECK(data.size() <= static_cast<size_t>(capacity_ - size_));
    CHECK(pipe_[1] >= 0);
    CHECK(write(pipe_[1], data.data(), data.size()) == static_cast<ssize_t>(data.size()))
        << Errno();
    size_ += data.size();
    CHECK(size_ >= 0);
    CHECK(size_ <= capacity_);
  }

  IoStatus WriteFrom(int fd) {
    CHECK(pipe_[1] >= 0);
    CHECK(size_ >= 0);
    CHECK(size_ <= capacity_);
    if (size_ == capacity_) return IoStatus::kNoOp;
    ssize_t ret = splice(fd, nullptr, pipe_[1], nullptr, capacity_ - size_,
                         SPLICE_F_NONBLOCK | SPLICE_F_MOVE);
    if (ret < 0) {
      if (errno == EAGAIN) return IoStatus::kNoOp;
      CHECK(close(pipe_[1]) == 0) << Errno();
      pipe_[1] = -1;
      return IoStatus::kError;
    }
    if (ret == 0) {
      CHECK(close(pipe_[1]) == 0) << Errno();
      pipe_[1] = -1;
      return IoStatus::kEof;
    }
    size_ += ret;
    CHECK(size_ >= 0);
    CHECK(size_ <= capacity_);
    return IoStatus::kData;
  }

  IoStatus ReadTo(int fd) {
    CHECK(pipe_[0] >= 0);
    CHECK(size_ >= 0);
    if (size_ == 0) {
      if (pipe_[1] >= 0) return IoStatus::kNoOp;
      CHECK(close(pipe_[0]) == 0) << Errno();
      pipe_[0] = -1;
      return IoStatus::kEof;
    }
    // There is a bug in splice() that makes it clear the pipe upon returning an error, be it EAGAIN
    // or something else. To work around it, we do two things. First, we issue a zero-byte write
    // to check whether the socket is writable and thus to reduce the chance splice() will return
    // EAGAIN. Second, we terminate the connection if we trigger the bug in splice().
    if (write(fd, "", 0) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return IoStatus::kNoOp;
    }
    ssize_t ret = splice(pipe_[0], nullptr, fd, nullptr, size_, SPLICE_F_NONBLOCK | SPLICE_F_MOVE);
    CHECK(ret != 0);
    if (ret < 0) {
      if (errno == EAGAIN) {
        int len = 0;
        CHECK(ioctl(pipe_[0], FIONREAD, &len) == 0) << Errno();
        if (len == size_) return IoStatus::kNoOp;
      }
      CHECK(close(pipe_[0]) == 0) << Errno();
      pipe_[0] = -1;
      return IoStatus::kError;
    }
    size_ -= ret;
    CHECK(size_ >= 0);
    CHECK(size_ <= capacity_);
    return IoStatus::kData;
  }

 private:
  int capacity_;
  int size_ = 0;
  int pipe_[2] = {-1, -1};
};

class LinkEventHandler : public EventHandler {
 public:
  static void New(EventLoop* loop, int client_fd, int server_fd, const Forwarder::Options& opt) {
    LOG(INFO) << "Forwarding traffic: "
              << "[" << client_fd << "] (client)"
              << " <=> "
              << "[" << server_fd << "] (server)";
    auto* client = new LinkEventHandler(client_fd, "client");
    auto* server = new LinkEventHandler(server_fd, "server");
    if (!client->out_.Init(opt.server_to_client_buffer_size_bytes) ||
        !server->out_.Init(opt.client_to_server_buffer_size_bytes)) {
      for (auto* p : {client, server}) {
        LOG(INFO) << "[" << p->fd() << "] (" << p->name_ << ") close";
        CHECK(close(p->fd()) == 0) << Errno();
        p->readable_ = false;
        p->writable_ = false;
        delete p;
      }
      return;
    }
    client->other_ = server;
    server->other_ = client;
    client->IncRef();
    server->IncRef();
    loop->Add(client, EPOLLIN | EPOLLOUT | EPOLLET);
    loop->Add(server, EPOLLIN | EPOLLOUT | EPOLLET);
    client->out_.Write(kResponse);
  }

  void OnEvent(EventLoop* loop, int events) override {
    // Returns true if some data has been transferred and the link isn't broken.
    auto Process = [&]() -> bool {
      if (HasBits(events, EPOLLERR)) {
        LOG(INFO) << "[" << fd() << "] (" << name_ << ") "
                  << "connection broke: " << Errno(SockError(fd()));
        Terminate(loop);
        return false;
      }
      return (HasBits(events, EPOLLOUT) && ForwardFromOther(loop)) |
             (HasBits(events, EPOLLIN) && other_->ForwardFromOther(loop));
    };
    other_->IncRef();
    if (Process()) {
      Refresh(loop);
      other_->Refresh(loop);
    }
    other_->DecRef();
  }

  void OnTimeout(EventLoop* loop) override {
    LOG(INFO) << "[" << fd() << "] (" << name_ << ") timed out waiting for IO";
    other_->IncRef();
    Terminate(loop);
    other_->DecRef();
  }

 private:
  LinkEventHandler(int fd, const char* name) : EventHandler(fd), name_(name) {}

  ~LinkEventHandler() override { CHECK(!readable_ && !writable_); }

  // Forwards as much data as possible from the other link to this.
  // Returns true if some data has been transferred and the link isn't broken.
  bool ForwardFromOther(EventLoop* loop) {
    bool res = false;
    while (true) {
      bool io = false;
      if (other_->readable_) {
        switch (out_.WriteFrom(other_->fd())) {
          case IoStatus::kData:
            io = true;
            break;
          case IoStatus::kEof:
            LOG(INFO) << "[" << other_->fd() << "] (" << other_->name_ << ") read EOF";
            io = true;
            other_->CloseForReading(loop);
            break;
          case IoStatus::kError:
            LOG(INFO) << "[" << other_->fd() << "] (" << other_->name_ << ") read error";
            Terminate(loop);
            return false;
          case IoStatus::kNoOp:
            break;
        }
      }
      if (writable_) {
        switch (out_.ReadTo(fd())) {
          case IoStatus::kData:
            io = true;
            break;
          case IoStatus::kEof:
            LOG(INFO) << "[" << fd() << "] (" << name_ << ") write EOF";
            io = true;
            CloseForWriting(loop);
            break;
          case IoStatus::kError:
            LOG(INFO) << "[" << fd() << "] (" << name_ << ") write error";
            Terminate(loop);
            return false;
          case IoStatus::kNoOp:
            break;
        }
      }
      if (io) {
        res = true;
      } else {
        return res;
      }
    }
  }

  void CloseForReading(EventLoop* loop) {
    CHECK(readable_);
    if (writable_) {
      LOG(INFO) << "[" << fd() << "] (" << name_ << ") shutdown(SHUT_RD)";
      readable_ = false;
      loop->Modify(this, EPOLLOUT | EPOLLET);
      CHECK(shutdown(fd(), SHUT_RD) == 0 || errno == ENOTCONN) << Errno();
    } else {
      Close(loop);
    }
  }

  void CloseForWriting(EventLoop* loop) {
    CHECK(writable_);
    if (readable_) {
      LOG(INFO) << "[" << fd() << "] (" << name_ << ") shutdown(SHUT_WR)";
      writable_ = false;
      loop->Modify(this, EPOLLIN | EPOLLET);
      CHECK(shutdown(fd(), SHUT_WR) == 0 || errno == ENOTCONN) << Errno();
    } else {
      Close(loop);
    }
  }

  // Closes this link if it's still open.
  void Close(EventLoop* loop) {
    if (readable_ || writable_) {
      LOG(INFO) << "[" << fd() << "] (" << name_ << ") close";
      readable_ = false;
      writable_ = false;
      other_->DecRef();
      loop->Remove(this);
      CHECK(close(fd()) == 0) << Errno();
    }
  };

  // Abnormally closes this and the other link, discarding all buffered data. Used on IO errors.
  void Terminate(EventLoop* loop) {
    Close(loop);
    other_->Close(loop);
  }

  void Refresh(EventLoop* loop) {
    if (readable_ || writable_) loop->Refresh(this);
  }

  const char* name_;
  Buffer out_;
  bool readable_ = true;
  bool writable_ = true;
  LinkEventHandler* other_ = nullptr;
};

}  // namespace

Forwarder::Forwarder(Options opt)
    : opt_(std::move(opt)), event_loop_(*new EventLoop(opt_.read_write_timeout)) {}

void Forwarder::Forward(int client_fd, int server_fd) {
  CHECK(client_fd >= 0);
  CHECK(server_fd >= 0);
  event_loop_.ScheduleOrRun(
      [=]() { LinkEventHandler::New(&event_loop_, client_fd, server_fd, opt_); });
}

}  // namespace hcproxy
