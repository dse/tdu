/*
 * node.c
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

#include "tdu.h"
#include "node.h"
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <glib.h>

/* Create a new node, initialize its contents, and return a pointer to it. */
node_s *
new_node (const char *name)	/* if not NULL, initialize node's name */
{
	node_s *node = malloc(sizeof(node_s));
	if (node == NULL) {
		perror("new_node: malloc");
		exit(1);
	}
	if (name) {
		if (!(node->name = strdup(name))) {
			perror("new_node: strdup");
			exit(1);
		}
	}
	else {
		node->name = NULL;
	}
	node->size = -1;	/* to be computed later unless specified */
	node->children = NULL;
	node->nchildren = 0;
	node->nchildrenblocks = 0;
	node->expanded = 0;
	node->parent = NULL;
	node->descendents = -1;	/* to be computed when tree is complete */
	node->is_last_child = 1;
	node->origindex = -1;
	node->children_by_name = NULL;
	return node;
}

/* Have a parent node adopt an existing node as a child. */
void 
add_child (node_s *parent, node_s *child)
{
	if (!parent || !child)
		return;

	if (!parent->children_by_name) {
		if (!(parent->children_by_name =
		      g_hash_table_new(g_str_hash, g_str_equal))) {
			perror("add_child: g_hash_table_new");
			exit(1);
		}
	}
	
	/* if necessary, (re)allocate a bigger block of children */
	if (parent->nchildren >= parent->nchildrenblocks * KIDSATATIME) {
		parent->children = realloc(parent->children,
					   ++parent->nchildrenblocks
					   * KIDSATATIME * sizeof(node_s *));
		if (!parent->children) {
			perror("add_child: realloc");
			exit(1);
		}
	}
	
	if (parent->nchildren) 
		parent->children[parent->nchildren - 1]->is_last_child = 0;
	
	child->is_last_child = 1;
	child->origindex = parent->nchildren; /* for "unsorting" */
	parent->children[parent->nchildren++] = child;
	child->parent = parent;
	
	g_hash_table_insert(parent->children_by_name, child->name, child);
}

/* Find an existing child with the specified name or create a new one.
   Returns it. */
node_s *
find_or_create_child (node_s *node, const char *name)
{
	node_s *child;
	gpointer found;

	if (!node || !name)
		return NULL;

	if (node->children_by_name) {
		found = g_hash_table_lookup(node->children_by_name, name);
		if (found) {
			return (node_s *)found;
		}
	}
	if ((child = new_node(name)) != NULL) {
		add_child(node, child);
		return child;
	}
	return NULL;
}

/* Link a pathname into the tree and set the destination node's size. */
void
add_node (node_s *root, const char *pathname, TDU_SIZE_T size)
{
	char *pathname_copy;
	char *name;
	node_s *node;

	if (!root || !pathname) return;
	
	node = root;

	pathname_copy = strdup(pathname);
	if (!pathname_copy) {
		perror("add_node: strdup");
		exit(1);
	}
	name = strtok(pathname_copy, "/");
	while (name) {
		node = find_or_create_child(node, name);
		name = strtok(NULL, "/");
	}
	node->size = size;

        if (node->children_by_name) {
                g_hash_table_destroy(node->children_by_name);
                node->children_by_name = NULL;
        }
}

/* Recursively fix any nodes in a tree whose size is not yet specified.
   Returns size of specified node. */
TDU_SIZE_T
fix_tree_sizes (node_s *node)
{
	TDU_SIZE_T size = 0;

	if (!node) return 0;
	if (node->size >= 0) return node->size; /* no need to recompute */
	
	if (node->children && node->nchildren) {
		int i;
		for (i = 0; i < node->nchildren; ++i) {
			size += fix_tree_sizes(node->children[i]);
		}
	}
	
	return node->size = size;
}

/* Recursively compute each node's number of descendents.
   Returns number of descendents in specified node. */
