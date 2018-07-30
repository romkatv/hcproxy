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

#ifndef ROMKATV_HCPROXY_CONNECTOR_H_
#define ROMKATV_HCPROXY_CONNECTOR_H_

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <chrono>
#include <functional>

#include "event_loop.h"
#include "time.h"

namespace hcproxy {

class Connector {
 public:
  struct Options {
    // Close the client connection if unable to establish a server connection
    // within this time. More specifically, this is how much time we allow
    // for the socket to become writable after we call connect() on it.
    Duration connect_timeout = std::chrono::seconds(10);
  };

  explicit Connector(const Options& opt);
  Connector(Connector&&) = delete;
  ~Connector() = delete;

  using Callback = std::function<void(int)>;

  // Creates a socket and attempts to connect it to the server at the specified
  // address. Then calls `cb` with the newly created socket file descriptor as
  // argument, or with -1 on error.
  //
  // Does not block.
  void Connect(const addrinfo& addr, Callback cb);

 private:
  EventLoop& event_loop_;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_CONNECTOR_H_
