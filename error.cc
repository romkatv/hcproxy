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
