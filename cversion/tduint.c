/*
 * tduint.c
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

#include "tduint.h"
#include "node.h"
#include "nowrap.h"

#include <stdlib.h>
#include <curses.h>
#include <stdio.h>
#include <signal.h>

/*****************************************************************************/

/* fake line-drawing characters to display */
static char *tree_chars_string[] = {
	"`- ",                        /* IAM_LAST       == 0 */
	"+- ",                        /* IAM_NOTLAST    == 1 */
	"   ",                        /* PARENT_LAST    == 2 */
	"|  "                         /* PARENT_NOTLAST == 3 */
};

int USE_ARROW_CURSOR = 1;
int THREE_ACS_CHARS = 1;
int USE_ACS_CHARS = 1;

/* Generic function to display "tree branch" characters for a node and
   its parents. */

/* === THIS IS A RECURSIVE FUNCTION. === */
void
display_tree_chars (node_s *node,   /* caller specifies node being displayed;
                                       we go up the chain of parents */
                    int levelsleft, /* max # "tree branch" levels to display,
                                       or -1 to go all the way */
                    int thisisit,   /* flag: is this child we're displaying? */
                    display_s ds
	)
{
	/* first display branches related to parents */
	if (node->parent && levelsleft > 0) {
		display_tree_chars(node->parent,
				   (levelsleft < 0) ? -1 : (levelsleft - 1),
				   0,ds);
	}
	if (levelsleft) {
		tree_chars_enum tc =
			(thisisit
			 ? (node->islastkid ? IAM_LAST : IAM_NOTLAST)
			 : (node->islastkid ? PARENT_LAST : PARENT_NOTLAST));
		if (ds.fpt) {
			(ds.fpt)(tc);
		} else {
			/* generic "default" code for debugging purposes */
			printf(tree_chars_string[tc]);
		}
	}
}

/* Generic function to display a node from a tree on a specific line
   of the display. */

void
display_node (int line,         /* line number on screen */
              node_s *node,     /* node to display */
              int level,        /* amount of indentation */
              bool iscursor,    /* display as cursor or not */
              display_s ds)
{
	if (ds.fpn)
		(ds.fpn)(line,node,level,iscursor);
	else {
		/* generic "default" code for debugging purposes */
		printf(iscursor ? "=> " : "   ");
		printf("[%3d] ",line);
		if (node) {
			display_tree_chars(node,level,1,ds);
			printf("%s (%ld)",node->name,node->size);
			if (!node->expanded && node->descendents) {
				printf(" ...");
			}
		}
		printf("\n");
	}
}

/* Generic function to display nodes from a tree on the screen. */

int                             /* returns number of lines displayed */
display_nodes (int line,        /* starting line number on screen */
               int lines,       /* number of lines to display on screen */
               node_s *node,    /* node of parent */
               long nodeline,   /* line # within tree to start displaying */
               long cursor,     /* line # within tree where cursor is
                                   located */
               display_s ds
	)        /* line # within tree where cursor is */
{
	int nodes = display_nodes_(line,lines,node,nodeline,cursor,ds,0);
	line += nodes;
	lines -= nodes;
	for (; lines > 0; ++line,--lines) {
		display_node(line,NULL,0,0,ds);
	}
	return nodes;
}

/* This function does the "dirty work" for display_nodes(). */
/* === THIS IS A RECURSIVE FUNCTION. === */
int                             /* returns number of lines dsplayed */
display_nodes_ (int line,       /* starting line number on screen */
                int lines,      /* number of lines to display on screen */
                node_s *node,   /* node of parent */
                long nodeline,  /* line # within tree to start displaying */
                long cursor,    /* line # within tree where cursor is loc'd */
                display_s ds,
                int level)      /* level of indentation */
{
	long ret = 0; int i; long l;

	if (node && nodeline >= 0 &&
	    nodeline < (1 + node->expanded) && lines) {

		/* Do we start displaying the tree at this node? */

		if (nodeline == 0) {
			display_node(line,node,level,nodeline == cursor,ds);
			++ret; ++line; --lines;   /* a line was displayed */
			--cursor; /* one node closer to cursor */
		} else {
			/* skip parent node */
			--nodeline; /* one node closer to start displaying */
			--cursor;   /* one node closer to cursor */
		}

		if (node->expanded && node->kids && node->nkids && lines) {

			/* Now traverse the children to discover which of
			   those children nodeline refers to a node inside. */

			i = 0;                    /* child number */
			while (i < node->nkids
			       && nodeline >= (l = 1 + node->kids[i]->expanded)) {
				nodeline -= l;
				cursor -= l;
				++i;
			}

			/* Now that nodeline is less than the number of
			   visible nodes, we start displaying nodes from
			   inside node->kids[i].  We may have to display nodes
			   from one or more of the next kids as well. */

			while (i < node->nkids && lines > 0) {
				l = display_nodes_(line,lines,node->kids[i],nodeline,cursor,
						   ds,level+1);
				ret += l; line += l; lines -= l;
				nodeline = 0; /* continue at top of next 
						 child's visible tree */
				cursor -= (1 + node->kids[i]->expanded);
				++i;
			}
		}
	}

	return ret;
}

