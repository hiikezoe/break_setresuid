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
#ifndef DIAG_H
#define DIAG_H

struct values {
  unsigned int address;
  unsigned short value;
};

typedef struct _diag_injection_addresses {
  unsigned long int target_address;
  unsigned long int delayed_rsp_id_address;
} diag_injection_addresses;

int inject(struct values *data, int data_length, unsigned int delayed_rsp_id_address);

#endif /* DIAG_H */
/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
