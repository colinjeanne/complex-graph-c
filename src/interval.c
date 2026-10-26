#include <float.h>
#include <math.h>

#include "interval.h"

struct interval negate_interval(struct interval u) {
  return (struct interval){ -u.maximum, -u.minimum };
}

struct interval invert_interval(struct interval u) {
  if (u.minimum <= 0 && u.maximum >= 0) {
    return (struct interval){ -INFINITY, INFINITY };
  }

  return (struct interval){ 1.0 / u.maximum, 1.0 / u.minimum };
}

struct interval add_interval(struct interval u, struct interval v) {
  double minimum;
  double maximum;

  if (isinf(u.minimum) && isinf(v.minimum) && u.minimum != v.minimum) {
    minimum = -INFINITY;
  } else {
    minimum = u.minimum + v.minimum;
  }

  if (isinf(u.maximum) && isinf(v.maximum) && u.maximum != v.maximum) {
    maximum = INFINITY;
  } else {
    maximum = u.maximum + v.maximum;
  }

  return (struct interval){ minimum, maximum };
}

struct interval subtract_interval(struct interval u, struct interval v) {
  double minimum;
  double maximum;

  if (isinf(u.minimum) && isinf(v.minimum) && u.minimum == v.minimum) {
    minimum = -INFINITY;
  } else {
    minimum = u.minimum - v.minimum;
  }

  if (isinf(u.maximum) && isinf(v.maximum) && u.maximum == v.maximum) {
    maximum = INFINITY;
  } else {
    maximum = u.maximum - v.maximum;
  }

  return (struct interval){ minimum, maximum };
}

double safe_multiply(double a, double b) {
  if (isinf(a) || isinf(b)) {
    if (
      (a < 0 && b > 0) ||
      (a > 0 && b < 0)
    ) {
      return -INFINITY;
    } else if (a == 0 || b == 0) {
      return 0;
    }

    return INFINITY;
  }

  return a * b;
}

struct interval multiply_interval(struct interval u, struct interval v) {
  double min_min = safe_multiply(u.minimum, v.minimum);
  double min_max = safe_multiply(u.minimum, v.maximum);
  double max_min = safe_multiply(u.maximum, v.minimum);
  double max_max = safe_multiply(u.maximum, v.maximum);

  return (struct interval){
    fmin(fmin(min_min, min_max), fmin(max_min, max_max)),
    fmax(fmax(min_min, min_max), fmax(max_min, max_max))
  };
}

struct interval divide_interval(struct interval u, struct interval v) {
  if (v.minimum == 0 && v.maximum == 0) {
    return (struct interval){ -INFINITY, INFINITY };
  }

  if (
    (isinf(u.minimum) || isinf(u.maximum)) &&
    (isinf(v.minimum) || isinf(v.maximum))
  ) {
    return (struct interval){ -INFINITY, INFINITY };
  }

  return multiply_interval(u, invert_interval(v));
}
