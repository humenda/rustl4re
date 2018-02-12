## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-isystem $(top_srcdir)/include/bsd/ \
	-include $(top_builddir)/config.h \
	-DLIBBSD_OVERLAY -DLIBBSD_DISABLE_DEPRECATED \
	-D__REENTRANT

libbsd_la_included_sources = \
	hash/helper.c \
	getentropy_aix.c \
	getentropy_bsd.c \
	getentropy_hpux.c \
	getentropy_hurd.c \
	getentropy_linux.c \
	getentropy_osx.c \
	getentropy_solaris.c \
	$(nil)

EXTRA_DIST = \
	libbsd.map \
	libbsd.pc.in \
	libbsd-ctor.pc.in \
	libbsd-overlay.pc.in \
	$(libbsd_la_included_sources) \
	$(nil)

CLEANFILES = \
	hash/md5hl.c \
	$(nil)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	libbsd.pc \
	libbsd-overlay.pc \
	$(nil)

lib_LTLIBRARIES = libbsd.la
lib_LIBRARIES =

if BUILD_LIBBSD_CTOR
pkgconfig_DATA += libbsd-ctor.pc

lib_LIBRARIES += libbsd-ctor.a
endif

hash/md5hl.c: $(srcdir)/hash/helper.c
	$(AM_V_at) $(MKDIR_P) hash
	$(AM_V_GEN) sed -e 's:hashinc:md5.h:g' -e 's:HASH:MD5:g' $< > $@

libbsd_la_DEPENDENCIES = \
	$(libbsd_la_included_sources) \
	libbsd.map
libbsd_la_LIBADD = \
	$(CLOCK_GETTIME_LIBS)
libbsd_la_LDFLAGS = \
	-Wl,--version-script=$(srcdir)/libbsd.map \
	-version-number $(LIBBSD_ABI)
libbsd_la_SOURCES = \
	arc4random.c \
	arc4random.h \
	arc4random_unix.h \
	arc4random_openbsd.h \
	arc4random_uniform.c \
	bsd_getopt.c \
	chacha_private.h \
	closefrom.c \
	dehumanize_number.c \
	err.c \
	expand_number.c \
	explicit_bzero.c \
	fgetln.c \
	fgetwln.c \
	flopen.c \
	fmtcheck.c \
	fparseln.c \
	fpurge.c \
	funopen.c \
	getbsize.c \
	getpeereid.c \
	hash/md5.c \
	hash/md5hl.c \
	hash/sha512.h \
	hash/sha512c.c \
	heapsort.c \
	humanize_number.c \
	inet_net_pton.c \
	local-elf.h \
	local-link.h \
	merge.c \
	nlist.c \
	pidfile.c \
	progname.c \
	radixsort.c \
	readpassphrase.c \
	reallocarray.c \
	reallocf.c \
	setmode.c \
	setproctitle.c \
	strlcat.c \
	strlcpy.c \
	stringlist.c \
	strmode.c \
	strnstr.c \
	strtonum.c \
	timeconv.c \
	unvis.c \
	vis.c \
	wcslcat.c \
	wcslcpy.c \
	$(nil)

if !HAVE_GETENTROPY
libbsd_la_SOURCES += \
	getentropy.c \
	$(nil)
endif

libbsd_ctor_a_SOURCES = \
	setproctitle_ctor.c \
	$(nil)

runtimelibdir = $(libdir)

install-exec-hook:
	if [ "$(libdir)" != "$(runtimelibdir)" ]; then \
		$(MKDIR_P) $(DESTDIR)$(runtimelibdir); \
		mv $(DESTDIR)$(libdir)/libbsd*.so.* \
		   $(DESTDIR)$(runtimelibdir)/; \
		soname=`readlink $(DESTDIR)$(libdir)/libbsd.so`; \
		sorelprefix=`echo $(libdir) | sed -r -e 's:(^/)?[^/]+:..:g'`; \
		ln -sf $$sorelprefix$(runtimelibdir)/$$soname \
		       $(DESTDIR)$(libdir)/libbsd.so; \
	fi

uninstall-hook:
	rm -f $(DESTDIR)$(runtimelibdir)/libbsd*.so*
