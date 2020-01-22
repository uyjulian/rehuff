/* This code is Copyright 2002 by Segher Boessenkool */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ogg/ogg.h>
#include "headers.h"


extern int verbose;


static int ilog(int x)
{
	int n;

	n = 0;
	while (x > 1) {
		n++;
		x >>= 1;
	}

	return n;
}


static int ilog_plus(int x)
{
	int n;

	n = 0;
	while (x > 0) {
		n++;
		x >>= 1;
	}

	return n;
}


int header0_read(ogg_packet *op, struct headers *headers)
{
	oggpack_buffer ob;

	oggpack_readinit(&ob, op->packet, op->bytes);

	if (oggpack_read(&ob, 32) != 0x726f7601)
		return -1;
	if (oggpack_read(&ob, 24) != 0x736962)
		return -1;

	if (oggpack_read(&ob, 32) != 0)
		return -1;

	headers->channels = oggpack_read(&ob, 8);
	headers->samplerate = oggpack_read(&ob, 32);

	oggpack_read(&ob, 32);
	oggpack_read(&ob, 32);
	oggpack_read(&ob, 32);

	headers->blocksize[0] = 1 << (oggpack_read(&ob, 4) - 1);
	headers->blocksize[1] = 1 << (oggpack_read(&ob, 4) - 1);

	if (oggpack_read(&ob, 1) != 1)
		return -1;

	if (verbose > 0)
		fprintf(stderr, "%d channels, %dHz samplerate, block sizes %d and %d\n\n",
			headers->channels, headers->samplerate, headers->blocksize[0], headers->blocksize[1]);

	return 0;
}


int header1_read(ogg_packet *op, struct headers *headers)
{


	return 0;
}


static int build_tree(struct book *book, int *length)
{
	int node[32];
	int new_node;
	int depth;
	int i, j;

	book->value = malloc(book->n * sizeof(*book->value));
	book->tree = calloc(book->n - 1, sizeof(*book->tree));

	book->values = 0;
	depth = 0;
	node[0] = 0;
	new_node = 1;

	for (i = 0; i < book->n; ) {
		if (length[i] == 0) {
			i++;
			continue;
		}

		for (j = i; length[j] <= depth && j < book->n; j++)
			;
		if (j == book->n)
			return -1;
		if (j == i)
			i++;

		book->value[book->values++] = j;

		while (depth + 1 < length[j])
			node[++depth] = new_node++;

		length[j] = 0;

		while (book->tree[node[depth]])
			if (depth-- == 0)
				goto done;

		book->tree[node[depth]] = new_node - node[depth];
	}

done:
	return 0;
}


static int unpack_book(oggpack_buffer *ob, struct book *book)
{
	int ordering;
	int *length;
	int maptype;
	int i;

	if (oggpack_read(ob, 24) != 0x564342)
		return -1;

	book->dim = oggpack_read(ob, 16);
	book->n = oggpack_read(ob, 24);

	ordering = oggpack_read(ob, 1);
	if (verbose > 0)
		fprintf(stderr, "dim = %d, entries = %d, ordering = %d\n", book->dim, book->n, ordering);

	length = alloca(book->n * sizeof(*length));

	if (ordering == 0) {
		if (oggpack_read(ob, 1) == 1)
			for (i = 0; i < book->n; i++)
				if (oggpack_read(ob, 1) == 1)
					length[i] = oggpack_read(ob, 5) + 1;
				else
					length[i] = 0;
		else
			for (i = 0; i < book->n; i++)
				length[i] = oggpack_read(ob, 5) + 1;
	} else {
		int l;

		l = oggpack_read(ob, 5) + 1;
		for (i = 0; i < book->n; ) {
			int n;

			n = oggpack_read(ob, ilog_plus(book->n - i));
			for ( ; n && i < book->n; n--)
				length[i++] = l;

			l++;
		}
	}

	if (verbose > 1) {
		fprintf(stderr, "\tlengths:");
		for (i = 0; i < book->n; i++) {
			if ((i & 15) == 0)
				fprintf(stderr, "\n\t\t");
			fprintf(stderr, "%3d", length[i]);
		}
		fprintf(stderr, "\n");
	}

	book->length = malloc(book->n * sizeof(*book->length));
	for (i = 0; i < book->n; i++)
		book->length[i] = length[i];

	if (build_tree(book, length) < 0)
		return -1;

	maptype = oggpack_read(ob, 4);
	if (maptype == 0)
		return 0;
	if (maptype == 1 || maptype == 2) {
		int quant;
		int num;

		oggpack_read(ob, 32);
		oggpack_read(ob, 32);
		quant = oggpack_read(ob, 4) + 1;
		oggpack_read(ob, 1);

		if (maptype == 1)
			num = floor(pow(book->n, 1. / book->dim));
		else
			num = book->n * book->dim;

		for (i = 0; i < num; i++)
			oggpack_read(ob, quant);

		return 0;
	}

	return -1;
}


