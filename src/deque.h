#ifndef DEQUE_H
#define DEQUE_H

#include <stddef.h>

struct deque;

typedef void (*deque_item_deleter)(void *data);

struct deque *malloc_deque(deque_item_deleter deleter);
void free_deque(struct deque *d);

void *deque_peek_back(struct deque *d);
void *deque_peek_front(struct deque *d);

int deque_push_back(struct deque *d, void *data);
int deque_push_front(struct deque *d, void *data);
void *deque_pop_back(struct deque *d);
void *deque_pop_front(struct deque *d);
size_t deque_len(struct deque *d);

struct iterator {
  struct deque *d;
  struct deque_item *item;
  bool is_before_beginning;
};

struct iterator iterator_for(struct deque *d);

void restart_iterator(struct iterator *it);
bool is_end_iterator(struct iterator it);
void *iterator_data(struct iterator it);
void *iterator_next(struct iterator *it);

int deque_insert_before(struct iterator it, void *data);
void *remove_at_and_backup_iterator(struct iterator *it);
void *replace_iterator_data(struct iterator *it, void *data);

#endif
