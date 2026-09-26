SUBDIRS = docs include libs clients test server

ACLOCAL_AMFLAGS = -I m4

pkglibexecdir = $(pkglibdir)
pkgsysconfdir = $(sysconfdir)/hedgeytty
pkgdatadir = $(datadir)/hedgeytty

pkgdata_DATA      = BUGS COPYING COPYING.LIB Changelog.txt INSTALL README.md README.twin.md twin-current.lsm
dist_pkgsysconf_DATA = htenvrc.sh hedgeyttyrc
dist_pkgdata_DATA = assets/hitomi-icon.png

install-exec-hook:
	@if echo "$(host_triplet)" | $(GREP) linux >/dev/null 2>&1 ; then \
	  CMD_LDCONFIG="ldconfig" ; \
	elif echo "$(host_triplet)" | $(GREP) freebsd >/dev/null 2>&1 ; then \
	  CMD_LDCONFIG="ldconfig -R" ; \
	else \
	  CMD_LDCONFIG="" ; \
	fi; \
	if test "x$$CMD_LDCONFIG" != x ; then \
	  if id 2>/dev/null | $(EGREP) "^uid=0" >/dev/null 2>&1 && test "x$(DESTDIR)" = x && test "x$(host_triplet)" = "x$(build_triplet)" ; then \
	    echo " $$CMD_LDCONFIG" ; \
	    $$CMD_LDCONFIG ; \
	  else \
	    echo "NOTE: run ldconfig after install if libraries are not found" ; \
	  fi; \
	fi