long
fix_tree_descendents (node_s *node)
{
	long descendents;

	if (!node) return 0;
	if (node->descendents >= 0) return node->descendents; /* don't
								 recompute */

	descendents = node->nchildren;

	if (node->children && node->nchildren) {
		int i;
		for (i = 0; i < node->nchildren; ++i) {
			descendents += fix_tree_descendents(node->children[i]);
		}
	}
	return node->descendents = descendents;
}

/* Perform other cleanups. */
void
cleanup_tree (node_s *node)
{
	int i;
	if (node && node->children_by_name) {
		g_hash_table_destroy(node->children_by_name);
		node->children_by_name = NULL;
		for (i = 0; i < node->nchildren; ++i) {
			cleanup_tree(node->children[i]);
		}
	}
}

/* "expand" a tree a certain level number of levels deep, or if -1 is
   specified, all the way.  Returns total number of nodes made visible. */
long
expand_tree (node_s *node, int level)
{
	node_s *p;
	long ret = expand_tree_(node, level);

	for (p = node->parent; p; p = p->parent)
		p->expanded += ret;

	return ret;
}

/* Performs dirty work for expand_tree(). */
long
expand_tree_ (node_s *node, int level)
{
	long ret = 0;
	long expanded;
	int i;

	if (!(node && node->nchildren && node->children && level)) return 0;

	/* if collapsed, expand this level */
	if (!node->expanded) ret += (node->expanded = node->nchildren);

	/* if any levels left, recursively call self on each of the
	   children */    
	if (level > 0) level -= 1;	/* how many levels left? */
	if (level) {
		for (i = 0; i < node->nchildren; ++i) {
			expanded = expand_tree_(node->children[i], level);
			ret += expanded;
			node->expanded += expanded;
		}
	}
	return ret;
}

/* "collapse" the tree at the specified node.  Does not hide specified node.
   Returns the total number of nodes made hidden. */
long
collapse_tree (node_s *node)
{
	node_s *p;
	long collapsed;
    
	if (!node) return 0;

	collapsed = node->expanded;

	if (collapsed) {
		for (p = node->parent; p; p = p->parent)
			p->expanded -= node->expanded;
		collapse_tree_(node);
	}
	
	return collapsed;
}

/* Performs the dirty work for collapse_tree(). */
void
collapse_tree_ (node_s *node)
{
	int i;
	if (!(node && node->nchildren && node->children && node->expanded))
		return;

	node->expanded = 0;
	for (i = 0; i < node->nchildren; ++i) {
		collapse_tree_(node->children[i]);
	}
}

/******************************************************************************

Okay, the next two functions may require a little explanation.  When using
tdu, each visible node has a "line number".  The root node's line number is 0;
each subsequent visible node has the next line number.

Line numbers are only used for cursor and screen positioning.  The cursor_line
and start_line variables in tduint.c refer to these line numbers.

Line
No.
----
 0        684 /
 1        684 `- usr
 2        684    `- local
 3        192       +- share ...
 4          4       +- bin ...
 5          4       +- games
 6         88       +- lib ...
 7          4       +- include
 8         12       +- sbin ...
 9        280       +- src ...
10          0       +- man
11         96       `- stow ...

As nodes are expanded and collapsed, the line numbers will be different.

Line
No.
----
 0        684 /
 1        684 `- usr
 2        684    `- local
 3        192       +- share
 4         48       |  +- man ...
 5         24       |  +- emacs ...
 6         88       |  +- perl ...
 7          8       |  +- texmf ...
 8          8       |  +- games ...
 9          8       |  +- zsh ...
10          4       |  `- fonts
11          4       +- bin ...
12          4       +- games
...

Line numbers are calculated using the two functions below, not stored as data
in each node.

******************************************************************************/

/* Find the <nodeline>th visible node in the tree. */
node_s *
find_node_numbered (node_s *node, long nodeline)
{
	int i; long l;
	if (node && nodeline >= 0 && nodeline < (1 + node->expanded)) {
		if (nodeline == 0) return node;
		--nodeline;
		if (node->expanded && node->children && node->nchildren) {
			i = 0;
			while (i < node->nchildren
			       && (nodeline >=
				   (l = 1 + node->children[i]->expanded))) {
				nodeline -= l;
				++i;
			}
			return find_node_numbered(node->children[i], nodeline);
		}
	}
	return NULL;
}

