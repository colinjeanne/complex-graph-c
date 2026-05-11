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

typedef int (*deque_walker)(void *current, void *extra);

void walk_deque(struct deque *d, deque_walker walker, void *extra);

struct iterator;

struct iterator *malloc_iterator(struct deque *d);
void free_iterator(struct iterator *it);

void restart_iterator(struct iterator *it);
bool is_end_iterator(struct iterator *it);
void *iterator_data(struct iterator *it);
void *iterator_next(struct iterator *it);

void *remove_at_and_backup_iterator(struct iterator *it);
void *replace_iterator_data(struct iterator *it, void *data);

#endif
