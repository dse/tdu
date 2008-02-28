/*
 * group.h
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

#ifndef GROUP_H
#define GROUP_H
/*****************************************************************************/

typedef struct groups {
	struct group **group;
	long ngroups;
	long nblocks;
} groups_s;

typedef struct group {
	char *name;
	char **wc;
	long nwcs;
	long nblocks;
} group_s;

/* group.c */
groups_s *new_groups(void);
group_s *new_group(const char *name);
group_s *find_group(groups_s *groups, const char *name);
void add_group(groups_s *groups, group_s *group);
void add_wc(group_s *group, const char *wc);
group_s *find_or_create_group(groups_s *groups, const char *name);
int pattern_matches(const char *s, const char *p);
int group_matches(group_s *group, const char *name);
group_s *find_matching_group(groups_s *groups, const char *name);
void dump_group(group_s *group);
void dump_groups(groups_s *groups);

/*****************************************************************************/
#endif /* GROUP_H */