/* Find the specified node's visible line number within the specified root. */
long
find_node_number_in (node_s *node, node_s *root)
{
	long n = 0;
	int i;

	if (!node) return -1;

	while (1) {
		node_s *parent = node->parent;
		if (node == root || parent == NULL) /* destination? */
			return n;

		++n;
		for (i = 0; (i < parent->nchildren) && 
			     (parent->children[i] != node); ++i)
			n += (1 + parent->children[i]->expanded);

		if (i == parent->nchildren) return -1; /* SHOULDN'T HAPPEN */

		node = parent;
	}
	return n;
}

/* sort comparison functions for tree nodes */

int 
node_cmp_size (const node_s *a, const node_s *b) 
{
	return (a->size - b->size);
}

int
node_cmp_unsort (const node_s *a, const node_s *b)
{
	return (a->origindex - b->origindex);
}

int
node_cmp_name (const node_s *a, const node_s *b)
{
	return strcmp(a->name, b->name); 
}

int
node_cmp_descendents (const node_s *a, const node_s *b)
{
	return (a->descendents - b->descendents);
}

/* internal variables set by tree_sort, used when sorting trees */
static node_sort_fp node_sort = node_cmp_unsort;
static bool node_sort_rev = 0;

int
node_qsort_cmp (const void *aa, const void *bb) 
{
	const node_s *a = *(const node_s **)aa;
	const node_s *b = *(const node_s **)bb;
	int ret = node_sort(a, b);
	if (node_sort_rev) ret = -ret;
	return ret;
}

/* Recursively (or not) sort the children of a tree node. */
void
tree_sort (node_s *node, node_sort_fp fp, bool reverse, bool isrecursive)
{
	int i;

	if (!(node && node->children && node->nchildren)) return;

	if (!fp) fp = node_cmp_unsort;

	node_sort = fp;
	node_sort_rev = reverse;
	qsort(node->children, node->nchildren, sizeof(node_s *),
	      node_qsort_cmp);

	for (i = 0; i < (node->nchildren-1); ++i)
		node->children[i]->is_last_child = 0;
	node->children[i]->is_last_child = 1;

	if (isrecursive) {
		for (i = 0; i < node->nchildren; ++i) {
			tree_sort(node->children[i], fp,
				  reverse, isrecursive);
		}
	}
}

/* Parse output of du and create a tree structure.
   Specify "-" or NULL for the filename to read from stdin.
   Returns pointer to parent node. */
node_s *
parse_file (const char *pathname)
{
	node_s *node;
	FILE *in;
	char line[PATH_MAX+256]; /* store lines as they are read from a file */
	char path[PATH_MAX];	 /* store pathnames as they are copied */
	TDU_SIZE_T size;
	long entries = 0;
	int show_progress = isatty(fileno(stderr));

	if (!pathname || !strcmp(pathname, "-")) {
		in = stdin;
	}
	else {
		in = fopen(pathname, "r");
		if (!in) {
			fprintf(stderr, "Cannot open %s: %s\n",
				pathname, strerror(errno));
			return NULL; 
		}
	}
  
	node = new_node(NULL);
	node->name = "[root]";	/* no strdup necessary or wanted */

	while (fgets(line, sizeof(line), in)) {
		sscanf(line, TDU_SIZE_T_SCANF " %[^\n]\n", &size, path);
		add_node(node, path, size);
		++entries;
		if (show_progress && !(entries % 100))
			fprintf(stderr, "%ld entries\r", entries);
	}
	if (show_progress) {
		fprintf(stderr, "%ld entries\n", entries);
	}

	if (in != stdin) {
		if (fclose(in)) {
			perror("parse_file: fclose");
			exit(1);
		}
	}

	fix_tree_sizes(node);
	fix_tree_descendents(node);
	cleanup_tree(node);
	return node;
}

