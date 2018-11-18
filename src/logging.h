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

#ifndef ROMKATV_HCPROXY_LOGGING_H_
#define ROMKATV_HCPROXY_LOGGING_H_

#include <cstdlib>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>

#ifndef HCP_MIN_LOG_LVL
#define HCP_MIN_LOG_LVL INFO
#endif

#define LOG(severity) LOG_I(severity)

#define LOG_I(severity)                                                                   \
  (::hcproxy::internal_logging::severity < ::hcproxy::internal_logging::HCP_MIN_LOG_LVL)  \
      ? static_cast<void>(0)                                                              \
      : ::hcproxy::internal_logging::Assignable() =                                       \
            ::hcproxy::internal_logging::LogStream(__FILE__, __LINE__,                    \
                                                   ::hcproxy::internal_logging::severity) \
                .strm()

namespace hcproxy {

struct Errno {
  Errno() {}
  Errno(int err) : err(err) {}
  std::optional<int> err;
};

std::ostream& operator<<(std::ostream& strm, const Errno& e);

namespace internal_logging {

enum Severity {
  INFO = 0,
  WARN = 1,
  ERROR = 2,
  FATAL = 3,
};

struct Assignable {
  template <class T>
  void operator=(const T&) const {}
};

class LogStream {
 public:
  LogStream(const char* file, int line, Severity severity);
  LogStream(LogStream&&) = delete;
  ~LogStream();

  std::ostream& strm() { return *strm_; }

 private:
  const char* file_;
  int line_;
  Severity severity_;
  std::unique_ptr<std::ostringstream> strm_;
};

}  // namespace internal_logging

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_LOGGING_H_
