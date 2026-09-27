#ifndef OPTIONS_H
#define OPTIONS_H

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
  char *out_path;
  int step_count;
  enum contour_mode contours;
  char *domain;
  double top;
  double left;
  double bottom;
  double right;
};

int get_options(int argc, char **argv, struct options *opts);

#endif
