# need this for Doxygen integration
include $(top_srcdir)/aminclude.am

AM_CPPFLAGS = -I${srcdir}/include -I${srcdir}/generated -I/usr/include -I$(top_srcdir)/include  -I${srcdir}/generated 
AM_CFLAGS = -Wall -Wno-strict-aliasing -g -D_FILE_OFFSET_BITS=64 -DTHREADED -O0 -fPIC -I${srcdir}/include -I${srcdir}/generated
CXXFLAGS = -Wall -g $(USEIPV6)
INCLUDE = -I/usr/local/include/fuse
LIB_LDFLAGS = -no-undefined -version-info 2
LIBS = -pthread -L/usr/local/lib -lfuse -lrt -ldl -lzmq -lczmq
pkginclude_HEADERS = include/zookeeper.h include/zookeeper_version.h include/zookeeper_log.h include/recordio.h generated/zookeeper.jute.h
EXTRA_DIST=LICENSE

HASHTABLE_SRC = src/zk/hashtable/hashtable_itr.h src/zk/hashtable/hashtable_itr.c \
    src/zk/hashtable/hashtable_private.h src/zk/hashtable/hashtable.h src/zk/hashtable/hashtable.c

noinst_LTLIBRARIES = libhashtable.la
libhashtable_la_SOURCES = $(HASHTABLE_SRC)

EXPORT_SYMBOLS = '(zoo_|zookeeper_|zhandle|Z|format_log_message|log_message|logLevel|deallocate_|zerror|is_unrecoverable)'	

COMMON_SRC = src/zk/zookeeper.c include/zookeeper.h include/zookeeper_version.h include/zookeeper_log.h \
    src/zk/recordio.c include/recordio.h include/proto.h \
    src/zk/zk_adaptor.h generated/zookeeper.jute.c \
    src/zk/zookeeper_log.h src/zk/zk_log.c src/zk/zk_hashtable.h src/zk/zk_hashtable.c
 
noinst_LTLIBRARIES += libzkmt.la
libzkmt_la_SOURCES = $(COMMON_SRC) src/zk/mt_adaptor.c
libzkmt_la_CFLAGS = -DTHREADED
libzkmt_la_LIBADD = -lm

lib_LTLIBRARIES = libzookeeper_mt.la
libzookeeper_mt_la_SOURCES =
libzookeeper_mt_la_LIBADD=libzkmt.la libhashtable.la -lpthread
libzookeeper_mt_la_DEPENDENCIES=libzkmt.la libhashtable.la
libzookeeper_mt_la_LDFLAGS = $(LIB_LDFLAGS)

lib_LTLIBRARIES += libzoolock.la
libzoolock_la_SOURCES = src/zk/zoo_lock.c include/zoo_lock.h
libzoolock_la_CPPFLAGS = -DDLOPEN_MODULE
libzoolock_la_LDFLAGS = -version-info 0:1:0

lib_LTLIBRARIES += libwres.la
libwres_la_SOURCES = src/wres/path.h src/wres/path.c src/wres/util.h src/wres/util.c src/wres/proc.h src/wres/proc.c src/wres/line.h src/wres/line.c src/wres/zk.h src/wres/zk.c src/wres/mds.h src/wres/mds.c\
	src/wres/resource.h src/wres/resource.c src/wres/member.h src/wres/member.c src/wres/trace.h src/wres/trace.c src/wres/lock.h src/wres/lock.c src/wres/rwlock.h src/wres/rwlock.c src/wres/prio.h src/wres/prio.c\
	src/wres/stringio.h src/wres/stringio.c src/wres/record.h src/wres/record.c src/wres/cache.h src/wres/cache.c src/wres/tsk.h src/wres/tsk.c \
	src/wres/rbtree.h src/wres/rbtree.c src/wres/event.h src/wres/event.c src/wres/page.h src/wres/page.c src/wres/redo.h src/wres/redo.c\
	src/wres/msg.h src/wres/msg.c src/wres/sem.h src/wres/sem.c src/wres/shm.h src/wres/shm.c
libwres_la_LIBADD=libzookeeper_mt.la
libwres_la_DEPENDENCIES=libzookeeper_mt.la
libwres_la_LDFLAGS = $(LIB_LDFLAGS)

bin_PROGRAMS = wlfs

wlfs_SOURCES = src/wln.c src/wcom.c src/wlnd.c src/wlfs.c
wlfs_LDADD = libzookeeper_mt.la libzoolock.la libwres.la

clean-local:
	$(RM) $(DX_CLEANFILES)
