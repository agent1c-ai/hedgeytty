SUBDIRS = hw

bin_PROGRAMS       = htdisplay hedgeytty hedgeytty_server
bin_SCRIPTS        = htstart
pkglib_LTLIBRARIES =

if LIBRCPARSE_la
  pkglib_LTLIBRARIES += librcparse.la
endif
if LIBSOCKET_la
  pkglib_LTLIBRARIES += libsocket.la
endif

AM_CPPFLAGS                = -I$(top_srcdir)/include $(LTDLINCL) -DPLUGINDIR="\"$(pkglibdir)\"" -DCONFDIR="\"$(sysconfdir)/hedgeytty\""
hedgeytty_CPPFLAGS         = -I$(top_srcdir)/include $(LTDLINCL) -DBINDIR="\"$(bindir)\""

htdisplay_SOURCES          = alloc.cpp display.cpp dl_helper.cpp hw.cpp log.cpp missing.cpp palette.cpp
hedgeytty_SOURCES          = wrapper.c
hedgeytty_server_SOURCES   = alloc.cpp builtin.cpp data.cpp dl.cpp dl_helper.cpp draw.cpp extreg.cpp \
                        hw.cpp hw_multi.cpp log.cpp main.cpp methods.cpp missing.cpp palette.cpp \
                        obj/all.cpp obj/display.cpp obj/gadget.cpp obj/group.cpp \
                        obj/id.cpp obj/menu.cpp obj/menuitem.cpp obj/module.cpp obj/msg.cpp obj/msgport.cpp \
                        obj/mutex.cpp obj/obj.cpp obj/row.cpp obj/screen.cpp obj/widget.cpp obj/window.cpp \
                        printk.cpp privilege.cpp pty.cpp rcopt.cpp rcrun.cpp remote.cpp resize.cpp shm.cpp scroller.cpp \
                        tterm.cpp tty.cpp util.cpp wm.cpp

librcparse_la_SOURCES = rcparse_tab.cpp rcparse_lex.cpp
libsocket_la_SOURCES  = md5.cpp socket.cpp

htdisplay_LDFLAGS     = -export-dynamic $(LDFLAGS_BIN_EXPORT_DYNAMIC)
hedgeytty_server_LDFLAGS   = -export-dynamic $(LDFLAGS_BIN_EXPORT_DYNAMIC)

librcparse_la_LDFLAGS = -export-dynamic -export-symbols-regex '^(Init|Quit)Module$$' -release $(PACKAGE_VERSION_MAJOR).$(PACKAGE_VERSION_MINOR).$(PACKAGE_VERSION_PATCH)
libsocket_la_LDFLAGS  = -export-dynamic -export-symbols-regex '^(Init|Quit)Module$$' -release $(PACKAGE_VERSION_MAJOR).$(PACKAGE_VERSION_MINOR).$(PACKAGE_VERSION_PATCH)

htdisplay_LDADD       = $(LIBHT) $(LIBTUTF) $(LIBDL) $(LIBTSTL)
hedgeytty_server_LDADD     =          $(LIBTUTF) $(LIBDL) $(LIBTSTL)

librcparse_la_LIBADD  = $(LIBTUTF) $(LIBTSTL)
libsocket_la_LIBADD   = $(LIBTUTF) $(LIBTSTL) $(LIBSOCK) $(LIBZ)
