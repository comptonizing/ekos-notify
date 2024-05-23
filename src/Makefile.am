bin_PROGRAMS=ekosnotify

gui.h: ekosnotify.glade Makefile
	echo -e "#include <iostream>\nconst std::string __guiData = R\"(" > $@
	cat $< >> $@
	echo ")\";" >> $@

ekosnotify_DEPENDENCIES=gui.h

ekosnotify_SOURCES=gui.h main.cpp frame.h frame.cpp