/*****************************************************************************/
/* tdu interface functions */

node_s *intnode;                /* root node of tree being displayed */
int intcursor;                  /* line # in tree where "cursor" is located */
int intstart;                   /* line # in tree where top line on screen is
                                   located */
int prev_intstart;              /* value of intstart when screen was
                                   refreshed */
int intlines;                   /* # lines on screen */
WINDOW *tduwin;

display_s tdu_interface_ds = {
	tdu_interface_display_node,	/* fpn */
	tdu_interface_display_tree_chars /* fpt */
};

static int tdu_cursor = 0;      /* is cursor "visible"? */

void
tdu_hide_cursor ()
{
	if (USE_ARROW_CURSOR) {
		curs_set(0);
		if (tdu_cursor) {
			tdu_cursor = 0;
			wmove(tduwin,intcursor - intstart, CURSORX);
			wprintw_custom(tduwin,"  ");
		}
	} else {
		curs_set(0);
	}
}

void
tdu_show_cursor ()
{
	if (USE_ARROW_CURSOR) {
		curs_set(0);
		tdu_cursor = 1;
		wmove(tduwin,intcursor - intstart, 0);
		wprintw_custom(tduwin,"=>");
	} else {
		wmove(tduwin,intcursor - intstart, CURSORX);
		curs_set(1);
	}
}

/* tdu interface function to display a specific node (and its "tree
   branch" characters) on a line on the screen */

void
tdu_interface_display_node (int line, /* screen line # */
                            node_s *node,
                            int level,
                            bool iscursor)
{
	wmove(tduwin,line,0);
	wclrtoeol(tduwin);

	if (node) {
		if (USE_ARROW_CURSOR) {
			/* room for arrow cursor */
			wprintw_custom(tduwin,"  %11ld ",node->size);
		} else {
			wprintw_custom(tduwin,"%11ld ",node->size);
		}
		display_tree_chars(node,level,1,tdu_interface_ds);
		wprintw_custom(tduwin,"%s",node->name);
		if(node->nkids && !node->expanded) {
			wprintw_custom(tduwin," ...");
		}
	}
}

/* tdu interface function used to display a node's "tree branch"
   characters */

void
tdu_interface_display_tree_chars (tree_chars_enum tc)
{
	if (USE_ACS_CHARS) {
		switch (tc) {
		case IAM_LAST:
			waddch_custom(tduwin,ACS_LLCORNER);
			if (THREE_ACS_CHARS) {
				waddch_custom(tduwin,ACS_HLINE);
			}
			break;
		case IAM_NOTLAST:
			waddch_custom(tduwin,ACS_LTEE);
			if (THREE_ACS_CHARS) {
				waddch_custom(tduwin,ACS_HLINE);
			}
			break;
		case PARENT_LAST:
			waddch_custom(tduwin,' ');
			if (THREE_ACS_CHARS) {
				waddch_custom(tduwin,' ');
			}
			break;
		case PARENT_NOTLAST:
			waddch_custom(tduwin,ACS_VLINE);
			if (THREE_ACS_CHARS) {
				waddch_custom(tduwin,' ');
			}
			break;
		}
		waddch_custom(tduwin,' ');
	} else {
		wprintw_custom(tduwin, tree_chars_string[tc]);
	}
}

void
tdu_interface_finish (int sig)
{
	endwin();
	if(sig >= 0) exit(0);
}

/* ensure that cursor is in a location on the screen, adjusting
   viewport if necessary, and refresh the display.  This function is
   usually invoked before the next character is read. */

