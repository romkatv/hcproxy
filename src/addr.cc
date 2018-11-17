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

namespace hcproxy {

const char* IP(const sockaddr& addr) {
  return inet_ntoa(reinterpret_cast<const sockaddr_in&>(addr).sin_addr);
}

const char* IP(const addrinfo& addr) { return IP(*addr.ai_addr); }

}  // namespace hcproxy
