/* This code is Copyright 2002 by Segher Boessenkool */

#include <stdio.h>
#include <stdlib.h>
#include <ogg/ogg.h>
#include "headers.h"
#include "recode.h"


struct heap {
	int *x;
	long *w;
	int n;
};


/* WARNING: return value only valid if n <= 32 */
long oggpack_copy(oggpack_buffer *ob_in, oggpack_buffer *ob_out, int n)
{
	long x;

	while (n > 32) {
		x = oggpack_read(ob_in, 32);
		oggpack_write(ob_out, x, 32);
		n -= 32;
	}
	
	x = oggpack_read(ob_in, n);
	oggpack_write(ob_out, x, n);

	return x;
}


static void heap_insert(struct heap *h, int x, long w)
{
	int j, k;

	h->n++;
	for (k = h->n; k > 1; k = j) {
		j = k / 2;
		if (h->w[j] <= w)
			break;
		h->x[k] = h->x[j];
		h->w[k] = h->w[j];
	}
	h->x[k] = x;
	h->w[k] = w;
}


static void heap_delete(struct heap *h)
{
	int j, k;

	h->x[1] = h->x[h->n];
	h->w[1] = h->w[h->n];
	h->n--;
	for (j = 1; ; j = k) {
		k = 2 * j;
		if (k > h->n)
			break;
		if (k < h->n && h->w[k] > h->w[k + 1])
			k++;
		if (h->w[j] > h->w[k]) {
			int x = h->x[j];
			long w = h->w[j];
			h->x[j] = h->x[k];
			h->w[j] = h->w[k];
			h->x[k] = x;
			h->w[k] = w;
		} else
			break;
	}
}


static int length_rec(int *parent, int *length, int i)
{
	if (length[i] < 0)
		length[i] = length_rec(parent, length, parent[i]) + 1;

	return length[i];
}


static void calc_new_length(struct book *book, long *count)
{
	struct heap heap;
	int *parent = alloca((2 * book->n - 1) * sizeof(*parent));
	int *length = alloca((2 * book->n - 1) * sizeof(*parent));
	int i;

	heap.n = 0;
	heap.x = alloca((book->n + 1) * sizeof(*heap.x));
	heap.w = alloca((book->n + 1) * sizeof(*heap.w));

	for (i = 0; i < book->n; i++)
		if (count[i])
			heap_insert(&heap, i, count[i]);

	while (heap.n > 1) {
		int x0, x1;
		long w0, w1;

		x0 = heap.x[1];
		w0 = heap.w[1];
		heap_delete(&heap);
		x1 = heap.x[1];
		w1 = heap.w[1];
		heap_delete(&heap);
		parent[x0] = i;
		parent[x1] = i;
		heap_insert(&heap, i, w0 + w1);
		i++;
	}

	if (heap.n) {
		for (i = 0; i < 2*book->n-1; i++)
			length[i] = -1;
		length[heap.x[1]] = (heap.x[1] < book->n ? 1 : 0);
	}

	book->new_length = malloc(book->n * sizeof(*book->new_length));
	for (i = 0; i < book->n; i++)
		book->new_length[i] = count[i] ? length_rec(parent, length, i) : 0;
}


static int calc_new_bits(struct book *book)
{
	long bits[32]; // this level's current ___KID's___ bitpattern
	int okie[32];
	int i;

	okie[0] = 1;
	bits[0] = -1;
	for (i = 1; i < 32; i++)
		okie[i] = 0;
	//for (i = 1; i < 32; i++) bits[i] = 0;

	book->new_bits = malloc(book->n * sizeof(*book->new_bits));

	for (i = 0; i < book->n; i++) {
		int l, j, dang;

		l = book->new_length[i];
		if (l == 0)
			continue;

		for (dang = l - 1; okie[dang] == 0; dang--)
			;

		bits[dang] += (1 << dang);
		if (dang)
			okie[dang] = 0;

		for (j = dang + 1; j < l; j++) {
			bits[j] = bits[j - 1];
			okie[j] = 1;
		}

		book->new_bits[i] = bits[l - 1];
	}

	return 0;
}


