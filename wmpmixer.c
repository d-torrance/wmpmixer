/* wmpmixer - PulseAudio mixer as a Window Maker dockapp
 * Copyright (C) 2021 Doug Torrance <dtorrance@piedmont.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <wraster.h>
#include <WINGs/WINGs.h>
#include <X11/extensions/shape.h>

#include "pulse.h"
#include "wmpmixer.h"

WMScreen *screen;
WMLabel *icon_label, *slider_label;

static char * left_xpm[] = {
	"4 7 2 1",
	" 	c #AEAAAE",
	".	c #000000",
	"   .",
	"  ..",
	" ...",
	"....",
	" ...",
	"  ..",
	"   ."};

static char * right_xpm[] = {
	"4 7 2 1",
	" 	c #000000",
	".	c #AEAAAE",
	" ...",
	"  ..",
	"   .",
	"    ",
	"   .",
	"  ..",
	" ..."};

static char * record_xpm[] = {
	"6 5 4 1",
	" 	c #AEAAAE",
	".	c #FA0808",
	"+	c #FF0000",
	"@	c #FE0000",
	" .++@ ",
	"..@.+@",
	".@@@.@",
	".@@@+@",
	" .@+@ "};

static char * mute_xpm[] = {
	"10 9 3 1",
	" 	c #FF0000",
	".	c #AEAAAE",
	"+	c #000000",
	" ........ ",
	". ....++ .",
	".. ..+.+..",
	"..+++. +..",
	"..+.  .+..",
	"..+++. +..",
	".. ..+.+..",
	". ....++ .",
	" ........ "};

void slider_event(XEvent *event, void *data);
void setup_window(WMWindow *window);

int main(int argc, char **argv)
{
	Display *display;
	WMWindow *window;

	WMInitializeApplication(PACKAGE_NAME, &argc, argv);

	/* for looking up icons */
	gtk_init(&argc, &argv);

	display = XOpenDisplay("");
	if (!display) {
		werror("could not connect to X server");
		exit(EXIT_FAILURE);
	}

	screen = WMCreateScreen(display, DefaultScreen(display));
	window = WMCreateWindow(screen, PACKAGE_NAME);
	setup_window(window);
	setup_pulse();

	WMAddPersistentTimerHandler(100, iterate_pulse_mainloop, NULL);
	WMScreenMainLoop(screen);
}

