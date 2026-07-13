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
  PARSE_ERROR_UNKNOWN_SYMBOL,
  PARSE_ERROR_EXPECTED_OPEN_PARENTHESIS,
};

struct parse_result {
  enum parse_error_type type;
  size_t index;
};

struct expression;

struct parse_result make_expression(const char *s, struct expression **ex);
void free_expression(struct expression *ex);
double _Complex evaluate_expression(const struct expression *expression);

#endif
