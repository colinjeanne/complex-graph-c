#include <complex.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
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

double _Complex LN10 = 2.30258509299404568;
double _Complex LN2 = 0.6931471805599453;
double _Complex PI = 3.1415926535897932;

typedef double _Complex (*unary_operation)(double _Complex z);
typedef double _Complex (*binary_operation)(double _Complex left, double _Complex right);

double _Complex complex_abs(double _Complex z) {
  return cabs(z);
}

double _Complex complex_arg(double _Complex z) {
  return carg(z);
}

double _Complex complex_imag(double _Complex z) {
  return cimag(z);
}

double _Complex complex_real(double _Complex z) {
  return creal(z);
}

double _Complex cacot(double _Complex z) {
  return (-I / 2) * clog((z + I) / (z - I));
}

double _Complex cacoth(double _Complex z) {
  return clog((z + 1) / (z - 1)) / 2;
}

double _Complex cacsc(double _Complex z) {
  return -I * clog(csqrt(1 - 1 / (z * z)) + I / z);
}

double _Complex cacsch(double _Complex z) {
  return clog(csqrt(1 + 1 / (z * z)) + 1 / z);
}

double _Complex casec(double _Complex z) {
  return -I * clog(I * csqrt(1 - 1 / (z * z)) + 1 / z);
}

double _Complex casech(double _Complex z) {
  return clog(csqrt(-1 + 1 / (z * z)) + 1 / z);
}

double _Complex cceil(double _Complex z) {
  return ceil(creal(z)) + ceil(cimag(z)) * I;
}

double _Complex ccot(double _Complex z) {
  return 1 / ctan(z);
}

double _Complex ccoth(double _Complex z) {
  return I * ccot(I * z);
}

double _Complex ccsc(double _Complex z) {
  return 1 / csin(z);
}

double _Complex ccsch(double _Complex z) {
  return I * ccsc(I * z);
}

double _Complex cfloor(double _Complex z) {
  return floor(creal(z)) + floor(cimag(z)) * I;
}

double _Complex cfrac(double _Complex z) {
  return creal(z) - trunc(creal(z)) +
    (cimag(z) - trunc(cimag(z))) * I;
}

double _Complex clog_with_base(double _Complex base, double _Complex z) {
  return clog(z) / clog(base);
}

double _Complex clog10(double _Complex z) {
  return clog(z) / LN10;
}

double _Complex clog2(double _Complex z) {
  return clog(z) / LN2;
}

double _Complex negate(double _Complex z) {
  return -z;
}

double _Complex cnint(double _Complex z) {
  return nearbyint(creal(z)) + nearbyint(cimag(z)) * I;
}

double _Complex cnorm(double _Complex z) {
  return z * conj(z);
}

double _Complex csec(double _Complex z) {
  return 1 / ccos(z);
}

double _Complex csech(double _Complex z) {
  return csec(I * z);
}

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
  TOKEN_UNARY_FUNCTION,
  TOKEN_BINARY_FUNCTION,
  TOKEN_COMMA,
  TOKEN_INVISIBLE_TIMES,
};

struct token {
  enum token_type type;
  size_t start_index;
  size_t len;
  double _Complex value;
  unary_operation unary_eval;
  binary_operation binary_eval;
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
  { TOKEN_NEGATE, 99, 50 },
  { TOKEN_NUMBER, 254, 255 },
  { TOKEN_PLUS, 5, 6 },
  { TOKEN_TIMES, 7, 8 },
  { TOKEN_OPEN_PARENTHESIS, 0, 0 },
  { TOKEN_CLOSE_PARENTHESIS, 0, 0 },
  { TOKEN_UNARY_FUNCTION, 99, 50 },
  { TOKEN_BINARY_FUNCTION, 99, 50 },
  { TOKEN_COMMA, 0, 0 },
  { TOKEN_INVISIBLE_TIMES, 9, 10 },
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
    case TOKEN_COMMA:
      return true;
  }

  return false;
}

