/* This code is Copyright 2002 by Segher Boessenkool */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ogg/ogg.h>
#include "headers.h"
#include "recode.h"
#include "sogg.h"


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


int header0_recode(ogg_packet *op_in, ogg_packet *op_out, struct headers *headers)
{
	sogg_copy_packet(op_in, op_out);

	return 0;
}


int header1_recode(ogg_packet *op_in, ogg_packet *op_out, struct headers *headers)
{
	sogg_copy_packet(op_in, op_out); // XXX: add stuff to vendor tag

	return 0;
}


static int recode_book(oggpack_buffer *ob_in, oggpack_buffer *ob_out, struct book *book)
{
	int old_ordering, new_ordering;
//	int *length;
	int maptype;
	int i, j;
	int count[33];

	oggpack_copy(ob_in, ob_out, 64);

	old_ordering = oggpack_read(ob_in, 1);
	if (old_ordering == 0) {
		if (oggpack_read(ob_in, 1) == 1) {
			for (i = 0; i < book->n; i++)
				if (oggpack_read(ob_in, 1) == 1)
					oggpack_read(ob_in, 5);
		} else
			for (i = 0; i < book->n; i++)
				oggpack_read(ob_in, 5);
	} else {
		oggpack_read(ob_in, 5);
		for (i = 0; i < book->n; )
			i += oggpack_read(ob_in, ilog_plus(book->n - i));
	}

	new_ordering = 1;
	count[0] = 0;
	for (i = 1, j = 0; i <= 32; i++) {
		count[i] = 0;
		for ( ; j < book->n && book->new_length[j] == i; j++)
			count[i]++;
		if (j == book->n)
			break;
		if (book->new_length[j] < i) {
			new_ordering = 0;
			break;
		}
	}

	oggpack_write(ob_out, new_ordering, 1);

	if (new_ordering == 0) {
		int has_zeroes = 0;

		for (i = 0; i < book->n; i++)
			if (book->new_length[i] == 0) {
				has_zeroes = 1;
				break;
			}

		oggpack_write(ob_out, has_zeroes, 1);

		if (has_zeroes)
			for (i = 0; i < book->n; i++) {
				if (book->new_length[i] == 0)
					oggpack_write(ob_out, 0, 1);
				else {
					oggpack_write(ob_out, 1, 1);
					oggpack_write(ob_out, book->new_length[i] - 1, 5);
				}
			}
		else
			for (i = 0; i < book->n; i++)
				oggpack_write(ob_out, book->new_length[i] - 1, 5);
	} else {
		oggpack_write(ob_out, book->new_length[0] - 1, 5);
		for (i = book->new_length[0]; i <= 32 && count[i - 1] < book->n; i++) {
			oggpack_write(ob_out, count[i], ilog_plus(book->n - count[i - 1]));
			count[i] += count[i - 1];
		}
	}

	maptype = oggpack_copy(ob_in, ob_out, 4);
	if (maptype == 0)
		return 0;
	if (maptype == 1 || maptype == 2) {
		int quant;
		int num;

		oggpack_copy(ob_in, ob_out, 64);
		quant = oggpack_copy(ob_in, ob_out, 4) + 1;
		oggpack_copy(ob_in, ob_out, 1);

		if (maptype == 1)
			num = floor(pow(book->n, 1. / book->dim));
		else
			num = book->n * book->dim;

		oggpack_copy(ob_in, ob_out, quant * num);

		return 0;
	}

	return -1;
}


static int recode_time(oggpack_buffer *ob_in, oggpack_buffer *ob_out, struct time *time)
{
	oggpack_copy(ob_in, ob_out, 16);

	return 0;
}


