/* This code is Copyright 2002 by Segher Boessenkool */

#ifndef _HEADERS_H
#define _HEADERS_H

#include <ogg/ogg.h>

struct book {
	int dim;
	int n;
	int *length;
	int *tree;
	int values;
	int *value;
	int *new_bits;
	int *new_length;
};

struct time {
	int type;
};

struct floor {
	int type;
	int partitions;
	int *class;
	int *class_dim;
	int *class_subs;
	int *class_book;
	int **class_subbook;
	int quant;
};

struct residue {
	int type;
	int begin;
	int end;
	int grouping;
	int partitions;
	int groupbook;
	int stages;
	int *secondstages;
	int **book;
};

struct mapping {
	int type;
	int submaps;
	int coupling_steps;
	int *chmux;
	int *time;
	int *floor;
	int *residue;
};

struct mode {
	int blockflag;
	int mapping;
};

struct headers {
	int channels;
	int samplerate;
	int blocksize[2];
	int stages;

	int books;
	struct book *book;

	int times;
	struct time *time;

	int floors;
	struct floor *floor;

	int residues;
	struct residue *residue;

	int mappings;
	struct mapping *mapping;

	int modes;
	struct mode *mode;
};


int header0_read(ogg_packet *, struct headers *);
int header1_read(ogg_packet *, struct headers *);
int header2_read(ogg_packet *, struct headers *);

int header0_recode(ogg_packet *, ogg_packet *, struct headers *);
int header1_recode(ogg_packet *, ogg_packet *, struct headers *);
int header2_recode(ogg_packet *, ogg_packet *, struct headers *);

#endif
