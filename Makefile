# This file is part of OpenPanel - The Open Source Control Panel
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/


include makeinclude

OBJ	= main.o sessionproxy.o textformat.o version.o

all: openpanel-cli README.openpanel-cli

version.cpp:
	grace mkversion version.cpp

openpanel-cli: $(OBJ)
	$(LD) $(LDFLAGS) -o openpanel-cli $(OBJ) $(LIBS) -lgrace-ssl

README.openpanel-cli: openpanel-cli.1
	groff -t -e -man -Tascii openpanel-cli.1 | col -bx > README.openpanel-cli

install:
	mkdir -p ${DESTDIR}/usr/bin/
	mkdir -p ${DESTDIR}/usr/share/man/man1
	install -m 755 openpanel-cli ${DESTDIR}/usr/bin/openpanel-cli
	gzip -c < openpanel-cli.1 > ${DESTDIR}/usr/share/man/man1/openpanel-cli.1.gz

clean:
	rm -f *.o openpanel-cli version.cpp

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -g $<
