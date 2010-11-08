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

all: opencli README.opencli

version.cpp:
	grace mkversion version.cpp

opencli: $(OBJ)
	$(LD) $(LDFLAGS) -o opencli $(OBJ) $(LIBS) -lgrace-ssl

README.opencli: opencli.1
	groff -t -e -man -Tascii opencli.1 | col -bx > README.opencli

clean:
	rm -f *.o opencli version.cpp

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -g $<
