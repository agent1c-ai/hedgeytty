SUBDIRS = mapscrn

bin_PROGRAMS  = htattach htcat htclip htclutter htcuckoo htdialog htevent htfindtwin htlsmsgport htlsobj htsendmsg htsetroot htsysmon htterm htthreadtest hedgeytty_hitomi_bg htdock
sbin_PROGRAMS = htdm

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/dock -DDATADIR="\"$(datadir)/hedgeytty\"" -DBINDIR="\"$(bindir)\""

htattach_SOURCES     = attach.c
htcat_SOURCES        = cat.c
htclip_SOURCES       = clip.c
htclutter_SOURCES    = clutter.c
htcuckoo_SOURCES     = cuckoo.c
htdialog_SOURCES     = dialog.c
htdm_SOURCES         = dm.c
htevent_SOURCES      = event.c util.c
htfindtwin_SOURCES   = findtwin.c
htlsmsgport_SOURCES  = lsmsgport.c
htlsobj_SOURCES      = lsobj.c
htsendmsg_SOURCES    = sendmsg.c
htsetroot_SOURCES    = setroot.c
htsysmon_SOURCES     = sysmon.c util.c
htterm_SOURCES       = pty.c term.c util.c
htthreadtest_SOURCES = threadtest.c
hedgeytty_hitomi_bg_SOURCES = hitomi-bg.c
htdock_SOURCES = dock/main.c dock/dock_core.c dock/dock_state.c dock/dock_render.c \
	dock/dock_input.c dock/dock_app.c

htattach_LDADD     = $(LIBHT)
htcat_LDADD        = $(LIBHT)
htclip_LDADD       = $(LIBHT)
htclutter_LDADD    = $(LIBHT)
htcuckoo_LDADD     = $(LIBHT)
htdialog_LDADD     = $(LIBHT)
htdm_LDADD         = $(LIBHT) $(LIBTUTF) $(LIBCRYPT)
htevent_LDADD      = $(LIBHT)
htfindtwin_LDADD   = $(LIBHT)
htlsmsgport_LDADD  = $(LIBHT)
htlsobj_LDADD      = $(LIBHT)
htsendmsg_LDADD    = $(LIBHT)
htsetroot_LDADD    = $(LIBHT)
htsysmon_LDADD     = $(LIBHT)
htterm_LDADD       = $(LIBHT) $(LIBTUTF)
htthreadtest_LDADD = $(LIBHT) $(LIBPTHREAD)
hedgeytty_hitomi_bg_LDADD = $(LIBHT)
htdock_LDADD = $(LIBHT)

install-exec-hook:
	rm -f            $(DESTDIR)$(bindir)/htdetach
	$(LN_S) htattach $(DESTDIR)$(bindir)/htdetach
	rm -f            $(DESTDIR)$(bindir)/hedgeytty-hitomi-bg
	$(LN_S) hedgeytty_hitomi_bg $(DESTDIR)$(bindir)/hedgeytty-hitomi-bg
	rm -f            $(DESTDIR)$(bindir)/hedgeytty-dock
	$(LN_S) htdock $(DESTDIR)$(bindir)/hedgeytty-dock
