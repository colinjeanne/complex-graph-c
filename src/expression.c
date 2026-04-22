#include <ctype.h>
#include <float.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "deque.h"
#include "expression.h"

#define MAKE_RESULT(type, index) ((struct parse_result) { type, (index) })

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

void token_deleter(void *data) {
  struct token *tok = (struct token *)data;
  free_token(tok);
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

struct parse_result tokenize(const char *s, struct deque **tokens) {
  *tokens = malloc_deque(token_deleter);
  size_t start_index = 0;
  size_t char_length = strlen(s);

  struct token *tok = nullptr;
  int deque_result;

  struct parse_result result;
  while (start_index < char_length) {
    int c = s[start_index];
    if (c == '\0') {
      // Reached the end of the string. Embedded nulls are not supported
      break;
    } else if (isspace(c)) {
      tok = malloc_token(TOKEN_SPACE, start_index, 1);
      if (tok == nullptr) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      deque_result = deque_push_back(*tokens, tok);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      ++start_index;
    } else if (is_numeric_character(c)) {
      size_t len;
      result = number_len(s, start_index, &len);
      CLEANUP_IF_FAILED(result);

      tok = malloc_token(TOKEN_NUMBER, start_index, len);
      if (tok == nullptr) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      deque_result = deque_push_back(*tokens, tok);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      start_index += len;
    } else {
      ++start_index;
    }
  }

  cleanup:
  free_token(tok);

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

void expression_deleter(void *data) {
  struct expression *ex = (struct expression *)data;
  free_expression(ex);
}

struct build_parse_tree_state {
  const char *s;
  struct parse_result last_result;
  struct deque *stack;
};

int build_parse_tree_walker(void *current, void *extra) {
  struct token *tok = (struct token *)current;
  struct build_parse_tree_state *state = (struct build_parse_tree_state *)extra;

  int deque_result;
  struct expression *ex = nullptr;
  switch (tok->type) {
    case TOKEN_NUMBER:
      double value = parse_number(state->s, tok->start_index, tok->len);
      ex = malloc_number_expression(value, tok->start_index, tok->len);
      if (ex == nullptr) {
        state->last_result = OOM_RESULT;
        goto cleanup;
      }

      deque_result = deque_push_back(state->stack, ex);
      if (deque_result != 0) {
        state->last_result = OOM_RESULT;
        CLEANUP_IF_FAILED(state->last_result);
      }

      ex = nullptr;
      break;
    
    case TOKEN_SPACE:
      break;
  }

  return 1;

  cleanup:
  free_expression(ex);

  return 0;
}

struct parse_result build_parse_tree(const char *s, struct deque *tokens, struct expression **root) {
  *root = nullptr;

  struct deque *stack = malloc_deque(expression_deleter);
  if (stack == nullptr) {
    return OOM_RESULT;
  }

  struct build_parse_tree_state state = { s, SUCCESS_RESULT, stack };
  walk_deque(tokens, build_parse_tree_walker, &state);

  struct parse_result result = state.last_result;
  CLEANUP_IF_FAILED(result);

  *root = deque_pop_front(stack);

  int len = deque_len(stack);

  if (*root == nullptr) {
    result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, 0);
  } else if (len != 0) {
    struct expression *ex = (struct expression *)deque_peek_front(stack);
    result = MAKE_RESULT(PARSE_ERROR_EXCESS_EXPRESSION, ex->start_index);
  } else {
    result = SUCCESS_RESULT;
  }

  cleanup:
  free_deque(stack);

  return result;
}

struct parse_result make_expression(const char *s, struct expression **ex) {
  *ex = nullptr;

  struct parse_result result;

  struct deque *tokens = nullptr;
  result = tokenize(s, &tokens);
  CLEANUP_IF_FAILED(result);

  struct expression *root;
  result = build_parse_tree(s, tokens, &root);
  CLEANUP_IF_FAILED(result);

  *ex = root;

  result = SUCCESS_RESULT;

  cleanup:
  free_deque(tokens);

  return result;
}

double _Complex evaluate_expression(const struct expression *ex) {
  switch (ex->type) {
    case EX_NUMBER:
      return evaluate_number_expression(ex);
  }

  return 0;
}
