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
  char *symbol;
  size_t len;
  double _Complex value;
};

#define MAKE_VARIABLE_VALUE(symbol, value) { (symbol), (sizeof(symbol) - 1), (value) }

struct variables {
  size_t count;
  struct variable_value *values;
};

struct expression;

struct parse_result make_expression(const char *s, struct expression **ex);
void free_expression(struct expression *ex);
struct eval_result evaluate_expression(
  const struct expression *expression,
  const struct variables *vars,
  double _Complex *result
);

struct variables *malloc_variables(const struct expression *ex);
void free_variables(struct variables *vars);
bool has_variable(const struct variables *vars, const char *symbol);
int set_variable(struct variables *vars, const char *symbol, double _Complex value);

#endif
