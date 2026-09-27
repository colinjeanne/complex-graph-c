#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "options.h"
#include "write_png.h"

int write_all_pngs(struct options opts, struct expression *ex, struct variables *vars) {
  if (opts.step_count == 1) {
    return write_png(opts.out_path, opts, ex, vars);
  }

  if (opts.step_count > 1000000000) {
    return -1;
  }

  int digit_count = ceil(log10(opts.step_count)) + 1;
  int root_path_len = strlen(opts.out_path);

  /*
   * 1 for the separator
   * 5 for "step_"
   * 4 for ".png"
   * 1 for the null terminator
   */
  int max_path_len = root_path_len + 1 + 5 + digit_count + 4 + 1;
  int result = 0;

  char *full_path = malloc(max_path_len);

  bool has_t = has_variable(vars, "t");

  double step_size = 1.0 / (opts.step_count - 1);
  for (int step = 0; step < opts.step_count; ++step) {
    double t = step * step_size;

    if (has_t) {
      result = set_variable(vars, "t", t);
      if (result != 0) {
        goto cleanup;
      }
    }

    result = snprintf(full_path, max_path_len, "%s/step_%0*d.png", opts.out_path, digit_count, step);
    if (result < 0) {
      goto cleanup;
    }

    result = write_png(full_path, opts, ex, vars);
    if (result != 0) {
      goto cleanup;
    }
  }

  cleanup:

  free(full_path);

  return result;
}

int main(int argc, char **argv) {
  struct options opts;
  if (get_options(argc, argv, &opts) == -1) {
    return -1;
  }

  struct expression *ex = nullptr;
  struct variables *vars = nullptr;
  int result = 0;

  struct parse_result ex_result = make_expression(opts.s, &ex);
  if (ex_result.type != PARSE_ERROR_SUCCESS) {
    fprintf(stderr, "Bad expression\n");
    result = -1;
    goto cleanup;
  }

  vars = malloc_variables(ex);
  if (vars == nullptr) {
    result = -1;
    goto cleanup;
  }

  result = write_all_pngs(opts, ex, vars);
  if (result != 0) {
    goto cleanup;
  }

cleanup:

  free_expression(ex);
  free_variables(vars);

  return result;
}