void setup_window(WMWindow *window) {
	Display *display;
	Window xid;
	XRectangle rect[3];
	XWMHints *hints;
	WMColor *bg;
	WMFrame *icon_frame, *slider_frame;
	WMButton *left_button, *right_button, *record_button, *mute_button;
	WMPixmap *left_pix, *right_pix, *record_pix, *mute_pix;

	WMRealizeWidget(window);
	WMResizeWidget(window, 64, 64);

	xid = WMWidgetXID(window);
	screen = WMWidgetScreen(window);
	display = WMScreenDisplay(screen);

	hints = XGetWMHints(display, xid);
	hints->flags |= WindowGroupHint;
	hints->window_group = xid;

	/* TODO - allow windowed mode */
	hints->flags |= IconWindowHint | StateHint;
	hints->icon_window = xid;
	hints->initial_state = WithdrawnState;

	XSetWMHints(display, xid, hints);
	XFree(hints);

	/* icons */
	rect[0].x = 4;
	rect[0].y = 4;
	rect[0].width = 26;
	rect[0].height = 24;

	/* buttons */
	rect[1].x = 4;
	rect[1].y = 33;
	rect[1].width = 26;
	rect[1].height = 26;

	/* volume slider */
	rect[2].x = 35;
	rect[2].y = 4;
	rect[2].width = 25;
	rect[2].height = 55;

	XShapeCombineRectangles(display, xid, ShapeBounding, 0, 0, rect, 3,
				ShapeSet, Unsorted);

	bg = WMCreateRGBColor(screen, 0x2800, 0x2800, 0x2800, False);

	icon_frame = WMCreateFrame(window);
	WMSetFrameRelief(icon_frame, WRPushed);
	WMResizeWidget(icon_frame, 26, 24);
	WMMoveWidget(icon_frame, 4, 4);
	WMRealizeWidget(icon_frame);

	icon_label = WMCreateLabel(icon_frame);
	WMResizeWidget(icon_label, 24, 22);
	WMMoveWidget(icon_label, 1, 1);
	WMSetWidgetBackgroundColor(icon_label, bg);
	WMSetLabelImagePosition(icon_label, WIPImageOnly);
	WMRealizeWidget(icon_label);

	slider_frame = WMCreateFrame(window);
	WMSetFrameRelief(slider_frame, WRPushed);
	WMResizeWidget(slider_frame, 25, 55);
	WMMoveWidget(slider_frame, 35, 4);
	WMSetWidgetBackgroundColor(slider_frame, bg);
	WMRealizeWidget(slider_frame);

	slider_label = WMCreateLabel(slider_frame);
	WMResizeWidget(slider_label, 23, 53);
	WMMoveWidget(slider_label, 1, 1);
	WMSetWidgetBackgroundColor(slider_label, bg);
	WMSetLabelImagePosition(slider_label, WIPImageOnly);
	WMCreateEventHandler(
		WMWidgetView(slider_label),
		ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
		slider_event, NULL);
	WMRealizeWidget(slider_label);

	left_button = WMCreateButton(window, WBTMomentaryPush);
	WMResizeWidget(left_button, 13, 13);
	WMMoveWidget(left_button, 4, 33);
	left_pix = WMCreatePixmapFromXPMData(screen, left_xpm);
	WMSetButtonImage(left_button, left_pix);
	WMSetButtonImagePosition(left_button, WIPImageOnly);
	WMSetButtonAction(left_button, decrement_current_device, NULL);
	WMRealizeWidget(left_button);

	right_button = WMCreateButton(window, WBTMomentaryPush);
	WMResizeWidget(right_button, 13, 13);
	WMMoveWidget(right_button, 17, 33);
	right_pix = WMCreatePixmapFromXPMData(screen, right_xpm);
	WMSetButtonImage(right_button, right_pix);
	WMSetButtonImagePosition(right_button, WIPImageOnly);
	WMSetButtonAction(right_button, increment_current_device, NULL);
	WMRealizeWidget(right_button);

	record_button = WMCreateButton(window, WBTToggle);
	WMResizeWidget(record_button, 13, 13);
	WMMoveWidget(record_button, 4, 46);
	record_pix = WMCreatePixmapFromXPMData(screen, record_xpm);
	WMSetButtonImage(record_button, record_pix);
	WMSetButtonImagePosition(record_button, WIPImageOnly);
	WMRealizeWidget(record_button);

	mute_button = WMCreateButton(window, WBTToggle);
	WMResizeWidget(mute_button, 13, 13);
	WMMoveWidget(mute_button, 17, 46);
	mute_pix = WMCreatePixmapFromXPMData(screen, mute_xpm);
	WMSetButtonImage(mute_button, mute_pix);
	WMSetButtonImagePosition(mute_button, WIPImageOnly);
	WMRealizeWidget(mute_button);

	WMMapWidget(window);
	WMMapSubwidgets(window);
	WMMapWidget(icon_label);
	WMMapWidget(slider_label);
}

WMScreen *get_screen(void) {
	return screen;
}

void update_device(void)
{
	int i;
	RImage *image;
	WMPixmap *slider_pix;

	RColor bg = {40, 40, 40, 255};

	WMSetBalloonTextForView(get_current_device_description(),
				WMWidgetView(icon_label));
	WMSetLabelImage(icon_label, get_current_device_icon());
	WMRedisplayWidget(icon_label);

	image = RCreateImage(23, 53, False);
	RFillImage(image, &bg);

	for (i = 0; i <= get_current_device_volume(); i++) {
		/* based on XHandler::mixColor() from wmmixer */
		RColor line_color = {
			255 * i / (50 - i),
			255 * (50 - 2 * i) / (50 - i), 0, 255};
		RDrawLine(image, 1, 50 - 2 * i, 20, 50 - 2 * i, &line_color);
	}

	slider_pix = WMCreatePixmapFromRImage(screen, image, 127);
	WMSetLabelImage(slider_label, slider_pix);
	WMRedisplayWidget(slider_label);

	RReleaseImage(image);
}

void slider_event(XEvent *event, void *data)
{
	if (((event->type == ButtonPress || event->type == ButtonRelease)
	     && event->xbutton.button == 1) ||
	    (event->type == MotionNotify && event->xmotion.state & Button1Mask))
		wmessage("y = %d\n", event->xbutton.y);
	else if (event->type == ButtonPress && event->xbutton.button == 4)
		wmessage("up!");
	else if (event->type == ButtonPress && event->xbutton.button == 5)
		wmessage("down!");
}
