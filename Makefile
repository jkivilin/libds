# Environment Variables

BINDIR=bin
LIBDIR=lib
INCLUDEDIR=include
SRCDIR=src
TMPDIR=tmp

# Compiler Flags

CFLAGS=-Wall -pedantic -g -std=c99 -O2
CXXFLAGS=-Wall -pedantic -g
LDFLAGS=-Llib -lds -lpthread
INCLUDEFLAGS=-Iinclude
DEBUG=-DDEBUG

# Source Directories

LIBDS=src

TESTDS=test

# Build Paths

all: lib test

# Directories
libdir:
	[ -e $(LIBDIR) ] || mkdir $(LIBDIR)
$(INCLUDEDIR):
	[ -e $(INCLUDEDIR) ] || mkdir $(INCLUDEDIR)
$(TMPDIR):
	[ -e $(TMPDIR) ] || mkdir $(TMPDIR)
$(BINDIR):
	[ -e $(BINDIR) ] || mkdir $(BINDIR)

# Libraries

lib: libds

LIBDS_HEADER=$(LIBDS)/ds.h

LIBDS_OBJS=\
	$(TMPDIR)/ds_linked_list.o \
	$(TMPDIR)/ds_xor_list.o \
	$(TMPDIR)/ds_queue.o \
	$(TMPDIR)/ds_async_queue.o \
	$(TMPDIR)/ds_append_buffer.o \
	$(TMPDIR)/ds_util.o

libds: $(LIBDIR)/libds.a $(INCLUDEDIR)/ds.h

$(LIBDIR)/libds.a: $(LIBDS_OBJS) libdir
	ar rcs $(LIBDIR)/libds.a $(LIBDS_OBJS)

$(INCLUDEDIR)/ds.h: $(LIBDS_HEADER) $(INCLUDEDIR)
	cp $(LIBDS_HEADER) $(INCLUDEDIR)/ds.h

$(TMPDIR)/%.o: $(LIBDS)/%.c $(LIBDS_HEADER) $(INCLUDEDIR)/ds.h $(TMPDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Tests

test: ds_test

ds_test: $(BINDIR)/ds_test

$(BINDIR)/ds_test: $(TMPDIR)/ds_test.o $(LIBDIR)/libds.a $(BINDIR)
	$(CC) $(TMPDIR)/ds_test.o -o $(BINDIR)/ds_test $(LDFLAGS)

$(TMPDIR)/ds_test.o: $(TESTDS)/ds_test.c $(INCLUDEDIR)/ds.h $(TMPDIR)
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(TMPDIR)
	rm -rf $(INCLUDEDIR)
	rm -rf $(LIBDIR)
	rm -rf $(BINDIR)
