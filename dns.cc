#include "dns.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>

namespace hcproxy {

namespace {

std::shared_ptr<const addrinfo> ResolveSync(const std::string& host_port) {
  const char* port = std::strchr(host_port.c_str(), ':');
  if (!port) return nullptr;
  std::string host(host_port.c_str(), port);
  ++port;
  addrinfo* res;
  addrinfo hint = {};
  hint.ai_family = AF_INET;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_NUMERICSERV;
  if (getaddrinfo(host.c_str(), port, &hint, &res) != 0) return nullptr;
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
