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
