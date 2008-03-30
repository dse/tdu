# tdu - a text-mode disk usage visualization utility
# Copyright (C) 2004-2008 Darren Stuart Embry.  
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307$

.PHONY: all
all: tdu

SRCS = tdu.c node.c tduint.c nowrap.c
HDRS = tduproto.h
program = tdu
OBJS = $(SRCS:.c=.o)
LIBS = -lncurses

$(program): $(OBJS)
	$(CC) `pkg-config --libs glib-2.0` $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.c.o:
	$(CC) `pkg-config --cflags glib-2.0` -Wall $(CFLAGS) -c $< -o $@

.PHONY: splint
splint:
	splint -weak -posix-lib `pkg-config --cflags glib-2.0` *.c *.h

install = /usr/bin/install
prefix = /usr/local

.PHONY: install
install: $(program)
	$(install) -d -m 0755 $(prefix)/bin
	$(install) -d -m 0755 $(prefix)/share/man/man1
	$(install) -m 0755 $(program) $(prefix)/bin
	$(install) -m 0644 tdu.1   $(prefix)/share/man/man1

.PHONY: tar
tar:
	tar cvvzf tdu-`date +%Y-%m-%d`.tar.gz \
		--transform='s:^\.:tdu:' \
		--show-transformed-names \
		* \
		--exclude=MyMakefile \
		--exclude=.svn \
		--exclude=test \
		--exclude=CVS \
                --exclude='#*#' \
                --exclude='.#*' \
                --exclude='.cvsignore' \
                --exclude='*~' \
                --exclude='.*~' \
		--exclude='tdu' \
		--exclude='*.o' \
		--exclude='*.d' \
		--exclude='.deps' \
		--exclude='*.1.*' \
		--exclude=tdu \
		--exclude=TAGS \
		--exclude='*.tar.gz' \
		--exclude='*.tmp'

%.1.txt: %.1 Makefile
	nroff -man $< | col -bx > $@.
	mv $@. $@
%.1.ps: %.1 Makefile
	groff -Tps -man $< >$@.
	mv $@. $@
%.1.html: %.1 Makefile
	groff -Thtml -man $< >$@.
	mv $@. $@

.PHONY: clean
clean:
	/bin/rm -fr $(program).1.* *.o *.d *~ core $(program) || true

%.d: %.c
	@ >&2 echo Fixing dependencies for $< . . .
	@ $(CC) -MM $(CPPFLAGS) $< | sed 's/^$*\.o/& $@/g' > $@ || true

-include $(SRCS:.c=.d)

-include MyMakefile

