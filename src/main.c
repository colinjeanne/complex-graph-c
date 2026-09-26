#include <stdio.h>

#include "expression.h"
#include "options.h"
#include "write_png.h"

int main(int argc, char **argv) {
  struct options opts;
  if (get_options(argc, argv, &opts) == -1) {
    return -1;
  }

  struct expression *ex = nullptr;
  int result = 0;

  struct parse_result ex_result = make_expression(opts.s, &ex);
  if (ex_result.type != PARSE_ERROR_SUCCESS) {
    fprintf(stderr, "Bad expression\n");
    result = -1;
    goto cleanup;
  }

  result = write_png(opts, ex);
  if (result == -1) {
    goto cleanup;
  }

cleanup:

  free_expression(ex);

  return result;
}
