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

#ifndef PULSE_H
#define PULSE_H

#include <WINGs/WINGs.h>
#include <X11/Xlib.h>

const char *get_current_device_description(void);
WMPixmap *get_current_device_icon(void);
int get_current_device_volume(void);
Bool get_current_device_muted(void);
void set_current_device_volume(int n);
void toggle_current_device_muted(WMWidget *widget, void *data);
void increment_current_device_volume(void);
void decrement_current_device_volume(void);
void increment_current_device(WMWidget *widget, void *data);
void decrement_current_device(WMWidget *widget, void *data);
void setup_pulse(void);
void iterate_pulse_mainloop(void *data);

#endif
