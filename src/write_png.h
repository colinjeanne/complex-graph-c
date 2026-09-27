#ifndef WRITE_PNG_H
#define WRITE_PNG_H

#include "expression.h"
#include "options.h"

int write_png(
    const char *out_file,
    struct options opts,
    struct expression *ex,
    struct variables *vars
);

#endif
