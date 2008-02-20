/*
 * group.c
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
#include "group.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*****************************************************************************/

groups_s *
new_groups (void) 
{
  groups_s *gs = (groups_s *)malloc(sizeof(groups_s));
  if(gs) {
    gs->group = NULL;
    gs->ngroups = 0;
    gs->nblocks = 0;
  }
  return gs;
}

group_s *
new_group (const char *name) 
{
  group_s *g = (group_s *)malloc(sizeof(group_s));
  if(g) {
    g->name = (name != NULL) ? strdup(name) : NULL;
    g->wc = NULL;
    g->nwcs = 0;
    g->nblocks = 0;
  }
  return g;
}

group_s *
find_group (groups_s *groups, const char *name) 
{
  int i;
  if (groups && name) {
    for (i = 0; i < groups->ngroups; ++i) {
      if (!strcmp(groups->group[i]->name,name))
	return groups->group[i];
    }
  }
  return NULL;
}

void
add_group (groups_s *groups, group_s *group) 
{
  if (groups && group) {
    if (groups->ngroups >= groups->nblocks * KIDSATATIME) {
      groups->group = (group_s **)realloc(groups->group,
					  ++groups->nblocks
					  * KIDSATATIME
					  * sizeof(group_s *));
    }
    groups->group[groups->ngroups++] = group;
  }
}

void
add_wc (group_s *group, const char *wc)
{
  if (group && wc) {
    if (group->nwcs >= group->nblocks * KIDSATATIME) {
      group->wc = (char **)realloc(group->wc,
				   ++group->nblocks
				   * KIDSATATIME
				   * sizeof(char *));
    }
    group->wc[group->nwcs++] = strdup(wc);
  }
}

group_s *
find_or_create_group (groups_s *groups, const char *name)
{
  if (groups && name) {
    group_s *g;
    if ((g = find_group(groups,name)) != NULL)
      return g;
    if ((g = new_group(name)) != NULL) {
      add_group(groups,g);
      return g;
    }
  }
  return NULL;
}

int
pattern_matches (const char *s, const char *p) 
{
  while (1) {
    if (*p == '?') {
      /* match *one* character */
      if (*s == '\0') break;	/* ? does not match at EOS */
      ++s; ++p; continue;
    }
    if (*p == '*') {
      /* match zero or more characters: string or any suffix matches
	 portion of pattern remaining after asterisk */
      for (++p; *p == '*'; ++p); /* skip duplicate asterisks */
      for (; *s; ++s) if (pattern_matches(s,p)) return 1;
      return pattern_matches(s,p);
    }
    if (*p == *s) {
      /* both characters match? */
      if (*p == '\0') return 1;	/* stop: success */
      ++s; ++p; continue;
    }
    break;
  }
  return 0;
}

int
group_matches (group_s *group, const char *name)
{
  if (group && name) {
    int i;
    for (i = 0; i < group->nwcs; ++i) {
      if (pattern_matches(name,group->wc[i])) return 1;
    }
  }
  return 0;
}

group_s *
find_matching_group (groups_s *groups, const char *name)
{
  if (groups && name) {
    int i;
    for (i = 0; i < groups->ngroups; ++i) {
      if (group_matches(groups->group[i],name))
	return groups->group[i];
    }
  }
  return NULL;
}

void
dump_group (group_s *group)
{
  if (group) {
    int i;
    printf("group \"%s\"\n",group->name);
    for (i = 0; i < group->nwcs; ++i) {
      printf("  pattern \"%s\"\n",group->wc[i]);
    }
  }
}

void
dump_groups (groups_s *groups)
{
  if (groups) {
    int i;
    for (i = 0; i < groups->ngroups; ++i) {
      dump_group(groups->group[i]);
    }
  }
}

