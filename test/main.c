#include <complex.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#include "expression.h"

struct test_case {
  const char *expression_str;
  enum parse_error_type type;
  size_t index;
  double _Complex value;
};

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
    printf("Expected error type %d received %d\n", c.type, result.type);
    success = -1;
    goto cleanup;
  } else if (result.index != c.index) {
    printf("Expected error index %lu received %lu\n", c.index, result.index);
    success = -1;
    goto cleanup;
  }

  if (result.type == PARSE_ERROR_SUCCESS) {
    double _Complex value = evaluate_expression(ex);
    if (!are_equal(value, c.value)) {
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

  struct test_case cases[] = {
    { "", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "1", PARSE_ERROR_SUCCESS, 0, 1 },
    { " 1 ", PARSE_ERROR_SUCCESS, 0, 1 },
    { "10", PARSE_ERROR_SUCCESS, 0, 10 },
    { "01", PARSE_ERROR_SUCCESS, 0, 1 },
    { ".5", PARSE_ERROR_SUCCESS, 0, 0.5 },
    { ".0", PARSE_ERROR_SUCCESS, 0, 0 },
    { ".005", PARSE_ERROR_SUCCESS, 0, 0.005 },
    { "1.1", PARSE_ERROR_SUCCESS, 0, 1.1 },
    { "1.0", PARSE_ERROR_SUCCESS, 0, 1 },
    { "10.00", PARSE_ERROR_SUCCESS, 0, 10 },
    { "10.05", PARSE_ERROR_SUCCESS, 0, 10.05 },
    { ".", PARSE_ERROR_EXPECTED_DIGIT, 1, 0 },
    { ".0.", PARSE_ERROR_EXPECTED_DIGIT, 3, 0 },
    { ".0.0", PARSE_ERROR_EXCESS_EXPRESSION, 2, 2 },
    { "1 1", PARSE_ERROR_EXCESS_EXPRESSION, 2, 2 },
    { "1+2", PARSE_ERROR_SUCCESS, 0, 3 },
    { "1-2", PARSE_ERROR_SUCCESS, 0, -1 },
    { "1+2-3+4", PARSE_ERROR_SUCCESS, 0, 4 },
    { "1/2", PARSE_ERROR_SUCCESS, 0, 0.5 },
    { "2*3", PARSE_ERROR_SUCCESS, 0, 6 },
    { "1 - 2 * 3", PARSE_ERROR_SUCCESS, 0, -5 },
    { "2 * 3 - 1", PARSE_ERROR_SUCCESS, 0, 5 },
    { "2 * 3 / 4 * 6", PARSE_ERROR_SUCCESS, 0, 9 },
    { "1+", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1, 0 },
    { "0.5 + .5", PARSE_ERROR_SUCCESS, 0, 1 },
    { "+", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "-", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "+1", PARSE_ERROR_SUCCESS, 0, 1 },
    { "-1", PARSE_ERROR_SUCCESS, 0, -1 },
    { "*1", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "-+1", PARSE_ERROR_SUCCESS, 0, -1 },
    { "--1", PARSE_ERROR_SUCCESS, 0, 1 },
    { "+ 1 1", PARSE_ERROR_EXCESS_EXPRESSION, 4, 0 },
    { "1 1 +", PARSE_ERROR_EXCESS_EXPRESSION, 2, 0 },
    { "1 ++ 1", PARSE_ERROR_SUCCESS, 0, 2 },
    { "1 +- 2", PARSE_ERROR_SUCCESS, 0, -1 },
    { "1 -- 2", PARSE_ERROR_SUCCESS, 0, 3 },
    { "-2*3", PARSE_ERROR_SUCCESS, 0, -6 },
    { "-2*+3", PARSE_ERROR_SUCCESS, 0, -6 },
    { "2 * -3", PARSE_ERROR_SUCCESS, 0, -6 },
    { "-2 * -3", PARSE_ERROR_SUCCESS, 0, 6 },
    { "(", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 0, 0 },
    { ")", PARSE_ERROR_EXCESS_CLOSE_PARENTHESIS, 0, 0 },
    { "()", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "(+)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "(-)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1, 0 },
    { "(*)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1, 0 },
    { "(1", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 0, 0 },
    { "1)", PARSE_ERROR_EXCESS_CLOSE_PARENTHESIS, 1, 0 },
    { "(1)", PARSE_ERROR_SUCCESS, 0, 1 },
    { "((1))", PARSE_ERROR_SUCCESS, 0, 1 },
    { "(-1)", PARSE_ERROR_SUCCESS, 0, -1 },
    { "-(1)", PARSE_ERROR_SUCCESS, 0, -1 },
    { "(-)1", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1, 0 },
    { "-(-1)", PARSE_ERROR_SUCCESS, 0, 1 },
    { "(1 + )", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3, 0 },
    { "(* 2)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1, 0 },
    { "(1 + 2)", PARSE_ERROR_SUCCESS, 0, 3 },
    { "(1) + (2)", PARSE_ERROR_SUCCESS, 0, 3 },
    { "1 + (", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 4, 0 },
    { "1 + ()", PARSE_ERROR_INCOMPLETE_EXPRESSION, 4, 0 },
    { "(1 1)", PARSE_ERROR_EXCESS_EXPRESSION, 3, 0 },
    { "2(3)", PARSE_ERROR_SUCCESS, 0, 6 },
    { "(2)3", PARSE_ERROR_SUCCESS, 0, 6 },
    { "(2) (3)", PARSE_ERROR_SUCCESS, 0, 6 },
    { "2 * (3 - 1)", PARSE_ERROR_SUCCESS, 0, 4 },
    { "abs(2)", PARSE_ERROR_SUCCESS, 0, 2 },
    { "abs(-2)", PARSE_ERROR_SUCCESS, 0, 2 },
    { "abs -2", PARSE_ERROR_EXPECTED_OPEN_PARENTHESIS, 3, 0 },
    { "abs()", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3, 0 },
    { "abs", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "abs(", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 3, 0 },
    { "-2 abs", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3, 0 },
    { "2 abs(3)", PARSE_ERROR_SUCCESS, 0, 6 },
    { "-2 abs(3)", PARSE_ERROR_SUCCESS, 0, -6 },
    { "(-2)abs(3)", PARSE_ERROR_SUCCESS, 0, -6 },
    { "abs(3)(-2)", PARSE_ERROR_SUCCESS, 0, -6 },
    { "abs(3)2", PARSE_ERROR_SUCCESS, 0, 6 },
    { "abs(3)abs(2)", PARSE_ERROR_SUCCESS, 0, 6 },
    { "-abs(2)", PARSE_ERROR_SUCCESS, 0, -2 },
    { "1 + abs(2)", PARSE_ERROR_SUCCESS, 0, 3 },
    { "abs(2, 3)", PARSE_ERROR_EXCESS_EXPRESSION, 5, 0 },
    { "pow(2, 3)", PARSE_ERROR_SUCCESS, 0, 8 },
    { "3pow(2, 3)", PARSE_ERROR_SUCCESS, 0, 24 },
    { "pow(2, 3)3", PARSE_ERROR_SUCCESS, 0, 24 },
    { "pow(2, 3)pow(2, 3)", PARSE_ERROR_SUCCESS, 0, 64 },
    { "pow(2)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "pow(2", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 3, 0 },
    { "pow(2, 3, 4)", PARSE_ERROR_EXCESS_EXPRESSION, 8, 0 },
    { "pow(2, 3", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 3, 0 },
    { "pow(, 3)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3, 0 },
    { "pow(,)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3, 0 },
    { "pow(2 3)", PARSE_ERROR_EXCESS_EXPRESSION, 6, 0 },
    { "pow(2), 3", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "pow(2) 3", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "2 pow(3)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 2, 0 },
    { "pow 2 3", PARSE_ERROR_EXPECTED_OPEN_PARENTHESIS, 3, 0 },
    { ",", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "1 + ,", PARSE_ERROR_INCOMPLETE_EXPRESSION, 4, 0 },
    { "(,)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "1 + (,)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 4, 0 },
    { "1 + (2,)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 6, 0 },
    { "1,", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1, 0 },
    { ",2", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0, 0 },
    { "1,2", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1, 0 },
  };

  size_t success_count = sizeof(cases) / sizeof(struct test_case);

  for (size_t i = 0; i < success_count; ++i) {
    int result = run_test_case(cases[i]);
    if (result != 0) {
      ++fails;
    }
  }

  return fails ? -1 : 0;
}
