/* This code is Copyright 2002 by Segher Boessenkool */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include "sogg.h"


int sogg_init_read(struct sogg *sogg, FILE *fp)
{
	sogg->fp = fp;

	ogg_sync_init(&sogg->oy);

	return 0;
}


int sogg_fini_read(struct sogg *sogg)
{
	ogg_stream_clear(&sogg->os);

	ogg_sync_clear(&sogg->oy);

	return 0;
}


int sogg_init_write(struct sogg *sogg, FILE *fp, long serialno)
{
	sogg->fp = fp;

	ogg_stream_init(&sogg->os, serialno);

	return 0;
}


int sogg_fini_write(struct sogg *sogg)
{
	ogg_stream_clear(&sogg->os);

	return 0;
}


static int sogg_get_data(struct sogg *sogg)
{
	char *buffer;
	int bytes;

	buffer = ogg_sync_buffer(&sogg->oy, 4096);
	if (!buffer) {
		fprintf(stderr, "ogg_sync_buffer() error\n");
		return -1;
	}

	bytes = fread(buffer, 1, 4096, sogg->fp);
	if (!bytes)
		return 0;

	ogg_sync_wrote(&sogg->oy, bytes);
	return 1;
}


static int sogg_get_page(struct sogg *sogg, ogg_page *og)
{
	for (;;) {
		int r;

		r = ogg_sync_pageout(&sogg->oy, og);
		if (r) {
			if (r < 0)
				fprintf(stderr, "ogg_sync_pageout() error\n");
			return r;
		}

		r = sogg_get_data(sogg);
		if (r < 1) {
			if (r < 0)
				fprintf(stderr, "sogg_get_data() error\n");
			return r;
		}
	}
}


int sogg_read(struct sogg *sogg, ogg_packet *op)
{
	ogg_page og;
	int r;

	for (;;) {
		r = ogg_stream_packetout(&sogg->os, op);
		if (r) {
			if (r < 0)
				fprintf(stderr, "ogg_stream_packetout() error\n");
			return r;
		}

		r = sogg_get_page(sogg, &og);
		if (r < 1) {
			if (r < 0)
				fprintf(stderr, "sogg_get_page() error\n");
			return r;
		}

		r = ogg_stream_pagein(&sogg->os, &og);
		if (r < 0) {
			if (r < 0)
				fprintf(stderr, "ogg_stream_pagein() error\n");
			return r;
		}
	}
}


int sogg_read_first(struct sogg *sogg, ogg_packet *op)
{
	ogg_page og;
	int r;

	r = sogg_get_page(sogg, &og);
	if (r < 1) {
		if (r < 0)
			fprintf(stderr, "sogg_get_page() error\n");
		return r;
	}

	ogg_stream_init(&sogg->os, ogg_page_serialno(&og));

	r = ogg_stream_pagein(&sogg->os, &og);
	if (r < 0) {
		if (r < 0)
			fprintf(stderr, "ogg_stream_pagein() error\n");
		return r;
	}

	return sogg_read(sogg, op);
}


int sogg_copy_packet(ogg_packet *op0, ogg_packet *op1)
{
	memcpy(op1, op0, sizeof(*op0));
	op1->packet = malloc(op0->bytes);
	memcpy(op1->packet, op0->packet, op0->bytes);

	return 0;
}


int sogg_write(struct sogg *sogg, ogg_packet *op)
{
	ogg_stream_packetin(&sogg->os, op);
	for (;;) {
		ogg_page og;
		int r;

		r = ogg_stream_pageout(&sogg->os, &og);
		if (r == 0)
			break;

		fwrite(og.header, 1, og.header_len, sogg->fp);
		fwrite(og.body, 1, og.body_len, sogg->fp);
	}

	return 0;
}


int sogg_flush(struct sogg *sogg)
{
	for (;;) {
		ogg_page og;
		int r;

		r = ogg_stream_flush(&sogg->os, &og);
		if (r == 0)
			break;

		fwrite(og.header, 1, og.header_len, sogg->fp);
		fwrite(og.body, 1, og.body_len, sogg->fp);
	}

	return 0;
}
