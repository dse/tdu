/*
 * tduint.h
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

#ifndef TDUINT_H
#define TDUINT_H
/*****************************************************************************/

#include "tdu.h"
#include "node.h"

extern int THREE_ACS_CHARS;
extern int USE_ACS_CHARS;

/* miscellaneous defines */
#define CURSORX 0               /* x position of terminal's cursor */

/* debugging purposes */
#define FULL_REDRAW 0           /* true => collapse and expand always redraw */

/* different types of "tree branches" that can be displayed" */
typedef enum tree_chars {
	IAM_LAST,		/* lower-left corner of box --- printed just
				   before the node's name if node is the
				   last child of its parent */
	IAM_NOTLAST,		/* tee pointing right --- printed just
				   before the node's name if node's
				   parent has more children */
	PARENT_LAST,		/* nothing --- printed if ancestor does
				   not have any more children left */
	PARENT_NOTLAST		/* vertical line --- printed if ancestor
				   has children left to display */
} tree_chars_enum;

/* Type of function used to display things on some type of display */

typedef void (*node_display_fp)
(int line,			/* line # on screen to display (based at 0) */
 node_s *node,			/* node to display */
 int level,			/* # levels out to "indent" the node */
 bool iscursor);		/* is the cursor on this line? */

typedef void (*tree_chars_display_fp)
(enum tree_chars e);		/* determines which type of "tree branch"
				   characters to display */

/* A display_s structure is a collection of all functions necessary to
   display "tree branch" characters and information stored in a node
   on some type of display (such as the tdu interface) */

typedef struct display {
	node_display_fp fpn;
	tree_chars_display_fp fpt;
} display_s;

void display_tree_chars(node_s *node, int levelsleft, int thisisit, display_s ds);
void display_node(int line, node_s *node, int level, bool iscursor, display_s ds);
int display_nodes(int line, int lines, node_s *node, long nodeline, long cursor, display_s ds);
int display_nodes_(int line, int lines, node_s *node, long nodeline, long cursor, display_s ds, int level);
void tdu_hide_cursor(void);
void tdu_show_cursor(void);
void tdu_interface_display_node(int line, node_s *node, int level, bool iscursor);
void tdu_interface_display_tree_chars(tree_chars_enum tc);
void tdu_interface_finish(int sig);
void tdu_interface_refresh(void);
void tdu_interface_display(void);
void tdu_interface_expand(int levels, int redraw);
void tdu_interface_collapse(int redraw);
void tdu_interface_moveup(int n);
void tdu_interface_movedown(int n);
void tdu_interface_setcursor(int n);
void tdu_interface_pageup(void);
void tdu_interface_pagedown(void);
void tdu_interface_sort(node_sort_fp fp, bool reverse, bool isrecursive);
void tdu_interface_run(node_s *node);

/*****************************************************************************/
#endif /* TDUINT_H */
