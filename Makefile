# Environment Variables

BINDIR=bin
LIBDIR=lib
INCLUDEDIR=include
SRCDIR=src
TMPDIR=tmp

# Compiler Flags

CFLAGS=-Wall -pedantic -g -std=c99 -O2 -fstack-protector -D_FORTIFY_SOURCE=2 -D_FILE_OFFSET_BITS=64
CXXFLAGS=-Wall -pedantic -g
LDFLAGS=-Llib -lio -lds -lzlib -lpthread -lm
INCLUDEFLAGS=-Iinclude
DEBUG=-DDEBUG

# Source Directories

LIBDS=src/ds
LIBIO=src/io
LIBZ=src/zlib

TESTDS=test
TESTIO=test

# Build Paths

all: lib test

# Directories
libdir:
	[ -e $(LIBDIR) ] || mkdir $(LIBDIR)
$(INCLUDEDIR):
	[ -e $(INCLUDEDIR) ] || mkdir $(INCLUDEDIR)
$(TMPDIR):
	[ -e $(TMPDIR) ] || mkdir $(TMPDIR)
$(BINDIR):
	[ -e $(BINDIR) ] || mkdir $(BINDIR)

# Libraries

lib: libds libzlib libio

LIBDS_HEADER=$(LIBDS)/ds.h

LIBIO_HEADER=$(LIBIO)/io.h

LIBZ_HEADER=$(LIBZ)/priv_zlib.h

LIBDS_OBJS=\
	$(TMPDIR)/ds_linked_list.o \
	$(TMPDIR)/ds_xor_list.o \
	$(TMPDIR)/ds_queue.o \
	$(TMPDIR)/ds_async_queue.o \
	$(TMPDIR)/ds_append_buffer.o \
	$(TMPDIR)/ds_util.o

LIBIO_OBJS=\
	$(TMPDIR)/io_parser.o \
	$(TMPDIR)/io_parser_gz.o \
	$(TMPDIR)/io_parser_text.o \
	$(TMPDIR)/io_input.o \
	$(TMPDIR)/io_input_external.o \
	$(TMPDIR)/io_input_file.o \
	$(TMPDIR)/io_input_generic_fd.o \
	$(TMPDIR)/io_main.o \
	$(TMPDIR)/io_save_file.o \
	$(TMPDIR)/io_util.o

LIBZ_OBJS=\
	$(TMPDIR)/z_adler32.o \
	$(TMPDIR)/z_crc32.o \
	$(TMPDIR)/z_deflate.o \
	$(TMPDIR)/z_gzlib.o \
	$(TMPDIR)/z_gzwrite.o \
	$(TMPDIR)/z_inffast.o \
	$(TMPDIR)/z_inflate.o \
	$(TMPDIR)/z_inftrees.o \
	$(TMPDIR)/z_trees.o \
	$(TMPDIR)/z_zutil.o

libds: $(LIBDIR)/libds.a $(INCLUDEDIR)/ds.h

$(LIBDIR)/libds.a: $(LIBDS_OBJS) libdir
	ar rcs $(LIBDIR)/libds.a $(LIBDS_OBJS)

$(INCLUDEDIR)/ds.h: $(LIBDS_HEADER) $(INCLUDEDIR)
	cp $(LIBDS_HEADER) $(INCLUDEDIR)/ds.h

$(TMPDIR)/%.o: $(LIBDS)/%.c $(LIBDS_HEADER) $(INCLUDEDIR)/ds.h $(TMPDIR)
	$(CC) $(CFLAGS) -c $< -o $@

libio: $(LIBDIR)/libio.a $(INCLUDEDIR)/io.h $(LIBDS_HEADER)

$(LIBDIR)/libio.a: $(LIBIO_OBJS) $(LIBDS_HEADER) libdir
	ar rcs $(LIBDIR)/libio.a $(LIBIO_OBJS)

$(INCLUDEDIR)/io.h: $(LIBIO_HEADER)
	cp $(LIBIO_HEADER) $(INCLUDEDIR)/io.h

$(TMPDIR)/%.o: $(LIBIO)/%.c $(LIBIO_HEADER) $(LIBDS_HEADER) \
	$(INCLUDEDIR)/priv_zlib.h $(INCLUDEDIR)/ds.h $(INCLUDEDIR)/io.h
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) -c $< -o $@

libzlib: $(LIBDIR)/libzlib.a $(INCLUDEDIR)/priv_zlib.h $(LIBZ_HEADER)

$(LIBDIR)/libzlib.a: $(LIBZ_OBJS) $(LIBZ_HEADER) libdir
	ar rcs $(LIBDIR)/libzlib.a $(LIBZ_OBJS)

$(INCLUDEDIR)/zconf.h: $(LIBZ)/zconf.h
	cp $< $(INCLUDEDIR)/zconf.h

$(INCLUDEDIR)/priv_zlib.h: $(LIBZ)/priv_zlib.h $(INCLUDEDIR)/zconf.h
	cp $< $(INCLUDEDIR)/priv_zlib.h

$(TMPDIR)/z_%.o: $(LIBZ)/%.c $(LIBZ_HEADER) $(INCLUDEDIR)/zconf.h \
	$(INCLUDEDIR)/priv_zlib.h
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) -c $< -o $@

# Tests

test: ds_test io_test

ds_test: $(BINDIR)/ds_test

$(BINDIR)/ds_test: $(TMPDIR)/ds_test.o libds $(BINDIR)
	$(CC) $(TMPDIR)/ds_test.o -o $(BINDIR)/ds_test $(LDFLAGS)

$(TMPDIR)/ds_test.o: $(TESTDS)/ds_test.c $(INCLUDEDIR)/ds.h $(TMPDIR)
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) -c $< -o $@

io_test: $(BINDIR)/io_test

$(BINDIR)/io_test: $(TMPDIR)/io_test.o libds libio libzlib $(BINDIR)
	$(CC) $(TMPDIR)/io_test.o -o $(BINDIR)/io_test $(LDFLAGS)

$(TMPDIR)/io_test.o: $(TESTIO)/io_test.c $(INCLUDEDIR)/io.h
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(TMPDIR)
	rm -rf $(INCLUDEDIR)
	rm -rf $(LIBDIR)
	rm -rf $(BINDIR)
