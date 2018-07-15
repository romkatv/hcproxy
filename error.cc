#include "error.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <sstream>

namespace hcproxy {
namespace internal_error {

void CheckErrno(bool cond, const char* expr, const char* file, int line) {
  if (cond) return;
  int e = errno;
  char buf[256];
  errno = 0;
  const char* desc = strerror_r(e, buf, sizeof(buf));
  if (errno != 0) desc = "unknown error";
  fprintf(stderr, "FATAL %s:%d: %s: %s\n", file, line, expr, desc);
  std::abort();
}

}  // namespace internal_error
}  // namespace hcproxy
