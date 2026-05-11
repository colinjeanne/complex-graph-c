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
  TOKEN_UNKNOWN,
  TOKEN_DIVIDE,
  TOKEN_MINUS,
  TOKEN_NEGATE,
  TOKEN_NUMBER,
  TOKEN_PLUS,
  TOKEN_TIMES,
};

struct token {
  enum token_type type;
  size_t start_index;
  size_t len;
  double _Complex value;
};

bool is_token_operator(enum token_type type) {
  switch (type) {
    case TOKEN_DIVIDE:
    case TOKEN_MINUS:
    case TOKEN_PLUS:
    case TOKEN_TIMES:
      return true;
  }

  return false;
}

bool is_token_unary_function(enum token_type type) {
  switch (type) {
    case TOKEN_NEGATE:
      return true;
  }

  return false;
}

bool is_token_value(enum token_type type) {
  switch (type) {
    case TOKEN_NUMBER:
      return true;
  }

  return false;
}

bool is_left_token_lower_or_equal_precedence(enum token_type left, enum token_type right) {
  return
    // Non-operators all have the same precedence
    (!is_token_operator(left) && !is_token_operator(right)) ||

    // Times and divide have the second highest precedence amongst operators
    ((right == TOKEN_DIVIDE) || (right == TOKEN_TIMES)) ||

    // The right token must be plus or minus so verify the left is also plus or
    // minus
    ((left == TOKEN_PLUS) || (left == TOKEN_PLUS));
}

bool is_token_unary_forcing(enum token_type type) {
  switch (type) {
    case TOKEN_DIVIDE:
    case TOKEN_MINUS:
    case TOKEN_NEGATE:
    case TOKEN_PLUS:
    case TOKEN_TIMES:
      return true;
  }

  return false;
}

struct token *malloc_token(enum token_type type, size_t start_index, size_t len, double _Complex value) {
  struct token *tok = malloc(sizeof(struct token));
  if (tok == nullptr) {
    return tok;
  }

  tok->type = type;
  tok->start_index = start_index;
  tok->len = len;
  tok->value = value;

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

struct parse_result parse_number(const char *s, size_t start_index, size_t *len, double _Complex *value) {
  *value = 0;

  bool seen_decimal = false;
  double base = 10;

  for (*len = 0;; ++*len) {
    char c = s[start_index + *len];

    int digit = -1;
    if (isdigit(c)) {
      digit = c - '0';
    } else if (c == '.' && !seen_decimal) {
      seen_decimal = true;
      continue;
    }

    if (digit == -1) {
      if (*len == 1 && seen_decimal) {
        // The only character we've seen is the decimal and we now see a
        // non-digit. This number is invalid.
        return MAKE_RESULT(PARSE_ERROR_EXPECTED_DIGIT, start_index + 1);
      }

      break;
    }

    if (!seen_decimal) {
      *value = *value * 10 + digit;
    } else {
      *value += digit / base;
      base *= 10;
    }
  }

  return SUCCESS_RESULT;
}

int add_token(struct deque *tokens, enum token_type type, size_t start_index, size_t len, double _Complex value) {
  struct token *tok = malloc_token(type, start_index, len, value);
  if (tok == nullptr) {
    return -1;
  }

  int result = deque_push_back(tokens, tok);
  if (result != 0) {
    free_token(tok);
    return -1;
  }

  return 0;
}

enum token_type token_type_from_char(char c) {
  if (c == '/') {
    return TOKEN_DIVIDE;
  } else if (c == '-') {
    return TOKEN_MINUS;
  } else if (c == '+') {
    return TOKEN_PLUS;
  } else if (c == '*') {
    return TOKEN_TIMES;
  }

  return TOKEN_UNKNOWN;
}

struct parse_result tokenize(const char *s, struct deque **tokens) {
  *tokens = malloc_deque(token_deleter);
  size_t start_index = 0;
  size_t char_length = strlen(s);
  int deque_result;

