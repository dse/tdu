/*
 * nowrap.c
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

#include "nowrap.h"
#include <curses.h>
#include <ctype.h>

/* output functions that handle the prevention of line-wrapping and,
   in some cases, avoiding non-printable characters. */

#define BUFFER_SIZE 1024

int
wprintw_nowrap(WINDOW *win, const char *fmt, ...)
{
	va_list ap;
	int y, x, maxy, maxx;
	char buffer[BUFFER_SIZE];
	char *p;		/* through buffer */

	va_start(ap, fmt);	/* looks like i need to do this to pass */
	va_end(ap);		/* arg list to vsnprintf(). */
	vsnprintf(buffer, BUFFER_SIZE, fmt, ap);

	getmaxyx(win, maxy, maxx);
	for (p = buffer; *p; ++p) {
		getyx(win, y, x);
		if (x >= (maxx - 1)) break;
		if (waddch(win, isprint(*p) ? *p : '?') == ERR) return ERR;
	}
	return OK;
}

int
waddch_nowrap(WINDOW *win, const chtype ch)
{
	int y, x, maxy, maxx;
	getmaxyx(win, maxy, maxx);
	getyx(win, y, x);

	if (x >= (maxx - 1)) return ERR;
	return waddch(win, ch);
}

