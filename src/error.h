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
