#include "list.h"

#include <cassert>

namespace hcproxy {

void List::AddTail(Node* node) {
  assert(node);
  assert(!node->prev_);
  assert(!node->next_);
  if (tail_) {
    assert(head_);
    assert(!tail_->next_);
    tail_->next_ = node;
  } else {
    assert(!head_);
    head_ = node;
  }
  node->prev_ = tail_;
  node->next_ = nullptr;
  tail_ = node;
  assert(head_ && tail_);
}

void List::Erase(Node* node) {
  assert(node);
  Node* const prev = node->prev_;
  Node* const next = node->next_;
  if (prev) {
    assert(head_ != node);
    assert(prev->next_ == node);
    prev->next_ = next;
    node->prev_ = nullptr;
  } else {
    assert(head_ == node);
    head_ = next;
  }
  if (next) {
    assert(tail_ != node);
    assert(next->prev_ == node);
    next->prev_ = prev;
    node->next_ = nullptr;
  } else {
    assert(tail_ == node);
    tail_ = prev;
  }
  assert(!head_ == !tail_);
}

}  // namespace hcproxy
