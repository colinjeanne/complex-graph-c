#include <ctype.h>
#include <float.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"

#define MAKE_RESULT(type, index) ((struct parse_result) { type, (index) })

#define RETURN_IF_FAILED(result) if (result.type != PARSE_ERROR_SUCCESS) { \
  return result; \
}

#define CLEANUP_IF_FAILED(result) if (result.type != PARSE_ERROR_SUCCESS) { \
  goto cleanup; \
}

const struct parse_result SUCCESS_RESULT = { PARSE_ERROR_SUCCESS, 0 };
const struct parse_result OOM_RESULT = { PARSE_ERROR_OOM, 0 };

enum token_type {
  TOKEN_SPACE,
  TOKEN_NUMBER,
};

struct token {
  enum token_type type;
  size_t start_index;
  size_t len;
  struct token *next_token;
};

struct token *malloc_token(enum token_type type, size_t start_index, size_t len) {
  struct token *tok = malloc(sizeof(struct token));
  if (tok == nullptr) {
    return tok;
  }

  tok->type = type;
  tok->start_index = start_index;
  tok->len = len;
  tok->next_token = nullptr;

  return tok;
}

void free_token(struct token *tok) {
  free(tok);
}

struct token_list {
  struct token *head;
  struct token *tail;
};

struct parse_result push_token(enum token_type type, size_t start_index, size_t len, struct token_list **tokens) {
  struct token *tok = malloc_token(type, start_index, len);
  if (tok == nullptr) {
    return OOM_RESULT;
  }

  if (*tokens == nullptr) {
    *tokens = malloc(sizeof(struct token_list));
    if (*tokens == nullptr) {
      free_token(tok);
      return OOM_RESULT;
    }

    (*tokens)->head = tok;
    (*tokens)->tail = tok;
  } else {
    (*tokens)->tail->next_token = tok;
    (*tokens)->tail = tok;
  }

  return SUCCESS_RESULT;
}

void free_token_list(struct token_list *tokens) {
  if (tokens == nullptr) {
    return;
  }

  struct token *current = tokens->head;
  while (current) {
    struct token *next = current->next_token;
    free_token(current);
    current = next;
  }

  free(tokens);
}

bool is_numeric_character(char c) {
  return (c == '.') || isdigit(c);
}

struct parse_result number_len(const char *s, size_t start_index, size_t *len) {
  bool seen_decimal = false;

  for (*len = 0;; ++*len) {
    char c = s[start_index + *len];

    if (isdigit(c)) {
      continue;
    } else if (c == '.' && !seen_decimal) {
      seen_decimal = true;
      continue;
    }

    if (*len == 1 && seen_decimal) {
      // The only character we've seen is the decimal and we now see a
      // non-digit. This number is invalid.
      return MAKE_RESULT(PARSE_ERROR_EXPECTED_DIGIT, start_index + 1);
    }

    return SUCCESS_RESULT;
  }

  return SUCCESS_RESULT;
}

double parse_number(const char *s, size_t start_index, size_t len) {
  double num = 0;
  bool seen_decimal = false;
  double base = 10;

  for (size_t current = start_index; current < len; ++current) {
    char c = s[current];

    int digit;
    if (isdigit(c)) {
      digit = c - '0';
    } else if (c == '.') {
      seen_decimal = true;
      continue;
    } else {
      break;
    }

    if (!seen_decimal) {
      num = num * 10 + digit;
    } else {
      num += digit / base;
      base *= 10;
    }
  }

  return num;
}

struct parse_result tokenize(const char *s, struct token_list **tokens) {
  *tokens = nullptr;
  size_t start_index = 0;
  size_t char_length = strlen(s);

  struct parse_result result;
  while (start_index < char_length) {
    int c = s[start_index];
    if (c == '\0') {
      // Reached the end of the string. Embedded nulls are not supported
      break;
    } else if (isspace(c)) {
      result = push_token(TOKEN_SPACE, start_index, 1, tokens);
      RETURN_IF_FAILED(result);

      ++start_index;
    } else if (is_numeric_character(c)) {
      size_t len;
      result = number_len(s, start_index, &len);
      RETURN_IF_FAILED(result);

      result = push_token(TOKEN_NUMBER, start_index, len, tokens);
      RETURN_IF_FAILED(result);

      start_index += len;
    } else {
      ++start_index;
    }
  }

  return SUCCESS_RESULT;
}

enum ex_type {
  EX_NUMBER,
};

struct expression {
  enum ex_type type;
  size_t start_index;
  size_t len;
  union {
    double value;
  };
};

struct expression *malloc_number_expression(double value, size_t start_index, size_t len) {
  struct expression *ex = malloc(sizeof(struct expression));
  if (ex == nullptr) {
    return nullptr;
  }