  struct parse_result result = SUCCESS_RESULT;
  while (start_index < char_length) {
    int c = s[start_index];
    enum token_type type = token_type_from_char(c);

    if (c == '\0') {
      // Reached the end of the string. Embedded nulls are not supported
      break;
    } else if (isspace(c)) {
      ++start_index;
    } else if (is_numeric_character(c)) {
      size_t len;
      double _Complex value;
      result = parse_number(s, start_index, &len, &value);
      CLEANUP_IF_FAILED(result);

      deque_result = add_token(*tokens, TOKEN_NUMBER, start_index, len, value);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      start_index += len;
    } else if (type != TOKEN_UNKNOWN) {
      deque_result = add_token(*tokens, type, start_index, 1, 0);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      ++start_index;
    } else {
      result = MAKE_RESULT(PARSE_ERROR_UNKNOWN_CHARACTER, start_index);
      CLEANUP_IF_FAILED(result);
    }
  }

  cleanup:

  return result;
}

struct parse_result add_implied_tokens(struct deque *tokens, struct deque **processed) {
  *processed = nullptr;
  if (tokens == nullptr) {
    return SUCCESS_RESULT;
  }

  *processed = malloc_deque(token_deleter);
  if (*processed == nullptr) {
    return OOM_RESULT;
  }

  struct parse_result result = SUCCESS_RESULT;

  struct token *tok;
  for (
    tok = deque_pop_front(tokens);
    tok != nullptr;
    tok = deque_pop_front(tokens)
  ) {
    int deque_result;
    struct token *next = deque_peek_front(tokens);
    struct token *last_processed = deque_peek_back(*processed);

    switch (tok->type) {
      case TOKEN_DIVIDE:
      case TOKEN_TIMES:
        deque_result = deque_push_back(*processed, tok);
        if (deque_result != 0) {
          result = OOM_RESULT;
          CLEANUP_IF_FAILED(result);
        }
        break;

      case TOKEN_MINUS:
      case TOKEN_PLUS:
        if (
          (deque_len(*processed) == 0) ||
          is_token_unary_forcing(last_processed->type)
        ) {
          // Only include a token for unary minus since unary plus is the
          // identity function and can be ignored.
          if (tok->type == TOKEN_MINUS) {
            deque_result = add_token(*processed, TOKEN_NEGATE, tok->start_index, tok->len, 0);
            if (deque_result != 0) {
              result = OOM_RESULT;
              CLEANUP_IF_FAILED(result);
            }
          }

          // We have replaced (or ignored) this token, free it
          free_token(tok);
          tok = nullptr;
        } else {
          deque_result = deque_push_back(*processed, tok);
          if (deque_result != 0) {
            result = OOM_RESULT;
            CLEANUP_IF_FAILED(result);
          }
        }
        break;
      
      case TOKEN_NUMBER:
        deque_result = deque_push_back(*processed, tok);
        if (deque_result != 0) {
          result = OOM_RESULT;
          CLEANUP_IF_FAILED(result);
        }
        break;
    }
  }

  cleanup:
  // If there was an error processing the tokens this could have been popped
  // without having been pushed to processed. This is the only case where this
  // will be non-null
  free_token(tok);

  return result;
}

struct parse_result convert_to_reverse_polish_notation(struct deque *tokens, struct deque **rpn_tokens) {
  *rpn_tokens = nullptr;
  if (tokens == nullptr) {
    return SUCCESS_RESULT;
  }

  *rpn_tokens = malloc_deque(token_deleter);
  if (*rpn_tokens == nullptr) {
    return OOM_RESULT;
  }

  struct parse_result result = SUCCESS_RESULT;
  struct deque *ops = malloc_deque(token_deleter);
  if (ops == nullptr) {
    result = OOM_RESULT;
    CLEANUP_IF_FAILED(result);
  }

  struct token *prev_token = nullptr;

