/*
 * tdu.h
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

#ifndef TDU_H
#define TDU_H
/*****************************************************************************/

#define TDU_VERSION "2008-03-28"

#define TDU_COPYRIGHT_INFO \
"This is tdu version " TDU_VERSION ".\n" \
"Copyright (C) 2004-2008 Darren Stuart Embry.\n" \
"\n" \
"This program comes with ABSOLUTELY NO WARRANTY.  This is free software,\n" \
"and you are welcome to redistribute it under the terms of the GNU General\n" \
"Public License, Verison 2, available here:\n" \
"\n" \
"http://dse.webonastick.com/tdu/COPYING.txt\n"

int debug (const char *fmt, ...);

/*****************************************************************************/
#endif /* TDU_H */
