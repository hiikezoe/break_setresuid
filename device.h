/*
 * Copyright (C) 2013 Hiroyuki Ikezoe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef DEVICE_H
#define DEVICE_H

typedef enum {
  UNSUPPORTED = -1,
  F03D_V24,
  F12C_V21
} DeviceId;

DeviceId detect_device(void);

#endif /* DEVICE_H */
/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
