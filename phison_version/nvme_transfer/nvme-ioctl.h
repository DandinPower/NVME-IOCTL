#ifndef _NVME_LIB_H
#define _NVME_LIB_H

#include <linux/types.h>
#include <stdbool.h>
#include "linux/nvme_ioctl.h"
// #include "nvme.h"

/* NVME_SUBMIT_IO */
int nvme_io(int fd, __u8 opcode, __u64 slba, __u16 nblocks, __u16 control,
	      __u32 dsmgmt, __u32 reftag, __u16 apptag,
	      __u16 appmask, void *data, void *metadata);

int nvme_read(int fd, __u64 slba, __u16 nblocks, __u16 control,
	      __u32 dsmgmt, __u32 reftag, __u16 apptag,
	      __u16 appmask, void *data, void *metadata);

int nvme_write(int fd, __u64 slba, __u16 nblocks, __u16 control,
	       __u32 dsmgmt, __u32 reftag, __u16 apptag,
	       __u16 appmask, void *data, void *metadata);
static int nvme_submit_io_passthru(int fd, struct nvme_passthru_cmd *cmd);
int nvme_flush(int fd, __u32 nsid);
// int nvme_write_zeros(int fd, __u32 nsid, __u64 slba, __u16 nlb,
// 		     __u16 control, __u32 reftag, __u16 apptag, __u16 appmask);

/* MAIN HANDLE */
int nvme_operation_handler(__u64 slba, __u64 size, unsigned short op, void *data);
#endif				/* _NVME_LIB_H */