void
tdu_interface_refresh ()
{
	int lines;

	if (prev_intstart < 0)
	{
		display_nodes(0,intlines,intnode,intstart,intcursor,tdu_interface_ds);
	}
	else
	{
		/* make sure intcursor is within reasonable range */
		if (intcursor < 0)
			intcursor = 0;
		else if (intcursor > intnode->expanded)
			intcursor = intnode->expanded;

		/* position intstart such that intcursor is on the screen */
		if (intcursor < intstart) /* up off the screen? */
			intstart = intcursor;
		else if (intcursor > (intstart+intlines-1)) /* down off the screen? */
			intstart = intcursor - (intlines - 1);

		/* make sure intstart is within reasonable range */
		if (intstart < 0)
			intstart = 0;
		else if (intstart > intnode->expanded)
			intstart = intnode->expanded;

		/* do we have to scroll the screen? */
		lines = intstart - prev_intstart;

		/* if we scroll too much, just redisplay the whole thing */
		if (lines <= -intlines || lines >= intlines)
			display_nodes(0,intlines,intnode,
				      intstart,intcursor,tdu_interface_ds);

		/* do we need to scroll down? */
		else if (lines < 0) {
			wscrl(tduwin,lines);
			display_nodes(0,-lines,intnode,
				      intstart,intcursor,tdu_interface_ds);
		}


		/* do we need to scroll up? */
		else if (lines > 0) {
			wscrl(tduwin,lines);
			display_nodes(intlines - lines,lines,intnode,
				      intstart + intlines - lines,
				      intcursor,tdu_interface_ds);
		}
	}

	tdu_show_cursor();
	wrefresh(tduwin);
	prev_intstart = intstart;
}

/* Re-display the entire screen */

void
tdu_interface_display ()
{
	display_nodes(0,intlines,intnode,intstart,intcursor,tdu_interface_ds);
	tdu_interface_refresh();
}

/* Expand the tree at the cursor and redraw whichever portions of the
   screen need to be redrawn (or redraw the cursor line and all lines
   below if redraw is nonzero).  Always redraws cursor line and all
   lines below if levels > 1. */

void
tdu_interface_expand (int levels, int redraw)
{
	node_s *n = find_node_number(intnode,intcursor);

	if (FULL_REDRAW) {
		redraw = 1;
	} else {
		if (!redraw && (levels > 1)) redraw = 1;
	}

	if (n) {
		long scrolllines = expand_tree(n,levels);
		if (scrolllines) {
			if (redraw) {
				display_nodes(intcursor - intstart,intlines - (intcursor - intstart),
					      intnode,intcursor,intcursor,
					      tdu_interface_ds);
				tdu_interface_refresh();
			} else {
				long maxlines = intlines - (intcursor - intstart);

				if (scrolllines >= maxlines - 1) {
					display_nodes(intcursor - intstart,intlines - (intcursor - intstart),
						      intnode,intcursor,intcursor,
						      tdu_interface_ds);
				} else {
					tdu_interface_refresh();
					insdelln(scrolllines);
					display_nodes(intcursor - intstart,scrolllines + 1,
						      intnode,intcursor,intcursor,
						      tdu_interface_ds);
				}
				tdu_interface_refresh();
			}
		}
	}
}

/* Collapse the tree at the cursor and redraw whichever portions of
   the screen need to be redrawn (or redraw the cursor line and all
   lines below if redraw is nonzero) */

void
tdu_interface_collapse (int redraw)
{
	node_s *n = find_node_number(intnode,intcursor);

	if (FULL_REDRAW) {
		redraw = 1;
	}

	if (n) {
		long scrolllines = collapse_tree(n);
		if (scrolllines) {
			if (redraw) {
				display_nodes(intcursor - intstart,intlines - (intcursor - intstart),
					      intnode,intcursor,intcursor,
					      tdu_interface_ds);
				tdu_interface_refresh();
			} else {
				long maxlines = intlines - (intcursor - intstart);
				if (scrolllines >= maxlines - 1) {
					display_nodes(intcursor - intstart,
						      intlines - (intcursor - intstart),
						      intnode,intcursor,intcursor,
						      tdu_interface_ds);
				} else {
					tdu_interface_refresh();
					winsdelln(tduwin,-scrolllines);
					display_nodes(intcursor - intstart,1,
						      intnode,intcursor,intcursor,
						      tdu_interface_ds);
					display_nodes(intlines - scrolllines,scrolllines,
						      intnode,intcursor + maxlines - scrolllines,intcursor,
						      tdu_interface_ds);
				}
				tdu_interface_refresh();
			}
		}
	}
}

void
tdu_interface_moveup (int n)
{
	intcursor -= n;
	tdu_interface_refresh();
}

void
tdu_interface_movedown (int n)
{
	intcursor += n;
	tdu_interface_refresh();
}

void
tdu_interface_setcursor (int n)
{
	intcursor = n;
	tdu_interface_refresh();
}

void
tdu_interface_pageup ()
{
	intcursor -= (intlines - 1);
	tdu_interface_refresh();
}

void
tdu_interface_pagedown ()
{
	intcursor += (intlines - 1);
	tdu_interface_refresh();
}

