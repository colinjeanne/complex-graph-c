#include <complex.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#include "expression.h"

struct test_case {
  const char *expression_str;
  const struct variable_value *variables;
  size_t variable_count;
  enum parse_error_type type;
  enum eval_error_type ev_type;
  size_t index;
  double _Complex value;
};

#define PARSE_ERROR_TEST_CASE(ex, type, index) { (ex), nullptr, 0, (type), EVAL_ERROR_SUCCESS, (index) }
#define EVAL_ERROR_TEST_CASE(ex, type, index, variables) { (ex), (variables), (sizeof(variables) / sizeof(struct variable_value)), PARSE_ERROR_SUCCESS, (type), (index) }
#define SUCCESS_TEST_CASE(ex, variables, value) { (ex), (variables), (sizeof(variables) / sizeof(struct variable_value)), PARSE_ERROR_SUCCESS, EVAL_ERROR_SUCCESS, 0, (value) }

const double EPISON = 0.000001;

int are_equal(double _Complex u, double _Complex v) {
  return fabs(creal(u) - creal(v)) < EPISON &&
    fabs(cimag(u) - cimag(v)) < EPISON;
}

int run_test_case(struct test_case c) {
  printf("Testing \"%s\": ", c.expression_str);

  int success = 0;
  const char *err = "";
  struct expression *ex;

  struct parse_result result = make_expression(c.expression_str, &ex);
  if (result.type != c.type) {
    printf("Expected parse error type %d received %d\n", c.type, result.type);
    success = -1;
    goto cleanup;
  } else if (
    (result.type != PARSE_ERROR_SUCCESS) &&
    (result.index != c.index)
  ) {
    printf("Expected parse error index %lu received %lu\n", c.index, result.index);
    success = -1;
    goto cleanup;
  }

  if (result.type == PARSE_ERROR_SUCCESS) {
    double _Complex value;
    struct eval_result ev_result = evaluate_expression(ex, c.variables, c.variable_count, &value);
    if (ev_result.type != c.ev_type) {
      printf("Expected eval error type %d received %d\n", c.type, ev_result.type);
      success = -1;
      goto cleanup;
    } else if (
      (ev_result.type != EVAL_ERROR_SUCCESS) &&
      (ev_result.index != c.index)
    ) {
      printf("Expected eval error index %lu received %lu\n", c.index, ev_result.index);
      success = -1;
      goto cleanup;
    }

    if ((ev_result.type == EVAL_ERROR_SUCCESS) && !are_equal(value, c.value)) {
      printf(
        "Expected value (%f, %f) received (%f, %f)\n",
        creal(c.value),
        cimag(c.value),
        creal(value),
        cimag(value)
      );
      success = -1;
      goto cleanup;
    }
  }

  if (success == 0) {
    printf("Pass\n");
  }

  cleanup:
  free_expression(ex);

  return success;
}

