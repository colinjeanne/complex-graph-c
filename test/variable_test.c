#include <stdio.h>

#include "expression.h"
#include "variable_test.h"

struct test_results run_variable_tests(void) {
  size_t fails = 0;
  size_t total_tests = 0;

  struct variables empty_vars = { 0, nullptr };

  ++total_tests;
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

  ++total_tests;
  printf("Testing setting empty symbol on non-empty variables: ");
  if (set_variable(&set_vars, "", 0) == 0) {
    ++fails;
    printf("Fail\n");
  } else {
    printf("Pass\n");
  }

  ++total_tests;
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
      ++total_tests;
      printf("Testing variable count of %s is 3: ", test_vars_str);
      if (test_vars->count == 3) {
        printf("Pass\n");
      } else {
        ++fails;
        printf("Unexpected %lu variables\n", test_vars->count);
      }

      ++total_tests;
      printf("Testing has variable a: ");
      if (has_variable(test_vars, "a")) {
        printf("Pass\n");
      } else {
        ++fails;
        printf("Fail\n");
      }

      ++total_tests;
      printf("Testing has variable b: ");
      if (has_variable(test_vars, "b")) {
        printf("Pass\n");
      } else {
        ++fails;
        printf("Fail\n");
      }

      ++total_tests;
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

  return (struct test_results){ total_tests, fails };
}
