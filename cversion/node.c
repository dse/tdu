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
#include "group.h"
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <glib.h>

/* as nodes are created this is used to initialize their structures */
void
initialize_node (node_s *node)
{
	node->name = NULL;
	node->size = -1;	/* to be computed later unless specified */
	node->kids = NULL;
	node->nkids = 0;
	node->nkidblocks = 0;
	node->expanded = 0;
	node->parent = NULL;
	node->descendents = -1;	/* to be computed when tree is complete */
	node->isdupath = 0;
	node->islastkid = 1;
	node->origindex = -1;
	node->kids_by_name = NULL;
}

/* Create a new node and initialize its contents. */
node_s *			/* returns pointer to new node */
new_node (const char *name)	/* if not NULL, initialize node's name */
{
	node_s *node = (node_s *)malloc(sizeof(node_s));
	initialize_node(node);
	if (name) node->name = strdup(name);
	return node;
}

/* Have a parent node adopt an existing node, kid, as a child. */
void 
add_kid (node_s *parent,
	 node_s *kid)
{
	if (!parent || !kid)
		return;

	if (!parent->kids_by_name)
		parent->kids_by_name = 
			g_hash_table_new(g_str_hash, g_str_equal);
	
	/* if necessary, (re)allocate a bigger block of children */
	if (parent->nkids >= parent->nkidblocks * KIDSATATIME) {
		parent->kids = (node_s **)realloc(parent->kids,
						  ++parent->nkidblocks
						  * KIDSATATIME
						  * sizeof(node_s *));
	}
	
	/* any former last kid is not a last kid anymore */
	if (parent->nkids) 
		parent->kids[parent->nkids - 1]->islastkid = 0;
	
	kid->islastkid = 1;
	kid->origindex = parent->nkids;
	parent->kids[parent->nkids++] = kid;
	kid->parent = parent;
	
	g_hash_table_insert(parent->kids_by_name, kid->name, kid);
}

/* Either find an existing child whose name is the name argument, or
   create a new one under the specified node. */

node_s *
find_or_create_child(node_s *node, const char *name)
{
	node_s *child;
	gpointer found;

	if (!node || !name)
		return NULL;

	if (node->kids_by_name) {
		found = g_hash_table_lookup(node->kids_by_name, name);
		if (found) {
			return (node_s *)found;
		}
	}
	if ((child = new_node(name)) != NULL) {
		add_kid(node,child);
		return child;
	}
	return NULL;
}

