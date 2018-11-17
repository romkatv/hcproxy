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

#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>

#include "acceptor.h"
#include "addr.h"
#include "check.h"
#include "connector.h"
#include "dns.h"
#include "forwarder.h"
#include "logging.h"
#include "parser.h"

namespace hcproxy {
namespace {

struct Options : Acceptor::Options,
                 Parser::Options,
                 DnsResolver::Options,
                 Connector::Options,
                 Forwarder::Options {
  // Refuse to connect to any port other than these. If empty, allow connections
  // to any port.
  std::unordered_set<std::string_view> allowed_ports = {};
  // If positive, set the maximum number of open file descriptors (NOFILE)
  // to this value on startup. The proxy uses 6 file descriptors per client
  // connection: 2 sockets + 2 pipes (each pipe is 2 file descriptors).
  // When the open file descriptor limit is reached, the proxy will stop
  // accepting new connections.
  rlim_t max_num_open_files = 0;
};

void RunProxy(const Options& opt) {
  auto IsAllowedPort = [&](std::string_view host_port) {
    auto sep = host_port.find(':');
    if (sep == std::string_view::npos) return false;
    std::string_view port = host_port.substr(sep + 1);
    return opt.allowed_ports.empty() || opt.allowed_ports.count(port);
  };

  signal(SIGPIPE, SIG_IGN);

  if (opt.max_num_open_files > 0) {
    struct rlimit lim;
    CHECK(getrlimit(RLIMIT_NOFILE, &lim) == 0) << Errno();
    lim.rlim_cur = opt.max_num_open_files;
    // It'll fail if opt.max_num_open_files is greater than lim.rlim_max.
    CHECK(setrlimit(RLIMIT_NOFILE, &lim) == 0) << Errno();
  }

  auto& acceptor = *new Acceptor(opt);
  auto& parser = *new Parser(opt);
  auto& dns_resolver = *new DnsResolver(opt);
  auto& connector = *new Connector(opt);
  auto& forwarder = *new Forwarder(opt);

  while (true) {
    int client_fd = acceptor.Accept();
    parser.ParseRequest(client_fd, [&, client_fd](std::string_view host_port) {
      if (host_port.empty() || !IsAllowedPort(host_port)) {
        CHECK(close(client_fd) == 0) << Errno();
        return;
      }
      dns_resolver.Resolve(host_port, [&, client_fd](std::shared_ptr<const addrinfo> addr) {
        if (!addr) {
          LOG(WARN) << "[" << client_fd << "] DNS error: " << host_port;
          CHECK(close(client_fd) == 0) << Errno();
          return;
        }
        LOG(INFO) << "[" << client_fd << "] tunnel to " << IP(*addr);
        connector.Connect(*addr, [&, client_fd](int server_fd) {
          if (server_fd < 0) {
            CHECK(close(client_fd) == 0) << Errno();
            return;
          }
          forwarder.Forward(client_fd, server_fd);
        });
      });
    });
  }
}

}  // namespace
}  // namespace hcproxy

int main(int argc, char* argv[]) {
  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << "\n\n"
              << "To customize, modify `opt` in `main()` (defined in " << __FILE__ << ")"
              << " and recompile." << std::endl;
    return 1;
  }
  hcproxy::Options opt;
  hcproxy::RunProxy(opt);
  LOG(FATAL) << "RunProxy is not supposed to return";
  return 1;
}
