/* This code is Copyright 2002 by Segher Boessenkool */

#ifndef _COUNT_H
#define _COUNT_H

#include <ogg/ogg.h>
#include "headers.h"

long **init_count(struct headers *);
int count_packet(ogg_packet *, struct headers *, long **);

#endif
