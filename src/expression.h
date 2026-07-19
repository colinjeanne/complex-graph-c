#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stddef.h>

enum parse_error_type {
  PARSE_ERROR_SUCCESS,
  PARSE_ERROR_OOM,
  PARSE_ERROR_UNKNOWN_CHARACTER,
  PARSE_ERROR_EXPECTED_DIGIT,
  PARSE_ERROR_INCOMPLETE_EXPRESSION,
  PARSE_ERROR_EXCESS_EXPRESSION,
  PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS,
  PARSE_ERROR_EXCESS_CLOSE_PARENTHESIS,
  PARSE_ERROR_EXPECTED_OPEN_PARENTHESIS,
};

struct parse_result {
  enum parse_error_type type;
  size_t index;
};

enum eval_error_type {
  EVAL_ERROR_SUCCESS,
  EVAL_ERROR_UNKNOWN_VARIABLE,
};

struct eval_result {
  enum eval_error_type type;
  size_t index;
};

struct variable_value {
  const char *symbol;
  size_t len;
  double _Complex value;
};

#define MAKE_VARIABLE_VALUE(symbol, value) { (symbol), (sizeof(symbol) - 1), (value) }

struct expression;

struct parse_result make_expression(const char *s, struct expression **ex);
void free_expression(struct expression *ex);
struct eval_result evaluate_expression(
  const struct expression *expression,
  const struct variable_value *variables,
  size_t variable_count,
  double _Complex *result
);

#endif
