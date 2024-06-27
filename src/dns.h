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

#ifndef ROMKATV_HCPROXY_DNS_H_
#define ROMKATV_HCPROXY_DNS_H_

#include <netdb.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>

#include "thread_pool.h"
#include "time.h"

namespace hcproxy {

class DnsResolver {
 public:
  struct Options {
    // Use this many threads to perform DNS resolution. DNS resolution is done by
    // means of getaddrinfo(), which is synchronous. This option specifies the
    // maximum number of concurrent calls to getaddrinfo(). All concurrent calls
    // are for different addresses. Concurrent calls to DnsResolver::Resolve() for
    // the same address are collapsed so that just one call to getaddrinfo() is
    // made and therefore just one thread is used.
    size_t num_dns_resolution_threads = 8;
    // Do not use the results of getaddrinfo() that were obtained longer than
    // this much time ago. If getaddrinfo() starts failing for an address for which
    // it has worked before, we'll forget the last successful result after this much
    // time.
    Duration dns_cache_ttl = std::chrono::seconds(300);
    // Call getaddrinfo() for the addresses we care about this often to obtain
    // fresh mapping.
    Duration dns_cache_refresh_period = std::chrono::seconds(75);
    // Whenever DnsResolver::Resolve() is called for a given address, keep resolving
    // the address periodically for this long afterwards. This is meant to keep the
    // cache fresh for addresses we care about.
    Duration dns_cache_refresh_duration = std::chrono::seconds(3600);
  };

  using Callback = std::function<void(std::shared_ptr<const addrinfo>)>;

  explicit DnsResolver(Options opt);
  DnsResolver(DnsResolver&&) = delete;

  // The callback is called exactly once. It may be called synchronously.
  // Its argument is null on error.
  //
  // If `host_port` isn't of the form "host_or_ip:port", you'll get an error.
  //
  // Does not block.
  void Resolve(std::string_view host_port, Callback cb);

 private:
  struct CacheData {
    void Use(const Time& now);

    std::vector<Callback> callbacks;
    std::shared_ptr<const addrinfo> addr;
    Time used_at;
    Time resolved_at;
    Time successfully_resolved_at;
    int64_t use_count = 0;
  };

  // We use std::map rather than std::unordered_map because the former supports
  // heterogenous lookup. We pass std::string_view to find() despite having
  // std::string as key type.
  using Cache = std::map<std::string, CacheData, std::less<>>;

  void ProcessCacheEntry(Cache::iterator it);

  const Options opt_;
  std::mutex mutex_;
  Cache cache_;
  ThreadPool threads_;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_DNS_H_
