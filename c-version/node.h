/*
 * node.h
 * This file is part of tdu, a text-mode disk usage visualization utility.
 *
 * Copyright (C) 2004-2008 Darren Stuart Embry.
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

#ifndef NODE_H
#define NODE_H
/*****************************************************************************/

#include "tdu.h"
#include <curses.h>

#include <glib.h>

/* Each node's list of children is allocated in blocks of this many. */
#define KIDSATATIME 256

/* Each pathname element has associated with it a node in a tree. */
typedef struct node {
	char *name;
	long size;
	struct node **children;	/* reallocated one block at a time */
	GHashTable *children_by_name;
	long nchildren;
	long nchildrenblocks;	/* allocated in this many "blocks" */
	struct node *parent;
	long expanded;		/* number of decendents visible -- used for
				   computing each node's "line number" */
	long descendents;
	bool is_last_child;	/* used for printing tree branches */
	int origindex;		/* for "unsorting" */
} node_s;

typedef int (*node_sort_fp)(const node_s *, const node_s *);

node_s *new_node (const char *name);
void add_child (node_s *parent, node_s *child);
node_s *find_or_create_child (node_s *node, const char *name);
void add_node (node_s *root, const char *pathname, TDU_SIZE_T size);
TDU_SIZE_T fix_tree_sizes (node_s *node);
long fix_tree_descendents (node_s *node);
void cleanup_tree (node_s *node);
void dump_tree (node_s *node, int level);
long expand_tree (node_s *node, int level);
long expand_tree_ (node_s *node, int level);
long collapse_tree (node_s *node);
void collapse_tree_ (node_s *node);
node_s *find_node_numbered (node_s *node, long nodeline);
long find_node_number_in (node_s *node, node_s *root);
int node_cmp_size (const node_s *a, const node_s *b);
int node_cmp_unsort (const node_s *a, const node_s *b);
int node_cmp_name (const node_s *a, const node_s *b);
int node_cmp_descendents (const node_s *a, const node_s *b);
int node_qsort_cmp (const void *aa, const void *bb);
void tree_sort (node_s *node, node_sort_fp fp, bool reverse, bool isrecursive);
node_s *parse_file (const char *pathname);

/*****************************************************************************/
#endif /* NODE_H */
