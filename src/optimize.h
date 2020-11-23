#ifndef JITBOY_OPTIMIZE_H
#define JITBOY_OPTIMIZE_H

#include <stdbool.h>
#include "glib.h"

bool optimize_block(GList **instructions, int opt_level);

#endif
