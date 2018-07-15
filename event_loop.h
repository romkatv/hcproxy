#ifndef ROMKATV_HCPROXY_EVENT_LOOP_H_
#define ROMKATV_HCPROXY_EVENT_LOOP_H_

#include <cassert>
#include <cstdint>
#include <functional>
#include <thread>

#include "epoll.h"
#include "list.h"
#include "time.h"

namespace hcproxy {

class EventLoop;

class EventHandler : private Node {
 public:
  explicit EventHandler(int fd) : fd_(fd) { assert(fd_ >= 0); }
  EventHandler(EventHandler&&) = delete;

  int fd() const { return fd_; }

  // Not thread-safe.
  void IncRef() {
    assert(ref_count_ >= 0);
    ++ref_count_;
  }

  // Not thread-safe.
  void DecRef() {
    assert(ref_count_ > 0);
    if (--ref_count_ == 0) delete this;
  }

  // Called from the EventLoop thread. The caller holds a ref count during the call.
  virtual void OnEvent(EventLoop* loop, int events) = 0;
  // Called from the EventLoop thread. The caller holds a ref count during the call.
  virtual void OnTimeout(EventLoop* loop) = 0;

 protected:
  virtual ~EventHandler() {
    assert(event_loop_ == nullptr);
    assert(ref_count_ == 0);
  }

 private:
  friend class EventLoop;

  const int fd_;
  Time deadline_;
  // Null iff this event handler isn't registered in an event loop.
  const EventLoop* event_loop_ = nullptr;
  int64_t ref_count_ = 0;
};

// A wrapper around a thread + epoll.
class EventLoop {
 public:
  explicit EventLoop(Duration timeout);
  EventLoop(EventLoop&&) = delete;
  ~EventLoop() = delete;

  // Can be called only from the Loop() thread.
  void Add(EventHandler* eh, int events);

  // Can be called only from the Loop() thread.
  // OnEvent() and OnTimeout() won't fire until Add() is called again.
  void Remove(EventHandler* eh);

  // Can be called only from the Loop() thread.
  void Modify(EventHandler* eh, int events);

  // Cannot be called from the Loop() thread. Can be called concurrently.
  void Schedule(std::function<void()> f);

  // When called from the Loop() thread, invokes `f` synchronously.
  // Otherwise calls Schedule(f).
  void ScheduleOrRun(std::function<void()> f);

 private:
  void Loop();

  void Refresh(EventHandler* eh);

  int pipe_[2];
  EPoll epoll_;
  // Event handlers sorted by expiration time. The head is the first to expire.
  List expire_;
  Duration timeout_;
  std::thread loop_;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_EVENT_LOOP_H_
