#include <complex.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <unistd.h>

#include "expression.h"

typedef unsigned int rgb;

#define RED 0x00FF0000
#define GREEN 0x0000FF00
#define BLUE 0x000000FF

#define GET_RED(c) (((c) & RED) >> 16)
#define GET_GREEN(c) (((c) & GREEN) >> 8)
#define GET_BLUE(c) ((c) & BLUE)
#define SET_RED(c, red) ((c & ~RED) | ((red) << 16))
#define SET_GREEN(c, green) ((c & ~GREEN) | ((green) << 8))
#define SET_BLUE(c, blue) ((c & ~BLUE) | (blue))

#define TAU 6.283185307179586476925286766559
#define PI_OVER_3 1.0471975511965977461542144610932

double clamp(double d) {
  if (d < 0) {
    return 0;
  } else if (d > 1) {
    return 1;
  }

  return d;
}

rgb hsl_to_rgb(double h, double s, double v) {
  rgb color = 0;

  h = fmod(h, TAU);
  if (h < 0) {
    h += TAU;
  }

  s = clamp(s);
  v = clamp(v);

  double chroma = s * v;
  double segment = h / PI_OVER_3;
  double x = chroma * (1 - fabs(fmod(segment, 2) - 1));

  unsigned char c_byte = (unsigned char)(chroma * 255);
  unsigned char x_byte = (unsigned char)(x * 255);

  unsigned char r = 0;
  unsigned char g = 0;
  unsigned char b = 0;
  if (segment < 1) {
    r = c_byte;
    g = x_byte;
  } else if (segment < 2) {
    r = x_byte;
    g = c_byte;
  } else if (segment < 3) {
    g = c_byte;
    b = x_byte;
  } else if (segment < 4) {
    g = x_byte;
    b = c_byte;
  } else if (segment < 5) {
    r = x_byte;
    b = c_byte;
  } else {
    r = c_byte;
    b = x_byte;
  }

  unsigned char m = (unsigned char)((v - chroma) * 255);
  r += m;
  g += m;
  b += m;

  color |= SET_RED(color, r);
  color |= SET_GREEN(color, g);
  color |= SET_BLUE(color, b);

  return color;
}

int calculate_values(const char *s, int width, int height, double _Complex **values, double *max_abs) {
  struct expression *ex = nullptr;
  *values = nullptr;

  struct parse_result result = make_expression(s, &ex);
  if (result.type != PARSE_ERROR_SUCCESS) {
    printf("Bad expression");
    free_expression(ex);
    return -1;
  }

  struct variable_value variables[1] = {
    MAKE_VARIABLE_VALUE("z", 0)
  };

  *values = malloc(sizeof(double _Complex) * width * height);
  if (*values == nullptr) {
    free_expression(ex);
    return -1;
  }

  *max_abs = 0;
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      variables[0].value = (col - width / 2) / 10.0 + I * (row - height / 2) / 10.0;
      double complex r;
      struct eval_result ev_result = evaluate_expression(ex, variables, 1, &r);
      if (ev_result.type != EVAL_ERROR_SUCCESS) {
        r = 0;
      }

      if (cabs(r) > *max_abs) {
        *max_abs = r;
      }
      
      (*values)[col + row * width] = r;
    }
  }

  free_expression(ex);

  return 0;
}

void free_image_data(png_bytepp data, int height) {
  if (data == nullptr) {
    return;
  }

  for (int i = 0; i < height; ++i) {
    if (data[i] != nullptr) {
      free(data[i]);
    }
  }

  free(data);
}

png_bytepp build_image_data(
  int width,
  int height,
  double max_abs,
  const double _Complex *values
) {
  int row_bytes = width * 3;
  png_bytepp row_ptr = calloc(height, sizeof(png_bytep));
  if (row_ptr == nullptr) {
    return nullptr;
  }

  for (int row = 0; row < height; ++row) {
    row_ptr[row] = malloc(row_bytes);
    if (row_ptr[row] == nullptr) {
      free_image_data(row_ptr, height);
      return nullptr;
    }
  }

  for (int row = 0; row < height; ++row) {
    png_bytep row_data = row_ptr[row];
    for (int col = 0; col < width; ++col) {
      png_bytep p = &row_data[col * 3];

      double _Complex r = values[col + row * width];
      rgb color = hsl_to_rgb(carg(r), cabs(r) / max_abs, 1.0);
      p[0] = GET_RED(color);
      p[1] = GET_GREEN(color);
      p[2] = GET_BLUE(color);
    }
  }

  return row_ptr;
}

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

struct options {
  int width;
  int height;
  char *s;
  char *out_file;
};

int get_options(int argc, char **argv, struct options *opts) {
  opts->width = -1;
  opts->height = -1;
  opts->s = nullptr;
  opts->out_file = nullptr;

  int opt;
  while ((opt = getopt(argc, argv, "w:h:f:")) != -1) {
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
    (opts->out_file != nullptr)
  ) {
    return 0;
  }

  usage:
  fprintf(
    stderr,
    "Usage: complex-graph -w width -h height -f function out_file\n"
    "  w:        The width of the image in pixels\n"
    "  h:        The height of the image in pixels\n"
    "  f:        The function to graph in the variable z\n"
    "  out_file: The path to the output file\n"
  );

  return -1;
}

int main(int argc, char **argv) {
  struct options opts;
  if (get_options(argc, argv, &opts) == -1) {
    return -1;
  }

  double _Complex *values = nullptr;
  FILE *fp = nullptr;
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  png_bytepp row_ptr = nullptr;
  double max_abs;

  int result = calculate_values(opts.s, opts.width, opts.height, &values, &max_abs);
  if (result != 0) {
    goto cleanup;
  }

  fp = fopen(opts.out_file, "wb");
  if (fp == nullptr) {
    result = -1;
    goto cleanup;
  }

  png_ptr = png_create_write_struct(
    PNG_LIBPNG_VER_STRING,
    nullptr,
    nullptr,
    nullptr
  );

  if (png_ptr == nullptr) {
    result = -1;
    goto cleanup;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == nullptr) {
    result = -1;
    goto cleanup;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    fprintf(stderr, "Error writing PNG\n");
    result = -1;
    goto cleanup;
  }

  png_init_io(png_ptr, fp);

  png_set_IHDR(
    png_ptr,
    info_ptr,
    opts.width,
    opts.height,
    8,
    PNG_COLOR_TYPE_RGB,
    PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT,
    PNG_FILTER_TYPE_DEFAULT
  );

  png_write_info(png_ptr, info_ptr);

  row_ptr = build_image_data(opts.width, opts.height, max_abs, values);
  if (row_ptr == nullptr) {
    result = -1;
    goto cleanup;
  }

  png_write_image(png_ptr, row_ptr);
  png_write_end(png_ptr, nullptr);

cleanup:

  free_image_data(row_ptr, opts.height);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);
  free(values);

  return result;
}