bool is_times_implied(enum token_type left_token, enum token_type right_token) {
  if (left_token == TOKEN_NUMBER) {
    return (right_token == TOKEN_OPEN_PARENTHESIS) ||
      (right_token == TOKEN_UNARY_FUNCTION) ||
      (right_token == TOKEN_BINARY_FUNCTION);
  } else if (left_token == TOKEN_CLOSE_PARENTHESIS) {
    return (right_token == TOKEN_OPEN_PARENTHESIS) ||
      (right_token == TOKEN_UNARY_FUNCTION) ||
      (right_token == TOKEN_BINARY_FUNCTION) ||
      (right_token == TOKEN_NUMBER);
  }

  return false;
}

struct symbol_details {
  const char *symbol;
  size_t len;
  enum token_type type;
  double _Complex value;
  unary_operation unary_eval;
  binary_operation binary_eval;
};

struct token *malloc_token(
  size_t start_index,
  struct symbol_details details
) {
  struct token *tok = malloc(sizeof(struct token));
  if (tok == nullptr) {
    return tok;
  }

  tok->type = details.type;
  tok->start_index = start_index;
  tok->len = details.len;
  tok->value = details.value;
  tok->unary_eval = details.unary_eval;
  tok->binary_eval = details.binary_eval;

  struct token_details tok_details = get_token_details(details.type);
  tok->left_binding = tok_details.left_binding;
  tok->right_binding = tok_details.right_binding;

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

size_t symbol_length(const char *s, size_t start_index) {
  char first = s[start_index];
  if (
    (first == '/') ||
    (first == '-') ||
    (first == '+') ||
    (first == '*') ||
    (first == '(') ||
    (first == ')') ||
    (first == ',')
  ) {
    return 1;
  }

  size_t len = 0;
  while (isalnum(s[start_index + len])) {
    ++len;
  }

  return len;
}

#define MAKE_SYMBOL(symbol, type, value, unary_eval, binary_eval) { (symbol), (sizeof(symbol) - 1), (type), (value), (unary_eval), (binary_eval) }
#define MAKE_NUMBER_SYMBOL(symbol, len, value) { (symbol), (len), TOKEN_NUMBER, (value) }
#define MAKE_UNARY_SYMBOL(symbol, unary_eval) MAKE_SYMBOL(symbol, TOKEN_UNARY_FUNCTION, 0, unary_eval, nullptr)
#define MAKE_BINARY_SYMBOL(symbol, binary_eval) MAKE_SYMBOL(symbol, TOKEN_BINARY_FUNCTION, 0, nullptr, binary_eval)
#define MAKE_OPERATOR_SYMBOL(symbol, type, binary_eval) MAKE_SYMBOL(symbol, type, 0, nullptr, binary_eval)
#define MAKE_SCOPE_SYMBOL(symbol, type) MAKE_SYMBOL(symbol, type, 0, nullptr, nullptr)

const struct symbol_details SYMBOL_DETAILS[] = {
  MAKE_SYMBOL("", TOKEN_UNKNOWN, 0, nullptr, nullptr),
  MAKE_UNARY_SYMBOL("abs", complex_abs),
  MAKE_UNARY_SYMBOL("arccos", cacos),
  MAKE_UNARY_SYMBOL("arccosh", cacosh),
  MAKE_UNARY_SYMBOL("arccot", cacot),
  MAKE_UNARY_SYMBOL("arccoth", cacoth),
  MAKE_UNARY_SYMBOL("arccsc", cacsc),
  MAKE_UNARY_SYMBOL("arccsch", cacsch),
  MAKE_UNARY_SYMBOL("arcsec", casec),
  MAKE_UNARY_SYMBOL("arcsech", casech),
  MAKE_UNARY_SYMBOL("arcsin", casin),
  MAKE_UNARY_SYMBOL("arcsinh", casinh),
  MAKE_UNARY_SYMBOL("arctan", catan),
  MAKE_UNARY_SYMBOL("arctanh", catanh),
  MAKE_UNARY_SYMBOL("arg", complex_arg),
  MAKE_UNARY_SYMBOL("ceil", cceil),
  MAKE_UNARY_SYMBOL("conj", conj),
  MAKE_UNARY_SYMBOL("cos", ccos),
  MAKE_UNARY_SYMBOL("cosh", ccosh),
  MAKE_UNARY_SYMBOL("cot", ccot),
  MAKE_UNARY_SYMBOL("coth", ccoth),
  MAKE_UNARY_SYMBOL("csc", ccsc),
  MAKE_UNARY_SYMBOL("csch", ccsch),
  MAKE_UNARY_SYMBOL("exp", cexp),
  MAKE_UNARY_SYMBOL("floor", cfloor),
  MAKE_UNARY_SYMBOL("frac", cfrac),
  MAKE_UNARY_SYMBOL("imag", complex_imag),
  MAKE_UNARY_SYMBOL("lg", clog2),
  MAKE_UNARY_SYMBOL("ln", clog),
  MAKE_BINARY_SYMBOL("log", clog_with_base),
  MAKE_UNARY_SYMBOL("log10", clog10),
  MAKE_UNARY_SYMBOL("nint", cnint),
  MAKE_UNARY_SYMBOL("norm", cnorm),
  MAKE_BINARY_SYMBOL("pow", cpow),
  MAKE_UNARY_SYMBOL("real", complex_real),
  MAKE_UNARY_SYMBOL("sec", csec),
  MAKE_UNARY_SYMBOL("sech", csech),
  MAKE_UNARY_SYMBOL("sin", csin),
  MAKE_UNARY_SYMBOL("sinh", csinh),
  MAKE_UNARY_SYMBOL("sqrt", csqrt),
  MAKE_UNARY_SYMBOL("tan", ctan),
  MAKE_UNARY_SYMBOL("tanh", ctanh),
  MAKE_OPERATOR_SYMBOL("/", TOKEN_DIVIDE, divide),
  MAKE_OPERATOR_SYMBOL("-", TOKEN_MINUS, minus),
  MAKE_OPERATOR_SYMBOL("+", TOKEN_PLUS, plus),
  MAKE_OPERATOR_SYMBOL("*", TOKEN_TIMES, times),
  MAKE_SCOPE_SYMBOL("(", TOKEN_OPEN_PARENTHESIS),
  MAKE_SCOPE_SYMBOL(")", TOKEN_CLOSE_PARENTHESIS),
  MAKE_SCOPE_SYMBOL(",", TOKEN_COMMA),
};

struct symbol_details get_symbol_details(const char *s, size_t start_index, size_t len) {
  for (size_t i = 1; i < sizeof(SYMBOL_DETAILS) / sizeof(struct symbol_details); ++i) {
    if (
      (SYMBOL_DETAILS[i].len == len) &&
      (strncmp((s + start_index), SYMBOL_DETAILS[i].symbol, len) == 0)
    ) {
      return SYMBOL_DETAILS[i];
    }
  }

  return SYMBOL_DETAILS[0];
}

int add_token(
  struct deque *tokens,
  size_t start_index,
  struct symbol_details details
) {
  struct token *tok = malloc_token(start_index, details);
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

struct parse_result tokenize(const char *s, struct deque **tokens) {
  *tokens = malloc_deque(token_deleter);
  size_t start_index = 0;
  size_t char_length = strlen(s);
  int deque_result;
  enum token_type last_type = TOKEN_UNKNOWN;

  struct parse_result result = SUCCESS_RESULT;
  while (start_index < char_length) {
    int c = s[start_index];

    size_t len = symbol_length(s, start_index);
    struct symbol_details details = get_symbol_details(s, start_index, len);

    bool was_function_last = (last_type == TOKEN_UNARY_FUNCTION) ||
      (last_type == TOKEN_BINARY_FUNCTION);
    if (was_function_last && (details.type != TOKEN_OPEN_PARENTHESIS)) {
      result = MAKE_RESULT(PARSE_ERROR_EXPECTED_OPEN_PARENTHESIS, start_index);
      CLEANUP_IF_FAILED(result);
    }

    if (c == '\0') {
      // Reached the end of the string. Embedded nulls are not supported
      break;
    } else if (isspace(c)) {
      ++start_index;
    } else if (is_numeric_character(c)) {
      size_t num_len;
      double _Complex value;
      result = parse_number(s, start_index, &num_len, &value);
      CLEANUP_IF_FAILED(result);

      struct symbol_details num_details = MAKE_NUMBER_SYMBOL(&s[start_index], num_len, value);

      deque_result = add_token(*tokens, start_index, num_details);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      start_index += num_len;
      last_type = num_details.type;
    } else if (details.type != TOKEN_UNKNOWN) {
      deque_result = add_token(*tokens, start_index, details);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      start_index += len;
      last_type = details.type;
    } else {
      result = MAKE_RESULT(PARSE_ERROR_UNKNOWN_SYMBOL, start_index);
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

    if (
      (last_token != nullptr) &&
      is_times_implied(last_token->type, current->type)
    ) {
      struct symbol_details details = MAKE_OPERATOR_SYMBOL("", TOKEN_INVISIBLE_TIMES, times);
      struct token *tok = malloc_token(current->start_index, details);
      if (tok == nullptr) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      deque_result = deque_insert_before(it, tok);
      if (deque_result != 0) {
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      last_token = tok;
    }

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
            struct symbol_details details = MAKE_SYMBOL("-", TOKEN_NEGATE, 0, negate, nullptr);
            struct token *tok = malloc_token(current->start_index, details);
            if (tok == nullptr) {
              result = OOM_RESULT;
              CLEANUP_IF_FAILED(result);
            }

            replace_iterator_data(&it, tok);
            last_token = tok;
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
  size_t open_start;
};

void free_scope(struct scope *s) {
  // The scope doesn't open the operator tokens and so cannot free them
  free_deque(s->expressions);

  free(s);
}

struct scope *malloc_scope(size_t start_index, size_t open_start) {
  struct scope *s = malloc(sizeof(struct scope));
  if (s == nullptr) {
    return nullptr;
  }

  s->start_index = start_index;
  s->open_start = open_start;
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

size_t expression_start_indices(
  struct deque *expressions,
  size_t start_after_index,
  size_t *indices,
  size_t count
) {
  size_t current_index = 0;
  struct iterator it = iterator_for(expressions);

  for (it; !is_end_iterator(it) && current_index < count; iterator_next(&it)) {
    struct expression *ex = iterator_data(it);
    if (ex->start_index > start_after_index) {
      indices[current_index] = ex->start_index;
      ++current_index;
    }
  }

  return current_index;
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

  size_t start_indices[3];
  size_t ex_count;

  struct token *tok = deque_pop_back(top->operators);

  switch (tok->type) {
    case TOKEN_DIVIDE:
    case TOKEN_MINUS:
    case TOKEN_PLUS:
    case TOKEN_TIMES:
    case TOKEN_INVISIBLE_TIMES:
      right_child = deque_pop_back(top->expressions);
      if (right_child == nullptr) {
        result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }

      if (right_child->start_index < tok->start_index) {
        result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }

      left_child = deque_pop_back(top->expressions);
      if (left_child == nullptr) {
        result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }

      if (left_child->start_index > tok->start_index) {
        result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }
      
      *ex = malloc_binary_operation_expression(
        tok->binary_eval,
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
    
    case TOKEN_BINARY_FUNCTION:
      ex_count = expression_start_indices(top->expressions, tok->start_index, start_indices, 3);
      if (ex_count < 2) {
        result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      } else if (ex_count > 2) {
        result = MAKE_RESULT(PARSE_ERROR_EXCESS_EXPRESSION, start_indices[2]);
        CLEANUP_IF_FAILED(result);
      }

      right_child = deque_pop_back(top->expressions);
      left_child = deque_pop_back(top->expressions);
      
      *ex = malloc_binary_operation_expression(
        tok->binary_eval,
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
    case TOKEN_UNARY_FUNCTION:
      ex_count = expression_start_indices(top->expressions, tok->start_index, start_indices, 2);
      if (ex_count < 1) {
        result = MAKE_RESULT(PARSE_ERROR_INCOMPLETE_EXPRESSION, tok->start_index);
        CLEANUP_IF_FAILED(result);
      } else if (ex_count > 1) {
        result = MAKE_RESULT(PARSE_ERROR_EXCESS_EXPRESSION, start_indices[1]);
        CLEANUP_IF_FAILED(result);
      }

      right_child = deque_pop_back(top->expressions);

      // Unary functions (and negate) need very large left bindings so that
      // they can stack as in `--1`. A low left binding will cause the first
      // negate to attempt to close itself immediately since the right binding
      // is so high. Unfortunately this causes issues with statements like
      // `-2 abs(3)` because the negate is unable to immediately bind with `2`
      // due to the high left binding of `abs`. When it comes time to complete
      // the negate the expression stack will be [`2`, `abs(3)`] and the negate
      // will attempt to apply to `abs(3)` improperly. This check detects that
      // case.
      struct expression *other_child = deque_peek_back(top->expressions);
      if ((other_child != nullptr) && (other_child->start_index > tok->start_index)) {
        result = MAKE_RESULT(PARSE_ERROR_EXCESS_EXPRESSION, right_child->start_index);
        CLEANUP_IF_FAILED(result);
      }

      *ex = malloc_unary_operation_expression(
        tok->unary_eval,
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

  struct scope *top = malloc_scope(0, 0);
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
      struct scope *new_top = malloc_scope(tok->start_index, tok->start_index);
      deque_result = deque_push_back(stack, new_top);
      if (deque_result != 0) {
        free_scope(new_top);
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }
    } else if (
      (tok->type == TOKEN_CLOSE_PARENTHESIS) ||
      (tok->type == TOKEN_COMMA)
    ) {
      if (deque_len(stack) == 1) {
        enum parse_error_type error_type = tok->type == TOKEN_COMMA ?
          PARSE_ERROR_INCOMPLETE_EXPRESSION :
          PARSE_ERROR_EXCESS_CLOSE_PARENTHESIS;
        result = MAKE_RESULT(error_type, tok->start_index);
        CLEANUP_IF_FAILED(result);
      }

      struct expression *ex;
      top = deque_peek_back(stack);

      result = close_scope(top, &ex);
      CLEANUP_IF_FAILED(result);

      size_t open_start = top->open_start;
      top = deque_pop_back(stack);
      free_scope(top);
      top = deque_peek_back(stack);

      deque_result = deque_push_back(top->expressions, ex);
      if (deque_result != 0) {
        free_expression(ex);
        result = OOM_RESULT;
        CLEANUP_IF_FAILED(result);
      }

      if (tok->type == TOKEN_COMMA) {
        struct scope *new_top = malloc_scope(tok->start_index, open_start);
        deque_result = deque_push_back(stack, new_top);
        if (deque_result != 0) {
          free_scope(new_top);
          result = OOM_RESULT;
          CLEANUP_IF_FAILED(result);
        }
      } else {
        struct token *top_op = deque_peek_back(top->operators);
        enum token_type type = top_op != nullptr ? top_op->type : TOKEN_UNKNOWN;
        if (type == TOKEN_BINARY_FUNCTION) {
          result = resolve_operator(top);
          CLEANUP_IF_FAILED(result);
        }
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
    result = MAKE_RESULT(PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, top->open_start);
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
