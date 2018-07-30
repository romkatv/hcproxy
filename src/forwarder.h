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

#ifndef ROMKATV_HCPROXY_FORWARDER_H_
#define ROMKATV_HCPROXY_FORWARDER_H_

#include <stddef.h>
#include <chrono>

#include "event_loop.h"
#include "time.h"

namespace hcproxy {

class Forwarder {
 public:
  struct Options {
    // Size of the buffer that holds data flowing from client to server.
    // Each connection has its own buffer of this kind.
    size_t client_to_server_buffer_size_bytes = 4 << 10;
    // Size of the buffer that holds data flowing from server to client.
    // Each connection has its own buffer of this kind.
    size_t server_to_client_buffer_size_bytes = 8 << 10;
    // If nothing gets received from or sent to a socket (either client
    // or server), close the connection.
    Duration read_write_timeout = std::chrono::seconds(300);
  };

  explicit Forwarder(Options opt);
  Forwarder(Forwarder&&) = delete;
  ~Forwarder();

  // First sends HTTP 200 response to the client. Then bidirectionally proxies
  // raw bytes between the two sockets.
  //
  // Does not block.
  void Forward(int client_fd, int server_fd);

 private:
  const Options opt_;
  EventLoop& event_loop_;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_FORWARDER_H_
