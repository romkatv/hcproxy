#ifndef ROMKATV_HCPROXY_BITS_H_
#define ROMKATV_HCPROXY_BITS_H_

namespace hcproxy {

inline bool HasBits(int bitmask, int bits) { return (bitmask & bits) == bits; }

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_BITS_H_
