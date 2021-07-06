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

#include <glib-object.h>
#include <gtk/gtk.h>
#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/introspect.h>
#include <pulse/mainloop-api.h>
#include <pulse/mainloop.h>
#include <pulse/proplist.h>
#include <pulse/volume.h>
#include <stdint.h>
#include <stdlib.h>
#include <WINGs/WINGs.h>
#include <WINGs/WUtil.h>
#include <wraster.h>
#include <X11/Xlib.h>

pa_mainloop *ml;
pa_context *ctx;

typedef enum {
	PULSE_SINK,
	PULSE_SOURCE,
	PULSE_SINK_INPUT,
	PULSE_SOURCE_OUTPUT
} pulse_type;

typedef struct {
	pulse_type type;
	uint32_t index;
	const char *description;
	WMPixmap *icon;
	pa_cvolume volume;
	Bool muted;
} PulseDevice;

int current_device = 0;
WMArray *pulse_devices;

PulseDevice *create_device(pulse_type type, uint32_t index,
			   const char *description, const char *icon_name,
			   pa_cvolume volume, Bool muted);
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
pa_volume_t int_to_volume(int n);
int volume_to_int(pa_cvolume volume);
void update_slider_cb(pa_context *ctx, int success, void *userdata);
void change_current_device_volume_by(int k);
PulseDevice *get_current_device(void);

WMPixmap *icon_name_to_pixmap(const char *icon_name) {
	const char *file;
	GtkIconTheme *theme;
	GtkIconInfo *icon_info;
	WMPixmap *pixmap;
	WMScreen *screen;

	RColor bg = {40, 40, 40, 255};

	screen = get_screen();

	if (!icon_name)
		goto error;

	theme = gtk_icon_theme_get_default();
	if (!theme)
		goto error;

	icon_info = gtk_icon_theme_lookup_icon(
		theme, icon_name, 22, GTK_ICON_LOOKUP_GENERIC_FALLBACK);
	if (!icon_info)
		goto error;

	file = gtk_icon_info_get_filename(icon_info);

	pixmap = WMCreateScaledBlendedPixmapFromFile(screen, file, &bg, 22, 22);

	g_object_unref(icon_info);

	return pixmap;

error:
	werror("unable to get icon");
	return WMCreatePixmap(screen, 22, 22, 0, False);
}

