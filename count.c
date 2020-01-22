/* This code is Copyright 2002 by Segher Boessenkool */

#include <stdio.h>
#include <stdlib.h>
#include <ogg/ogg.h>
#include "headers.h"
#include "count.h"


long **init_count(struct headers *headers)
{
	long **count;
	int i;

	count = malloc(headers->books * sizeof(*count));
	for (i = 0; i < headers->books; i++)
		count[i] = calloc(headers->book[i].n, sizeof(*count[i]));

	return count;
}


static int huff_read(oggpack_buffer *ob, struct headers *headers, int book, long **count)
{
	int p, l, r;
	int *tree;
	int value;

	p = 0;
	l = 0;
	r = headers->book[book].values;
	tree = headers->book[book].tree;

	while (l + 1 != r) {
		int val;
		int bit;

		bit = oggpack_read(ob, 1);
		if (bit < 0)
			return -1;

		val = tree[p];
		if (bit == 0) {
			r = l + val;
			p++;
		} else {
			l += val;
			p += val;
		}
	}

	value = headers->book[book].value[l];
	count[book][value]++;
	return value;
}


static int count_floor(oggpack_buffer *ob, struct headers *headers, struct floor *floor, int *nonzero, long **count)
{
	int i, j;

	static int quant_bits[] = { 8, 7, 7, 6 };

	*nonzero = oggpack_read(ob, 1);
	if (*nonzero != 1)
		return *nonzero;

	oggpack_read(ob, quant_bits[floor->quant]);
	oggpack_read(ob, quant_bits[floor->quant]);

	for (i = 0; i < floor->partitions; i++) {
		int class;
		int class_sub_bits;
		int class_val;

		class = floor->class[i];
		class_sub_bits = floor->class_subs[class];

		if (class_sub_bits)
			class_val = huff_read(ob, headers, floor->class_book[class], count);
		else
			class_val = 0;

		for (j = 0; j < floor->class_dim[class]; j++) {
			int book;

			book = floor->class_subbook[class][class_val & ((1 << class_sub_bits) - 1)];
			class_val >>= class_sub_bits;
			if (book >= 0)
				if (huff_read(ob, headers, book, count) < 0)
					return -1;
		}
	}

	return 0;
}


static int count_residue(oggpack_buffer *ob, struct headers *headers, struct residue *residue, int ch, long **count)
{
	int partvals;
	int partitions_per_word;
	int *part;
	int stage;
	int i, j, k;

	partvals = (residue->end - residue->begin) / residue->grouping;
	partitions_per_word = headers->book[residue->groupbook].dim;
	part = alloca((partvals + partitions_per_word - 1) * sizeof(*part));

	for (stage = 0; stage < residue->stages; stage++) {
		for (i = 0, j = 0; i < partvals; j++) {
			if (stage == 0) {
				int group;

				group = huff_read(ob, headers, residue->groupbook, count);
				if (group < 0)
					goto done;
				for (k = partitions_per_word - 1; k >= 0; k--) {
					part[j * partitions_per_word + k] = group % residue->partitions;
					group /= residue->partitions;
				}
			}

			for (k = 0; k < partitions_per_word && i < partvals; k++, i++)
				if (residue->secondstages[part[i]] & (1 << stage)) {
					int stagebook;

					stagebook = residue->book[part[i]][stage];

					if (stagebook >= 0) {
						//XXX this is messy
						int offset, chptr, ii, jj;
				offset=i*residue->grouping+residue->begin;

						chptr = 0;
						for (ii = offset / ch; ii < (offset + residue->grouping) / ch; ) {
							if (huff_read(ob, headers, stagebook, count) < 0)
								goto done;
							for (jj = 0; jj < headers->book[stagebook].dim; jj++) {
								chptr++;
								if (chptr == ch) {
									chptr = 0;
									ii++;
//fprintf(stderr, "%d,%d = %d -> %d\n", stage, ii, stage*size+ii, oggpack_bits(ob));
								}
							}
						}






					}
				}
		}
	}

done:

	return 0;
}


static int count_mapping(oggpack_buffer *ob, struct headers *headers, struct mapping *mapping, long **count)
{
	int *nonzero;
	int i, j;

	nonzero = alloca(headers->channels * sizeof(*nonzero));

	for (i = 0; i < headers->channels; i++) {
		if (count_floor(ob, headers, headers->floor + mapping->floor[mapping->chmux[i]], nonzero + i, count) < 0)
			return -1;
	}
//fprintf(stderr, "after floors, %ld bits are read\n", oggpack_bits(ob));

	//XXX need to to coupling here

	for (i = 0; i < mapping->submaps; i++) {
		int ch;
		int allzero;

		ch = 0;
		allzero = 1;
		for (j = 0; j < headers->channels; j++) {
			if (mapping->chmux[j] == i) {
				if (nonzero[j])
					allzero = 0;
				ch++;
			}
		}
		if (!allzero)
			count_residue(ob, headers, headers->residue + mapping->residue[i], ch, count);
	}
//fprintf(stderr, "after residues, %ld bits are read\n", oggpack_bits(ob));

	return 0;
}


int count_packet(ogg_packet *op, struct headers *headers, long **count)
{
	oggpack_buffer ob;
	int mode;
	int blocktype;
	int ret;

	oggpack_readinit(&ob, op->packet, op->bytes);

	if (oggpack_read(&ob, 1) != 0)
		return -1;

	mode = oggpack_read(&ob, 1); // XXX: modebits
	if (mode < 0)
		return -1;
//fprintf(stderr, "mode = %d, blockflag = %d\n", mode, headers->mode[mode].blockflag);

	blocktype = headers->mode[mode].blockflag;
	if (blocktype == 1)
		if (oggpack_read(&ob, 2) < 0)
			return -1;

	ret = count_mapping(&ob, headers, headers->mapping + headers->mode[mode].mapping, count);
//fprintf(stderr, "status = %d\n", ret);

	return ret;
}
