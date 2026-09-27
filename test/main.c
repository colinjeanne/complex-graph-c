#include <complex.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#include "expression.h"

struct test_case {
  const char *expression_str;
  const struct variables vars;
  enum parse_error_type type;
  enum eval_error_type ev_type;
  size_t index;
  double _Complex value;
};

struct variables empty_vars = { 0, nullptr };

#define PARSE_ERROR_TEST_CASE(ex, type, index) { (ex), (empty_vars), (type), EVAL_ERROR_SUCCESS, (index) }
#define EVAL_ERROR_TEST_CASE(ex, type, index, vars) { (ex), (vars), PARSE_ERROR_SUCCESS, (type), (index) }
#define SUCCESS_TEST_CASE(ex, vars, value) { (ex), (vars), PARSE_ERROR_SUCCESS, EVAL_ERROR_SUCCESS, 0, (value) }

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
    struct eval_result ev_result = evaluate_expression(ex, &c.vars, &value);
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

  struct variable_value values[] = {
    MAKE_VARIABLE_VALUE("x", 1),
    MAKE_VARIABLE_VALUE("y", I),
    MAKE_VARIABLE_VALUE("quux", 3),
  };

  struct variables vars = {
    sizeof(values) / sizeof(struct variable_value),
    values
  };

  struct test_case cases[] = {
    PARSE_ERROR_TEST_CASE("", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    SUCCESS_TEST_CASE("1", empty_vars, 1),
    SUCCESS_TEST_CASE(" 1 ", empty_vars, 1),
    SUCCESS_TEST_CASE("10", empty_vars, 10),
    SUCCESS_TEST_CASE("01", empty_vars, 1),
    SUCCESS_TEST_CASE(".5", empty_vars, 0.5),
    SUCCESS_TEST_CASE(".0", empty_vars, 0),
    SUCCESS_TEST_CASE(".005", empty_vars, 0.005),
    SUCCESS_TEST_CASE("1.1", empty_vars, 1.1),
    SUCCESS_TEST_CASE("1.0", empty_vars, 1),
    SUCCESS_TEST_CASE("10.00", empty_vars, 10),
    SUCCESS_TEST_CASE("10.05", empty_vars, 10.05),
    PARSE_ERROR_TEST_CASE(".", PARSE_ERROR_EXPECTED_DIGIT, 1),
    PARSE_ERROR_TEST_CASE(".0.", PARSE_ERROR_EXPECTED_DIGIT, 3),
    PARSE_ERROR_TEST_CASE(".0.0", PARSE_ERROR_EXCESS_EXPRESSION, 2),
    PARSE_ERROR_TEST_CASE("1 1", PARSE_ERROR_EXCESS_EXPRESSION, 2),
    PARSE_ERROR_TEST_CASE("i i", PARSE_ERROR_EXCESS_EXPRESSION, 2),
    SUCCESS_TEST_CASE("1+2", empty_vars, 3),
    SUCCESS_TEST_CASE("1-2", empty_vars, -1),
    SUCCESS_TEST_CASE("1+2-3+4", empty_vars, 4),
    SUCCESS_TEST_CASE("1/2", empty_vars, 0.5),
    SUCCESS_TEST_CASE("2*3", empty_vars, 6),
    SUCCESS_TEST_CASE("1 - 2 * 3", empty_vars, -5),
    SUCCESS_TEST_CASE("2 * 3 - 1", empty_vars, 5),
    SUCCESS_TEST_CASE("2 * 3 / 4 * 6", empty_vars, 9),
    PARSE_ERROR_TEST_CASE("1+", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    SUCCESS_TEST_CASE("0.5 + .5", empty_vars, 1),
    PARSE_ERROR_TEST_CASE("+", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("-", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    SUCCESS_TEST_CASE("+1", empty_vars, 1),
    SUCCESS_TEST_CASE("-1", empty_vars, -1),
    PARSE_ERROR_TEST_CASE("*1", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    SUCCESS_TEST_CASE("-+1", empty_vars, -1),
    SUCCESS_TEST_CASE("--1", empty_vars, 1),
    PARSE_ERROR_TEST_CASE("+ 1 1", PARSE_ERROR_EXCESS_EXPRESSION, 4),
    PARSE_ERROR_TEST_CASE("1 1 +", PARSE_ERROR_EXCESS_EXPRESSION, 2),
    SUCCESS_TEST_CASE("1 ++ 1", empty_vars, 2),
    SUCCESS_TEST_CASE("1 +- 2", empty_vars, -1),
    SUCCESS_TEST_CASE("1 -- 2", empty_vars, 3),
    SUCCESS_TEST_CASE("-2*3", empty_vars, -6),
    SUCCESS_TEST_CASE("-2*+3", empty_vars, -6),
    SUCCESS_TEST_CASE("2 * -3", empty_vars, -6),
    SUCCESS_TEST_CASE("-2 * -3", empty_vars, 6),
    SUCCESS_TEST_CASE("2i", empty_vars, 2 * I),
    PARSE_ERROR_TEST_CASE("(", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 0),
    PARSE_ERROR_TEST_CASE(")", PARSE_ERROR_EXCESS_CLOSE_PARENTHESIS, 0),
    PARSE_ERROR_TEST_CASE("()", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("(+)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("(-)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    PARSE_ERROR_TEST_CASE("(*)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    PARSE_ERROR_TEST_CASE("(1", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 0),
    PARSE_ERROR_TEST_CASE("1)", PARSE_ERROR_EXCESS_CLOSE_PARENTHESIS, 1),
    SUCCESS_TEST_CASE("(1)", empty_vars, 1),
    SUCCESS_TEST_CASE("((1))", empty_vars, 1),
    SUCCESS_TEST_CASE("(-1)", empty_vars, -1),
    SUCCESS_TEST_CASE("-(1)", empty_vars, -1),
    PARSE_ERROR_TEST_CASE("(-)1", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    SUCCESS_TEST_CASE("-(-1)", empty_vars, 1),
    PARSE_ERROR_TEST_CASE("(1 + )", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3),
    PARSE_ERROR_TEST_CASE("(* 2)", PARSE_ERROR_INCOMPLETE_EXPRESSION, 1),
    SUCCESS_TEST_CASE("(1 + 2)", empty_vars, 3),
    SUCCESS_TEST_CASE("(1) + (2)", empty_vars, 3),
    PARSE_ERROR_TEST_CASE("1 + (", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 4),
    PARSE_ERROR_TEST_CASE("1 + ()", PARSE_ERROR_INCOMPLETE_EXPRESSION, 4),
    PARSE_ERROR_TEST_CASE("(1 1)", PARSE_ERROR_EXCESS_EXPRESSION, 3),
    SUCCESS_TEST_CASE("2(3)", empty_vars, 6),
    SUCCESS_TEST_CASE("(2)3", empty_vars, 6),
    SUCCESS_TEST_CASE("(2) (3)", empty_vars, 6),
    SUCCESS_TEST_CASE("2 * (3 - 1)", empty_vars, 4),
    SUCCESS_TEST_CASE("abs(2)", empty_vars, 2),
    SUCCESS_TEST_CASE("abs(-2)", empty_vars, 2),
    PARSE_ERROR_TEST_CASE("abs -2", PARSE_ERROR_EXPECTED_OPEN_PARENTHESIS, 3),
    PARSE_ERROR_TEST_CASE("abs()", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3),
    PARSE_ERROR_TEST_CASE("abs", PARSE_ERROR_INCOMPLETE_EXPRESSION, 0),
    PARSE_ERROR_TEST_CASE("abs(", PARSE_ERROR_UNMATCHED_OPEN_PARENTHESIS, 3),
    PARSE_ERROR_TEST_CASE("-2 abs", PARSE_ERROR_INCOMPLETE_EXPRESSION, 3),
    SUCCESS_TEST_CASE("2 abs(3)", empty_vars, 6),
    SUCCESS_TEST_CASE("-2 abs(3)", empty_vars, -6),
    SUCCESS_TEST_CASE("(-2)abs(3)", empty_vars, -6),
    SUCCESS_TEST_CASE("abs(3)(-2)", empty_vars, -6),
    SUCCESS_TEST_CASE("abs(3)2", empty_vars, 6),
    SUCCESS_TEST_CASE("abs(3)abs(2)", empty_vars, 6),
    SUCCESS_TEST_CASE("-abs(2)", empty_vars, -2),
    SUCCESS_TEST_CASE("1 + abs(2)", empty_vars, 3),
    PARSE_ERROR_TEST_CASE("abs(2, 3)", PARSE_ERROR_EXCESS_EXPRESSION, 5),
    SUCCESS_TEST_CASE("pow(2, 3)", empty_vars, 8),
    SUCCESS_TEST_CASE("3pow(2, 3)", empty_vars, 24),
    SUCCESS_TEST_CASE("pow(2, 3)3", empty_vars, 24),
    SUCCESS_TEST_CASE("pow(2, 3)pow(2, 3)", empty_vars, 64),
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
    SUCCESS_TEST_CASE("x", vars, 1),
    SUCCESS_TEST_CASE("y", vars, I),
    SUCCESS_TEST_CASE("3", vars, 3),
    SUCCESS_TEST_CASE("2x", vars, 2),
    SUCCESS_TEST_CASE("quux * 4", vars, 12),
    SUCCESS_TEST_CASE("pow(y, 2)", vars, -1),
    SUCCESS_TEST_CASE("abs(-y)", vars, 1),
    EVAL_ERROR_TEST_CASE("x", EVAL_ERROR_UNKNOWN_VARIABLE, 0, empty_vars),
    EVAL_ERROR_TEST_CASE("3 + z", EVAL_ERROR_UNKNOWN_VARIABLE, 4, vars),
  };

  size_t success_count = sizeof(cases) / sizeof(struct test_case);

  for (size_t i = 0; i < success_count; ++i) {
    int result = run_test_case(cases[i]);
    if (result != 0) {
      ++fails;
    }
  }

  ++success_count;
  printf("Testing setting empty symbol on empty variables: ");
  if (set_variable(&empty_vars, "", 0) == 0) {
    ++fails;
    printf("Fail\n");
  } else {
    printf("Pass\n");
  }

  struct variable_value set_values[] = {
    MAKE_VARIABLE_VALUE("x", 2)
  };

  struct variables set_vars = {
    1,
    set_values
  };

  ++success_count;
  printf("Testing setting empty symbol on non-empty variables: ");
  if (set_variable(&set_vars, "", 0) == 0) {
    ++fails;
    printf("Fail\n");
  } else {
    printf("Pass\n");
  }

  ++success_count;
  printf("Testing setting known symbol on non-empty variables: ");
  if (set_variable(&set_vars, "x", 3) != 0) {
    ++fails;
    printf("Fail\n");
  }

  if (set_vars.values[0].value != 3) {
    ++fails;
    printf("Unexpected value\n");
  } else {
    printf("Pass\n");
  }

  const char test_vars_str[] = "3a + b + -a * e / foo";
  struct expression *test_vars_ex = nullptr;
  struct parse_result test_vars_result = make_expression(test_vars_str, &test_vars_ex);
  if (test_vars_result.type != PARSE_ERROR_SUCCESS) {
    printf("Failed to allocate test expression\n");
    free_expression(test_vars_ex);
  } else {
    struct variables *test_vars = malloc_variables(test_vars_ex);
    if (test_vars == nullptr) {
      printf("Failed to allocate test variables\n");
      free_expression(test_vars_ex);
    } else {
      ++success_count;
      printf("Testing variable count of %s is 3: ", test_vars_str);
      if (test_vars->count == 3) {
        printf("Pass\n");
      } else {
        ++fails;
        printf("Unexpected %lu variables\n", test_vars->count);
      }

      ++success_count;
      printf("Testing has variable a: ");
      if (has_variable(test_vars, "a")) {
        printf("Pass\n");
      } else {
        ++fails;
        printf("Fail\n");
      }

      ++success_count;
      printf("Testing has variable b: ");
      if (has_variable(test_vars, "b")) {
        printf("Pass\n");
      } else {
        ++fails;
        printf("Fail\n");
      }

      ++success_count;
      printf("Testing has variable foo: ");
      if (has_variable(test_vars, "foo")) {
        printf("Pass\n");
      } else {
        ++fails;
        printf("Fail\n");
      }

      free_variables(test_vars);
      free_expression(test_vars_ex);
    }
  }

  printf("\n%lu/%lu tests succeeded\n", success_count - fails, success_count);

  return fails ? -1 : 0;
}
