/* This code is Copyright 2002 by Segher Boessenkool */

#ifndef _SOGG_H
#define _SOGG_H

#include <stdio.h>
#include <ogg/ogg.h>


struct sogg {
	/* common */
	FILE *fp;
	ogg_stream_state os;

	/* for read */
	ogg_sync_state oy;

	/* for write */
};


int sogg_init_read(struct sogg *, FILE *);
int sogg_read_first(struct sogg *, ogg_packet *);
int sogg_read(struct sogg *, ogg_packet *);
int sogg_fini_read(struct sogg *);

int sogg_init_write(struct sogg *, FILE *, long serialno);
int sogg_write(struct sogg *, ogg_packet *);
int sogg_fini_write(struct sogg *);

int sogg_copy_packet(ogg_packet *, ogg_packet *);

int sogg_flush(struct sogg *);

#endif
