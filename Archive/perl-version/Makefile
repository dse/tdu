.PHONY: install stow-install clean test-for-perl-modules

program = tdu
prefix  = /usr/local
stow    = $(prefix)/stow/$(program)

stow-install:
	mkdir -p -m 0755 $(stow)
	make install prefix=$(stow)
	(cd "`dirname $(stow)`" && stow $(program))

install: test-for-perl-modules $(program).1
	install -d -m 0755           $(prefix)/bin
	install -d -m 0755           $(prefix)/share/man/man1
	install -m 0755 $(program)   $(prefix)/bin
	install -m 0644 $(program).1 $(prefix)/share/man/man1

clean:
	/bin/rm *~ $(program).1

# generate man page(s)
%.1: %.pod Makefile
	pod2man --center=' ' $< $@

test-for-perl-modules:
	@if perl -mCurses /dev/null 2>/dev/null ; then true ;\
	else >&2 echo "Please install the Curses module and try again.";\
	fi

-include MyMakefile

