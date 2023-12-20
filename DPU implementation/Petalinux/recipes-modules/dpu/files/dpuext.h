/*
 * Copyright (C) 2019 Xilinx, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 */

#ifndef _DPUEXT_H_
#define _DPUEXT_H_

#include <asm/cacheflush.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/thread_info.h>
#include <asm/uaccess.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
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
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/wait.h>

#include "dpucore.h"

typedef struct {
	volatile uint32_t done; //< 0x000	command done reg (1：done，0：not）
	volatile uint32_t sm_len_x; //< 0x004	vector length（unit:float）
	volatile uint32_t sm_len_y; //< 0x008	vector count
	volatile uint32_t src; //< 0x00c	source address, require 256 byte alignment
	volatile uint32_t dst; //< 0x010	destination address, require 256 byte alignment
	volatile uint32_t scale; //< 0x014	fix point
	volatile uint32_t sm_offset; //< 0x018	offset
	volatile uint32_t clr; //< 0x01c	clear interrupt	reg （1:clear，0：not）
	volatile uint32_t start; //< 0x020	start reg: valid on rising_edge,
	volatile uint32_t fc_input_channel; //< 0x024	fc input channel, maxinum 4096B
	volatile uint32_t fc_output_channel; //< 0x028	fc output channel,maxinum 4096B
	volatile uint32_t fc_batch; //< 0x02c	fc batch,
	volatile uint32_t fc_weight_start; //< 0x030	fc weight and bias start addr, 256B alignment
	volatile uint32_t fc_weight_end; //< 0x034	fc weight and bias end addr, 256B alignment
	volatile uint32_t calc_mod; //< 0x038	0: softmax; 1: fc
	volatile uint32_t dst_addr_sel; //< 0x03c	fix to 1: ddr,
	volatile uint32_t fc_relu_en; //< 0x040	fc relu,

} smfc_reg_t;

typedef struct {
	smfc_reg_t *regs;
	spinlock_t lck_reg;
	struct semaphore sem;
	wait_queue_head_t wq;
	int irqno;
	int intflg;
	int timeout;
} run_smfc_t;

typedef struct {
	volatile uint32_t ctrl; //< 0x000 ctrl register
	volatile uint32_t channel; //< 0x004 source channel
	volatile uint32_t src_size; //< 0x008 source size([31:16]height,[15:0]width)
	volatile uint32_t dst_size; //< 0x00c target size([31:16]height,[15:0]width)
	volatile uint32_t scale_w; //< 0x010 scale width
	volatile uint32_t scale_h; //< 0x014 scale height
	volatile uint32_t src_addr; //< 0x018 source address
	volatile uint32_t dst_addr; //< 0x01c target address
	volatile uint32_t addr_stride; //< 0x020 stride
	volatile uint32_t method; //< 0x024 interpolation method
	volatile uint32_t start; //< 0x028 start register: valid on rising_edge
	volatile uint32_t done_irs; //< 0x02c interrupt raw status(1：done，0：not)
	volatile uint32_t clr; //< 0x030 interrupt clear register
	volatile uint32_t bias; //< 0x034 reisze bias([15:8]dst,[7:0]src)
} resize_reg_t;

typedef struct __run_resize_t {
	resize_reg_t *regs;
	spinlock_t lck_reg;
	struct semaphore sem;
	wait_queue_head_t wq;
	int irqno;
	int intflg;
	int timeout;
} run_resize_t;

//////////////////////////////////////////////////
// extend modules functions declare
int init_ext(struct device_node *pnode);
void exit_ext(void);
void _show_ext_regs(struct seq_file *file, u32 mask);
int dpu_softmax(struct ioc_softmax_t *para);
int dpu_fullconnect(struct ioc_fc_t *para);
int dpu_resize(run_resize_t *presize);
irqreturn_t dpu_ext_isr(int irq, void *data);
//////////////////////////////////////////////////
#endif
