#ifndef ROMKATV_HCPROXY_TIME_H_
#define ROMKATV_HCPROXY_TIME_H_

#include <chrono>

namespace hcproxy {

using Clock = std::chrono::steady_clock;
using Time = std::chrono::time_point<Clock>;
using Duration = Clock::duration;

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_TIME_H_