/* link a pathname into the tree and set the destination node's size */
void
add_node (node_s *node,		/* root node to which pathname is relative */
	  const char *pathname,	/* path relative to node */
	  long size,		/* set destination node's size to this */
	  groups_s *groups)
{
	int i;
	const char *p = pathname;
	char name[PATH_MAX]; /* will contain first element of pathname */
	node_s *child;

	if (!node || !pathname)
		return;

	while (1) {

		/* each iteration in this loop begins by copying the
		   next element (sequence of non-slash characters) of
		   pathname to name.  If the pathname is absolute
		   (i.e., begins with a slash), then name includes a
		   leading slash. */

		i = 0;

		if (*p == '/') {
			/* leading "/"?  use "/" as the first node. */
			name[i++] = *(p++);
			name[i] = '\0';
			while (*p == '/') ++p;
		} else {
			while (*p && *p != '/')
				name[i++] = *(p++);
			name[i] = '\0';
			while (*p == '/') ++p;	/* skip slashes; go to
						   next pathname
						   element */
		}

		/* *after* the previous iteration of this loop
		   processes the last pathname element, name is empty
		   and we're at the destination node; update its
		   contents. */

		if (!i) {
			if (node->size >= 0 && node->size != size) {
				fprintf(stderr,
					"WARNING!! conflicting entry for %s\n",
					pathname);
			} else {
				node->size = size;
				node->isdupath = 1;
			}
			return;
		}

		/* if we reach this point we have a path element in
		   name.  p points to a relative path consisting of
		   the rest of the elements.  find or create a child
		   with name as its name and go to that child using
		   the rest of the pathname (p). */

		/* If name is the last path element we assume a file.
		   If filename matches one of the groups, find or
		   create a child with the group's name and put the
		   file under it.*/

		if (!*p) {
			group_s *group = 
				find_matching_group(groups,name);
			if (group) {
				node = find_or_create_child(node,group->name);
				if (node->size < 0) node->size = 0;
				node->size += size;
			}
		}

		/* find or create a child that matches this pathname
		   element */
		child = find_or_create_child(node,name);

		/* descend to the next pathname element to reach the
		   destination */
		node = child;
	}

	if (child->kids_by_name) {
		g_hash_table_destroy(child->kids_by_name);
		child->kids_by_name = NULL;
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
	
	if (node->kids && node->nkids) {
		int i;
		
		/* compute size as the sum of the size of each child */
		for (i = 0; i < node->nkids; ++i) {
			size += fix_tree_sizes(node->kids[i]);
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

	descendents = node->nkids;

	/* return existing #descendents if already computed */
	if (node->descendents >= 0) return node->descendents;

	if (node->kids && node->nkids) {
		int i;

		/* compute #descendents as this node's number of kids
		   plus the sum of each child's #descendents */
		for (i = 0; i < node->nkids; ++i) {
			descendents += fix_tree_descendents(node->kids[i]);
		}
	}
	return node->descendents = descendents;
}

/* === THIS IS A RECURSIVE FUNCTION. === */
void
dump_tree (node_s *node, int level)
{
	if (!node)
		return;

	int i;
	printf("%10ld %5lda %5lde %c ",node->size,
	       node->descendents,
	       node->expanded,
	       node->isdupath?'*':' ');
	if (level)
		for (i = 0; i < level; ++i) 
			fputs("  ",stdout);
	printf("%s\n",node->name);
	if (node->kids && node->nkids) {
		for(i = 0; i < node->nkids; ++i) {
			dump_tree(node->kids[i],level+1);
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

	if (!node || !node->nkids || !node->kids || !level)
		return 0;

	/* if collapsed, expand this level */
	if (!node->expanded) ret += (node->expanded = node->nkids);

	/* if any levels left, recursively call self on each of the
	   kids */    
	if (level > 0) level -= 1;	/* how many levels left? */
	if (level) {
		for (i = 0; i < node->nkids; ++i) {
			expanded = expand_tree_(node->kids[i],level);
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
	if (node && node->nkids && node->kids && node->expanded) {
		node->expanded = 0;
		for (i = 0; i < node->nkids; ++i) {
			collapse_tree_(node->kids[i]);
		}
	}
}

/* Find the <nodeline>th visible node in the tree.  See the comments
   in display_nodes_() if you can't figure out how this works. */

node_s *
find_node_number (node_s *node,long nodeline)
{
	int i; long l;
	if(node && nodeline >= 0 && nodeline < (1 + node->expanded)) {
		if (nodeline == 0) return node;
		--nodeline;
		if (node->expanded && node->kids && node->nkids) {
			i = 0;
			while (i < node->nkids
			       && nodeline >= (l = 1 + node->kids[i]->expanded)) {
				nodeline -= l;
				++i;
			}
			return find_node_number(node->kids[i],nodeline);
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
my_node_number (node_s *node, node_s *root)
{
	if (node) {
		long n = 0;
		int i;
		while (1) {
			node_s *parent = node->parent;
			if (node == root || parent == NULL) /* destination? */
				return n;
      
			++n;
			for (i = 0; (i < parent->nkids) && 
				     (parent->kids[i] != node); ++i)
				n += (1 + parent->kids[i]->expanded);
      
			if (i == parent->nkids) return -1; /* SHOULDN'T HAPPEN */

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
	if (node && node->kids && node->nkids) {
		int i;
		if (!fp) fp = node_cmp_unsort; /* provide a default
						  incase fp == NULL */
		node_sort = fp;
		node_sort_rev = reverse;
		qsort(node->kids,node->nkids,sizeof(node_s *),node_qsort_cmp);

		/* islastkid values of children may have to be reinitialized */
		for (i = 0; i < (node->nkids-1); ++i)
			node->kids[i]->islastkid = 0;
		node->kids[i]->islastkid = 1;

		/* if operating recursively, work on the kids now */
		if (isrecursive) {
			for (i = 0; i < node->nkids; ++i) {
				tree_sort(node->kids[i], fp,
					  reverse,isrecursive);
			}
		}
	}
}

/* create a tree structure for filenames and their sizes, grab each
   line from a file or stdin, parse each line (assuming output is
   similar to that of du) for blocksize and pathname, and add each
   pathname and size to the tree. */

node_s *			/* returns pointer to root of tree */
parse_file (const char *pathname, /* filename, or "-" or NULL for stdin */
	    groups_s *groups)
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
	node->name = "[root]"; /* no strdup necessary or wanted */

	while (fgets(line,sizeof(line),in)) {
		sscanf(line,"%ld %[^\n]\n",&size,path);
		add_node(node,path,size,groups);
		/* display progress */
		++entries;
		if (show_progress && !(entries % 100))
			fprintf(stderr,"%ld entries\r",entries);
	}

	if (in == stdin) {
		/* DIRTY HACK: reopen from tty for curses.  
		   I hope this works everywhere. */
		freopen("/dev/tty","r",stdin);
	} else {
		fclose(in);
	}
  
	fix_tree_sizes(node);
	fix_tree_descendents(node);
	return node;
}

