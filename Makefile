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
