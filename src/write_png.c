#include <complex.h>
#include <math.h>
#include <png.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "expression.h"
#include "write_png.h"

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

struct precalculated_complex {
  double _Complex value;
  double phase;
  double magnitude;
};

int calculate_row(
  struct precalculated_complex *values,
  struct expression *ex,
  struct variables *vars,
  size_t pixel_width,
  size_t samples,
  double min_real,
  double max_real,
  double imag
) {
  int result = 0;

  size_t pixel_samples = pixel_width * samples;
  for (size_t sample = 0; sample < pixel_samples; ++sample) {
    double real = interpolate(((double)sample) / pixel_samples, min_real, max_real);
    result = set_variable(vars, "z", real + imag * I);
    if (result != 0) {
      break;
    }

    double _Complex u;
    struct eval_result ev_result = evaluate_expression(ex, vars, &u);
    if (ev_result.type != EVAL_ERROR_SUCCESS) {
      u = 0;
    }

    struct precalculated_complex pc = { u, carg(u), cabs(u) };
    values[sample] = pc;
  }

  return result;
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

png_bytepp build_image_data(struct options opts, struct expression *ex, struct variables *vars) {
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
      int result = calculate_row(
        values[sample],
        ex,
        vars,
        opts.width,
        samples,
        opts.left,
        opts.right,
        interpolate((base_sample + sample) / pixel_samples, opts.top, opts.bottom)
      );

      if (result != 0) {
        goto cleanup;
      }
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

int write_png(const char *out_file, struct options opts, struct expression *ex, struct variables *vars) {
  FILE *fp = nullptr;
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  png_bytepp row_ptr = nullptr;
  int result = 0;

  fp = fopen(out_file, "wb");
  if (fp == nullptr) {
    fprintf(stderr, "Unable to open file %s\n", out_file);
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

  png_text txt_data[2] = {};
  txt_data[0].compression = PNG_TEXT_COMPRESSION_NONE;
  txt_data[0].key = "Function";
  txt_data[0].text = opts.s;

  txt_data[1].compression = PNG_TEXT_COMPRESSION_NONE;
  txt_data[1].key = "Domain";
  txt_data[1].text = opts.domain;

  png_set_text(
    png_ptr,
    info_ptr,
    txt_data,
    sizeof(txt_data) / sizeof(png_text)
  );

  png_write_info(png_ptr, info_ptr);

  row_ptr = build_image_data(opts, ex, vars);
  if (row_ptr == nullptr) {
    result = -1;
    goto cleanup;
  }

  png_write_image(png_ptr, row_ptr);
  png_write_end(png_ptr, nullptr);

cleanup:

  free_image_data(row_ptr, opts.height);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  if (fp != nullptr) {
    fclose(fp);
  }

  return result;
}
