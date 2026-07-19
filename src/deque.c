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
  if (d == nullptr) {
    return;
  }
  
  if (deque_len(d) > 0) {
    struct deque_item *current = d->head;
    while (current != nullptr) {
      struct deque_item *to_free = current;
      current = current->next;

      d->deleter(to_free->data);
      free_deque_item(to_free);
    }
  }

  free(d);
}

void *deque_peek_back(struct deque *d) {
  if (deque_len(d) == 0) {
    return nullptr;
  }

  return d->tail->data;
}

void *deque_peek_front(struct deque *d) {
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

  if (deque_len(d) == 0) {
    d->head = tail;
    d->tail = tail;
  } else {
    tail->prev = d->tail;
    d->tail->next = tail;
    d->tail = tail;
  }

  ++d->len;

  return 0;
}

int deque_push_front(struct deque *d, void *data) {
  struct deque_item *head = malloc_deque_item(data);
  if (head == nullptr) {
    return -1;
  }

  if (deque_len(d) == 0) {
    d->head = head;
    d->tail = head;
  } else {
    head->next = d->head;
    d->head->prev = head;
    d->head = head;
  }

  ++d->len;

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

  --d->len;

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

  --d->len;
  
  free_deque_item(head);

  return data;
}

size_t deque_len(struct deque *d) {
  return d->len;
}

struct iterator iterator_for(struct deque *d) {
  struct iterator it = { d };
  restart_iterator(&it);
  return it;
}

void restart_iterator(struct iterator *it) {
  it->item = it->d->head;
  it->is_before_beginning = false;
}

bool is_end_iterator(struct iterator it) {
  return !it.is_before_beginning && it.item == nullptr;
}

void *iterator_data(struct iterator it) {
  if (it.item == nullptr) {
    return nullptr;
  }

  return it.item->data;
}

void *iterator_next(struct iterator *it) {
  if (it->is_before_beginning) {
    restart_iterator(it);
    return iterator_data(*it);
  }

  if (it->item == nullptr) {
    return nullptr;
  }

  it->item = it->item->next;
  return iterator_data(*it);
}

int deque_insert_before(struct iterator it, void *data) {
  if (it.is_before_beginning) {
    return -1;
  }

  if (it.item == it.d->head) {
    return deque_push_front(it.d, data);
  }

  struct deque_item *item = malloc_deque_item(data);
  if (item == nullptr) {
    return -1;
  }

  struct deque_item *prev = it.item->prev;
  item->prev = prev;
  item->next = it.item;
  prev->next = item;
  it.item->prev = item;
  
  ++it.d->len;

  return 0;
}

void *remove_at_and_backup_iterator(struct iterator *it) {
  struct deque_item *old_item = it->item;
  void *data = old_item->data;

  old_item->data = nullptr;

  if (it->item->prev != nullptr) {
    it->item = it->item->prev;
    it->item->next = old_item->next;

    if (old_item->next != nullptr) {
      old_item->next->prev = it->item;
    } else {
      it->d->tail = it->item;
    }
  } else {
    it->item = nullptr;
    it->is_before_beginning = true;

    it->d->head = old_item->next;
    if (it->d->head != nullptr) {
      it->d->head->prev = nullptr;
    }

    if (it->d->tail == old_item) {
      it->d->tail = it->d->head;
    }
  }

  --it->d->len;

  free_deque_item(old_item);

  return data;
}

void *replace_iterator_data(struct iterator *it, void *data) {
  void *old = it->item->data;
  it->item->data = data;
  return old;
}
