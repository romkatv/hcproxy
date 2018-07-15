#ifndef ROMKATV_HCPROXY_ERROR_H_
#define ROMKATV_HCPROXY_ERROR_H_

#include <errno.h>

// The argument must be an expression convertible to bool.
// Does nothing if the expression evalutes to true. Otherwise
// prints an error messages that includes errno and aborts the
// process.
#define HCP_CHECK(cond...) \
  ::hcproxy::internal_error::CheckErrno(!!(cond), #cond, __FILE__, __LINE__)

namespace hcproxy {
namespace internal_error {

void CheckErrno(bool cond, const char* expr, const char* file, int line);

}  // namespace internal_error
}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_ERROR_H_