static int unpack_time(oggpack_buffer *ob, struct time *time)
{
	time->type = oggpack_read(ob, 16);
	if (verbose > 0)
		fprintf(stderr, "type %d\n", time->type);
	if (time->type != 0)
		return -1;

	return 0;
}


static int unpack_floor(oggpack_buffer *ob, struct floor *floor)
{
	int classes;
	int rangebits;
	int i, j;

	floor->type = oggpack_read(ob, 16);
	if (verbose > 0)
		fprintf(stderr, "type %d, ", floor->type);
	if (floor->type != 1) // XXX handle floor0
		return -1;

	floor->partitions = oggpack_read(ob, 5);
	if (verbose > 0)
		fprintf(stderr, "%d partitions, classes are", floor->partitions);

	floor->class = malloc(floor->partitions * sizeof(*floor->class));

	classes = 0;
	for (i = 0; i < floor->partitions; i++) {
		floor->class[i] = oggpack_read(ob, 4);
		if (verbose > 0)
			fprintf(stderr, " %d", floor->class[i]);
		if (floor->class[i] > classes)
			classes = floor->class[i];
	}
	classes++;

	if (verbose > 0)
		fprintf(stderr, ", %d classes\n", classes);
	floor->class_dim = malloc(classes * sizeof(*floor->class_dim));
	floor->class_subs = malloc(classes * sizeof(*floor->class_subs));
	floor->class_book = malloc(classes * sizeof(*floor->class_book));
	floor->class_subbook = malloc(classes * sizeof(*floor->class_subbook));
	if (verbose > 1)
		fprintf(stderr, "\tclasses:\n");
	for (i = 0; i < classes; i++) {
		floor->class_dim[i] = oggpack_read(ob, 3) + 1;
		floor->class_subs[i] = oggpack_read(ob, 2);
		if (verbose > 1)
			fprintf(stderr, "\t\tdim %d, subs %d, ", floor->class_dim[i], floor->class_subs[i]);
		if (floor->class_subs[i] != 0) {
			floor->class_book[i] = oggpack_read(ob, 8);
			if (verbose > 1)
				fprintf(stderr, "book %d, ", floor->class_book[i]);
		}
		floor->class_subbook[i] = malloc((1 << floor->class_subs[i]) * sizeof(*floor->class_subbook[i]));
		if (verbose > 1)
			fprintf(stderr, "subbooks:");
		for (j = 0; j < (1 << floor->class_subs[i]); j++) {
			floor->class_subbook[i][j] = oggpack_read(ob, 8) - 1;
			if (verbose > 1)
				fprintf(stderr, "%4d", floor->class_subbook[i][j]);
		}
		if (verbose > 1)
			fprintf(stderr, "\n");
	}

	floor->quant = oggpack_read(ob, 2);
	rangebits = oggpack_read(ob, 4);
	for (i = 0; i < floor->partitions; i++)
		for (j = 0; j < floor->class_dim[floor->class[i]]; j++)
			oggpack_read(ob, rangebits);

	return 0;
}


