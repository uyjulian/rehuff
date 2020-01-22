/* This code is Copyright 2002 by Segher Boessenkool */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "sogg.h"
#include "count.h"
#include "recode.h"
#include "headers.h"


int verbose = 1;


void usage(void)
{
	fprintf(stderr, "usage: rehuff <infile> <outfile>\n");
	exit(1);
}


int main(int argc, char **argv)
{
	struct sogg sogg_in, sogg_out;
	ogg_packet op_in, op_out;
	FILE *fp_in, *fp_out;
	struct headers headers;
	long **count;

	if (argc != 3)
		usage();

	fp_in = fopen(argv[1], "rb");
	if (fp_in == 0) {
		perror(argv[1]);
		exit(1);
	}
	fp_out = fopen(argv[2], "wb");
	if (fp_out == 0) {
		perror(argv[2]);
		exit(1);
	}

	/* first, read the input, counting the huffwords encountered */

	sogg_init_read(&sogg_in, fp_in);

	sogg_read_first(&sogg_in, &op_in);
	if (header0_read(&op_in, &headers) < 0)
		exit(1);

	sogg_read(&sogg_in, &op_in);
	if (header1_read(&op_in, &headers) < 0)
		exit(1);

	sogg_read(&sogg_in, &op_in);
	if (header2_read(&op_in, &headers) < 0)
		exit(1);

	count = init_count(&headers);

	while (sogg_read(&sogg_in, &op_in))
		count_packet(&op_in, &headers, count);

	sogg_fini_read(&sogg_in);

//show_it(&headers, count);

	/* now, do processing */

	init_recode(&headers, count);

	/* then, read input again, and write output */

	fseek(fp_in, 0, 0);

	sogg_init_read(&sogg_in, fp_in);
	sogg_init_write(&sogg_out, fp_out, 12345 /* XXX: serialno */);

	sogg_read_first(&sogg_in, &op_in);
	if (header0_recode(&op_in, &op_out, &headers) < 0)
		exit(1);
	sogg_write(&sogg_out, &op_out);

	sogg_read(&sogg_in, &op_in);
	if (header1_recode(&op_in, &op_out, &headers) < 0)
		exit(1);
	sogg_write(&sogg_out, &op_out);

	sogg_read(&sogg_in, &op_in);
	if (header2_recode(&op_in, &op_out, &headers) < 0)
		exit(1);
	sogg_write(&sogg_out, &op_out);
	sogg_flush(&sogg_out);

	while (sogg_read(&sogg_in, &op_in)) {
		recode_packet(&op_in, &op_out, &headers);
		sogg_write(&sogg_out, &op_out);
	}

	sogg_fini_read(&sogg_in);
	sogg_fini_write(&sogg_out);

	/* finally, clean up */

	fclose(fp_out);
	fclose(fp_in);

	return 0;
}
