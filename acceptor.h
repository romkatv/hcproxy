#ifndef ROMKATV_HCPROXY_ACCEPTOR_H_
#define ROMKATV_HCPROXY_ACCEPTOR_H_

#include <stddef.h>
#include <cstdint>
#include <string>

namespace hcproxy {

class Acceptor {
 public:
  struct Options {
    // Listen for incoming connections on this address.
    std::string listen_addr = "0.0.0.0";
    // Listen for incoming connections on this port.
    std::uint16_t listen_port = 8889;
    // Queue up to this many incoming, not yet accepted, connections.
    // Any extra incoming connections will get rejected.
    size_t accept_queue_size = 64;
  };

  explicit Acceptor(const Options& opt);
  Acceptor(Acceptor&&) = delete;
  ~Acceptor();

  // Blocks until there is an incoming connection and returns it.
  // The result is always a valid socket file description. Must not
  // be called concurrently.
  int Accept();

 private:
  int fd_;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_ACCEPTOR_H_
