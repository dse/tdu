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
#include <sys/ioctl.h>
#include <pty.h>

/*****************************************************************************/

node_s *root_node;		/* root node of tree being displayed */
int cursor_line;		/* line # in tree where "cursor" is located */
int start_line;			/* line # in tree where top line on screen is
				   located */
int prev_start_line;		/* value of start_line when screen was
				   refreshed */
int visible_lines;		/* # lines on screen */
WINDOW *tdu_window;

WINDOW *main_window;
WINDOW *status_window;

/* fake line-drawing characters to display */
static char *tree_chars_string[] = {
	"`- ",                        /* IAM_LAST       == 0 */
	"+- ",                        /* IAM_NOTLAST    == 1 */
	"   ",                        /* PARENT_LAST    == 2 */
	"|  "                         /* PARENT_NOTLAST == 3 */
};

int USE_ACS_CHARS = 1;

/*****************************************************************************/

/* Generic function to display "tree branch" characters for a node and
   its parents. */

/* === THIS IS A RECURSIVE FUNCTION. === */
void
display_tree_chars (node_s *node,   /* caller specifies node being displayed;
                                       we go up the chain of parents */
                    int levelsleft, /* max # "tree branch" levels to display,
                                       or -1 to go all the way */
                    int thisisit)   /* flag: is this child we're displaying? */
{
	/* first display branches related to parents */
	if (node->parent && levelsleft > 0) {
		display_tree_chars(node->parent,
				   (levelsleft < 0) ? -1 : (levelsleft - 1),
				   0);
	}
	if (levelsleft) {
		tree_chars_enum tc =
			(thisisit
			 ? (node->islastkid ? IAM_LAST : IAM_NOTLAST)
			 : (node->islastkid ? PARENT_LAST : PARENT_NOTLAST));
		if (USE_ACS_CHARS) {
			switch (tc) {
			case IAM_LAST:
				waddch_nowrap(main_window,ACS_LLCORNER);
				waddch_nowrap(main_window,ACS_HLINE);
				break;
			case IAM_NOTLAST:
				waddch_nowrap(main_window,ACS_LTEE);
				waddch_nowrap(main_window,ACS_HLINE);
				break;
			case PARENT_LAST:
				waddch_nowrap(main_window,' ');
				waddch_nowrap(main_window,' ');
				break;
			case PARENT_NOTLAST:
				waddch_nowrap(main_window,ACS_VLINE);
				waddch_nowrap(main_window,' ');
				break;
			}
			waddch_nowrap(main_window,' ');
		}
		else {
			wprintw_nowrap(main_window, tree_chars_string[tc]);
		}
	}
}

/* Generic function to display a node from a tree on a specific line
   of the display. */

void
display_node (int line,         /* line number on screen */
              node_s *node,     /* node to display */
              int level,        /* amount of indentation */
              bool iscursor)    /* display as cursor or not */
{
	tdu_interface_display_node(line, node, level, iscursor);	
}

/* Generic function to display nodes from a tree on the screen. */

int                             /* returns number of lines displayed */
display_nodes (int line,        /* starting line number on screen */
               int lines,       /* number of lines to display on screen */
               node_s *node,    /* node of parent */
               long nodeline,   /* line # within tree to start displaying */
               long cursor)     /* line # within tree where cursor is
                                   located */
{
	int nodes = display_nodes_(line,lines,node,nodeline,cursor,0);
	line += nodes;
	lines -= nodes;
	for (; lines > 0; ++line,--lines) {
		display_node(line,NULL,0,0);
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
                int level)      /* level of indentation */
{
	long ret = 0; int i; long l;

	if (!(node && nodeline >= 0 &&
	      nodeline < (1 + node->expanded) && lines))
		return 0;

	/* Do we start displaying the tree at this node? */
	
	if (nodeline == 0) {
		display_node(line,node,level,nodeline == cursor);
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
					   level+1);
			ret += l; line += l; lines -= l;
			nodeline = 0; /* continue at top of next 
					 child's visible tree */
			cursor -= (1 + node->kids[i]->expanded);
			++i;
		}
	}

	return ret;
}

