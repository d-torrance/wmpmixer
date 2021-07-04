bin_PROGRAMS = wmpmixer
wmpmixer_SOURCES = wmpmixer.c wmpmixer.h pulse.c pulse.h

AM_CFLAGS = $(PULSE_CFLAGS) $(WRLIB_CFLAGS) $(GTK_CFLAGS) $(X11_CFLAGS) \
	$(WINGS_CFLAGS) $(XEXT_CFLAGS)
LIBS += $(PULSE_LIBS) $(WRLIB_LIBS) $(GTK_LIBS) $(X11_LIBS) \
	$(WINGS_LIBS) $(XEXT_LIBS)
