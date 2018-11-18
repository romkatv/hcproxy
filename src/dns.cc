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

#include "dns.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>

#include "addr.h"
#include "check.h"
#include "logging.h"

namespace hcproxy {

namespace {

std::shared_ptr<const addrinfo> ResolveSync(const std::string& host_port) {
  const char* port = std::strchr(host_port.c_str(), ':');
  if (!port) {
    LOG(WARN) << "Malformed host:port: " << host_port;
    return nullptr;
  }
  std::string host(host_port.c_str(), port);
  ++port;
  addrinfo* res;
  addrinfo hint = {};
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_NUMERICSERV;
  int ret = getaddrinfo(host.c_str(), port, &hint, &res);
  if (ret != 0) {
    LOG(WARN) << "DNS error for '" << host_port << "': " << gai_strerror(ret);
    return nullptr;
  }
  CHECK(res->ai_addr->sa_family == AF_INET);
  LOG(INFO) << "Resolved " << host_port << " as " << IpPort(*res);
  return std::shared_ptr<const addrinfo>(res, freeaddrinfo);
}

}  // namespace

DnsResolver::DnsResolver(Options opt)
    : opt_(std::move(opt)), threads_(opt_.num_dns_resolution_threads) {}

void DnsResolver::Resolve(std::string_view host_port, Callback cb) {
  const Time now = Clock::now();
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = cache_.find(host_port);
  if (it == cache_.end()) {
    CacheData c;
    c.callbacks.push_back(std::move(cb));
    it = cache_.insert({std::string(host_port), std::move(c)}).first;
    lock.unlock();
    threads_.Schedule(now, [=] { ProcessCacheEntry(it); });
    return;
  }
  CacheData& c = it->second;
  if (!c.callbacks.empty()) {
    c.callbacks.push_back(std::move(cb));
    return;
  }
  std::shared_ptr<const addrinfo> addr;
  if (c.successfully_resolved_at + opt_.dns_cache_ttl > now) {
    addr = c.addr;
  }
  c.used_at = std::max(c.used_at, now);
  lock.unlock();
  cb(std::move(addr));
}

void DnsResolver::ProcessCacheEntry(Cache::iterator it) {
  CacheData& c = it->second;
  Time now = Clock::now();
  std::unique_lock<std::mutex> lock(mutex_);
  if (c.callbacks.empty() && c.used_at + opt_.dns_cache_refresh_duration <= now) {
    cache_.erase(it);
    return;
  }
  if (!c.callbacks.empty() || c.resolved_at + opt_.dns_cache_refresh_period <= now) {
    lock.unlock();
    std::shared_ptr<const addrinfo> addr = ResolveSync(it->first);
    std::vector<Callback> callbacks;
    now = Clock::now();
    lock.lock();
    if (!c.callbacks.empty()) c.used_at = now;
    c.callbacks.swap(callbacks);
    c.resolved_at = now;
    if (addr) {
      c.addr = addr;
      c.successfully_resolved_at = now;
    }
    if (!callbacks.empty()) {
      lock.unlock();
      for (const auto& f : callbacks) f(addr);
      lock.lock();
    }
  }
  Time next = std::min(c.resolved_at + opt_.dns_cache_refresh_period,
                       c.used_at + opt_.dns_cache_refresh_duration);
  lock.unlock();
  threads_.Schedule(next, [=] { ProcessCacheEntry(it); });
}

}  // namespace hcproxy