void
tdu_hide_cursor ()
{
	curs_set(0);
}

void
tdu_show_cursor ()
{
	wmove(main_window,cursor_line - start_line, 0);
	curs_set(1);
	wrefresh(main_window);
}

/* tdu interface function to display a specific node (and its "tree
   branch" characters) on a line on the screen */

void
tdu_interface_display_node (int line, /* screen line # */
                            node_s *node,
                            int level,
                            bool iscursor)
{
	wmove(main_window,line,0);
	wclrtoeol(main_window);

	if (node) {
		wprintw_nowrap(main_window,"%11ld ",node->size);
		display_tree_chars(node,level,1);
		wprintw_nowrap(main_window,"%s",node->name);
		if(node->nkids && !node->expanded) {
			wprintw_nowrap(main_window," ...");
		}
	}
}

void
tdu_interface_finish (int sig)
{
	endwin();
	if (sig >= 0) exit(0);
}

/* ensure that cursor is in a location on the screen, adjusting
   viewport if necessary, and refresh the display.  This function is
   usually invoked before the next character is read. */

void
tdu_interface_refresh ()
{
	int lines;

	if (prev_start_line < 0)
	{
		display_nodes(0,visible_lines,root_node,start_line,cursor_line);
	}
	else
	{
		/* make sure cursor_line is within reasonable range */
		if (cursor_line < 0)
			cursor_line = 0;
		else if (cursor_line > root_node->expanded)
			cursor_line = root_node->expanded;

		/* position start_line such that cursor_line is on the screen */
		if (cursor_line < start_line) /* up off the screen? */
			start_line = cursor_line;
		else if (cursor_line > (start_line+visible_lines-1)) /* down off the screen? */
			start_line = cursor_line - (visible_lines - 1);

		/* make sure start_line is within reasonable range */
		if (start_line < 0)
			start_line = 0;
		else if (start_line > root_node->expanded)
			start_line = root_node->expanded;

		/* do we have to scroll the screen? */
		lines = start_line - prev_start_line;

		/* if we scroll too much, just redisplay the whole thing */
		if (lines <= -visible_lines || lines >= visible_lines)
			display_nodes(0,visible_lines,root_node,
				      start_line,cursor_line);

		/* do we need to scroll down? */
		else if (lines < 0) {
			wscrl(main_window,lines);
			display_nodes(0,-lines,root_node,
				      start_line,cursor_line);
		}


		/* do we need to scroll up? */
		else if (lines > 0) {
			wscrl(main_window,lines);
			display_nodes(visible_lines - lines,lines,root_node,
				      start_line + visible_lines - lines,
				      cursor_line);
		}
	}

	wrefresh(status_window);
	wrefresh(main_window);
	tdu_show_cursor();
	prev_start_line = start_line;
}

/* Re-display the entire screen */

void
tdu_interface_display ()
{
	display_nodes(0,visible_lines,root_node,start_line,cursor_line);
	tdu_interface_refresh();
}

/* Expand the tree at the cursor and redraw whichever portions of the
   screen need to be redrawn (or redraw the cursor line and all lines
   below if redraw is nonzero).  Always redraws cursor line and all
   lines below if levels > 1. */

