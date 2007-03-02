lib_LIBRARIES	= libqmt.a
check_PROGRAMS	= pthread_sync
TESTS		= $(check_PROGRAMS)
include_HEADERS	= qmt.h qmt_atomic.h qmt_lock.h
libqmt_a_SOURCES	= qmt.h qmt_atomic.c qmt_atomic.h \
			qmt_lock.c qmt_lock.h \
			qmt_pthread.c qmt_pthread.h

if QMT_BUILD_32BIT
libqmt_a_SOURCES	+= qmt_lock_impl_i386.S
EXTRA_DIST		= qmt_lock_impl_x86_64.S
else
EXTRA_DIST		= qmt_lock_impl_i386.S qmt_lock_impl_x86_64.S
endif

AM_CFLAGS	= @BARRIER_DEF@ @RELEASE_DEF@ @CPU_DEF@

pthread_sync_SOURCES	= pthread_sync.c
pthread_sync_LDADD	= libqmt.a -lpthread -lm

qmt_lock_impl_i386.o: qmt_lock_impl_i386.S
	rm -f $@
	$(CC) -traditional -c $< -o $@