  struct token *tok;
  for (
    tok = deque_pop_front(tokens);
    tok != nullptr;
    tok = deque_pop_front(tokens)
  ) {
    int deque_result;
    if (is_token_value(tok->type)) {
      if (prev_token && prev_token->type == TOKEN_NUMBER) {
        result = MAKE_RESULT(PARSE_ERROR_EXCESS_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }

      deque_result = deque_push_back(*rpn_tokens, tok);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }
    } else if (is_token_operator(tok->type)) {
      if (deque_len(*rpn_tokens) == 0) {
        result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }

      while (deque_len(ops) > 0) {
        struct token *top = deque_peek_front(ops);
        if (
          !is_left_token_lower_or_equal_precedence(tok->type, top->type)
        ) {
          break;
        }
        
        if (is_token_operator(top->type)) {
          deque_result = deque_push_back(*rpn_tokens, top);
        } else {
          break;
        }

        deque_pop_front(ops);
      }

      deque_result = deque_push_front(ops, tok);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }
    } else if (is_token_unary_function(tok->type)) {
      deque_result = deque_push_front(ops, tok);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }
    }

    prev_token = tok;
  }

  while (deque_len(ops) > 0) {
    struct token *op = deque_pop_front(ops);
    int deque_result = deque_push_back(*rpn_tokens, op);
    if (deque_result != 0) {
      free_token(op);
      result = OOM_RESULT;
      CLEANUP_IF_FAILED(result);
    }
  }

  cleanup:
  free_deque(ops);

  // If there was an error processing the tokens this could have been popped
  // without having been pushed to rpn_tokens. This is the only case where this
  // will be non-null
  free_token(tok);

  return result;
}

enum ex_type {
  EX_BINARY_OPERATION,
  EX_NUMBER,
  EX_UNARY_OPERATION,
};

typedef double _Complex (*unary_operation)(double _Complex operand);

double _Complex negate(double _Complex operand) {
  return -operand;
}

unary_operation unary_operation_from_token(enum token_type type) {
  switch (type) {
    case TOKEN_NEGATE:
      return negate;
  }

  return nullptr;
}

typedef double _Complex (*binary_operation)(double _Complex left, double _Complex right);

double _Complex divide(double _Complex left, double _Complex right) {
  return left / right;
}

double _Complex minus(double _Complex left, double _Complex right) {
  return left - right;
}

double _Complex plus(double _Complex left, double _Complex right) {
  return left + right;
}

double _Complex times(double _Complex left, double _Complex right) {
  return left * right;
}

binary_operation operation_from_operator(enum token_type type) {
  switch (type) {
    case TOKEN_DIVIDE:
      return divide;

    case TOKEN_MINUS:
      return minus;
    
    case TOKEN_PLUS:
      return plus;
    
    case TOKEN_TIMES:
      return times;
  }

  return nullptr;
}