static void calc_new_book(struct book *book, long *count)
{
	calc_new_length(book, count);

	calc_new_bits(book);
}


void init_recode(struct headers *headers, long **count)
{
	int i;

	for (i = 0; i < headers->books; i++)
		calc_new_book(&headers->book[i], count[i]);
}


static int huff_recode(oggpack_buffer *ob_in, oggpack_buffer *ob_out, struct headers *headers, int book)
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

		bit = oggpack_read(ob_in, 1);
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

	oggpack_write(ob_out, headers->book[book].new_bits[value], headers->book[book].new_length[value]);

	return value;
}


static int recode_floor(oggpack_buffer *ob_in, oggpack_buffer *ob_out, struct headers *headers, struct floor *floor, int *nonzero)
{
	int i, j;

	static int quant_bits[] = { 8, 7, 7, 6 };

	*nonzero = oggpack_copy(ob_in, ob_out, 1);
	if (*nonzero != 1)
		return *nonzero;

	oggpack_copy(ob_in, ob_out, quant_bits[floor->quant]);
	oggpack_copy(ob_in, ob_out, quant_bits[floor->quant]);

	for (i = 0; i < floor->partitions; i++) {
		int class;
		int class_sub_bits;
		int class_val;

		class = floor->class[i];
		class_sub_bits = floor->class_subs[class];

		if (class_sub_bits)
			class_val = huff_recode(ob_in, ob_out, headers, floor->class_book[class]);
		else
			class_val = 0;

		for (j = 0; j < floor->class_dim[class]; j++) {
			int book;

			book = floor->class_subbook[class][class_val & ((1 << class_sub_bits) - 1)];
			class_val >>= class_sub_bits;
			if (book >= 0)
				if (huff_recode(ob_in, ob_out, headers, book) < 0)
					return -1;
		}
	}

	return 0;
}


static int recode_residue(oggpack_buffer *ob_in, oggpack_buffer *ob_out, struct headers *headers, struct residue *residue, int ch)
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

				group = huff_recode(ob_in, ob_out, headers, residue->groupbook);
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
							if (huff_recode(ob_in, ob_out, headers, stagebook) < 0)
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


static int recode_mapping(oggpack_buffer *ob_in, oggpack_buffer *ob_out, struct headers *headers, struct mapping *mapping)
{
	int *nonzero;
	int i, j;

	nonzero = alloca(headers->channels * sizeof(*nonzero));

	for (i = 0; i < headers->channels; i++) {
		if (recode_floor(ob_in, ob_out, headers, headers->floor + mapping->floor[mapping->chmux[i]], nonzero + i) < 0)
			return -1;
	}
//fprintf(stderr, "after floors, %ld bits are read\n", oggpack_bits(ob_in));

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
			recode_residue(ob_in, ob_out, headers, headers->residue + mapping->residue[i], ch);
	}
//fprintf(stderr, "after residues, %ld bits are read\n", oggpack_bits(ob_in));

	return 0;
}


int recode_packet(ogg_packet *op_in, ogg_packet *op_out, struct headers *headers)
{
	oggpack_buffer ob_in, ob_out;
	int mode;
	int blocktype;
	int ret;

	oggpack_readinit(&ob_in, op_in->packet, op_in->bytes);
	oggpack_writeinit(&ob_out);

	if (oggpack_copy(&ob_in, &ob_out, 1) != 0)
		return -1;

	mode = oggpack_copy(&ob_in, &ob_out, 1); // XXX: modebits
	if (mode < 0)
		return -1;
//fprintf(stderr, "mode = %d, blockflag = %d\n", mode, headers->mode[mode].blockflag);

	blocktype = headers->mode[mode].blockflag;
	if (blocktype == 1)
		if (oggpack_copy(&ob_in, &ob_out, 2) < 0)
			return -1;

	ret = recode_mapping(&ob_in, &ob_out, headers, headers->mapping + headers->mode[mode].mapping);
//fprintf(stderr, "status = %d\n", ret);

	*op_out = *op_in;
	op_out->packet = oggpack_get_buffer(&ob_out);
	op_out->bytes = oggpack_bytes(&ob_out);

	return ret;
}