int main(void) {
  int fails = 0;

  struct variable_value variables[] = {
    MAKE_VARIABLE_VALUE("x", 1),
    MAKE_VARIABLE_VALUE("y", I),
    MAKE_VARIABLE_VALUE("quux", 3),
  };

  struct test_case cases[] = {
    PARSE_ERROR_TEST_CASE("", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    SUCCESS_TEST_CASE("1", nullptr, 1),
    SUCCESS_TEST_CASE(" 1 ", nullptr, 1),
    SUCCESS_TEST_CASE("10", nullptr, 10),
    SUCCESS_TEST_CASE("01", nullptr, 1),
    SUCCESS_TEST_CASE(".5", nullptr, 0.5),
    SUCCESS_TEST_CASE(".0", nullptr, 0),
    SUCCESS_TEST_CASE(".005", nullptr, 0.005),
    SUCCESS_TEST_CASE("1.1", nullptr, 1.1),
    SUCCESS_TEST_CASE("1.0", nullptr, 1),
    SUCCESS_TEST_CASE("10.00", nullptr, 10),
    SUCCESS_TEST_CASE("10.05", nullptr, 10.05),
    PARSE_ERROR_TEST_CASE(".", PARSE_ERROR_EXPECTED_DIGIT, 1),
    PARSE_ERROR_TEST_CASE(".0.", PARSE_ERROR_EXPECTED_DIGIT, 3),
    PARSE_ERROR_TEST_CASE(".0.0", PARSE_ERROR_EXCESS_EXPRESSION, 2),
    PARSE_ERROR_TEST_CASE("1 1", PARSE_ERROR_EXCESS_EXPRESSION, 2),
    PARSE_ERROR_TEST_CASE("i i", PARSE_ERROR_EXCESS_EXPRESSION, 2),
    SUCCESS_TEST_CASE("1+2", nullptr, 3),
    SUCCESS_TEST_CASE("1-2", nullptr, -1),
    SUCCESS_TEST_CASE("1+2-3+4", nullptr, 4),
    SUCCESS_TEST_CASE("1/2", nullptr, 0.5),
    SUCCESS_TEST_CASE("2*3", nullptr, 6),
    SUCCESS_TEST_CASE("1 - 2 * 3", nullptr, -5),
    SUCCESS_TEST_CASE("2 * 3 - 1", nullptr, 5),
    SUCCESS_TEST_CASE("2 * 3 / 4 * 6", nullptr, 9),
    PARSE_ERROR_TEST_CASE("1+", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    SUCCESS_TEST_CASE("0.5 + .5", nullptr, 1),
    PARSE_ERROR_TEST_CASE("+", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("-", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    SUCCESS_TEST_CASE("+1", nullptr, 1),
    SUCCESS_TEST_CASE("-1", nullptr, -1),
    PARSE_ERROR_TEST_CASE("*1", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    SUCCESS_TEST_CASE("-+1", nullptr, -1),
    SUCCESS_TEST_CASE("--1", nullptr, 1),
    PARSE_ERROR_TEST_CASE("+ 1 1", PARSE_ERROR_EXCESS_EXPRESSION, 4),
    PARSE_ERROR_TEST_CASE("1 1 +", PARSE_ERROR_EXCESS_EXPRESSION, 2),
    SUCCESS_TEST_CASE("1 ++ 1", nullptr, 2),
    SUCCESS_TEST_CASE("1 +- 2", nullptr, -1),
    SUCCESS_TEST_CASE("1 -- 2", nullptr, 3),
    SUCCESS_TEST_CASE("-2*3", nullptr, -6),
    SUCCESS_TEST_CASE("-2*+3", nullptr, -6),
    SUCCESS_TEST_CASE("2 * -3", nullptr, -6),
    SUCCESS_TEST_CASE("-2 * -3", nullptr, 6),
    SUCCESS_TEST_CASE("2i", nullptr, 2 * I),
    PARSE_ERROR_TEST_CASE("(", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 0),
    PARSE_ERROR_TEST_CASE(")", PARSE_ERROR_EXCESS_CLOSE_PARENTHESIS, 0),
    PARSE_ERROR_TEST_CASE("()", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("(+)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("(-)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    PARSE_ERROR_TEST_CASE("(*)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    PARSE_ERROR_TEST_CASE("(1", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 0),
    PARSE_ERROR_TEST_CASE("1)", PARSE_ERROR_EXCESS_CLOSE_PARENTHESIS, 1),
    SUCCESS_TEST_CASE("(1)", nullptr, 1),
    SUCCESS_TEST_CASE("((1))", nullptr, 1),
    SUCCESS_TEST_CASE("(-1)", nullptr, -1),
    SUCCESS_TEST_CASE("-(1)", nullptr, -1),
    PARSE_ERROR_TEST_CASE("(-)1", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    SUCCESS_TEST_CASE("-(-1)", nullptr, 1),
    PARSE_ERROR_TEST_CASE("(1 + )", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3),
    PARSE_ERROR_TEST_CASE("(* 2)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    SUCCESS_TEST_CASE("(1 + 2)", nullptr, 3),
    SUCCESS_TEST_CASE("(1) + (2)", nullptr, 3),
    PARSE_ERROR_TEST_CASE("1 + (", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 4),
    PARSE_ERROR_TEST_CASE("1 + ()", PARSE_ERROR_INCOMPLETE_EXPRESSION, 4),
    PARSE_ERROR_TEST_CASE("(1 1)", PARSE_ERROR_EXCESS_EXPRESSION, 3),
    SUCCESS_TEST_CASE("2(3)", nullptr, 6),
    SUCCESS_TEST_CASE("(2)3", nullptr, 6),
    SUCCESS_TEST_CASE("(2) (3)", nullptr, 6),
    SUCCESS_TEST_CASE("2 * (3 - 1)", nullptr, 4),
    SUCCESS_TEST_CASE("abs(2)", nullptr, 2),
    SUCCESS_TEST_CASE("abs(-2)", nullptr, 2),
    PARSE_ERROR_TEST_CASE("abs -2", PARSE_ERROR_EXPECTED_OPEN_PARENTHESIS, 3),
    PARSE_ERROR_TEST_CASE("abs()", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3),
    PARSE_ERROR_TEST_CASE("abs", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("abs(", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 3),
    PARSE_ERROR_TEST_CASE("-2 abs", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3),
    SUCCESS_TEST_CASE("2 abs(3)", nullptr, 6),
    SUCCESS_TEST_CASE("-2 abs(3)", nullptr, -6),
    SUCCESS_TEST_CASE("(-2)abs(3)", nullptr, -6),
    SUCCESS_TEST_CASE("abs(3)(-2)", nullptr, -6),
    SUCCESS_TEST_CASE("abs(3)2", nullptr, 6),
    SUCCESS_TEST_CASE("abs(3)abs(2)", nullptr, 6),
    SUCCESS_TEST_CASE("-abs(2)", nullptr, -2),
    SUCCESS_TEST_CASE("1 + abs(2)", nullptr, 3),
    PARSE_ERROR_TEST_CASE("abs(2, 3)", PARSE_ERROR_EXCESS_EXPRESSION, 5),
    SUCCESS_TEST_CASE("pow(2, 3)", nullptr, 8),
    SUCCESS_TEST_CASE("3pow(2, 3)", nullptr, 24),
    SUCCESS_TEST_CASE("pow(2, 3)3", nullptr, 24),
    SUCCESS_TEST_CASE("pow(2, 3)pow(2, 3)", nullptr, 64),
    PARSE_ERROR_TEST_CASE("pow(2)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("pow(2", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 3),
    PARSE_ERROR_TEST_CASE("pow(2, 3, 4)", PARSE_ERROR_EXCESS_EXPRESSION, 8),
    PARSE_ERROR_TEST_CASE("pow(2, 3", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 3),
    PARSE_ERROR_TEST_CASE("pow(, 3)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3),
    PARSE_ERROR_TEST_CASE("pow(,)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3),
    PARSE_ERROR_TEST_CASE("pow(2 3)", PARSE_ERROR_EXCESS_EXPRESSION, 6),
    PARSE_ERROR_TEST_CASE("pow(2), 3", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("pow(2) 3", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("2 pow(3)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 2),
    PARSE_ERROR_TEST_CASE("pow 2 3", PARSE_ERROR_EXPECTED_OPEN_PARENTHESIS, 3),
    PARSE_ERROR_TEST_CASE(",", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("1 + ,", PARSE_ERROR_INCOMPLETE_EXPRESSION, 4),
    PARSE_ERROR_TEST_CASE("(,)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("1 + (,)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 4),
    PARSE_ERROR_TEST_CASE("1 + (2,)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 6),
    PARSE_ERROR_TEST_CASE("1,", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    PARSE_ERROR_TEST_CASE(",2", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("1,2", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    SUCCESS_TEST_CASE("x", variables, 1),
    SUCCESS_TEST_CASE("y", variables, I),
    SUCCESS_TEST_CASE("3", variables, 3),
    SUCCESS_TEST_CASE("2x", variables, 2),
    SUCCESS_TEST_CASE("quux * 4", variables, 12),
    SUCCESS_TEST_CASE("pow(y, 2)", variables, -1),
    SUCCESS_TEST_CASE("abs(-y)", variables, 1),
    EVAL_ERROR_TEST_CASE("x", EVAL_ERROR_UNKNOWN_VARIABLE, 0, nullptr),
    EVAL_ERROR_TEST_CASE("3 + z", EVAL_ERROR_UNKNOWN_VARIABLE, 4, variables),
  };

  size_t success_count = sizeof(cases) / sizeof(struct test_case);

  for (size_t i = 0; i < success_count; ++i) {
    int result = run_test_case(cases[i]);
    if (result != 0) {
      ++fails;
    }
  }

  printf("\n%lu/%lu tests succeeded\n", success_count - fails, success_count);

  return fails ? -1 : 0;
}
