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

#include "acceptor.h"

#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>

#include "error.h"

namespace hcproxy {

namespace {

addrinfo* Resolve(const char* host, uint16_t port) {
  addrinfo* res;
  addrinfo hint = {};
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_PASSIVE;
  HCP_CHECK(getaddrinfo(host, std::to_string(port).c_str(), &hint, &res) == 0);
  return res;
}

void SetSockOpt(int fd, int level, int optname) {
  int one = 1;
  HCP_CHECK(setsockopt(fd, level, optname, &one, sizeof(one)) == 0);
}

}  // namespace

Acceptor::Acceptor(const Options& opt) {
  addrinfo* addr = Resolve(opt.listen_addr.c_str(), opt.listen_port);
  HCP_CHECK((fd_ = socket(addr->ai_family, SOCK_STREAM, 0)) >= 0);
  SetSockOpt(fd_, SOL_SOCKET, SO_REUSEADDR);
  HCP_CHECK(bind(fd_, addr->ai_addr, addr->ai_addrlen) == 0);
  HCP_CHECK(listen(fd_, opt.accept_queue_size) == 0);
  freeaddrinfo(addr);
}

Acceptor::~Acceptor() { HCP_CHECK(close(fd_) == 0); }

int Acceptor::Accept() {
  int res;
  // Note: This will crash if we reach the open file descriptor limit.
  HCP_CHECK((res = accept4(fd_, nullptr, nullptr, SOCK_NONBLOCK)) >= 0);
  SetSockOpt(res, IPPROTO_TCP, TCP_NODELAY);
  return res;
}

}  // namespace hcproxy
