#ifndef ROMKATV_HCPROXY_PARSER_H_
#define ROMKATV_HCPROXY_PARSER_H_

#include <stddef.h>
#include <chrono>
#include <functional>
#include <optional>
#include <string>

#include "time.h"

namespace hcproxy {

class EventLoop;

class Parser {
 public:
  struct Options {
    // Close the incoming connection if its HTTP CONNECT request is longer than this.
    size_t max_request_size_bytes = 1024;
    // Close the incoming connection if unable to read anything from it for this long
    // before we've received the full HTTP CONNECT request.
    Duration accept_timeout = std::chrono::seconds(5);
  };

  using Callback = std::function<void(std::string_view)>;

  explicit Parser(Options opt);
  Parser(Parser&&) = delete;
  ~Parser() = delete;

  // Reads and parses an HTTP CONNECT request from the specified socket file descriptor.
  // On success, calls `cb` with host_port from the request as the argument. On error,
  // calls `cb` with empty string as the argument.
  //
  // Does not block.
  void ParseRequest(int fd, Callback cb);

 private:
  const Options opt_;
  EventLoop& event_loop_;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_PARSER_H_
