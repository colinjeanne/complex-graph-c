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
  TOKEN_OPEN_PARENTHESIS,
  TOKEN_CLOSE_PARENTHESIS,
};

struct token {
  enum token_type type;
  size_t start_index;
  size_t len;
  double _Complex value;
  unsigned char left_binding;
  unsigned char right_binding;
};

struct token_details {
  enum token_type type;
  unsigned char left_binding;
  unsigned char right_binding;
};

const struct token_details TOKEN_DETAILS[] = {
  { TOKEN_UNKNOWN, 0, 0 },
  { TOKEN_DIVIDE, 7, 8 },
  { TOKEN_MINUS, 5, 6 },
  { TOKEN_NEGATE, 99, 9 },
  { TOKEN_NUMBER, 254, 255 },
  { TOKEN_PLUS, 5, 6 },
  { TOKEN_TIMES, 7, 8 },
  { TOKEN_OPEN_PARENTHESIS, 0, 0 },
  { TOKEN_CLOSE_PARENTHESIS, 0, 0 },
};

struct token_details get_token_details(enum token_type type) {
  for (size_t i = 1; i < sizeof(TOKEN_DETAILS) / sizeof(struct token_details); ++i) {
    if (TOKEN_DETAILS[i].type == type) {
      return TOKEN_DETAILS[i];
    }
  }

  return TOKEN_DETAILS[0];
}

