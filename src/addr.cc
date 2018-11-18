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

#include "addr.h"

#include "check.h"

namespace hcproxy {

namespace {

const sockaddr_in& Cast(const sockaddr& addr) {
  CHECK(addr.sa_family == AF_INET);
  return reinterpret_cast<const sockaddr_in&>(addr);
}

const sockaddr_in& Cast(const addrinfo& addr) {
  CHECK(addr.ai_addr);
  return Cast(*addr.ai_addr);
}

}  // namespace

IpPort::IpPort(const sockaddr_in& addr) : addr(addr) {}
IpPort::IpPort(const sockaddr& addr) : addr(Cast(addr)) {}
IpPort::IpPort(const addrinfo& addr) : addr(Cast(addr)) {}

std::ostream& operator<<(std::ostream& strm, const IpPort& x) {
  return strm << inet_ntoa(x.addr.sin_addr) << ':' << ntohs(x.addr.sin_port);
}

}  // namespace hcproxy
