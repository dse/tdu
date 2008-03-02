/*
 * tdu.c
 * This file is part of tdu, a text-mode disk usage visualization utility.
 *
 * Copyright (C) 2004 Darren Stuart Embry.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *                                                                             
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *                                                                             
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307$
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tdu.h"
#include "node.h"
#include "tduint.h"

static char *optstring = "hG:I:AVP";
static char *progname = "tdu";

void
version_exit (int status)
{
	fprintf(stderr,
		"\n"
		"This is tdu version " TDU_VERSION ".  Copyright (C) 2004 Darren Stuart Embry.\n\n"
		"This program may be copied under the terms of the GNU General Public License:\n"
		"  http://dse.webonastick.com/tdu/copying.txt\n\n"
		"Documentation and other information about this program can be found here:\n"
		"  http://dse.webonastick.com/tdu/\n\n"
		"Run \"tdu -h\" for a usage summary for this program.\n\n"
		);
	exit(status);
}

void
usage_exit (int status)
{
	fprintf(stderr,
		"usage: %s [-h] [-A] [-V] [<file> ...]\n"
		"       du [<arg> ...] | %s [-h] [-A]\n"
		"options: -h              display this message\n"
		"         -A              use ascii chars\n"
		"         -V              show version, license terms\n"
		,progname,progname);
	exit(status);
}

typedef struct options {
	bool help;
	int optind;
	bool parse_only;
} options_s;

options_s *
get_options (int argc, char **argv)
{
	options_s *options;
	int c;

	options = (options_s *)malloc(sizeof(options_s));
	if (options == NULL) return NULL;
	options->help = 1;
	options->optind = -1;
	options->parse_only = 0;

	while ((c = getopt(argc,argv,optstring)) != -1) {
		switch (c) {
		case 'G':
			fprintf(stderr,
				"-G option is no longer implemented in tdu.\n"
				"Please use dugroup, a separate program.\n");
			exit(1);
		case 'I':
			fprintf(stderr,
				"-I option is no longer implemented in tdu.\n"
				"Please use dugroup, a separate program.\n");
			exit(1);
		case 'A':
			USE_ACS_CHARS = 0;
			break;
		case 'h':
			usage_exit(0);
			break;
		case 'V':
			version_exit(0);
			break;
		case 'P':
			options->parse_only = 1;
			break;
		default:
			usage_exit(1);
			break;
		}
	}

	options->optind = optind;
	return options;
}

/*****************************************************************************/

int
main (int argc, char **argv)
{
	node_s *node;
	options_s *options;

	if (NULL == (options = get_options(argc,argv))) {
		--argc,++argv;
	} else {
		argc -= options->optind;
		argv += options->optind;
	}

	node = parse_file(*argv);

	if (options->parse_only) {
		return 0;
	}

	if (node) {
		expand_tree(node,1);
		tdu_interface_run(node);
	}

	return 0;
}