void
tdu_interface_sort (node_sort_fp fp,bool reverse,bool isrecursive)
{
	node_s *n = find_node_number(intnode,intcursor);
	if (n && n->expanded) {

		long lines = n->expanded;
		long maxlines = intlines - (intcursor - intstart) - 1;

		tree_sort(n,fp,reverse,isrecursive);

		if (lines > maxlines) lines = maxlines;

		display_nodes(intcursor - intstart + 1,
			      lines,
			      intnode,
			      intcursor + 1,
			      intcursor,
			      tdu_interface_ds);
		tdu_interface_refresh();
	}
}

/* Initialize the screen and run the getch()-action loop. */

void
tdu_interface_run (node_s *node)
{
	int bx,by,ex,ey;
	int sortrecursive;
	int lastkey,key;
	int expandlevel;

	intnode = node;
	intstart = 0;
	intcursor = 0;
	prev_intstart = -1;

	if (intnode->nkids == 1) {
		intnode = intnode->kids[0];
	}

	signal(SIGINT,tdu_interface_finish);
	tduwin = initscr();
	keypad(tduwin,TRUE);
	nonl();
	cbreak();
	noecho();
	scrollok(tduwin,1);
	getbegyx(tduwin,by,bx);
	getmaxyx(tduwin,ey,ex);
	intlines = ey - by;

	tdu_interface_display();

	while (1) {
		key = wgetch(tduwin);
		sortrecursive = (lastkey == '=');

		switch (key) {

		case '=':
			break;

		case 3:                     /* C-c */
		case 27:                    /* ESC */
		case 'Q':
		case 'q':
		case 'X':
		case 'x':
			tdu_interface_finish(-1);
			goto endloop;

		case 16:                    /* C-p */
		case 'K':
		case 'k':
		case KEY_PREVIOUS:
		case KEY_UP:
			tdu_hide_cursor();
			tdu_interface_moveup(1);
			break;

		case 14:                    /* C-n */
		case 'j':
		case 'J':
		case KEY_NEXT:
		case KEY_DOWN:
			tdu_hide_cursor();
			tdu_interface_movedown(1);
			break;

		case '<':
		case KEY_HOME:
			tdu_hide_cursor();
			tdu_interface_setcursor(0);
			break;

		case '>':
		case KEY_END:
			tdu_hide_cursor();
			tdu_interface_setcursor(intnode->expanded - 1);
			break;

		case KEY_SPREVIOUS:
			tdu_hide_cursor();
			tdu_interface_moveup(10);
			break;

		case KEY_SNEXT:
			tdu_hide_cursor();
			tdu_interface_movedown(10);
			break;

		case KEY_PPAGE:
			tdu_hide_cursor();
			tdu_interface_pageup();
			break;

		case KEY_NPAGE:
			tdu_hide_cursor();
			tdu_interface_pagedown();
			break;

		case 'P':
		case 'p':
		{
			node_s *node = find_node_number(intnode,intcursor);
			node_s *parent = node ? node->parent : NULL;
			if (parent) {
				tdu_hide_cursor();
				tdu_interface_setcursor(my_node_number(parent,intnode));
			}
			break;
		}

		case 'u': tdu_interface_sort(node_cmp_unsort,0,sortrecursive); break;
		case 'U': tdu_interface_sort(node_cmp_unsort,1,sortrecursive); break;
		case 's': tdu_interface_sort(node_cmp_size,0,sortrecursive); break;
		case 'S': tdu_interface_sort(node_cmp_size,1,sortrecursive); break;
		case 'n': tdu_interface_sort(node_cmp_name,0,sortrecursive); break;
		case 'N': tdu_interface_sort(node_cmp_name,1,sortrecursive); break;

		case 'l':
		case 'L':
		case '1':
		case 6:                     /* C-f */
		case KEY_RIGHT:
			key = KEY_RIGHT;          /* so lastkey can be checked for repeats */
			if (lastkey == KEY_RIGHT || (lastkey >= '2' && lastkey <= '9'))
				++expandlevel;
			else
				expandlevel = 1;
			tdu_interface_expand(expandlevel,expandlevel > 1);
			break;

		case 12:                    /* C-l */
			intstart = intcursor - intlines / 2;
			tdu_interface_refresh();
			break;

		case 18:                    /* C-r */
			redrawwin(curscr);
			wrefresh(curscr);
			break;

		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			expandlevel = key - '0';
			tdu_interface_expand(expandlevel,1);
			break;

		case '*':
			tdu_interface_expand(-1 /* fully */,1);
			break;

		case 'h':
		case 'H':
		case '0':
		case 2:                     /* C-b */
		case KEY_LEFT:
			expandlevel = 0;
			tdu_interface_collapse(0);
			break;

		default:
			beep();
			break;

		}

		lastkey = key;

	}
endloop:
	return;
}

