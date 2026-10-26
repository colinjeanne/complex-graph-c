#ifndef INTERVAL_H
#define INTERVAL_H

/**
 * An implementation of Kahan Extended Interval Arithmetic
 */

struct interval {
  double minimum;
  double maximum;
};

struct interval negate_interval(struct interval u);
struct interval invert_interval(struct interval u);

struct interval add_interval(struct interval u, struct interval v);
struct interval subtract_interval(struct interval u, struct interval v);
struct interval multiply_interval(struct interval u, struct interval v);
struct interval divide_interval(struct interval u, struct interval v);

#endif