bool is_token_unary_forcing(enum token_type type) {
  switch (type) {
    case TOKEN_DIVIDE:
    case TOKEN_MINUS:
    case TOKEN_NEGATE:
    case TOKEN_PLUS:
    case TOKEN_TIMES:
    case TOKEN_OPEN_PARENTHESIS:
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

  struct token_details details = get_token_details(type);
  tok->left_binding = details.left_binding;
  tok->right_binding = details.right_binding;

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
  } else if (c == '(') {
    return TOKEN_OPEN_PARENTHESIS;
  } else if (c == ')') {
    return TOKEN_CLOSE_PARENTHESIS;
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

struct parse_result add_implied_tokens(struct deque *tokens) {
  if (tokens == nullptr) {
    return SUCCESS_RESULT;
  }

  struct iterator it = iterator_for(tokens);
  struct parse_result result = SUCCESS_RESULT;

  struct token *last_token = nullptr;
  for (; !is_end_iterator(it); iterator_next(&it)) {
    struct token *current = iterator_data(it);
    int deque_result;

    switch (current->type) {
      case TOKEN_MINUS:
      case TOKEN_PLUS:
        if (
          (last_token == nullptr) ||
          is_token_unary_forcing(last_token->type)
        ) {
          // Only include a token for unary minus since unary plus is the
          // identity function and can be ignored.
          if (current->type == TOKEN_MINUS) {
            struct token *negate = malloc_token(TOKEN_NEGATE, current->start_index, current->len, 0);
            if (negate == nullptr) {
              result = OOM_RESULT;
              CLEANUP_IF_FAILED(result);
            }

            replace_iterator_data(&it, negate);
            last_token = negate;
          } else {
            remove_at_and_backup_iterator(&it);
          }

          free_token(current);
        } else {
          last_token = current;
        }
        break;
      
      case TOKEN_NUMBER:
        if ((last_token != nullptr) && (last_token->type == TOKEN_NUMBER)) {
          result = MAKE_RESULT(PARSE_ERROR_EXCESS_EXPRESSION, current->start_index);
          CLEANUP_IF_FAILED(result);
        }

        last_token = current;
        break;
      
      default:
        last_token = current;
    }
  }

  cleanup:

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

struct scope {
  struct deque *expressions;
  struct deque *operators;
  size_t start_index;
};

void free_scope(struct scope *s) {
  // The scope doesn't open the operator tokens and so cannot free them
  free_deque(s->expressions);

  free(s);
}

struct scope *malloc_scope(size_t start_index) {
  struct scope *s = malloc(sizeof(struct scope));
  if (s == nullptr) {
    return nullptr;
  }

  s->start_index = start_index;
  s->expressions = nullptr;

  // The scope does not own the operator tokens
  s->operators = nullptr;

  s->expressions = malloc_deque(expression_deleter);
  if (s->expressions == nullptr) {
    free_scope(s);
    return nullptr;
  }

  s->operators = malloc_deque(token_deleter);
  if (s->operators == nullptr) {
    free_scope(s);
    return nullptr;
  }

  return s;
}

void scope_deleter(void *data) {
  struct scope *s = (struct scope *)data;
  free_scope(s);
}

struct parse_result malloc_expression(const struct scope *top, struct expression **ex) {
  *ex = nullptr;

  if (top == nullptr) {
    return SUCCESS_RESULT;
  }

  int deque_result;
  struct parse_result result = SUCCESS_RESULT;
  struct expression *left_child = nullptr;
  struct expression *right_child = nullptr;

  struct token *tok = deque_pop_back(top->operators);

  switch (tok->type) {
    case TOKEN_DIVIDE:
    case TOKEN_MINUS:
    case TOKEN_PLUS:
    case TOKEN_TIMES:
      right_child = deque_pop_back(top->expressions);
      if (right_child == nullptr) {
        result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }

      left_child = deque_pop_back(top->expressions);
      if (left_child == nullptr) {
        result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }
      
      *ex = malloc_binary_operation_expression(
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
      if (*ex == nullptr) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      right_child = nullptr;
      left_child = nullptr;
      break;
    
    case TOKEN_NEGATE:
      right_child = deque_pop_back(top->expressions);
      if (right_child == nullptr) {
        result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }

      *ex = malloc_unary_operation_expression(
        unary_operation_from_token(tok->type),
        right_child,
        tok->start_index,
        merge_expression_len(
          tok->start_index,
          right_child->start_index,
          right_child->len
        )
      );
      if (*ex == nullptr) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      right_child = nullptr;
      break;
    
    case TOKEN_NUMBER:
      *ex = malloc_number_expression(tok->value, tok->start_index, tok->len);
      if (*ex == nullptr) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }
      break;
  }

  cleanup:
  free_expression(left_child);
  free_expression(right_child);

  return result;
}

struct parse_result resolve_operator(struct scope *s) {
  struct expression *ex = nullptr;
  struct parse_result result = malloc_expression(s, &ex);
  CLEANUP_IF_FAILED(result);

  if (ex == nullptr) {
    return result;
  }

  int deque_result = deque_push_back(s->expressions, ex);
  if (deque_result != 0) {
    result = OOM_RESULT;
    CLEANUP_IF_FAILED(result);
  }

  ex = nullptr;

  cleanup:
  free_expression(ex);

  return result;
}

struct parse_result close_scope(struct scope *s, struct expression **ex) {
  *ex = nullptr;

  struct parse_result result = SUCCESS_RESULT;
  while (deque_len(s->operators) > 0) {
    result = resolve_operator(s);
    CLEANUP_IF_FAILED(result);
  }

  *ex = deque_pop_front(s->expressions);
  if (*ex == nullptr) {
    result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, s->start_index);
    CLEANUP_IF_FAILED(result);
  }

  // Expand scope to include the opening parenthesis
  (*ex)->start_index = s->start_index;

  struct expression *rest = deque_peek_front(s->expressions);
  if (rest != nullptr) {
    free_expression(*ex);
    ex = nullptr;

    result = MAKE_RESULT(PARSE_ERROR_EXCESS_EXPRESSION, rest->start_index);
    CLEANUP_IF_FAILED(result);
  }

  cleanup:
  return result;
}

struct parse_result build_parse_tree(const char *s, struct deque *tokens, struct expression **root) {
  *root = nullptr;

  struct parse_result result = SUCCESS_RESULT;
  struct iterator it = iterator_for(tokens);
  struct deque *stack = malloc_deque(scope_deleter);
  if (stack == nullptr) {
    result = OOM_RESULT;
    CLEANUP_IF_FAILED(result);
  }

  struct scope *top = malloc_scope(0);
  if (top == nullptr) {
    result = OOM_RESULT;
    CLEANUP_IF_FAILED(result);
  }

  int deque_result = deque_push_back(stack, top);
  if (deque_result != 0) {
    free_scope(top);
    result = OOM_RESULT;
    CLEANUP_IF_FAILED(result);
  }

  for (it; !is_end_iterator(it); iterator_next(&it)) {
    struct token *tok = iterator_data(it);

    if (tok->type == TOKEN_OPEN_PARENTHESIS) {
      struct scope *new_top = malloc_scope(tok->start_index);
      deque_result = deque_push_back(stack, new_top);
      if (deque_result != 0) {
        free_scope(new_top);
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }
    } else if (tok->type == TOKEN_CLOSE_PARENTHESIS) {
      if (deque_len(stack) == 1) {
        result = MAKE_RESULT(PARSE_ERROR_EXCESS_CLOSE_PARENTHESIS, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }

      struct expression *ex;
      top = deque_peek_back(stack);

      result = close_scope(top, &ex);
      CLEANUP_IF_FAILED(result);

      top = deque_pop_back(stack);
      free_scope(top);
      top = deque_peek_back(stack);

      deque_result = deque_push_back(top->expressions, ex);
      if (deque_result != 0) {
        free_expression(ex);
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }
    } else {
      top = deque_peek_back(stack);
      struct token *top_op = deque_peek_back(top->operators);
      unsigned char right_binding = top_op != nullptr ? top_op->right_binding : 0;

      while (right_binding > tok->left_binding) {
        result = resolve_operator(top);
        CLEANUP_IF_FAILED(result);

        top_op = deque_peek_back(top->operators);
        right_binding = top_op != nullptr ? top_op->right_binding : 0;
      }

      deque_result = deque_push_back(top->operators, tok);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }
    }
  }

  top = deque_peek_back(stack);
  if (deque_len(stack) > 1) {
    result = MAKE_RESULT(PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, top->start_index);
    CLEANUP_IF_FAILED(result);
  }

  result = close_scope(top, root);
  CLEANUP_IF_FAILED(result);

  cleanup:
  free_deque(stack);

  return result;
}

struct parse_result make_expression(const char *s, struct expression **ex) {
  *ex = nullptr;

  struct parse_result result;
  struct deque *tokens = nullptr;
  struct deque *rpn_tokens = nullptr;
  
  result = tokenize(s, &tokens);
  CLEANUP_IF_FAILED(result);

  result = add_implied_tokens(tokens);
  CLEANUP_IF_FAILED(result);

  struct expression *root;
  result = build_parse_tree(s, tokens, &root);
  CLEANUP_IF_FAILED(result);

  *ex = root;

  result = SUCCESS_RESULT;

  cleanup:
  free_deque(tokens);
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
