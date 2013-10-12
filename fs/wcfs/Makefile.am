# need this for Doxygen integration
AM_CPPFLAGS = -I${srcdir}/include -I${srcdir}/generated -I/usr/include -I$(top_srcdir)/include  -I${srcdir}/generated 
AM_CFLAGS = -Wall -Wno-strict-aliasing -g -D_FILE_OFFSET_BITS=64 -DTHREADED -O0 -fPIC -I${srcdir}/include -I${srcdir}/generated
CXXFLAGS = -Wall -g $(USEIPV6)
INCLUDE = -I/usr/local/include/fuse
LIBS = -pthread -L/usr/local/lib -lfuse
EXTRA_DIST=LICENSE

bin_PROGRAMS = wcfs

wcfs_SOURCES = src/wcfs.c src/util.c src/file.c src/path.c

clean-local:
	$(RM) $(DX_CLEANFILES)
