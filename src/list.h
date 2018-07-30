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

#ifndef ROMKATV_HCPROXY_LIST_H_
#define ROMKATV_HCPROXY_LIST_H_

namespace hcproxy {

class List;

class Node {
 public:
  Node* prev() const { return prev_; }
  Node* next() const { return next_; }

 private:
  friend class List;

  Node* prev_ = nullptr;
  Node* next_ = nullptr;
};

// Doubly linked list. Doesn't own its nodes.
class List {
 public:
  void AddTail(Node* node);
  void Erase(Node* node);

  // Null iff the list is empty.
  Node* head() const { return head_; }
  // Null iff the list is empty.
  Node* tail() const { return tail_; }

 private:
  Node* head_ = nullptr;
  Node* tail_ = nullptr;
};

}  // namespace hcproxy

#endif  // ROMKATV_HCPROXY_LIST_H_