static int unpack_residue(oggpack_buffer *ob, struct residue *residue)
{
	int stages;
	int stage;
	int i;

	residue->type = oggpack_read(ob, 16);
	if (verbose > 0)
		fprintf(stderr, "type %d, ", residue->type);
	if (residue->type != 2) // XXX handle res0, res1
		return -1;

	residue->begin = oggpack_read(ob, 24);
	residue->end = oggpack_read(ob, 24);
	residue->grouping = oggpack_read(ob, 24) + 1;
	residue->partitions = oggpack_read(ob, 6) + 1;
	residue->groupbook = oggpack_read(ob, 8);
	if (verbose > 0)
		fprintf(stderr, "begin %d, end %d, grouping %d, partitions %d, groupbook %d\n",
			residue->begin, residue->end, residue->grouping, residue->partitions, residue->groupbook);

	residue->secondstages = malloc(residue->partitions * sizeof(*residue->secondstages));
	stages = 0;
	if (verbose > 1)
		fprintf(stderr, "\tsecondstages:");
	for (i = 0; i < residue->partitions; i++) {
		residue->secondstages[i] = oggpack_read(ob, 3);
		if (oggpack_read(ob, 1) == 1)
			residue->secondstages[i] |= oggpack_read(ob, 5) << 3;
		if (verbose > 1)
			fprintf(stderr, "%4d", residue->secondstages[i]);
		if (residue->secondstages[i] > stages)
			stages = residue->secondstages[i];
	}
	residue->stages = ilog(stages) + 1;
	if (verbose > 1)
		fprintf(stderr, "\n");

	residue->book = malloc(residue->partitions * sizeof(*residue->book));
	if (verbose > 1)
		fprintf(stderr, "\tbooks:\n");
	for (i = 0; i < residue->partitions; i++) {
		residue->book[i] = malloc(residue->stages * sizeof(*residue->book[i]));
		if (verbose > 1)
			fprintf(stderr, "\t\t");
		for (stage = 0; stage < residue->stages; stage++) {
			if (residue->secondstages[i] & (1 << stage))
				residue->book[i][stage] = oggpack_read(ob, 8);
			else
				residue->book[i][stage] = -1;
			if (verbose > 1)
				fprintf(stderr, "%4d", residue->book[i][stage]);
		}
		if (verbose > 1)
			fprintf(stderr, "\n");
	}

	return 0;
}


static int unpack_mapping(oggpack_buffer *ob, struct mapping *mapping, struct headers *headers)
{
	int i;

	mapping->type = oggpack_read(ob, 16);
	if (verbose > 0)
		fprintf(stderr, "type %d, ", mapping->type);
	if (mapping->type != 0)
		return -1;

	if (oggpack_read(ob, 1) == 1)
		mapping->submaps = oggpack_read(ob, 4) + 1;
	else
		mapping->submaps = 1;
	if (verbose > 0)
		fprintf(stderr, "%d submaps, ", mapping->submaps);

	if (oggpack_read(ob, 1) == 1) {
		mapping->coupling_steps = oggpack_read(ob, 8) + 1;
		for (i = 0; i < mapping->coupling_steps; i++) {
			oggpack_read(ob, ilog(headers->channels));
			oggpack_read(ob, ilog(headers->channels));
		}
	} else
		mapping->coupling_steps = 0;
	if (verbose > 0)
		fprintf(stderr, "%d coupling steps, ", mapping->coupling_steps);

	if (oggpack_read(ob, 2) != 0)
		return -1;

	mapping->chmux = malloc(headers->channels * sizeof(*mapping->chmux));
	if (mapping->submaps > 1)
		for (i = 0; i < headers->channels; i++)
			mapping->chmux[i] = oggpack_read(ob, 4);
	else
		for (i = 0; i < headers->channels; i++)
			mapping->chmux[i] = 0;
	if (verbose > 0)
		fprintf(stderr, "chmux");
	if (verbose > 0)
		for (i = 0; i < headers->channels; i++) fprintf(stderr, " %d", mapping->chmux[i]);
	if (verbose > 0)
		fprintf(stderr, "\n");

	if (verbose > 1)
		fprintf(stderr, "\tsubmaps:\n");
	mapping->time = malloc(mapping->submaps * sizeof(*mapping->time));
	mapping->floor = malloc(mapping->submaps * sizeof(*mapping->floor));
	mapping->residue = malloc(mapping->submaps * sizeof(*mapping->residue));
	for (i = 0; i < mapping->submaps; i++) {
		mapping->time[i] = oggpack_read(ob, 8);
		mapping->floor[i] = oggpack_read(ob, 8);
		mapping->residue[i] = oggpack_read(ob, 8);
		if (verbose > 1)
			fprintf(stderr, "\t\ttime %d, floor %d, residue %d\n", mapping->time[i], mapping->floor[i], mapping->residue[i]);
	}

	return 0;
}


