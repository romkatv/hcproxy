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

#include "list.h"

#include "check.h"

namespace hcproxy {

void List::AddTail(Node* node) {
  CHECK(node);
  CHECK(!node->prev_);
  CHECK(!node->next_);
  if (tail_) {
    CHECK(head_);
    CHECK(!tail_->next_);
    tail_->next_ = node;
  } else {
    CHECK(!head_);
    head_ = node;
  }
  node->prev_ = tail_;
  node->next_ = nullptr;
  tail_ = node;
  CHECK(head_ && tail_);
}

void List::Erase(Node* node) {
  CHECK(node);
  Node* const prev = node->prev_;
  Node* const next = node->next_;
  if (prev) {
    CHECK(head_ != node);
    CHECK(prev->next_ == node);
    prev->next_ = next;
    node->prev_ = nullptr;
  } else {
    CHECK(head_ == node);
    head_ = next;
  }
  if (next) {
    CHECK(tail_ != node);
    CHECK(next->prev_ == node);
    next->prev_ = prev;
    node->next_ = nullptr;
  } else {
    CHECK(tail_ == node);
    tail_ = prev;
  }
  CHECK(!head_ == !tail_);
}

}  // namespace hcproxy
