/*
 * Copyright (C) 2019 Xilinx, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 */

#ifndef _DPUCORE_H_
#define _DPUCORE_H_

#include <asm/cacheflush.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/thread_info.h>
#include <asm/uaccess.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>
#include <linux/export.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mempolicy.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/version.h>
#include <linux/wait.h>

#include "dpudef.h"

#define DPU_DRIVER_VERSION "3.0.0"

#define DPU_IDLE 0
#define DPU_RUNNING 1
#define DPU_DISABLE 2
#define FALSE 0
#define TRUE 1

#define MAX_CORE_NUM 4

#define PLEVEL_ERR 9
#define PLEVEL_INFO 5
#define PLEVEL_DBG 1

#define MAX_REG_NUM 32

#define dprint(level, fmt, args...)                          		\
	do {                                                       	\
		if ((level) >= debuglevel)                              \
			printk(KERN_ERR "[DPU][%d]" fmt, current->pid,  \
			       ##args);                                 \
	} while (0)

#define dpr_init(fmt, args...) pr_alert("[DPU][%d]" fmt, current->pid, ##args);

/*dpu registers*/
typedef struct __DPUReg {
	/*dpu pmu registers*/
	struct __regs_dpu_pmu {
		volatile uint32_t version;
		volatile uint32_t reset;
		volatile uint32_t _rsv[62];
	} pmu;

	/*dpu rgbout registers*/
	struct __regs_dpu_rgbout {
		volatile uint32_t display;
		volatile uint32_t _rsv[63];
	} rgbout;

	/*dpu control registers struct*/
	struct __regs_dpu_ctrl {
		volatile uint32_t hp_ctrl;
		volatile uint32_t addr_io;
		volatile uint32_t addr_weight;
		volatile uint32_t addr_code;
		volatile uint32_t addr_prof;
		volatile uint32_t prof_value;
		volatile uint32_t prof_num;
		volatile uint32_t prof_en;
		volatile uint32_t start;
		volatile uint32_t com_addr[16]; //< extension for DPUv1.3.0
		volatile uint32_t _rsv[39];

	} ctlreg[MAX_CORE_NUM];

	/*dpu interrupt registers struct*/
	struct __regs_dpu_intr {
		volatile uint32_t isr;
		volatile uint32_t imr;
		volatile uint32_t irsr;
		volatile uint32_t icr;
		volatile uint32_t _rsv[60];

	} intreg;

} DPUReg;
/*dpu device struct*/
struct dpu_dev {
	struct miscdevice dev; //< dpu device

	atomic_t core; //< dpu core number available
	atomic_t ref_count; //< dpu device open count

	struct semaphore sem; //< task semaphore, in simple schedule strategy

	struct list_head tasklist; //< task list waiting

	wait_queue_head_t waitqueue[MAX_CORE_NUM]; //< waitqueue of each DPU

	int irq_no[MAX_CORE_NUM]; //< interrupt NO. of DPU
	pid_t pid[MAX_CORE_NUM]; //< pid of current task
	int state[MAX_CORE_NUM]; //< state of DPU (IDEL/RUNNING)
	unsigned long task_id[MAX_CORE_NUM]; //< task id of current task
	u64 time_start[MAX_CORE_NUM]; //< start time of current task
	u64 time_end[MAX_CORE_NUM]; //< finish time of current task

	int intflg[MAX_CORE_NUM];
};

/*task node struct*/
struct task_node {
	unsigned long task_id;
	unsigned long pid;
	unsigned long priority;

	wait_queue_head_t runwait;
	struct semaphore runsem;
	struct list_head list;
};

/*memory block node struct*/
struct memblk_node {
	unsigned long size;
	unsigned long virt_addr;
	dma_addr_t phy_addr;
	pid_t pid;
	struct list_head list;
};

//////////////////////////////////////////////////
// helper functions declare
struct device_node *dpu_compatible_node(const char *compat);
//////////////////////////////////////////////////
#endif /*_DPU_H_*/

