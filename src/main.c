#include <complex.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define PI 3.141592653589793238462643383279
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

double interpolate(double t, double start, double end) {
  return t * (end - start) + start;
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

enum contour_mode {
  CONTOURS_NONE      = 0x00,
  CONTOURS_MAGNITUDE = 0x01,
  CONTOURS_PHASE     = 0x02,
  CONTOURS_BOTH      = 0x03,
};

struct options {
  int width;
  int height;
  char *s;
  char *out_file;
  enum contour_mode contours;
  double top;
  double left;
  double bottom;
  double right;
};

struct precalculated_complex {
  double _Complex value;
  double phase;
  double magnitude;
};

void calculate_row(
  struct precalculated_complex *values,
  struct expression *ex,
  size_t pixel_width,
  size_t samples,
  double min_real,
  double max_real,
  double imag
) {
  struct variable_value variable = MAKE_VARIABLE_VALUE("z", 0);

  size_t pixel_samples = pixel_width * samples;
  for (size_t sample = 0; sample < pixel_samples; ++sample) {
    double real = interpolate(((double)sample) / pixel_samples, min_real, max_real);
    variable.value = real + imag * I;

    double _Complex u;
    struct eval_result ev_result = evaluate_expression(ex, &variable, 1, &u);
    if (ev_result.type != EVAL_ERROR_SUCCESS) {
      u = 0;
    }

    struct precalculated_complex pc = { u, carg(u), cabs(u) };
    values[sample] = pc;
  }
}

rgb color_sample(
  struct precalculated_complex pc,
  double real_span,
  enum contour_mode mode
) {
  double hue = pc.phase;
  double sat = 1;
  double lum = 1 - pow(0.5, pc.magnitude);

  double shading = 1;

  double phase_lines = 12.0;
  double target_lines = 5.0;
  double raw_step = abs(real_span) / target_lines;
  double density = pow(10, floor(log10(raw_step)));
  
  double line_thickness = 2.5;

  if (mode & CONTOURS_MAGNITUDE) {
    double v = log(pc.magnitude) / density;
    double frac = v - floor(v);

    shading *= interpolate(frac, 0.8, 1);
  }

  if (mode & CONTOURS_PHASE) {
    double v = pc.phase / TAU * phase_lines;
    double frac = v - floor(v);
    shading *= interpolate(frac, 0.8, 1);
  }

  return hsl_to_rgb(hue, sat, lum * shading);
}

void color_row(
  png_bytep row_data,
  struct precalculated_complex **values,
  size_t width,
  size_t samples,
  double real_span,
  enum contour_mode mode
) {
  double sample_weight = 1.0 / (samples * samples);

  for (int col = 0; col < width; ++col) {
    png_bytep p = &row_data[col * 3];

    size_t base_sample = col * samples;

    int total_r = 0;
    int total_g = 0;
    int total_b = 0;
    for (int sy = 0; sy < samples; ++sy) {
      for (int sx = 0; sx < samples; ++sx) {
        struct precalculated_complex pc = values[sy][sx + base_sample];

        rgb sample_color = color_sample(pc, real_span, mode);
        total_r += GET_RED(sample_color);
        total_g += GET_GREEN(sample_color);
        total_b += GET_BLUE(sample_color);
      }
    }

    p[0] = (unsigned char)(total_r * sample_weight);
    p[1] = (unsigned char)(total_g * sample_weight);
    p[2] = (unsigned char)(total_b * sample_weight);
  }
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

png_bytepp build_image_data(struct options opts, struct expression *ex) {
  png_bytepp row_ptr = nullptr;
  size_t samples = 3;
  struct precalculated_complex *values[samples] = {};

  int row_bytes = opts.width * 3;
  row_ptr = calloc(opts.height, sizeof(png_bytep));
  if (row_ptr == nullptr) {
    goto cleanup;
  }

  for (size_t row = 0; row < opts.height; ++row) {
    row_ptr[row] = malloc(row_bytes);
    if (row_ptr[row] == nullptr) {
      goto cleanup;
    }
  }

  for (size_t row = 0; row < samples; ++row) {
    values[row] = malloc(sizeof(struct precalculated_complex) * opts.width * samples);
    if (values[row] == nullptr) {
      goto cleanup;
    }
  }

  double real_span = abs(opts.right - opts.left);
  double imag_span = abs(opts.bottom - opts.top);
  size_t pixel_samples = opts.height * samples;
  for (size_t row = 0; row < opts.height; ++row) {
    png_bytep row_data = row_ptr[row];

    double base_sample = row * samples;
    for (size_t sample = 0; sample < samples; ++sample) {
      calculate_row(
        values[sample],
        ex,
        opts.width,
        samples,
        opts.left,
        opts.right,
        interpolate((base_sample + sample) / pixel_samples, opts.top, opts.bottom)
      );
    }

    color_row(row_data, values, opts.width, samples, real_span, opts.contours);
  }

  for (size_t row = 0; row < samples; ++row) {
    free(values[row]);
  }

  return row_ptr;

  cleanup:
  for (size_t row = 0; row < samples; ++row) {
    free(values[row]);
  }
  free_image_data(row_ptr, opts.height);

  return nullptr;
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
    "  d:     The domain of the function formatted as the top left and\n"
    "           bottom right corners of the domain separated by a semicolon\n"
    "           For example: -5 + 5i;5 -5i\n"
    "           Supports the same expression syntax as -f\b"
    "  w:     The width of the image in pixels\n"
    "  h:     The height of the image in pixels\n"
    "  f:     The function to graph in the variable z\n"
    "  path:  The path to the output file\n"
  );

  return -1;
}

int main(int argc, char **argv) {
  struct options opts;
  if (get_options(argc, argv, &opts) == -1) {
    return -1;
  }

  FILE *fp = nullptr;
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  png_bytepp row_ptr = nullptr;
  struct expression *ex = nullptr;
  int result = 0;

  struct parse_result ex_result = make_expression(opts.s, &ex);
  if (ex_result.type != PARSE_ERROR_SUCCESS) {
    fprintf(stderr, "Bad expression\n");
    result = -1;
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

  row_ptr = build_image_data(opts, ex);
  if (row_ptr == nullptr) {
    result = -1;
    goto cleanup;
  }

  png_write_image(png_ptr, row_ptr);
  png_write_end(png_ptr, nullptr);

cleanup:

  free_expression(ex);
  free_image_data(row_ptr, opts.height);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);

  return result;
}