static int recode_floor(oggpack_buffer *ob_in, oggpack_buffer *ob_out, struct floor *floor)
{
	int classes;
	int rangebits;
	int i;

	oggpack_copy(ob_in, ob_out, 21);

	oggpack_copy(ob_in, ob_out, 4 * floor->partitions);

	classes = 0;
	for (i = 0; i < floor->partitions; i++)
		if (floor->class[i] > classes)
			classes = floor->class[i];
	classes++;

	for (i = 0; i < classes; i++) {
		oggpack_copy(ob_in, ob_out, 5);
		if (floor->class_subs[i] != 0)
			oggpack_copy(ob_in, ob_out, 8);
		oggpack_copy(ob_in, ob_out, 8 * (1 << floor->class_subs[i]));
	}

	oggpack_copy(ob_in, ob_out, 2);
	rangebits = oggpack_copy(ob_in, ob_out, 4);
	for (i = 0; i < floor->partitions; i++)
		oggpack_copy(ob_in, ob_out, rangebits * floor->class_dim[floor->class[i]]);

	return 0;
}


static int recode_residue(oggpack_buffer *ob_in, oggpack_buffer *ob_out, struct residue *residue)
{
	int stage;
	int i;

	oggpack_copy(ob_in, ob_out, 102);

	for (i = 0; i < residue->partitions; i++) {
		oggpack_copy(ob_in, ob_out, 3);
		if (oggpack_copy(ob_in, ob_out, 1) == 1)
			oggpack_copy(ob_in, ob_out, 5);
	}

	for (i = 0; i < residue->partitions; i++) {
		for (stage = 0; stage < residue->stages; stage++) {
			if (residue->secondstages[i] & (1 << stage))
				oggpack_copy(ob_in, ob_out, 8);
		}
	}

	return 0;
}


static int recode_mapping(oggpack_buffer *ob_in, oggpack_buffer *ob_out, struct mapping *mapping, struct headers *headers)
{
	oggpack_copy(ob_in, ob_out, 16);

	if (oggpack_copy(ob_in, ob_out, 1) == 1)
		oggpack_copy(ob_in, ob_out, 4);

	if (oggpack_copy(ob_in, ob_out, 1) == 1) {
		oggpack_copy(ob_in, ob_out, 8);
		oggpack_copy(ob_in, ob_out, mapping->coupling_steps * 2 * ilog(headers->channels));
	}

	oggpack_copy(ob_in, ob_out, 2);

	if (mapping->submaps > 1)
		oggpack_copy(ob_in, ob_out, 4 * headers->channels);

	oggpack_copy(ob_in, ob_out, 24 * mapping->submaps);

	return 0;
}


static int recode_mode(oggpack_buffer *ob_in, oggpack_buffer *ob_out, struct mode *mode)
{
	oggpack_copy(ob_in, ob_out, 41);

	return 0;
}


int header2_recode(ogg_packet *op_in, ogg_packet *op_out, struct headers *headers)
{
	oggpack_buffer ob_in, ob_out;
	int i;
	
	oggpack_readinit(&ob_in, op_in->packet, op_in->bytes);
	oggpack_writeinit(&ob_out);

	oggpack_copy(&ob_in, &ob_out, 56);

	oggpack_copy(&ob_in, &ob_out, 8);
	for (i = 0; i < headers->books; i++)
		recode_book(&ob_in, &ob_out, headers->book + i);

	oggpack_copy(&ob_in, &ob_out, 6);
	for (i = 0; i < headers->times; i++)
		recode_time(&ob_in, &ob_out, headers->time + i);

	oggpack_copy(&ob_in, &ob_out, 6);
	for (i = 0; i < headers->floors; i++)
		recode_floor(&ob_in, &ob_out, headers->floor + i);

	oggpack_copy(&ob_in, &ob_out, 6);
	for (i = 0; i < headers->residues; i++)
		recode_residue(&ob_in, &ob_out, headers->residue + i);

	oggpack_copy(&ob_in, &ob_out, 6);
	for (i = 0; i < headers->mappings; i++)
		recode_mapping(&ob_in, &ob_out, headers->mapping + i, headers);

	oggpack_copy(&ob_in, &ob_out, 6);
	for (i = 0; i < headers->modes; i++)
		recode_mode(&ob_in, &ob_out, headers->mode + i);

	oggpack_copy(&ob_in, &ob_out, 1);

	*op_out = *op_in;
	op_out->packet = oggpack_get_buffer(&ob_out);
	op_out->bytes = oggpack_bytes(&ob_out);

	return 0;
}
