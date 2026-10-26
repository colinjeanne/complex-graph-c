#include <math.h>
#include <stdio.h>

#include "interval.h"
#include "interval_test.h"

enum test_type {
  NEGATE,
  INVERT,
  ADD,
  SUBTRACT,
  MULTIPLY,
  DIVIDE
};

struct interval empty = { 0, 0 };

struct test_case {
  enum test_type type;
  struct interval u;
  struct interval v;
  struct interval expected;
};

#define NEGATE_TEST_CASE(u_min, u_max, ex_min, ex_max) { NEGATE, { (u_min), (u_max) }, empty, { (ex_min), (ex_max) } }
#define INVERT_TEST_CASE(u_min, u_max, ex_min, ex_max) { INVERT, { (u_min), (u_max) }, empty, { (ex_min), (ex_max) } }
#define ADD_TEST_CASE(u_min, u_max, v_min, v_max, ex_min, ex_max) { ADD, { (u_min), (u_max) }, { (v_min), (v_max) }, { (ex_min), (ex_max) } }
#define SUBTRACT_TEST_CASE(u_min, u_max, v_min, v_max, ex_min, ex_max) { SUBTRACT, { (u_min), (u_max) }, { (v_min), (v_max) }, { (ex_min), (ex_max) } }
#define MULTIPLY_TEST_CASE(u_min, u_max, v_min, v_max, ex_min, ex_max) { MULTIPLY, { (u_min), (u_max) }, { (v_min), (v_max) }, { (ex_min), (ex_max) } }
#define DIVIDE_TEST_CASE(u_min, u_max, v_min, v_max, ex_min, ex_max) { DIVIDE, { (u_min), (u_max) }, { (v_min), (v_max) }, { (ex_min), (ex_max) } }

bool are_intervals_equal(struct interval u, struct interval v) {
  const double EPSILON = 0.000001;

  if (
    (isinf(u.minimum) != isinf(v.minimum)) ||
    (isinf(u.maximum) != isinf(v.maximum))
  ) {
    return false;
  }

  if (!isinf(u.minimum) && (fabs(u.minimum - v.minimum) > EPSILON)) {
    return false;
  }

  if (!isinf(u.maximum) && (fabs(u.maximum - v.maximum) > EPSILON)) {
    return false;
  }

  return true;
}

void print_interval(struct interval u) {
  printf("[%.4lf, %.4lf]", u.minimum, u.maximum);
}

void print_test_cast(struct test_case c) {
  switch (c.type) {
    case NEGATE:
      printf("Testing -");
      print_interval(c.u);
      printf(": ");
      break;

    case INVERT:
      printf("Testing 1/");
      print_interval(c.u);
      printf(": ");
      break;

    case ADD:
      printf("Testing ");
      print_interval(c.u);
      printf(" + ");
      print_interval(c.v);
      printf(": ");
      break;
    
    case SUBTRACT:
      printf("Testing ");
      print_interval(c.u);
      printf(" - ");
      print_interval(c.v);
      printf(": ");
      break;
    
    case MULTIPLY:
      printf("Testing ");
      print_interval(c.u);
      printf(" * ");
      print_interval(c.v);
      printf(": ");
      break;
    
    case DIVIDE:
      printf("Testing ");
      print_interval(c.u);
      printf(" / ");
      print_interval(c.v);
      printf(": ");
      break;
  }
}

struct interval evaluate_test_case(struct test_case c) {
  switch (c.type) {
    case NEGATE:
      return negate_interval(c.u);

    case INVERT:
      return invert_interval(c.u);

    case ADD:
      return add_interval(c.u, c.v);
    
    case SUBTRACT:
      return subtract_interval(c.u, c.v);
    
    case MULTIPLY:
      return multiply_interval(c.u, c.v);
    
    case DIVIDE:
      return divide_interval(c.u, c.v);
  }

  return (struct interval){ NAN, NAN };
}

bool run_interval_test_case(struct test_case c) {
  print_test_cast(c);

  struct interval result = evaluate_test_case(c);
  if (!are_intervals_equal(c.expected, result)) {
    printf("Expected ");
    print_interval(c.expected);
    printf(" received ");
    print_interval(result);
    printf("\n");
    return false;
  }
  
  printf("Pass\n");
  return true;
}

struct test_results run_interval_tests(void) {
  size_t fail_count = 0;

