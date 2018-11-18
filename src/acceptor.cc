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

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>

#include "addr.h"
#include "check.h"
#include "logging.h"

namespace hcproxy {

namespace {

addrinfo* Resolve(const char* host, uint16_t port) {
  addrinfo* res;
  addrinfo hint = {};
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_PASSIVE;
  int ret;
  CHECK((ret = getaddrinfo(host, std::to_string(port).c_str(), &hint, &res)) == 0)
      << gai_strerror(ret);
  CHECK(res->ai_addr->sa_family == AF_INET);
  return res;
}

void SetSockOpt(int fd, int level, int optname) {
  int one = 1;
  CHECK(setsockopt(fd, level, optname, &one, sizeof(one)) == 0) << Errno();
}

}  // namespace

Acceptor::Acceptor(const Options& opt) {
  addrinfo* addr = Resolve(opt.listen_addr.c_str(), opt.listen_port);
  LOG(INFO) << "Listening on " << IpPort(*addr);
  CHECK((fd_ = socket(addr->ai_family, SOCK_STREAM, 0)) >= 0) << Errno();
  SetSockOpt(fd_, SOL_SOCKET, SO_REUSEADDR);
  CHECK(bind(fd_, addr->ai_addr, addr->ai_addrlen) == 0) << Errno();
  CHECK(listen(fd_, opt.accept_queue_size) == 0) << Errno();
  freeaddrinfo(addr);
}

Acceptor::~Acceptor() { CHECK(close(fd_) == 0) << Errno(); }

int Acceptor::Accept() {
  while (true) {
    struct sockaddr addr = {};
    socklen_t addrlen = sizeof(addr);
    int conn = accept4(fd_, &addr, &addrlen, SOCK_NONBLOCK);
    if (conn >= 0) {
      CHECK(addrlen == sizeof(addr));
      CHECK(addr.sa_family == AF_INET);
      LOG(INFO) << "[" << conn << "] accepted connection from " << IpPort(addr);
      SetSockOpt(conn, IPPROTO_TCP, TCP_NODELAY);
      return conn;
    }
    LOG(ERROR) << "accept4() failed: " << Errno();
    CHECK(errno == EMFILE || errno == ENFILE || errno == ENOBUFS || errno == ENOMEM) << Errno();
  }
}

}  // namespace hcproxy