static int unpack_mode(oggpack_buffer *ob, struct mode *mode)
{
	mode->blockflag = oggpack_read(ob, 1);
	oggpack_read(ob, 16);
	oggpack_read(ob, 16);
	mode->mapping = oggpack_read(ob, 8);
	if (verbose > 0)
		fprintf(stderr, "blockflag %d, mapping %d\n", mode->blockflag, mode->mapping);

	return 0;
}


int header2_read(ogg_packet *op, struct headers *headers)
{
	oggpack_buffer ob;
	int stages;
	int i;
	
	oggpack_readinit(&ob, op->packet, op->bytes);

	if (oggpack_read(&ob, 32) != 0x726f7605)
		return -1;
	if (oggpack_read(&ob, 24) != 0x736962)
		return -1;

	headers->books = oggpack_read(&ob, 8) + 1;
	if (verbose > 0)
		fprintf(stderr, "%d books\n", headers->books);
	headers->book = malloc(headers->books * sizeof(*headers->book));
	for (i = 0; i < headers->books; i++) {
		if (verbose > 0)
			fprintf(stderr, "book %d: ", i);
		if (unpack_book(&ob, headers->book + i) < 0)
			return -1;
	}
	if (verbose > 0)
		fprintf(stderr, "\n");

	headers->times = oggpack_read(&ob, 6) + 1;
	if (verbose > 0)
		fprintf(stderr, "%d times\n", headers->times);
	headers->time = malloc(headers->times * sizeof(*headers->time));
	for (i = 0; i < headers->times; i++) {
		if (verbose > 0)
			fprintf(stderr, "time %d: ", i);
		if (unpack_time(&ob, headers->time + i) < 0)
			return -1;
	}
	if (verbose > 0)
		fprintf(stderr, "\n");

	headers->floors = oggpack_read(&ob, 6) + 1;
	if (verbose > 0)
		fprintf(stderr, "%d floors\n", headers->floors);
	headers->floor = malloc(headers->floors * sizeof(*headers->floor));
	for (i = 0; i < headers->floors; i++) {
		if (verbose > 0)
			fprintf(stderr, "floor %d: ", i);
		if (unpack_floor(&ob, headers->floor + i) < 0)
			return -1;
	}
	if (verbose > 0)
		fprintf(stderr, "\n");

	headers->residues = oggpack_read(&ob, 6) + 1;
	if (verbose > 0)
		fprintf(stderr, "%d residues\n", headers->residues);
	headers->residue = malloc(headers->residues * sizeof(*headers->residue));
	stages = 0;
	for (i = 0; i < headers->residues; i++) {
		if (verbose > 0)
			fprintf(stderr, "residue %d: ", i);
		if (unpack_residue(&ob, headers->residue + i) < 0)
			return -1;
		if (headers->residue[i].stages > stages)
			stages = headers->residue[i].stages;
	}
	headers->stages = stages;
	if (verbose > 0)
		fprintf(stderr, "\n");

	headers->mappings = oggpack_read(&ob, 6) + 1;
	if (verbose > 0)
		fprintf(stderr, "%d mappings\n", headers->mappings);
	headers->mapping = malloc(headers->mappings * sizeof(*headers->mapping));
	for (i = 0; i < headers->mappings; i++) {
		if (verbose > 0)
			fprintf(stderr, "mapping %d: ", i);
		if (unpack_mapping(&ob, headers->mapping + i, headers) < 0)
			return -1;
	}
	if (verbose > 0)
		fprintf(stderr, "\n");

	headers->modes = oggpack_read(&ob, 6) + 1;
	if (verbose > 0)
		fprintf(stderr, "%d modes\n", headers->modes);
	headers->mode = malloc(headers->modes * sizeof(*headers->mode));
	for (i = 0; i < headers->modes; i++) {
		if (verbose > 0)
			fprintf(stderr, "mode %d: ", i);
		if (unpack_mode(&ob, headers->mode + i) < 0)
			return -1;
	}
	if (verbose > 0)
		fprintf(stderr, "\n");

	if (oggpack_read(&ob, 1) != 1)
		return -1;

	return 0;
}
