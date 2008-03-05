/*
 * node.c
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

/* Create a new node and initialize its contents. */
node_s *			/* returns pointer to new node */
new_node (const char *name)	/* if not NULL, initialize node's name */
{
	node_s *node = (node_s *)malloc(sizeof(node_s));
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

/* Have a parent node adopt an existing node, child, as a child. */
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
		parent->children = (node_s **)realloc(parent->children,
						      ++parent->nchildrenblocks
						      * KIDSATATIME
						      * sizeof(node_s *));
		if (!parent->children) {
			perror("add_child: realloc");
			exit(1);
		}
	}
	
	/* any former last child is not a last child anymore */
	if (parent->nchildren) 
		parent->children[parent->nchildren - 1]->is_last_child = 0;
	
	child->is_last_child = 1;
	child->origindex = parent->nchildren; /* for "unsorting" */
	parent->children[parent->nchildren++] = child;
	child->parent = parent;
	
	g_hash_table_insert(parent->children_by_name, child->name, child);
}

/* Either find an existing child whose name is the name argument, or
   create a new one under the specified node. */

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
		add_child(node,child);
		return child;
	}
	return NULL;
}

/* link a pathname into the tree and set the destination node's size */
void
add_node (node_s *root,		/* root node to which pathname is relative */
	  const char *pathname,	/* path relative to node */
	  long size)		/* set destination node's size to this */
{
	if (!root || !pathname) return;
	
	char *pathname_copy;
	char *name;
	node_s *node;

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

/* compute sizes of nodes in a tree whose sizes aren't initialized */
/* === THIS IS A RECURSIVE FUNCTION. === */
long				/* returns the size of the node */
fix_tree_sizes (node_s *node)
{
	long size = 0;

	if (!node)
		return 0;
	
	/* return existing size if already computed */
	if (node->size >= 0) return node->size;
	
	if (node->children && node->nchildren) {
		int i;
		
		/* compute size as the sum of the size of each child */
		for (i = 0; i < node->nchildren; ++i) {
			size += fix_tree_sizes(node->children[i]);
		}
	}
	
	return node->size = size;
}

/* compute #descendents of nodes in a tree whose descendents isn't
   already computed */
/* === THIS IS A RECURSIVE FUNCTION. === */
long				/* returns node's #descendents */
fix_tree_descendents (node_s *node)
{
	long descendents;

	if (!node)
		return 0;

	descendents = node->nchildren;

	if (node->descendents >= 0) /* if already computed */
		return node->descendents;

	if (node->children && node->nchildren) {
		int i;

		/* compute #descendents as this node's number of children
		   plus the sum of each child's #descendents */
		for (i = 0; i < node->nchildren; ++i) {
			descendents += fix_tree_descendents(node->children[i]);
		}
	}
	return node->descendents = descendents;
}

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

/* === THIS IS A RECURSIVE FUNCTION. === */
void
dump_tree (node_s *node, int level)
{
	if (!node)
		return;

	int i;
	printf("%10ld %5lda %5lde ",node->size,
	       node->descendents,
	       node->expanded);
	if (level)
		for (i = 0; i < level; ++i) 
			fputs("  ",stdout);
	printf("%s\n",node->name);
	if (node->children && node->nchildren) {
		for(i = 0; i < node->nchildren; ++i) {
			dump_tree(node->children[i],level+1);
		}
	}
}

/*****************************************************************************/
/* tree node expansion/collapsing functions */

/* "expand" a tree a certain level number of levels deep, or all the way. */
long				/* returns #descendents MADE visible */
expand_tree (node_s *node,	/* tree to expand */
	     int level)		/* #levels deep, or -1 to expand fully */
{
	node_s *p;
	long ret = expand_tree_(node,level);

	/* increment each ancestor's expand values once the work is
	   complete.  For performance reasons this is not done
	   recursively. */
	for (p = node->parent; p; p = p->parent)
		p->expanded += ret;

	return ret;
}

/* does the dirty work for expand_tree() */
/* === THIS IS A RECURSIVE FUNCTION. === */
long				/* returns #descendents MADE visible */
expand_tree_ (node_s *node,	/* tree to expand */
	      int level)	/* #levels deep, or -1 to expand fully */
{
	long ret = 0;
	long expanded;
	int i;

	if (!node || !node->nchildren || !node->children || !level)
		return 0;

	/* if collapsed, expand this level */
	if (!node->expanded) ret += (node->expanded = node->nchildren);

	/* if any levels left, recursively call self on each of the
	   children */    
	if (level > 0) level -= 1;	/* how many levels left? */
	if (level) {
		for (i = 0; i < node->nchildren; ++i) {
			expanded = expand_tree_(node->children[i],level);
			ret += expanded;
			node->expanded += expanded;
		}
	}
	return ret;
}

/* Make all children of this node invisible. */
long				/* returns number of nodes MADE invisible */
collapse_tree (node_s *node)	/* ptr to tree to collapse */
{
	node_s *p;
	long collapsed;
    
	if (!node)
		return 0;

	collapsed = node->expanded;

	/* First we decrement each ancestor's expand values by the
	   number of descendents we will make invisible.  For
	   performance reasons this is not done recursively. */
	if (collapsed) {
		for (p = node->parent; p; p = p->parent)
			p->expanded -= node->expanded;
		collapse_tree_(node);
	}
	
	return collapsed;
}

/* Does the dirty work for collapse_tree(). */
/* === THIS IS A RECURSIVE FUNCTION. === */
void				/* returns number of nodes MADE invisible */
collapse_tree_ (node_s *node)	/* ptr to tree to collapse */
{
	int i;
	if (node && node->nchildren && node->children && node->expanded) {
		node->expanded = 0;
		for (i = 0; i < node->nchildren; ++i) {
			collapse_tree_(node->children[i]);
		}
	}
}

/* Find the <nodeline>th visible node in the tree.  See the comments
   in display_nodes_() if you can't figure out how this works. */

node_s *
find_node_numbered (node_s *node,long nodeline)
{
	int i; long l;
	if(node && nodeline >= 0 && nodeline < (1 + node->expanded)) {
		if (nodeline == 0) return node;
		--nodeline;
		if (node->expanded && node->children && node->nchildren) {
			i = 0;
			while (i < node->nchildren
			       && nodeline >= (l = 1 + node->children[i]->expanded)) {
				nodeline -= l;
				++i;
			}
			return find_node_numbered(node->children[i],nodeline);
		}
	}
	return NULL;
}

/* Find a node's "visible line number" address within the tree whose
   root node is specified by root.  If root is NULL, this function
   simply iterates through the chain of parents until it finds a node
   with no parents, and considers that for all intents and purposes to
   be the root.  This is used so tdu can move the cursor to the cursor
   node's parent. */

long
find_node_number_in (node_s *node, node_s *root)
{
	if (node) {
		long n = 0;
		int i;
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
	return -1;
}

/*****************************************************************************/
/* directory tree sorting functions */

/* sort comparison functions for tree nodes */

int 
node_cmp_size(const node_s *a,const node_s *b) 
{
	return (a->size - b->size);
}

int
node_cmp_unsort(const node_s *a,const node_s *b)
{
	return (a->origindex - b->origindex);
}

int
node_cmp_name(const node_s *a,const node_s *b)
{
	return strcmp(a->name,b->name); 
}

int
node_cmp_descendents(const node_s *a,const node_s *b)
{
	return (a->descendents - b->descendents);
}

/* see below */
static node_sort_fp node_sort = node_cmp_unsort;
static bool node_sort_rev = 0;

/* sort comparison function that is passed directly to qsort().
   node_sort_fp and node_sort_rev above are used to select which
   sorting function to use before using this function and whether
   to reverse or normally sort. */

int
node_qsort_cmp(const void *aa,const void *bb) 
{
	const node_s *a = *(const node_s **)aa;
	const node_s *b = *(const node_s **)bb;
	int ret = node_sort(a,b);
	if (node_sort_rev) ret = -ret;
	return ret;
}

/* sort the children of a tree node */
/* === THIS FUNCTION CAN OPERATE RECURSIVELY. === */
void
tree_sort (node_s *node,	/* node whose children to sort */
	   node_sort_fp fp,	/* sort comparison function */
	   bool reverse,	/* sort reverse? */
	   bool isrecursive)	/* go recursive? */
{
	if (node && node->children && node->nchildren) {
		int i;
		if (!fp) fp = node_cmp_unsort; /* provide a default
						  incase fp == NULL */
		node_sort = fp;
		node_sort_rev = reverse;
		qsort(node->children,node->nchildren,sizeof(node_s *),node_qsort_cmp);

		/* is_last_child values of children may have to be reinitialized */
		for (i = 0; i < (node->nchildren-1); ++i)
			node->children[i]->is_last_child = 0;
		node->children[i]->is_last_child = 1;

		/* if operating recursively, work on the children now */
		if (isrecursive) {
			for (i = 0; i < node->nchildren; ++i) {
				tree_sort(node->children[i], fp,
					  reverse,isrecursive);
			}
		}
	}
}

/* create a tree structure for filenames and their sizes, grab each
   line from a file or stdin, parse each line (assuming output is
   similar to that of du) for blocksize and pathname, and add each
   pathname and size to the tree. */

node_s *			  /* returns pointer to root of tree */
parse_file (const char *pathname) /* filename, or "-" or NULL for stdin */
{
	node_s *node;
	FILE *in;
	char line[PATH_MAX+256]; /* store lines as they are read from a file */
	char path[PATH_MAX];	 /* store pathnames as they are copied */
	long size;
	long entries = 0;
	int show_progress = isatty(fileno(stderr));

	if (!pathname || !strcmp(pathname,"-")) {
		in = stdin;
	} else {
		in = fopen(pathname,"r");
		if(!in) { 
			fprintf(stderr,"Cannot open %s: %s\n",
				pathname,strerror(errno));
			return NULL; 
		}
	}
  
	node = new_node(NULL);
	node->name = "[root]";	/* no strdup necessary or wanted */

	while (fgets(line,sizeof(line),in)) {
		sscanf(line,"%ld %[^\n]\n",&size,path);
		add_node(node,path,size);
		/* display progress */
		++entries;
		if (show_progress && !(entries % 100))
			fprintf(stderr,"%ld entries\r",entries);
	}
	if (show_progress) {
		fprintf(stderr, "%ld entries\n", entries);
	}

	if (in != stdin) {
		fclose(in);
	}

	fix_tree_sizes(node);
	fix_tree_descendents(node);
	cleanup_tree(node);
	return node;
}

