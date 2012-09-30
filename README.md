# tdu

tdu is a text-terminal program that displays disk space utilization in
an interactive full-screen folding outline. Branches of the tree may
be hidden or displayed, and files/directories in a branch can be
sorted by filename or total space utilized.

tdu uses the ncurses library to interact with the terminal; unlike
most disk usage visualization programs, it does not require the X
Window System. It is therefore ideal in situations where you're logged
in to a remote Unix system and for various reasons you do not wish to
run an X program such as xdu remotely or copy the output of du over
the network.

Like the classic xdu utility, tdu does not traverse a directory tree
computing disk usage itself; it requires a program such as du to
provide that information.

# More Information

The web site: http://webonastick.com/tdu/

# Source Code

The github: https://github.com/dse/tdu/

# See Also

tdu, a du(1) output filter program that groups files together:
- http://webonastick.com/dugroup/
- https://github.com/dse/dugroup/

