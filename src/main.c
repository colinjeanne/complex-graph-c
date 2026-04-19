#include <complex.h>
#include <stdio.h>

#include "expression.h"

int main(void) {
  struct expression *ex;
  struct parse_result result = make_expression(".0.0", &ex);
  if (result.type != PARSE_ERROR_SUCCESS) {
    printf("Bad expression");
    free_expression(ex);
    return -1;
  }

  double complex r = evaluate_expression(ex);
  free_expression(ex);
  printf("%f, %f\n", creal(r), cimag(r));
}
