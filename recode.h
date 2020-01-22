/* This code is Copyright 2002 by Segher Boessenkool */

#ifndef _RECODE_H
#define _RECODE_H

#include <ogg/ogg.h>
#include "headers.h"

long oggpack_copy(oggpack_buffer *, oggpack_buffer *, int);

void init_recode(struct headers *, long **);
int recode_packet(ogg_packet *, ogg_packet *, struct headers *);

#endif