void setup_pulse(void)
{
	pa_mainloop_api *mlapi;

	pulse_devices = WMCreateArray(0);

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

PulseDevice *create_device(pulse_type type, uint32_t index,
			   const char *description, const char *icon_name,
			   pa_cvolume volume, Bool muted)
{
	PulseDevice *device;

	device = wmalloc(sizeof(PulseDevice));
	device->type = type;
	device->index = index;
	device->description = wstrdup(description);
	device->icon = icon_name_to_pixmap(icon_name);
	device->volume = volume;
	device->muted = muted;

	return device;
}


void sink_info_cb(pa_context *ctx, const pa_sink_info *info,
		      int eol, void *userdata)
{
	(void)userdata;

	if (eol) {
		pa_context_get_source_info_list(ctx, source_info_cb, NULL);
		return;
	} else {
		const char *icon_name;
		PulseDevice *device;

		icon_name = pa_proplist_gets(info->proplist,
					     "device.icon_name");
		device = create_device(PULSE_SINK, info->index,
				       info->description, icon_name,
				       info->volume, info->mute);
		WMAddToArray(pulse_devices, device);
	}
}

void source_info_cb(pa_context *ctx, const pa_source_info *info,
		    int eol, void *userdata)
{
	(void)userdata;

	if (eol) {
		pa_context_get_sink_input_info_list(ctx, sink_input_info_cb,
						    NULL);
		return;
	} else {
		const char *icon_name;
		PulseDevice *device;

		icon_name = pa_proplist_gets(info->proplist,
					     "device.icon_name");
		device = create_device(PULSE_SOURCE, info->index,
				       info->description, icon_name,
				       info->volume, info->mute);
		WMAddToArray(pulse_devices, device);
	}
}

void sink_input_info_cb(pa_context *ctx, const pa_sink_input_info *info,
			int eol, void *userdata)
{
	(void)userdata;

	if (eol) {
		pa_context_get_source_output_info_list(ctx,
						       source_output_info_cb,
						       NULL);
		return;
	} else {
		const char *name, *icon_name;
		PulseDevice *device;

		name = pa_proplist_gets(info->proplist, "application.name");
		icon_name = pa_proplist_gets(info->proplist,
					     "application.icon_name");
		device = create_device(PULSE_SINK_INPUT, info->index,
				       name, icon_name, info->volume,
				       info->mute);
		WMAddToArray(pulse_devices, device);
	}
}

void source_output_info_cb(pa_context *ctx, const pa_source_output_info *info,
			int eol, void *userdata)
{
	(void)ctx;
	(void)userdata;

	if (eol) {
		update_device();
		return;
	} else {
		const char *name, *icon_name;
		PulseDevice *device;

		name = pa_proplist_gets(info->proplist, "application.name");
		icon_name = pa_proplist_gets(info->proplist,
					     "application.icon_name");
		device = create_device(PULSE_SOURCE_OUTPUT, info->index,
				       name, icon_name, info->volume,
				       info->mute);
		WMAddToArray(pulse_devices, device);
	}
}

void state_cb(pa_context *ctx, void *userdata) {
	pa_context_state_t state;

	(void)userdata;

        state = pa_context_get_state(ctx);
	/* TODO - display this info on the dockapp in some way */

	if (state == PA_CONTEXT_READY)
		pa_context_get_sink_info_list(ctx, sink_info_cb, NULL);
}

void iterate_pulse_mainloop(void *data)
{
	(void)data;

	pa_mainloop_iterate(ml, 0, NULL);
}

PulseDevice *get_current_device(void)
{
	return WMGetFromArray(pulse_devices, current_device);
}

const char *get_current_device_description(void)
{
	PulseDevice *device;

	device = get_current_device();
	return device->description;
}

WMPixmap *get_current_device_icon(void)
{
	PulseDevice *device;

	device = WMGetFromArray(pulse_devices, current_device);
	return device->icon;
}

/* returns an int between 0 (= muted) and 25 (= 150% of normal) */
int volume_to_int(pa_cvolume volume)
{
	int result;
	pa_volume_t average;

	average = pa_cvolume_avg(&volume);
	result = (25 / (1.5 * PA_VOLUME_NORM - PA_VOLUME_MUTED)) *
		(average - PA_VOLUME_MUTED) + 0.5;

	if (result < 0)
		return 0;
	else if (result > 25)
		return 25;
	else
		return result;
}

int get_current_device_volume(void)
{
	PulseDevice *device;

	device = get_current_device();
	return volume_to_int(device->volume);
}

pa_volume_t int_to_volume(int n)
{
	pa_volume_t result;

	result = (1.5 * PA_VOLUME_NORM - PA_VOLUME_MUTED) / 25 * n +
		PA_VOLUME_MUTED + 0.5;

	if (result < PA_VOLUME_MUTED)
		return PA_VOLUME_MUTED;
	else if (result > 1.5 * PA_VOLUME_NORM)
		return 1.5 * PA_VOLUME_NORM;
	else
		return result;
}

void increment_current_device_volume(void)
{
	change_current_device_volume_by(1);
}

void decrement_current_device_volume(void)
{
	change_current_device_volume_by(-1);
}

void change_current_device_volume_by(int k)
{
	int n;
	PulseDevice *device;

	device = get_current_device();
	n = volume_to_int(device->volume) + k;

	if (n < 0)
		n = 0;
	else if (n > 25)
		n = 25;

	set_current_device_volume(n);
}

void set_current_device_volume(int n)
{
	int i;
	PulseDevice *device;
	pa_cvolume volume;
	pa_volume_t channel_volume;

	device = get_current_device();

	volume.channels = device->volume.channels;
	channel_volume = int_to_volume(n);
	for (i = 0; i < volume.channels; i++)
		volume.values[i] = channel_volume;
	device->volume = volume;

	switch (device->type) {
	case PULSE_SINK:
		pa_context_set_sink_volume_by_index(
			ctx, device->index, &volume, update_slider_cb, NULL);
		break;

	case PULSE_SOURCE:
		pa_context_set_source_volume_by_index(
			ctx, device->index, &volume, update_slider_cb, NULL);
		break;

	case PULSE_SINK_INPUT:
		pa_context_set_sink_input_volume(
			ctx, device->index, &volume, update_slider_cb, NULL);
		break;

	case PULSE_SOURCE_OUTPUT:
		pa_context_set_source_output_volume(
			ctx, device->index, &volume, update_slider_cb, NULL);
		break;

	default:
		wwarning("unknown device type");
		break;
	}

}

void update_slider_cb(pa_context *ctx, int success, void *userdata)
{
	(void)ctx;
	(void)success;
	(void)userdata;

	update_slider();
}


void increment_current_device(WMWidget *widget, void *data)
{
	(void)widget;
	(void)data;

	current_device++;

	if (current_device >= WMGetArrayItemCount(pulse_devices))
		current_device = 0;

	update_device();
}

void decrement_current_device(WMWidget *widget, void *data)
{
	(void)widget;
	(void)data;

	current_device--;

	if (current_device < 0)
		current_device = WMGetArrayItemCount(pulse_devices) - 1;

	update_device();
}