  struct test_case cases[] = {
    NEGATE_TEST_CASE(0, 0, 0, 0),
    NEGATE_TEST_CASE(-1, 1, -1, 1),
    NEGATE_TEST_CASE(-2, 3, -3, 2),
    NEGATE_TEST_CASE(-INFINITY, 0, 0, INFINITY),
    NEGATE_TEST_CASE(0, INFINITY, -INFINITY, 0),
    INVERT_TEST_CASE(0, 0, -INFINITY, INFINITY),
    INVERT_TEST_CASE(-1, 1, -INFINITY, INFINITY),
    INVERT_TEST_CASE(0, 10, -INFINITY, INFINITY),
    INVERT_TEST_CASE(0, INFINITY, -INFINITY, INFINITY),
    INVERT_TEST_CASE(-10, 0, -INFINITY, INFINITY),
    INVERT_TEST_CASE(-INFINITY, 0, -INFINITY, INFINITY),
    INVERT_TEST_CASE(-INFINITY, INFINITY, -INFINITY, INFINITY),
    INVERT_TEST_CASE(1, 2, 0.5, 1),
    INVERT_TEST_CASE(2, 2, 0.5, 0.5),
    INVERT_TEST_CASE(-2, -1, -1, -0.5),
    ADD_TEST_CASE(0, 0, 0, 0, 0, 0),
    ADD_TEST_CASE(1, 2, 3, 4, 4, 6),
    ADD_TEST_CASE(-1, 1, 1, 2, 0, 3),
    ADD_TEST_CASE(-INFINITY, INFINITY, 3, 4, -INFINITY, INFINITY),
    ADD_TEST_CASE(-INFINITY, 0, 3, 4, -INFINITY, 4),
    ADD_TEST_CASE(-INFINITY, 0, -INFINITY, 0, -INFINITY, 0),
    ADD_TEST_CASE(0, INFINITY, 3, 4, 3, INFINITY),
    ADD_TEST_CASE(0, INFINITY, -INFINITY, 0, -INFINITY, INFINITY),
    ADD_TEST_CASE(INFINITY, INFINITY, -INFINITY, 0, -INFINITY, INFINITY),
    ADD_TEST_CASE(INFINITY, INFINITY, -INFINITY, -INFINITY, -INFINITY, INFINITY),
    SUBTRACT_TEST_CASE(0, 0, 0, 0, 0, 0),
    SUBTRACT_TEST_CASE(1, 2, 3, 4, -2, -2),
    SUBTRACT_TEST_CASE(-1, 1, 1, 2, -2, -1),
    SUBTRACT_TEST_CASE(-INFINITY, INFINITY, 3, 4, -INFINITY, INFINITY),
    SUBTRACT_TEST_CASE(-INFINITY, 0, 3, 4, -INFINITY, -4),
    SUBTRACT_TEST_CASE(-INFINITY, 0, -INFINITY, 0, -INFINITY, 0),
    SUBTRACT_TEST_CASE(0, INFINITY, 3, 4, -3, INFINITY),
    SUBTRACT_TEST_CASE(0, INFINITY, -INFINITY, 0, INFINITY, INFINITY),
    SUBTRACT_TEST_CASE(INFINITY, INFINITY, -INFINITY, 0, INFINITY, INFINITY),
    SUBTRACT_TEST_CASE(INFINITY, INFINITY, -INFINITY, -INFINITY, INFINITY, INFINITY),
    SUBTRACT_TEST_CASE(INFINITY, INFINITY, INFINITY, INFINITY, -INFINITY, INFINITY),
    MULTIPLY_TEST_CASE(0, 0, 0, 0, 0, 0),
    MULTIPLY_TEST_CASE(1, 2, 3, 4, 3, 8),
    MULTIPLY_TEST_CASE(-1, 1, 1, 2, -2, 2),
    MULTIPLY_TEST_CASE(-INFINITY, INFINITY, 0, 0, 0, 0),
    MULTIPLY_TEST_CASE(INFINITY, INFINITY, 0, 0, 0, 0),
    MULTIPLY_TEST_CASE(-INFINITY, INFINITY, 3, 4, -INFINITY, INFINITY),
    MULTIPLY_TEST_CASE(-INFINITY, 0, 3, 4, -INFINITY, 0),
    MULTIPLY_TEST_CASE(-INFINITY, 0, -INFINITY, 0, 0, INFINITY),
    MULTIPLY_TEST_CASE(0, INFINITY, 3, 4, 0, INFINITY),
    MULTIPLY_TEST_CASE(0, INFINITY, -INFINITY, 0, -INFINITY, 0),
    MULTIPLY_TEST_CASE(INFINITY, INFINITY, -INFINITY, 0, -INFINITY, 0),
    MULTIPLY_TEST_CASE(INFINITY, INFINITY, -INFINITY, -INFINITY, -INFINITY, -INFINITY),
    MULTIPLY_TEST_CASE(INFINITY, INFINITY, INFINITY, INFINITY, INFINITY, INFINITY),
    DIVIDE_TEST_CASE(0, 0, 0, 0, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(1, 1, 0, 0, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(1, 2, 4, 8, 0.125, 0.5),
    DIVIDE_TEST_CASE(-1, 1, 1, 2, -1, 1),
    DIVIDE_TEST_CASE(-INFINITY, INFINITY, 0, 0, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(INFINITY, INFINITY, 0, 0, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(-INFINITY, INFINITY, 4, 8, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(3, 4, -INFINITY, INFINITY, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(-INFINITY, 0, 4, 8, -INFINITY, 0),
    DIVIDE_TEST_CASE(-INFINITY, -2, 4, 8, -INFINITY, -0.25),
    DIVIDE_TEST_CASE(-INFINITY, 0, -INFINITY, 0, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(-INFINITY, -1, -INFINITY, -1, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(0, INFINITY, 4, 8, 0, INFINITY),
    DIVIDE_TEST_CASE(1, INFINITY, 4, 8, 0.125, INFINITY),
    DIVIDE_TEST_CASE(0, INFINITY, -INFINITY, 0, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(1, INFINITY, -INFINITY, -1, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(INFINITY, INFINITY, -INFINITY, 0, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(INFINITY, INFINITY, -INFINITY, 1, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(INFINITY, INFINITY, -INFINITY, -INFINITY, -INFINITY, INFINITY),
    DIVIDE_TEST_CASE(INFINITY, INFINITY, INFINITY, INFINITY, -INFINITY, INFINITY),
  };

  size_t total_tests = sizeof(cases) / sizeof(struct test_case);

  for (size_t i = 0; i < total_tests; ++i) {
    bool success = run_interval_test_case(cases[i]);
    if (!success) {
      ++fail_count;
    }
  }
  
  return (struct test_results){ total_tests, fail_count };
}
