/*
 * tduint.h
 * This file is part of tdu, a text-mode disk usage visualization utility.
 *
 * Copyright (C) 2004-2012 Darren Stuart Embry.
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

#ifndef TDUINT_H
#define TDUINT_H
/*****************************************************************************/

#include "tdu.h"
#include "node.h"

extern int ascii_tree_chars;

/* different types of "tree branches" that can be displayed" */
typedef enum tree_chars {
	IAM_LAST,		/* lower-left corner of box */
	IAM_NOTLAST,		/* tee pointing right */
	PARENT_LAST,		/* blank */
	PARENT_NOTLAST		/* vertical line */
} tree_chars_enum;

void display_tree_chars (node_s *node, int levelsleft, int thisisit);
void display_node (int line, node_s *node, int level);
int display_nodes (int line, int lines, node_s *node, long nodeline,
		   long cursor);
int display_nodes_ (int line, int lines, node_s *node, long nodeline,
		    long cursor, int level);
void tdu_hide_cursor (void);
void tdu_show_cursor (void);
void tdu_interface_finish (int sig);
void tdu_interface_refresh (void);
void tdu_interface_display (void);
void tdu_interface_expand (int levels, int redraw);
void tdu_interface_collapse (int redraw);
void tdu_interface_move_up (int n);
void tdu_interface_move_down (int n);
void tdu_interface_move_to (int n);
void tdu_interface_page_up (void);
void tdu_interface_page_down (void);
void tdu_interface_sort (node_sort_fp fp, bool reverse, bool isrecursive);
void tdu_interface_run (node_s *node);

#define TDU_ONLINE_HELP \
"NAVIGATION:\n" \
"  UP,DOWN,PGUP,PGDOWN   up,down by line,page\n" \
"  HOME or <,END or >    first, last line\n" \
"  p                     up to parent directory\n" \
"  Ctrl-L                recenter line\n" \
"EXPANDING/COLLAPSING:\n" \
"  LEFT or 0, RIGHT   collapse, expand\n" \
"  1-9,*              expand 1-9,all levels\n" \
"SORTING CHILDREN:\n" \
"  s,S   sort,reverse sort by size\n" \
"  n,N   sort,reverse sort by name\n" \
"  u,U   unsort to original order\n" \
"  d,D   sort,reverse sort by number of descendents\n" \
"  =s, =S, =u, =U, =n, =N   sort recursively\n" \
"MISCELLANY:\n" \
"  #         show/hide number of descendents\n" \
"  A         toggle ASCII line-drawing characters\n" \
"  Ctrl-R    refresh display\n" \
"  q,x,ESC   quit\n" \
"  ?         online help\n" \
"  C         copyright info\n"

/*****************************************************************************/
#endif /* TDUINT_H */
