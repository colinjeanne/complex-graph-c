#include <stdlib.h>

#include "deque.h"

struct deque_item {
  void *data;
  struct deque_item *next;
  struct deque_item *prev;
};

struct deque_item *malloc_deque_item(void *data) {
  struct deque_item *item = malloc(sizeof(struct deque_item));
  if (item == nullptr) {
    return nullptr;
  }

  item->next = nullptr;
  item->prev = nullptr;
  item->data = data;
  return item;
}

void free_deque_item(struct deque_item *d) {
  free(d);
}

struct deque {
  struct deque_item *head;
  struct deque_item *tail;
  size_t len;

  deque_item_deleter deleter;
};

struct deque *malloc_deque(deque_item_deleter deleter) {
  struct deque *d = malloc(sizeof(struct deque));
  if (d == nullptr) {
    return nullptr;
  }

  d->head = nullptr;
  d->tail = nullptr;
  d->len = 0;
  d->deleter = deleter;

  return d;
}

void free_deque(struct deque *d) {
  if (deque_len(d) == 0) {
    free(d);
  }

  struct deque_item *current = d->head;
  while (current != nullptr) {
    struct deque_item *to_free = current;
    current = current->next;

    d->deleter(to_free->data);
    free_deque_item(to_free);
  }

  free(d);
}

void *peek_front(struct deque *d) {
  if (deque_len(d) == 0) {
    return nullptr;
  }

  return d->head->data;
}

int deque_push_back(struct deque *d, void *data) {
  struct deque_item *tail = malloc_deque_item(data);
  if (tail == nullptr) {
    return -1;
  }

  tail->prev = d->tail;
  d->tail->next = tail;
  d->tail = tail;

  return 0;
}

int deque_push_front(struct deque *d, void *data) {
  struct deque_item *head = malloc_deque_item(data);
  if (head == nullptr) {
    return -1;
  }

  head->next = d->head;
  d->head->prev = head;
  d->head = head;

  return 0;
}

void *deque_pop_back(struct deque *d) {
  if (deque_len(d) == 0) {
    return nullptr;
  }

  struct deque_item *tail = d->tail;
  void *data = tail->data;
  if (d->head == d->tail) {
    d->head = nullptr;
    d->tail = nullptr;
  } else {
    d->tail = tail->prev;
    d->tail->next = nullptr;
  }

  free_deque_item(tail);

  return data;
}

void *deque_pop_front(struct deque *d) {
  if (deque_len(d) == 0) {
    return nullptr;
  }

  struct deque_item *head = d->head;
  void *data = head->data;
  if (d->head == d->tail) {
    d->head = nullptr;
    d->tail = nullptr;
  } else {
    d->head = head->next;
    d->head->prev = nullptr;
  }
  
  free_deque_item(head);

  return data;
}

size_t deque_len(struct deque *d) {
  return d->len;
}

void walk_deque(struct deque *d, deque_walker walker, void *extra) {
  for (
    struct deque_item *current = d->head;
    current != nullptr;
    current = current->next
  ) {
    int should_continue = walker(current, extra);
    if (!should_continue) {
      break;
    }
  }
}
