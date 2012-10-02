# tdu - a text-mode disk usage visualization utility
# Copyright (C) 2012 Darren Embry.  
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

SRCS = tdu.c node.c tduint.c nowrap.c
# HDRS = node.h nowrap.h tdu.h tduint.h
program = tdu
OBJS = $(SRCS:.c=.o)
PKGCONFIG_PKGS = ncurses glib-2.0
manpage = $(program).1

VERSION = $(shell sed -n '/^\#define[ 	][ 	]*TDU_VERSION[ 	][ 	]*"/{s/^[^"]*"//;s/".*$$//;p;}' tdu.h)
DEBPKGS = pkg-config libglib2.0-dev libncurses5-dev gcc

###############################################################################

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o
ifdef manpage
.SUFFIXES: .1 .1.txt .1.ps .1.html .ps .pdf
endif

EXTRA_CFLAGS =
EXTRA_LIBS =

# on legacy systems (old version of Debian) where libncurses5-dev
# isn't nicely all pkg-config'ed up.
ifeq (0,$(shell pkg-config --cflags ncurses >/dev/null 2>/dev/null))
else
  PKGCONFIG_PKGS = glib-2.0
  EXTRA_CFLAGS = 
  EXTRA_LIBS   = -lncurses
endif

PKGCONFIG_CFLAGS = `pkg-config --cflags $(PKGCONFIG_PKGS)`
PKGCONFIG_LIBS   = `pkg-config --libs   $(PKGCONFIG_PKGS)`

# Common prefix for installation directories.
# NOTE: This directory must exist when you start the install.
prefix = /usr/local
datarootdir = $(prefix)/share
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
mandir = $(datarootdir)/man

DESTDIR =

INSTALL = /usr/bin/install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA    = $(INSTALL) -m 0644
INSTALL_MKDIR   = $(INSTALL) -d

.PHONY: all
all: $(program)

$(program): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(PKGCONFIG_LIBS) $(EXTRA_LIBS) $(CFLAGS)

%.o: %.c
	$(CC) -c $(CPPFLAGS) $(PKGCONFIG_CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS) $<

.PHONY: install
install: $(program)
	$(INSTALL_MKDIR) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) $(program) $(DESTDIR)$(bindir)
    ifdef manpage
	$(INSTALL_MKDIR) $(DESTDIR)$(mandir)/man1
	$(INSTALL_DATA) $(manpage) $(DESTDIR)$(mandir)/man1
    endif

.PHONY: uninstall
uninstall:
	rm $(DESTDIR)$(bindir)/$(program)
    ifdef manpage
	rm $(DESTDIR)$(mandir)/man1/$(manpage)
    endif

DIST = $(program)-$(VERSION).tar.gz

.PHONY: dist
dist:
	tar cvvzf $(DIST) \
		--transform='s:^:$(program)-$(VERSION)/:' \
		--show-transformed-names \
		* \
		--exclude=MyMakefile \
		--exclude=CVS \
		--exclude=.svn \
		--exclude=.git \
		--exclude=test \
		--exclude='*.rej' \
                --exclude='#*#' \
                --exclude='.#*' \
                --exclude='.cvsignore' \
                --exclude='.svnignore' \
                --exclude='.gitignore' \
                --exclude='*~' \
                --exclude='.*~' \
		--exclude='.deps' \
		--exclude='*.1.*' \
		--exclude=$(program) \
		--exclude=TAGS \
		--exclude='*.o' \
		--exclude='*.d' \
		--exclude='*.tar.gz' \
		--exclude='*.tmp.*' \
		--exclude='*.tmp' \
		--exclude='*.log'

.PHONY: clean
clean:
	2>/dev/null rm $(program) *.d *.o '#'*'#' .'#'* *~ .*~ *.bak core || true

.PHONY: version
version:
	@echo $(VERSION)

###############################################################################
# Dependencies

.SUFFIXES: .d
%.d: %.c
	@ >&2 echo Fixing dependencies for $< . . .
	$(CC) -MM $(CPPFLAGS) $(PKGCONFIG_CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS) $< | \
		sed 's/^$*\.o/& $@/g' > $@ || true

-include $(SRCS:.c=.d)

###############################################################################
# Documentation

%.1.txt: %.1 Makefile
	nroff -man $< | col -bx > $@.tmp
	mv $@.tmp $@
%.1.ps: %.1 Makefile
	groff -Tps -man $< >$@.tmp
	mv $@.tmp $@
%.1.html: %.1 Makefile
	groff -Thtml -man $< >$@.tmp
	mv $@.tmp $@
%.pdf: %.ps Makefile
	ps2pdf $< $@.tmp.pdf
	mv $@.tmp.pdf $@

###############################################################################
# Debian

.PHONY: install-debian-prerequisites
install-debian-prerequisites:
	sudo apt-get install $(DEBPKGS)

###############################################################################
# My makefile customizations about which you need not be concerned :-)

-include MyMakefile

