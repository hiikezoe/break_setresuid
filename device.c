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
#include "device.h"

#include <stdio.h>
#include <string.h>
#include <sys/system_properties.h>

typedef struct _supported_device {
  DeviceId device;
  const char *model;
  const char *build_id;
} supported_device;

static supported_device supported_devices[] = {
  { F03D_V24, "F-03D", "V24R33Cc" },
  { F12C_V21, "F-12C", "V21"      }
};

static int n_supported_devices = sizeof(supported_devices) / sizeof(supported_devices[0]);

DeviceId
detect_device(void)
{
  int i;
  char model[PROP_VALUE_MAX];
  char build_id[PROP_VALUE_MAX];

  __system_property_get("ro.product.model", model);
  __system_property_get("ro.build.display.id", build_id);

  for (i = 0; i < n_supported_devices; i++) {
    if (!strcmp(model, supported_devices[i].model) &&
        !strcmp(build_id, supported_devices[i].build_id)) {
      return supported_devices[i].device;
    }
  }
  printf("%s (%s) is not supported.\n", model, build_id);

  return UNSUPPORTED;
}

