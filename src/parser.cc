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

#include "parser.h"

#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <utility>

#include "bits.h"
#include "check.h"
#include "event_loop.h"
#include "logging.h"
#include "sock.h"

namespace hcproxy {

namespace {

constexpr std::string_view kConnectPrefix = "CONNECT ";

bool EndsWith(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() &&
         memcmp(s.data() + s.size() - suffix.size(), suffix.data(), suffix.size()) == 0;
}

// Handler for an incoming HTTP CONNECT request.
//
// Valid requests are at most Options::max_request_size_bytes in length and match the
// following regular expression: "CONNECT ([^ \r]*).*\r\n\r\n". The capture is host_port.
class ParseEventHandler : public EventHandler {
 public:
  ParseEventHandler(const Parser::Options& opt, int fd, Parser::Callback cb)
      : EventHandler(fd), cb_(std::move(cb)), content_(opt.max_request_size_bytes) {}

  void OnEvent(EventLoop* loop, int events) override {
    if (HasBits(events, EPOLLERR)) {
      LOG(WARN) << "[" << fd() << "] error reading request data: " << Errno(SockError(fd()));
      Finish(loop, "");
    } else if (HasBits(events, EPOLLIN)) {
      if (std::optional<std::string_view> host_port = Read()) {
        Finish(loop, *host_port);
      }
    }
  }

  void OnTimeout(EventLoop* loop) override {
    LOG(WARN) << "[" << fd() << "] timed out waiting for request data";
    Finish(loop, "");
  }

 private:
  void Finish(EventLoop* loop, std::string_view host_port) {
    loop->Remove(this);
    cb_(host_port);
  }

  // Reads data from the socket. Returns:
  //
  //   nullopt: Incomplete request. Must wait for more data from the socket.
  //   ""     : Malformed HTTP CONNECT request. Must close the socket. Must not call Read() again.
  //   else:  : host_port of the HTTP CONNECT request. Must not call Read() again.
  std::optional<std::string_view> Read() {
    CHECK(size_ < content_.size());
    while (true) {
      int ret = read(fd(), content_.data() + size_, content_.size() - size_);
      if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return std::nullopt;
        LOG(WARN) << "[" << fd() << "] error reading request: " << Errno();
        return "";
      }
      // Verify that the request starts with kConnectPrefix.
      if (size_ < kConnectPrefix.size()) {
        size_t n = std::min<size_t>(ret, kConnectPrefix.size() - size_);
        if (memcmp(content_.data() + size_, kConnectPrefix.data() + size_, n) != 0) {
          LOG(WARN) << "[" << fd() << "] invalid request prefix";
          return "";
        }
      }
      size_ += ret;
      std::string_view req(content_.data(), size_);
      if (EndsWith(req, "\r\n\r\n")) {
        size_t start = kConnectPrefix.size();
        std::string_view host_port = req.substr(start, req.find_first_of(" \r", start) - start);
        if (host_port.empty()) {
          LOG(WARN) << "[" << fd() << "] empty host:port in the request";
        } else {
          LOG(INFO) << "[" << fd() << "] CONNECT " << host_port;
        }
        return host_port;
      }
      if (ret == 0) {
        LOG(WARN) << "[" << fd() << "] incomplete request";
        return "";
      }
      if (size_ == content_.size()) {
        LOG(WARN) << "[" << fd() << "] request too big";
        return "";
      }
    }
  }

  const Parser::Callback cb_;
  std::vector<char> content_;
  size_t size_ = 0;
};

}  // namespace

Parser::Parser(Options opt)
    : opt_(std::move(opt)), event_loop_(*new EventLoop(opt_.accept_timeout)) {}

void Parser::ParseRequest(int fd, Callback cb) {
  CHECK(fd >= 0);
  CHECK(cb);
  auto* eh = new ParseEventHandler(opt_, fd, std::move(cb));
  event_loop_.ScheduleOrRun([this, eh]() { event_loop_.Add(eh, EPOLLIN); });
}

}  // namespace hcproxy
