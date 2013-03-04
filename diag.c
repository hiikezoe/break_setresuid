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
/*
 * Based on htcj_butterflay_diaggetroot.zip
 * <https://docs.google.com/file/d/0B8LDObFOpzZqQzducmxjRExXNnM/edit?pli=1>
 */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#include "diag.h"

#define DIAG_IOCTL_GET_DELAYED_RSP_ID   8
struct diagpkt_delay_params {
  void *rsp_ptr;
  int size;
  int *num_bytes_ptr;
};

static int
send_delay_params(int fd, struct diagpkt_delay_params *params)
{
  int ret;
  ret = ioctl(fd, DIAG_IOCTL_GET_DELAYED_RSP_ID, params);
  if (ret < 0) {
    printf("failed to ioctl due to %s.\n", strerror(errno));
  }
  return ret;
}

static int
reset_delayed_rsp_id(int fd, unsigned int delayed_rsp_id_address)
{
  struct diagpkt_delay_params params;
  params.size = 2;
  params.num_bytes_ptr = (void *)delayed_rsp_id_address;

  return send_delay_params(fd, &params);
}

static int
confirm_delayed_rsp_id(int fd)
{
  int ret;
  int delayed_rsp_id;
  struct diagpkt_delay_params params;

  params.rsp_ptr = &delayed_rsp_id;
  params.size = 2;

  ret = send_delay_params(fd, &params);
  if (ret < 0) {
    return ret;
  }
  return delayed_rsp_id;
}

static int
inject_value (unsigned int address, int value,
              int fd, unsigned int delayed_rsp_id_address)
{
  uint16_t delayed_rsp_id_value;
  int i, loop_count;
  int ret;

  ret = reset_delayed_rsp_id(fd, delayed_rsp_id_address);
  if (ret < 0) {
    return ret;
  }

  ret = confirm_delayed_rsp_id(fd);
  if (ret < 0) {
    return ret;
  }
  delayed_rsp_id_value = ret;

  loop_count = (value - delayed_rsp_id_value) & 0xffff;
  printf("loop = %04x\n", loop_count);

  for (i = 0; i < loop_count; i++) {
    struct diagpkt_delay_params params;
    params.rsp_ptr = (void *)address;
    params.size = 2;

    ret = send_delay_params(fd, &params);
    if (ret < 0) {
      return ret;
    }
  }
  return 0;
}

int
inject_with_file_descriptor (struct values *data, int data_length,
                             unsigned int delayed_rsp_id_address, int fd)
{
  int i;

  for (i = 0; i < data_length; i++) {
    int ret;
    printf("data[%d] addr=0x%08x value=%04x\n",
           i, data[i].address, data[i].value);
    ret = inject_value(data[i].address, data[i].value, fd, delayed_rsp_id_address);
    if (ret < 0)
      return ret;
  }

  return 0;
}

/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
