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
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <limits>
#include <string>
#include <utility>

#include "bits.h"
#include "error.h"
#include "event_loop.h"

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
  explicit Buffer(size_t size_bytes) {
    assert(size_bytes > 0 && size_bytes < std::numeric_limits<int>::max());
    // Note: This will crash if we reach the open file descriptor limit.
    HCP_CHECK(pipe(pipe_) == 0);
    HCP_CHECK((capacity_ = fcntl(pipe_[0], F_SETPIPE_SZ, size_bytes)) > 0);
    for (int fd : pipe_) {
      assert(fd >= 0);
      assert(fcntl(fd, F_GETPIPE_SZ) == capacity_);
    }
  }

  Buffer(Buffer&&) = delete;

  ~Buffer() {
    for (int fd : pipe_) {
      if (fd >= 0) HCP_CHECK(close(fd) == 0);
    }
  }

  void Write(std::string_view data) {
    assert(size_ >= 0);
    assert(size_ <= capacity_);
    assert(data.size() <= static_cast<size_t>(capacity_ - size_));
    assert(pipe_[1] >= 0);
    HCP_CHECK(write(pipe_[1], data.data(), data.size()) == static_cast<ssize_t>(data.size()));
    size_ += data.size();
    assert(size_ >= 0);
    assert(size_ <= capacity_);
  }

  IoStatus WriteFrom(int fd) {
    assert(pipe_[1] >= 0);
    assert(size_ >= 0);
    assert(size_ <= capacity_);
    if (size_ == capacity_) return IoStatus::kNoOp;
    ssize_t ret = splice(fd, nullptr, pipe_[1], nullptr, capacity_ - size_,
                         SPLICE_F_NONBLOCK | SPLICE_F_MOVE);
    if (ret < 0) {
      if (errno == EAGAIN) return IoStatus::kNoOp;
      HCP_CHECK(close(pipe_[1]) == 0);
      pipe_[1] = -1;
      return IoStatus::kError;
    }
    if (ret == 0) {
      HCP_CHECK(close(pipe_[1]) == 0);
      pipe_[1] = -1;
      return IoStatus::kEof;
    }
    size_ += ret;
    assert(size_ >= 0);
    assert(size_ <= capacity_);
    return IoStatus::kData;
  }

  IoStatus ReadTo(int fd) {
    assert(pipe_[0] >= 0);
    assert(size_ >= 0);
    if (size_ == 0) {
      if (pipe_[1] >= 0) return IoStatus::kNoOp;
      HCP_CHECK(close(pipe_[0]) == 0);
      pipe_[0] = -1;
      return IoStatus::kEof;
    }
    ssize_t ret = splice(pipe_[0], nullptr, fd, nullptr, size_, SPLICE_F_NONBLOCK | SPLICE_F_MOVE);
    assert(ret != 0);
    if (ret < 0) {
      if (errno == EAGAIN) return IoStatus::kNoOp;
      HCP_CHECK(close(pipe_[0]) == 0);
      pipe_[0] = -1;
      return IoStatus::kError;
    }
    size_ -= ret;
    assert(size_ >= 0);
    assert(size_ <= capacity_);
    return IoStatus::kData;
  }

 private:
  int capacity_;
  int size_ = 0;
  int pipe_[2];
};

class LinkEventHandler : public EventHandler {
 public:
  static void New(EventLoop* loop, int client_fd, int server_fd, const Forwarder::Options& opt) {
    auto* client = new LinkEventHandler(client_fd, opt.server_to_client_buffer_size_bytes);
    auto* server = new LinkEventHandler(server_fd, opt.client_to_server_buffer_size_bytes);
    client->other_ = server;
    server->other_ = client;
    client->IncRef();
    server->IncRef();
    loop->Add(client, EPOLLIN | EPOLLOUT | EPOLLET);
    loop->Add(server, EPOLLIN | EPOLLOUT | EPOLLET);
    client->out_.Write(kResponse);
  }

  void OnEvent(EventLoop* loop, int events) override {
    auto Process = [&]() {
      if (HasBits(events, EPOLLERR)) return Terminate(loop);
      if (HasBits(events, EPOLLOUT)) {
        if (!ForwardFromOther(loop)) return;
      }
      if (HasBits(events, EPOLLIN)) {
        if (!other_->ForwardFromOther(loop)) return;
      }
    };
    other_->IncRef();
    Process();
    other_->DecRef();
  }

  void OnTimeout(EventLoop* loop) override {
    other_->IncRef();
    Terminate(loop);
    other_->DecRef();
  }

 private:
  LinkEventHandler(int fd, size_t write_buffer_size_bytes)
      : EventHandler(fd), out_(write_buffer_size_bytes) {}

  ~LinkEventHandler() override { assert(!readable_ && !writable_); }

  // Forwards as much data as possible from the other link to this.
  // On error, terminates both links and returns false.
  bool ForwardFromOther(EventLoop* loop) {
    while (true) {
      bool io = false;
      if (other_->readable_) {
        switch (out_.WriteFrom(other_->fd())) {
          case IoStatus::kData:
            io = true;
            break;
          case IoStatus::kEof:
            io = true;
            other_->CloseForReading(loop);
            break;
          case IoStatus::kError:
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
            io = true;
            CloseForWriting(loop);
            break;
          case IoStatus::kError:
            Terminate(loop);
            return false;
          case IoStatus::kNoOp:
            break;
        }
      }
      if (!io) return true;
    }
  }

  void CloseForReading(EventLoop* loop) {
    assert(readable_);
    if (writable_) {
      readable_ = false;
      loop->Modify(this, EPOLLOUT | EPOLLET);
      HCP_CHECK(shutdown(fd(), SHUT_RD) == 0 || errno == ENOTCONN);
    } else {
      Close(loop);
    }
  }

  void CloseForWriting(EventLoop* loop) {
    assert(writable_);
    if (readable_) {
      writable_ = false;
      loop->Modify(this, EPOLLIN | EPOLLET);
      HCP_CHECK(shutdown(fd(), SHUT_WR) == 0 || errno == ENOTCONN);
    } else {
      Close(loop);
    }
  }

  // Closes this link if it's still open.
  void Close(EventLoop* loop) {
    if (readable_ || writable_) {
      readable_ = false;
      writable_ = false;
      other_->DecRef();
      loop->Remove(this);
      HCP_CHECK(close(fd()) == 0);
    }
  };

  // Abnormally closes this and the other link, discarding all buffered data. Used on IO errors.
  void Terminate(EventLoop* loop) {
    Close(loop);
    other_->Close(loop);
  }

  Buffer out_;
  bool readable_ = true;
  bool writable_ = true;
  LinkEventHandler* other_ = nullptr;
};

}  // namespace

Forwarder::Forwarder(Options opt)
    : opt_(std::move(opt)), event_loop_(*new EventLoop(opt_.read_write_timeout)) {}

void Forwarder::Forward(int client_fd, int server_fd) {
  assert(client_fd >= 0);
  assert(server_fd >= 0);
  event_loop_.ScheduleOrRun(
      [=]() { LinkEventHandler::New(&event_loop_, client_fd, server_fd, opt_); });
}

}  // namespace hcproxy
