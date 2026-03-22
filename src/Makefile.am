bin_PROGRAMS=ekosnotify

gui.h: ekosnotify.glade Makefile
	echo -e "#include <iostream>\nconst std::string __guiData = R\"(" > $@
	cat $< >> $@
	echo ")\";" >> $@

settings.h: settings.json
	python -m json.tool $< >/dev/null
	echo -e "#include <iostream>\nconst std::string __defaultSettingsData = R\"(" > $@
	cat $< >> $@
	echo ")\";" >> $@

ekosnotify_DEPENDENCIES=gui.h settings.h

ekosnotify_SOURCES=gui.h settings.h main.cpp frame.h frame.cpp

CLEANFILES=gui.h settings.h
