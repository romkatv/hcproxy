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
    // Close the incoming connection if unable to read the full HTTP CONNECT request
    // within this time.
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