void
tdu_interface_expand (int levels, int redraw)
{
	node_s *n = find_node_numbered(root_node,cursor_line);

	if (FULL_REDRAW) {
		redraw = 1;
	} else {
		if (!redraw && (levels > 1)) redraw = 1;
	}

	if (n) {
		long scrolllines = expand_tree(n,levels);
		if (scrolllines) {
			if (redraw) {
				display_nodes(cursor_line - start_line,visible_lines - (cursor_line - start_line),
					      root_node,cursor_line,cursor_line);
				tdu_interface_refresh();
			} else {
				long maxlines = visible_lines - (cursor_line - start_line);

				if (scrolllines >= maxlines - 1) {
					display_nodes(cursor_line - start_line,visible_lines - (cursor_line - start_line),
						      root_node,cursor_line,cursor_line);
				} else {
					tdu_interface_refresh();
					insdelln(scrolllines);
					display_nodes(cursor_line - start_line,scrolllines + 1,
						      root_node,cursor_line,cursor_line);
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
	node_s *n = find_node_numbered(root_node,cursor_line);

	if (FULL_REDRAW) {
		redraw = 1;
	}

	if (n) {
		long scrolllines = collapse_tree(n);
		if (scrolllines) {
			if (redraw) {
				display_nodes(cursor_line - start_line,visible_lines - (cursor_line - start_line),
					      root_node,cursor_line,cursor_line);
				tdu_interface_refresh();
			} else {
				long maxlines = visible_lines - (cursor_line - start_line);
				if (scrolllines >= maxlines - 1) {
					display_nodes(cursor_line - start_line,
						      visible_lines - (cursor_line - start_line),
						      root_node,cursor_line,cursor_line);
				} else {
					tdu_interface_refresh();
					winsdelln(main_window,-scrolllines);
					display_nodes(cursor_line - start_line,1,
						      root_node,cursor_line,cursor_line);
					display_nodes(visible_lines - scrolllines,scrolllines,
						      root_node,cursor_line + maxlines - scrolllines,cursor_line);
				}
				tdu_interface_refresh();
			}
		}
	}
}

void
tdu_interface_move_up (int n)
{
	cursor_line -= n;
	tdu_interface_refresh();
}

void
tdu_interface_move_down (int n)
{
	cursor_line += n;
	tdu_interface_refresh();
}

void
tdu_interface_move_to (int n)
{
	cursor_line = n;
	tdu_interface_refresh();
}

void
tdu_interface_page_up ()
{
	cursor_line -= (visible_lines - 1);
	tdu_interface_refresh();
}

void
tdu_interface_page_down ()
{
	cursor_line += (visible_lines - 1);
	tdu_interface_refresh();
}

void
tdu_interface_sort (node_sort_fp fp,bool reverse,bool isrecursive)
{
	node_s *n = find_node_numbered(root_node,cursor_line);
	if (n && n->expanded) {

		long lines = n->expanded;
		long maxlines = visible_lines - (cursor_line - start_line) - 1;

		tree_sort(n,fp,reverse,isrecursive);

		if (lines > maxlines) lines = maxlines;

		display_nodes(cursor_line - start_line + 1,
			      lines,
			      root_node,
			      cursor_line + 1,
			      cursor_line);
		tdu_interface_refresh();
	}
}

void
tdu_interface_get_screen_size(int *lines, int *columns)
{
	FILE *tty;
	struct winsize ws;

	tty = fopen("/dev/tty", "r+");
	if (!tty) {
		endwin();
		fprintf(stderr, "oh crap, no tty!\n");
		exit(1);
	}
	if (ioctl(fileno(tty), TIOCGWINSZ, &ws)) {
		endwin();
		fprintf(stderr, "TIOCGWINSZ failed\n");
		exit(1);
	}
	fclose(tty);

	*lines = ws.ws_row;
	*columns = ws.ws_col;
}

void
tdu_interface_compute_visible_lines ()
{
	int bx,by,ex,ey;
	getbegyx(main_window, by, bx);
	getmaxyx(main_window, ey, ex);
	visible_lines = ey - by;
}

void
tdu_interface_resize_handler (int sig)
{
	int lines, columns;
	char number[sizeof(int) * 8 + 1]; /* overkill, I know. I don't care. */

	endwin();
	tdu_interface_get_screen_size(&lines, &columns);
	COLS = columns;
	LINES = lines;

	if (getenv("LINES")) {
		sprintf(number, "%d", lines);
		setenv("LINES", number, 1);
	}
	if (getenv("COLS")) {
		sprintf(number, "%d", columns);
		setenv("COLS", number, 1);
	}
	
	delwin(main_window);
	delwin(status_window);

	werase(tdu_window);
	wresize(tdu_window, lines, columns);
	doupdate();

	main_window = newwin(LINES - 1, COLS, 0, 0);
	status_window = newwin(1, COLS, LINES - 1, 0);

	keypad(main_window, TRUE);
	scrollok(main_window, 1);
	
	tdu_interface_compute_visible_lines();

	prev_start_line = -1;
	if (cursor_line > (start_line + visible_lines - 1)) {
		start_line = cursor_line - (visible_lines - 1);
	}
	
	tdu_interface_display();
}

void
tdu_interface_init_ncurses ()
{
	freopen("/dev/tty", "r", stdin);
  
	tdu_window = initscr();
	keypad(tdu_window, TRUE);
	nonl();
	cbreak();
	noecho();
	scrollok(tdu_window, 1);

	main_window = newwin(LINES - 1, COLS, 0, 0);
	status_window = newwin(1, COLS, LINES - 1, 0);

	keypad(main_window, TRUE);
	scrollok(main_window, 1);
	
	tdu_interface_compute_visible_lines();

	prev_start_line = -1;
	start_line = 0;
	cursor_line = 0;

	tdu_interface_display();
}

void
tdu_interface_keypress (int key)
{
	int sortrecursive;
	static int expandlevel = 0;
	static int lastkey = -1;

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
		exit(0);

	case 16:                    /* C-p */
	case 'K':
	case 'k':
	case KEY_PREVIOUS:
	case KEY_UP:
		tdu_hide_cursor();
		tdu_interface_move_up(1);
		break;

	case 14:                    /* C-n */
	case 'j':
	case 'J':
	case KEY_NEXT:
	case KEY_DOWN:
		tdu_hide_cursor();
		tdu_interface_move_down(1);
		break;

	case '<':
	case KEY_HOME:
		tdu_hide_cursor();
		tdu_interface_move_to(0);
		break;

	case '>':
	case KEY_END:
		tdu_hide_cursor();
		tdu_interface_move_to(root_node->expanded - 1);
		break;

	case KEY_SPREVIOUS:
		tdu_hide_cursor();
		tdu_interface_move_up(10);
		break;

	case KEY_SNEXT:
		tdu_hide_cursor();
		tdu_interface_move_down(10);
		break;

	case KEY_PPAGE:
		tdu_hide_cursor();
		tdu_interface_page_up();
		break;

	case KEY_NPAGE:
		tdu_hide_cursor();
		tdu_interface_page_down();
		break;

	case 'P':
	case 'p':
	{
		node_s *node = find_node_numbered(root_node,cursor_line);
		node_s *parent = node ? node->parent : NULL;
		if (parent) {
			tdu_hide_cursor();
			tdu_interface_move_to(find_node_number_in(parent,root_node));
		}
		break;
	}

	case 'u':
		tdu_interface_sort(node_cmp_unsort,
				   0, sortrecursive); break;
	case 'U':
		tdu_interface_sort(node_cmp_unsort,
				   1, sortrecursive); break;
	case 's':
		tdu_interface_sort(node_cmp_size,
				   0, sortrecursive); break;
	case 'S':
		tdu_interface_sort(node_cmp_size,
				   1, sortrecursive); break;
	case 'n':
		tdu_interface_sort(node_cmp_name,
				   0, sortrecursive); break;
	case 'N':
		tdu_interface_sort(node_cmp_name,
				   1, sortrecursive); break;
	case 'd':
		tdu_interface_sort(node_cmp_descendents,
				   0, sortrecursive); break;
	case 'D':
		tdu_interface_sort(node_cmp_descendents,
				   1, sortrecursive); break;

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
		start_line = cursor_line - visible_lines / 2;
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

void
tdu_interface_run (node_s *node)
{
	int key;

	root_node = node;
	if (root_node->nkids == 1) {
		root_node = root_node->kids[0];
	}

	signal(SIGINT,   tdu_interface_finish);
	signal(SIGWINCH, tdu_interface_resize_handler);

	tdu_interface_init_ncurses();
	tdu_show_cursor();

	while (1) {
		key = wgetch(main_window);
		tdu_interface_keypress(key);
	}
}

