#include <stdio.h>

#include "expression_test.h"
#include "interval_test.h"
#include "variable_test.h"

int main(void) {
  struct test_results expression_results = run_expression_tests();
  struct test_results interval_results = run_interval_tests();
  struct test_results variable_results = run_variable_tests();

  size_t total_tests = expression_results.total_tests +
    interval_results.total_tests +
    variable_results.total_tests;
  size_t fail_count = expression_results.fail_count +
    interval_results.fail_count +
    variable_results.fail_count;

  printf("\n%lu/%lu tests succeeded\n", total_tests - fail_count, total_tests);

  return fail_count ? -1 : 0;
}
