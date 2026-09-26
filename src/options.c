#include <complex.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "expression.h"
#include "options.h"

int parse_positive_int(const char *s) {
  int i = 0;
  for (int index = 0; s[index] != '\0'; ++index) {
    if (!isdigit(s[index])) {
      return -1;
    }

    i *= 10;
    i += (s[index] - '0');
  }

  return i;
}

int parse_domain(const char *s, struct options *opts) {
  char *sep = strpbrk(s, ";");
  if (sep == nullptr) {
    return -1;
  }

  int result = 0;
  struct expression *ex = nullptr;

  size_t top_left_length = (sep - s);
  char *top_left = malloc(sizeof(char) * (top_left_length + 1));
  if (top_left == nullptr) {
    return -1;
  }

  strncpy(top_left, s, top_left_length);
  top_left[top_left_length] = '\0';

  char *bottom_right = sep + 1;

  struct parse_result pr = make_expression(top_left, &ex);
  if (pr.type != PARSE_ERROR_SUCCESS) {
    result = -1;
    goto cleanup;
  }

  double _Complex u;
  struct eval_result ev = evaluate_expression(ex, nullptr, 0, &u);
  if (ev.type != EVAL_ERROR_SUCCESS) {
    result = -1;
    goto cleanup;
  }

  opts->top = cimag(u);
  opts->left = creal(u);

  free_expression(ex);
  ex = nullptr;

  pr = make_expression(bottom_right, &ex);
  if (pr.type != PARSE_ERROR_SUCCESS) {
    result = -1;
    goto cleanup;
  }

  ev = evaluate_expression(ex, nullptr, 0, &u);
  if (ev.type != EVAL_ERROR_SUCCESS) {
    result = -1;
    goto cleanup;
  }

  opts->bottom = cimag(u);
  opts->right = creal(u);

  if ((opts->top <= opts->bottom) || (opts->left >= opts->right)) {
    result = -1;
    goto cleanup;
  }

  cleanup:
  free_expression(ex);
  free(top_left);

  return result;
}

int get_options(int argc, char **argv, struct options *opts) {
  opts->width = -1;
  opts->height = -1;
  opts->s = nullptr;
  opts->out_file = nullptr;
  opts->contours = CONTOURS_NONE;
  opts->domain = nullptr;
  opts->top = 0;
  opts->left = 0;
  opts->bottom = 0;
  opts->right = 0;

  int opt;
  while ((opt = getopt(argc, argv, "c:d:m:w:h:f:")) != -1) {
    switch (opt) {
      case 'w':
        opts->width = parse_positive_int(optarg);
        if (opts->width <= 0) {
          fprintf(stderr, "Invalid width: %s\n", optarg);
          goto usage;
        }
        break;

      case 'h':
        opts->height = parse_positive_int(optarg);
        if (opts->height <= 0) {
          fprintf(stderr, "Invalid height: %s\n", optarg);
          goto usage;
        }
        break;
      
      case 'f':
        opts->s = optarg;
        break;
      
      case 'c':
        if (strcmp(optarg, "none") == 0) {
          opts->contours = CONTOURS_NONE;
        } else if (strcmp(optarg, "phase") == 0) {
          opts->contours = CONTOURS_PHASE;
        } else if (strcmp(optarg, "magnitude") == 0) {
          opts->contours = CONTOURS_MAGNITUDE;
        } else if (strcmp(optarg, "both") == 0) {
          opts->contours = CONTOURS_BOTH;
        } else {
          fprintf(stderr, "Invalid contour mode: %s\n", optarg);
          goto usage;
        }
        break;
      
      case 'd':
        if (parse_domain(optarg, opts) != 0) {
          fprintf(stderr, "Invalid domain: %s\n", optarg);
          goto usage;
        }

        opts->domain = optarg;
        break;
      
      default:
        goto usage;
    }
  }

  if (optind == argc - 1) {
    opts->out_file = argv[optind];
  }

  if (
    (opts->width > 0) &&
    (opts->height > 0) &&
    (opts->s != nullptr) &&
    (opts->out_file != nullptr) &&
    (opts->top > opts->bottom) &&
    (opts->left < opts->right)
  ) {
    return 0;
  }

  usage:
  fprintf(
    stderr,
    "complex-graph [-c mode] -d domain -w width -h height -f function path\n"
    "  c:     The contour mode; one of\n"
    "           none, phase, magnitude, or both\n"
    "           Defaults to none\n"
    "  d:     The domain of the function formatted as the top left and\n"
    "           bottom right corners of the domain separated by a semicolon\n"
    "           For example: -5 + 5i;5 -5i\n"
    "           Supports the same expression syntax as -f\n"
    "  w:     The width of the image in pixels\n"
    "  h:     The height of the image in pixels\n"
    "  f:     The function to graph in the variable z\n"
    "  path:  The path to the output file\n"
  );

  return -1;
}
