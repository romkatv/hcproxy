#include "parser.h"

#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cassert>
#include <utility>

#include "bits.h"
#include "event_loop.h"

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
      Finish(loop, "");
    } else if (HasBits(events, EPOLLIN)) {
      if (std::optional<std::string_view> host_port = Read()) {
        Finish(loop, *host_port);
      }
    }
  }

  void OnTimeout(EventLoop* loop) override { Finish(loop, ""); }

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
    assert(size_ < content_.size());
    while (true) {
      int ret = read(fd(), content_.data() + size_, content_.size() - size_);
      if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return std::nullopt;
        return "";
      }
      // Verify that the request starts with kConnectPrefix.
      if (size_ < kConnectPrefix.size()) {
        size_t n = std::min<size_t>(ret, kConnectPrefix.size() - size_);
        if (memcmp(content_.data() + size_, kConnectPrefix.data() + size_, n) != 0) return "";
      }
      size_ += ret;
      std::string_view req(content_.data(), size_);
      if (EndsWith(req, "\r\n\r\n")) {
        size_t start = kConnectPrefix.size();
        return req.substr(start, req.find_first_of(" \r", start) - start);
      }
      if (ret == 0 || size_ == content_.size()) return "";
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
  assert(fd >= 0);
  assert(cb);
  auto* eh = new ParseEventHandler(opt_, fd, std::move(cb));
  event_loop_.ScheduleOrRun([this, eh]() { event_loop_.Add(eh, EPOLLIN); });
}

}  // namespace hcproxy