struct expression {
  enum ex_type type;
  size_t start_index;
  size_t len;
  union {
    double value;
    struct {
      struct expression *left_child;
      struct expression *right_child;
      binary_operation binary_eval;
    };
    struct {
      struct expression *operand;
      unary_operation unary_eval;
    };
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

struct expression *malloc_binary_operation_expression(
  binary_operation eval,
  struct expression *left_child,
  struct expression *right_child,
  size_t start_index,
  size_t len
) {
  struct expression *ex = malloc(sizeof(struct expression));
  if (ex == nullptr) {
    return nullptr;
  }

  ex->type = EX_BINARY_OPERATION;
  ex->binary_eval = eval;
  ex->left_child = left_child;
  ex->right_child = right_child;
  ex->start_index = start_index;
  ex->len = len;

  return ex;
}

double _Complex evaluate_binary_operation_expression(const struct expression *ex) {
  return ex->binary_eval(evaluate_expression(ex->left_child), evaluate_expression(ex->right_child));
}

struct expression *malloc_unary_operation_expression(
  unary_operation eval,
  struct expression *operand,
  size_t start_index,
  size_t len
) {
  struct expression *ex = malloc(sizeof(struct expression));
  if (ex == nullptr) {
    return nullptr;
  }

  ex->type = EX_UNARY_OPERATION;
  ex->unary_eval = eval;
  ex->operand = operand;
  ex->start_index = start_index;
  ex->len = len;

  return ex;
}

double _Complex evaluate_unary_operation_expression(const struct expression *ex) {
  return ex->unary_eval(evaluate_expression(ex->operand));
}

void free_expression(struct expression *ex) {
  if (ex == nullptr) {
    return;
  }

  switch (ex->type) {
    case EX_BINARY_OPERATION:
      free(ex->left_child);
      free(ex->right_child);
      break;
    
    case EX_UNARY_OPERATION:
      free(ex->operand);
      break;
  }

  free(ex);
}

int merge_expression_len(size_t left_start_index, size_t right_start_index, size_t right_len) {
  return (right_start_index + right_len) - left_start_index;
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
  struct expression *left_child = nullptr;
  struct expression *right_child = nullptr;
  switch (tok->type) {
    case TOKEN_NUMBER:
      ex = malloc_number_expression(tok->value, tok->start_index, tok->len);
      if (ex == nullptr) {
        state->last_result = OOM_RESULT;
        CLEANUP_IF_FAILED(state->last_result);
      }

      deque_result = deque_push_back(state->stack, ex);
      if (deque_result != 0) {
        state->last_result = OOM_RESULT;
        CLEANUP_IF_FAILED(state->last_result);
      }

      ex = nullptr;
      break;
    
    case TOKEN_DIVIDE:
    case TOKEN_MINUS:
    case TOKEN_PLUS:
    case TOKEN_TIMES:
      int len = deque_len(state->stack);
      if (len < 2) {
        state->last_result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(state->last_result);
      }

      right_child = deque_pop_back(state->stack);
      left_child = deque_pop_back(state->stack);
      ex = malloc_binary_operation_expression(
        operation_from_operator(tok->type),
        left_child,
        right_child,
        left_child->start_index,
        merge_expression_len(
          left_child->start_index,
          right_child->start_index,
          right_child->len
        )
      );
      if (ex == nullptr) {
        state->last_result = OOM_RESULT;
        CLEANUP_IF_FAILED(state->last_result);
      }

      deque_result = deque_push_back(state->stack, ex);
      if (deque_result != 0) {
        state->last_result = OOM_RESULT;
        CLEANUP_IF_FAILED(state->last_result);
      }

      ex = nullptr;
      left_child = nullptr;
      right_child = nullptr;
      break;

    case TOKEN_NEGATE:
      if (deque_len(state->stack) == 0) {
        state->last_result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(state->last_result);
      }

      right_child = deque_pop_back(state->stack);
      ex = malloc_unary_operation_expression(
        unary_operation_from_token(tok->type),
        right_child,
        tok->start_index,
        merge_expression_len(
          tok->start_index,
          right_child->start_index,
          right_child->len
        )
      );

      deque_result = deque_push_back(state->stack, ex);
      if (deque_result != 0) {
        state->last_result = OOM_RESULT;
        CLEANUP_IF_FAILED(state->last_result);
      }

      ex = nullptr;
      right_child = nullptr;

      break;
  }

  return 1;

  cleanup:
  free_expression(ex);
  free_expression(left_child);
  free_expression(right_child);

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
  struct deque *processed = nullptr;
  struct deque *rpn_tokens = nullptr;
  
  result = tokenize(s, &tokens);
  CLEANUP_IF_FAILED(result);

  result = add_implied_tokens(tokens, &processed);
  CLEANUP_IF_FAILED(result);

  result = convert_to_reverse_polish_notation(processed, &rpn_tokens);
  CLEANUP_IF_FAILED(result);

  struct expression *root;
  result = build_parse_tree(s, rpn_tokens, &root);
  CLEANUP_IF_FAILED(result);

  *ex = root;

  result = SUCCESS_RESULT;

  cleanup:
  free_deque(tokens);
  free_deque(processed);
  free_deque(rpn_tokens);

  return result;
}

double _Complex evaluate_expression(const struct expression *ex) {
  switch (ex->type) {
    case EX_BINARY_OPERATION:
      return evaluate_binary_operation_expression(ex);

    case EX_NUMBER:
      return evaluate_number_expression(ex);
    
    case EX_UNARY_OPERATION:
      return evaluate_unary_operation_expression(ex);
  }

  return 0;
}
