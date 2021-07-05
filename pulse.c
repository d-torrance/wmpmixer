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

#include "pulse.h"
#include "wmpmixer.h"

#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>
#include <WINGs/WINGs.h>
#include <WINGs/WUtil.h>

pa_mainloop *ml;

typedef struct {
	const char *description;
	WMPixmap *icon;
	pa_cvolume volume;
} PulseDevice;

int current_device = 0;
int num_devices = 0;
PulseDevice pulse_devices[100];

WMPixmap *icon_name_to_pixmap(const char *icon_name);
void mainloop_iterate(void *data);
void sink_info_cb(pa_context *ctx, const pa_sink_info *info,
		      int eol, void *userdata);
void source_info_cb(pa_context *ctx, const pa_source_info *info,
		    int eol, void *userdata);
void sink_input_info_cb(pa_context *ctx, const pa_sink_input_info *info,
			int eol, void *userdata);
void source_output_info_cb(pa_context *ctx, const pa_source_output_info *info,
			int eol, void *userdata);
void state_cb(pa_context *c, void *userdata);

WMPixmap *icon_name_to_pixmap(const char *icon_name) {
	const char *file;
	GtkIconTheme *theme;
	GtkIconInfo *icon_info;
	WMPixmap *pixmap;
	WMScreen *screen;

	RColor bg = {40, 40, 40, 255};

	screen = get_screen();

	if (!icon_name)
		return WMCreatePixmap(screen, 22, 22, 0, False);

	theme = gtk_icon_theme_get_default();
	/* TODO - error handling */
	icon_info = gtk_icon_theme_lookup_icon(
		theme, icon_name, 22, GTK_ICON_LOOKUP_GENERIC_FALLBACK);
	file = gtk_icon_info_get_filename(icon_info);

	pixmap = WMCreateScaledBlendedPixmapFromFile(screen, file, &bg, 22, 22);

	g_object_unref(icon_info);

	return pixmap;
}

void setup_pulse(void)
{
	pa_context *ctx;
	pa_mainloop_api *mlapi;

	ml = pa_mainloop_new();
	if (!ml) {
		werror("pa_mainloop_new() failed");
		exit(EXIT_FAILURE);
	}
	mlapi = pa_mainloop_get_api(ml);
	ctx = pa_context_new(mlapi, PACKAGE_NAME);
	if (!ctx) {
		werror("pa_context_new() failed");
		exit(EXIT_FAILURE);
	}
	pa_context_connect(ctx, NULL, 0, NULL);
	pa_context_set_state_callback(ctx, state_cb, NULL);
}

void sink_info_cb(pa_context *ctx, const pa_sink_info *info,
		      int eol, void *userdata)
{
	const char *icon_name;

	if (eol) {
		pa_context_get_source_info_list(ctx, source_info_cb, NULL);
		return;
	}

	pulse_devices[num_devices].description = wstrdup(info->description);
	icon_name = pa_proplist_gets(info->proplist, "device.icon_name");
	pulse_devices[num_devices].icon = icon_name_to_pixmap(icon_name);
	pulse_devices[num_devices].volume = info->volume;
	num_devices++;
}

void source_info_cb(pa_context *ctx, const pa_source_info *info,
		    int eol, void *userdata)
{
	const char *icon_name;
	if (eol) {
		pa_context_get_sink_input_info_list(ctx, sink_input_info_cb,
						    NULL);
		return;
	}

	pulse_devices[num_devices].description = wstrdup(info->description);
	icon_name = pa_proplist_gets(info->proplist, "device.icon_name");
	pulse_devices[num_devices].icon = icon_name_to_pixmap(icon_name);
	pulse_devices[num_devices].volume = info->volume;
	num_devices++;
}

void sink_input_info_cb(pa_context *ctx, const pa_sink_input_info *info,
			int eol, void *userdata)
{
	const char *name, *icon_name;
	if (eol) {
		pa_context_get_source_output_info_list(ctx,
						       source_output_info_cb,
						       NULL);
		update_device();
		return;
	}

	name = pa_proplist_gets(info->proplist, "application.name");
	pulse_devices[num_devices].description = wstrdup(name);
	icon_name = pa_proplist_gets(info->proplist, "application.icon_name");
	pulse_devices[num_devices].icon = icon_name_to_pixmap(icon_name);
	pulse_devices[num_devices].volume = info->volume;
	num_devices++;
}

void source_output_info_cb(pa_context *ctx, const pa_source_output_info *info,
			int eol, void *userdata)
{
	const char *name, *icon_name;
	if (eol) {
		update_device();
		return;
	}

	name = pa_proplist_gets(info->proplist, "application.name");
	pulse_devices[num_devices].description = wstrdup(name);
	icon_name = pa_proplist_gets(info->proplist, "application.icon_name");
	pulse_devices[num_devices].icon = icon_name_to_pixmap(icon_name);
	pulse_devices[num_devices].volume = info->volume;
	num_devices++;
}

void state_cb(pa_context *ctx, void *userdata) {
	pa_context_state_t state;

        state = pa_context_get_state(ctx);
	/* TODO - display this info on the dockapp in some way */

	if (state == PA_CONTEXT_READY)
		pa_context_get_sink_info_list(ctx, sink_info_cb, NULL);
}

void iterate_pulse_mainloop(void *data)
{
	pa_mainloop_iterate(ml, 0, NULL);
}

const char *get_current_device_description(void)
{
	return pulse_devices[current_device].description;
}

WMPixmap *get_current_device_icon(void)
{
	return pulse_devices[current_device].icon;
}

/* returns an int between -1 (= muted) and 24 (= 150% of normal) */
int get_current_device_volume(void)
{
	pa_cvolume volume;
	pa_volume_t average;
	int result;

	volume = pulse_devices[current_device].volume;
	average = pa_cvolume_avg(&volume);

	result = (25 / (1.5 * PA_VOLUME_NORM - PA_VOLUME_MUTED)) *
		(average - PA_VOLUME_MUTED) - 1;
	if (result < -1)
		return -1;
	else if (result > 24)
		return 24;
	else
		return result;
}

void increment_current_device(WMWidget *widget, void *data)
{
	current_device++;

	if (current_device >= num_devices)
		current_device = 0;

	update_device();
}

void decrement_current_device(WMWidget *widget, void *data)
{
	current_device--;

	if (current_device < 0)
		current_device = num_devices - 1;

	update_device();
}
