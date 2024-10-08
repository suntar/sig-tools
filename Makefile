all:
	make -C sig_filter
	make -C sig_viewer

#dir=/usr$(subst ${prefix},,${libdir})/tcl

name=SigLoad
ver=1.0

DESTDIR    ?=
prefix     ?= $(DESTDIR)/usr
bindir     ?= $(DESTDIR)/usr/bin
datadir    ?= $(DESTDIR)/usr/share
libdir     ?= $(DESTDIR)/usr/lib64
sysconfdir ?= $(DESTDIR)/etc
initdir    ?= $(DESTDIR)/etc/init.d
tcldatadir ?= $(DESTDIR)/usr/share/tcl
man1dir    ?= $(DESTDIR)/usr/share/man/man1
srvdir     ?= $(DESTDIR)/usr/lib/systemd/system

bindir ?= /usr/bin

install: all
	mkdir -p ${bindir} ${libdir}/tcl/ ${tcldatadir}/${name}-${ver}
	install -m755 sig_filter/sig_filter sig_filter/sig_pnmtopng sig_filter/sig_pnginfo ${bindir}
	install -m755 sig_pngfig/sig_pngfig ${bindir}
	install -m644 sig_viewer/sig_load.so ${libdir}/tcl/
	sed 's|%LIB_DIR%|${dir}|' sig_viewer/pkgIndex.tcl > ${datadir}/tcl/${name}-${ver}/pkgIndex.tcl
	sed 's|^load ./sig_load.so|package require SigLoad|' sig_viewer/sig_viewer > ${bindir}/sig_viewer
	chmod 755 ${bindir}/sig_viewer

#