  ex->type = EX_NUMBER;
  ex->value = value;
  ex->start_index = start_index;
  ex->len = len;

  return ex;
}

double _Complex evaluate_number_expression(const struct expression *ex) {
  return ex->value;
}

void free_expression(struct expression *ex) {
  if (ex == nullptr) {
    return;
  }

  switch (ex->type) {
    case EX_NUMBER:
      free(ex);
  }
}

struct expression_list_item {
  struct expression *self;
  struct expression_list_item *next;
  struct expression_list_item *prev;
};

struct expression_list {
  struct expression_list_item *head;
  struct expression_list_item *tail;
  size_t len;
};

struct expression_list_item *malloc_expression_list_item(struct expression *ex) {
  struct expression_list_item *item = malloc(sizeof(struct expression_list_item));
  if (item == nullptr) {
    return nullptr;
  }

  item->self = ex;
  item->next = nullptr;
  item->prev = nullptr;

  return item;
}

void free_expression_list_item(struct expression_list_item *item) {
  if (item == nullptr) {
    return;
  }

  free_expression(item->self);
  free(item);
}

struct expression_list *malloc_expression_list() {
  struct expression_list *list = malloc(sizeof(struct expression_list));
  if (list == nullptr) {
    return nullptr;
  }

  list->head = nullptr;
  list->tail = nullptr;
  list->len = 0;

  return list;
}

void free_expression_list(struct expression_list *list) {
  if (list == nullptr) {
    return;
  }

  struct expression_list_item *current = list->head;
  while (current != nullptr) {
    struct expression_list_item *next = current->next;
    free_expression_list_item(current);

    current = next;
  }
}

int expression_list_len(struct expression_list *list) {
  if (list == nullptr) {
    return 0;
  }

  return list->len;
}

struct parse_result push_back_expression(struct expression *ex, struct expression_list **list) {
  struct expression_list_item *item = malloc_expression_list_item(ex);
  if (item == nullptr) {
    return OOM_RESULT;
  }

  if (*list == nullptr) {
    *list = malloc_expression_list();
    if (*list == nullptr) {
      free_expression_list_item(item);
      return OOM_RESULT;
    }

    (*list)->head = item;
    (*list)->tail = item;
    (*list)->len = 1;

    return SUCCESS_RESULT;
  }

  item->prev = (*list)->tail;
  (*list)->tail->next = item;
  (*list)->tail = item;
  ++(*list)->len;

  return SUCCESS_RESULT;
}

struct expression *pop_front_expression(struct expression_list *list) {
  if (expression_list_len(list) == 0) {
    return nullptr;
  }

  struct expression_list_item *first = list->head;
  list->head = first->next;

  if (list->head) {
    list->head->prev = nullptr;
  }

  if (list->tail == first) {
    list->tail = nullptr;
  }

  struct expression *ex = first->self;
  first->self = nullptr;
  --list->len;

  free_expression_list_item(first);

  return ex;
}

struct parse_result build_parse_tree(const char *s, struct token_list *tokens, struct expression **root) {
  *root = nullptr;
  struct expression_list *list = nullptr;

  struct parse_result result;
  for (
    struct token *current = tokens != nullptr ? tokens->head : nullptr;
    current != nullptr;
    current = current->next_token
  ) {
    switch (current->type) {
      case TOKEN_NUMBER:
        double value = parse_number(s, current->start_index, current->len);
        struct expression *ex = malloc_number_expression(value, current->start_index, current->len);
        if (ex == nullptr) {
          result = OOM_RESULT;
          goto cleanup;
        }

        result = push_back_expression(ex, &list);
        CLEANUP_IF_FAILED(result);
        break;
      
      case TOKEN_SPACE:
        break;
    }
  }

  *root = pop_front_expression(list);

  int len = expression_list_len(list);

  if (*root == nullptr) {
    result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, 0);
  } else if (len != 0) {
    result = MAKE_RESULT(PARSE_ERROR_EXCESS_EXPRESSION, list->head->self->start_index);
  } else {
    result = SUCCESS_RESULT;
  }

  cleanup:
  free_expression_list(list);

  return result;
}

struct parse_result make_expression(const char *s, struct expression **ex) {
  *ex = nullptr;

  struct parse_result result;

  struct token_list *tokens = nullptr;
  result = tokenize(s, &tokens);
  CLEANUP_IF_FAILED(result);

  struct expression *root;
  result = build_parse_tree(s, tokens, &root);
  CLEANUP_IF_FAILED(result);

  *ex = root;

  result = SUCCESS_RESULT;

  cleanup:
  free_token_list(tokens);

  return result;
}

double _Complex evaluate_expression(const struct expression *ex) {
  switch (ex->type) {
    case EX_NUMBER:
      return evaluate_number_expression(ex);
  }

  return 0;
}
