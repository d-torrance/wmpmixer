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
#include <stdlib.h>
#include <WINGs/WINGs.h>
#include <WINGs/WUtil.h>
#include <wraster.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/shapeconst.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "pulse.h"
#include "wmpmixer.h"

#define MARGIN 4
#define PADDING 5
#define DOCKAPP_MEASURE 64
#define ICON_MEASURE 22
#define BUTTON_Y MARGIN + ICON_MEASURE + 2 + PADDING
#define BUTTON_MEASURE 13
#define SLIDER_WIDTH 25
#define SLIDER_HEIGHT ICON_MEASURE + 2 + PADDING + 2 * BUTTON_MEASURE
#define SLIDER_X MARGIN + 2 * BUTTON_MEASURE + PADDING

WMScreen *screen;
WMLabel *icon_label, *slider_label;
WMButton *mute_button;
RColor slider_color[25];

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

void create_slider_colors(void);
void slider_event(XEvent *event, void *data);
void setup_window(WMWindow *window);
int y_to_bar(int y);

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
	create_slider_colors();
	setup_window(window);
	setup_pulse();

	WMAddPersistentTimerHandler(100, iterate_pulse_mainloop, NULL);
	WMScreenMainLoop(screen);

	return 0;
}

void setup_window(WMWindow *window) {
	Display *display;
	Window xid;
	XRectangle rect[3];
	XWMHints *hints;
	WMColor *bg;
	WMFrame *icon_frame, *slider_frame;
	WMButton *left_button, *right_button, *record_button;
	WMPixmap *left_pix, *right_pix, *record_pix, *mute_pix;

	WMRealizeWidget(window);
	WMResizeWidget(window, DOCKAPP_MEASURE, DOCKAPP_MEASURE);

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
	rect[0].x = MARGIN;
	rect[0].y = MARGIN;
	rect[0].width = 2 * BUTTON_MEASURE;
	rect[0].height = ICON_MEASURE + 2;

	/* buttons */
	rect[1].x = MARGIN;
	rect[1].y = BUTTON_Y;
	rect[1].width = 2 * BUTTON_MEASURE;
	rect[1].height = 2 * BUTTON_MEASURE;

	/* volume slider */
	rect[2].x = SLIDER_X;
	rect[2].y = MARGIN;
	rect[2].width = SLIDER_WIDTH;
	rect[2].height = SLIDER_HEIGHT;

	XShapeCombineRectangles(display, xid, ShapeBounding, 0, 0, rect, 3,
				ShapeSet, Unsorted);

	bg = WMCreateRGBColor(screen, 0x2800, 0x2800, 0x2800, False);

	icon_frame = WMCreateFrame(window);
	WMSetFrameRelief(icon_frame, WRPushed);
	WMResizeWidget(icon_frame, 2 * BUTTON_MEASURE, ICON_MEASURE + 2);
	WMMoveWidget(icon_frame, MARGIN, MARGIN);
	WMRealizeWidget(icon_frame);

	icon_label = WMCreateLabel(icon_frame);
	WMResizeWidget(icon_label, ICON_MEASURE + 2, ICON_MEASURE);
	WMMoveWidget(icon_label, 1, 1);
	WMSetWidgetBackgroundColor(icon_label, bg);
	WMSetLabelImagePosition(icon_label, WIPImageOnly);
	WMRealizeWidget(icon_label);

	slider_frame = WMCreateFrame(window);
	WMSetFrameRelief(slider_frame, WRPushed);
	WMResizeWidget(slider_frame, SLIDER_WIDTH, SLIDER_HEIGHT);
	WMMoveWidget(slider_frame, SLIDER_X, MARGIN);
	WMSetWidgetBackgroundColor(slider_frame, bg);
	WMRealizeWidget(slider_frame);

	slider_label = WMCreateLabel(slider_frame);
	WMResizeWidget(slider_label, SLIDER_WIDTH - 2, SLIDER_HEIGHT - 2);
	WMMoveWidget(slider_label, 1, 1);
	WMSetWidgetBackgroundColor(slider_label, bg);
	WMSetLabelImagePosition(slider_label, WIPImageOnly);
	WMCreateEventHandler(
		WMWidgetView(slider_label),
		ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
		slider_event, NULL);
	WMRealizeWidget(slider_label);

	left_button = WMCreateButton(window, WBTMomentaryPush);
	WMResizeWidget(left_button, BUTTON_MEASURE, BUTTON_MEASURE);
	WMMoveWidget(left_button, MARGIN, BUTTON_Y);
	left_pix = WMCreatePixmapFromXPMData(screen, left_xpm);
	WMSetButtonImage(left_button, left_pix);
	WMSetButtonImagePosition(left_button, WIPImageOnly);
	WMSetButtonAction(left_button, decrement_current_device, NULL);
	WMRealizeWidget(left_button);

	right_button = WMCreateButton(window, WBTMomentaryPush);
	WMResizeWidget(right_button, BUTTON_MEASURE, BUTTON_MEASURE);
	WMMoveWidget(right_button, MARGIN + BUTTON_MEASURE, BUTTON_Y);
	right_pix = WMCreatePixmapFromXPMData(screen, right_xpm);
	WMSetButtonImage(right_button, right_pix);
	WMSetButtonImagePosition(right_button, WIPImageOnly);
	WMSetButtonAction(right_button, increment_current_device, NULL);
	WMRealizeWidget(right_button);

	record_button = WMCreateButton(window, WBTToggle);
	WMResizeWidget(record_button, BUTTON_MEASURE, BUTTON_MEASURE);
	WMMoveWidget(record_button, MARGIN, BUTTON_Y + BUTTON_MEASURE);
	record_pix = WMCreatePixmapFromXPMData(screen, record_xpm);
	WMSetButtonImage(record_button, record_pix);
	WMSetButtonImagePosition(record_button, WIPImageOnly);
	WMRealizeWidget(record_button);

	mute_button = WMCreateButton(window, WBTToggle);
	WMResizeWidget(mute_button, BUTTON_MEASURE, BUTTON_MEASURE);
	WMMoveWidget(mute_button, MARGIN + BUTTON_MEASURE,
		     BUTTON_Y + BUTTON_MEASURE);
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

void create_slider_colors(void)
{
	int i;

	/* based on XHandler::mixColor() from wmmixer */
	for (i = 0; i < 25; i++) {
		slider_color[i].red = 255 * i / (50 - i);
		slider_color[i].green = 255 * (50 - 2 * i) / (50 - i);
		slider_color[i].blue = 0;
		slider_color[i].alpha = 255;
	}
}

void update_device(void)
{
	WMSetBalloonTextForView(get_current_device_description(),
				WMWidgetView(icon_label));
	WMSetLabelImage(icon_label, get_current_device_icon());
	WMRedisplayWidget(icon_label);

	WMSetButtonSelected(mute_button, get_current_device_muted());
	WMRedisplayWidget(mute_button);

	update_slider();
}

void update_slider(void)
{
	int i;
	RImage *image;
	WMPixmap *slider_pix;

	RColor bg = {40, 40, 40, 255};


	image = RCreateImage(SLIDER_WIDTH - 2, SLIDER_HEIGHT - 2, False);
	RFillImage(image, &bg);

	for (i = 0; i < get_current_device_volume(); i++)
		RDrawLine(image, 1, SLIDER_HEIGHT - 5 - 2 * i,
			  SLIDER_WIDTH - 5, SLIDER_HEIGHT - 5 - 2 * i,
			  &slider_color[i]);

	slider_pix = WMCreatePixmapFromRImage(screen, image, 127);
	WMSetLabelImage(slider_label, slider_pix);
	WMRedisplayWidget(slider_label);

	RReleaseImage(image);
}

void slider_event(XEvent *event, void *data)
{
	(void)data;

	if (((event->type == ButtonPress || event->type == ButtonRelease)
	     && event->xbutton.button == 1) ||
	    (event->type == MotionNotify && event->xmotion.state & Button1Mask))
		set_current_device_volume(y_to_bar(event->xbutton.y));
	else if (event->type == ButtonPress && event->xbutton.button == 4)
		increment_current_device_volume();
	else if (event->type == ButtonPress && event->xbutton.button == 5)
		decrement_current_device_volume();
}

int y_to_bar(int y)
{
	int result;

	result = (SLIDER_HEIGHT - 2 - y) / 2;
	if (result < 0)
		return 0;
	else if (result > 25)
		return 25;
	else
		return result;
}
