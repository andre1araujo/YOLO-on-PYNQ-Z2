/*
 * Copyright (C) 2019 Xilinx, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 */

#include "dpucore.h"
#include "dpuext.h"

#define DEVICE_NAME "dpu"
#define PROCFNAME "dpu_info"

//DPU signature base address
unsigned long signature_addr = SIG_BASE;
module_param(signature_addr, ulong, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
//control which extension is enabled(corresponding bit set to 1)
unsigned long extension = -1; // whether use cache; 0:no, 1:yes
module_param(extension, ulong, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#if defined(CACHE_OFF)
int cache = 0; // whether use cache; 0:no, 1:yes
#else
int cache = 1; // whether use cache; 0:no, 1:yes
#endif
module_param(cache, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
int timeout = 5; // (s)
module_param(timeout, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
char *mode = "normal";
module_param(mode, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
int profiler = 0;
module_param(profiler, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
int debuglevel = PLEVEL_ERR; // debug level
module_param(debuglevel, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
char *version = "DPU Driver version " DPU_DRIVER_VERSION "\nBuild Label: " __DATE__ " " __TIME__;
module_param(version, charp, S_IRUSR | S_IRGRP | S_IROTH);
uint coremask = 0xff;
module_param(coremask, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
uint accipmask = 0x0;
module_param(accipmask, uint, S_IRUSR | S_IRGRP | S_IROTH);

// the following parameters read from device tree
static int DPU_CORE_NUM;
//static uint32_t DPU_BASE;
void *RAM_BASEADDR_VIRT;
dma_addr_t RAM_BASEADDR;
static uint32_t RAM_SIZE;
static uint32_t PROF_RAM_SIZE = 0x100000;

// debug proc file infomation
struct proc_dir_entry *proc_file;

static struct device *dev_handler;
static struct dpu_dev dpudev;

dpu_caps_t dpu_caps;

// debug vars
int _run_counter[MAX_CORE_NUM] = { 0 }, _int_counter[MAX_CORE_NUM] = { 0 };

/*dpu registers*/
DPUReg *pdpureg;

#ifdef PORT_COUNTER
spinlock_t portlock;
u32 *port_counter;
#endif

struct list_head head_alloc; /*head of alloced memory block*/

struct semaphore memblk_lock;
spinlock_t tasklstlock;
spinlock_t idlock, corelock, taskidlock;
spinlock_t reglock;

/**
 * dpu_alloc_mem -  alloc a memory block from the available memory list.
 * @memsize : size of memory
 *
 *  RETURN: address of alloced memory;  NULL returned if no enough space exists
 */
unsigned long dpu_alloc_mem(uint32_t memsize)
{
	void *virtaddr;
	dma_addr_t phy_addr;
	struct memblk_node *pnewnode;

	memsize = (memsize + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1); //at least one page frame

	virtaddr = dma_alloc_coherent(dev_handler, memsize, &phy_addr, GFP_KERNEL);
	if (NULL != virtaddr) {
		pnewnode = kmalloc(sizeof(struct memblk_node), GFP_KERNEL);
		dprint(PLEVEL_DBG, "Alloc Mem:0x%lx size=0x%x\n", (unsigned long)phy_addr, memsize);

		if (pnewnode) {
			pnewnode->virt_addr = (unsigned long)virtaddr;
			pnewnode->size = memsize;
			pnewnode->phy_addr = phy_addr;
			pnewnode->pid = current->pid;

			down(&memblk_lock);
			list_add(&pnewnode->list, &head_alloc);
			up(&memblk_lock);
		} else {
			dma_free_coherent(dev_handler, memsize, virtaddr, phy_addr);
			phy_addr = 0;
			dprint(PLEVEL_ERR, "kmalloc fail when adding memory node\n");
		}
		return phy_addr;
	} else {
		return 0;
	}
}

/**
 * dpu_free_mem - remove the memory block frome alloc list to the available
 *                memory list and merge with the neighbor node if necessary
 * @paddr :  address of memory block to be free
 */
int dpu_free_mem(void *paddr)
{
	struct list_head *plist;
	struct memblk_node *p;

	down(&memblk_lock);

	list_for_each (plist, &head_alloc) {
		p = list_entry(plist, struct memblk_node, list);
		if (p->phy_addr == (dma_addr_t)paddr) {
			dma_free_coherent(dev_handler, p->size, (void *)p->virt_addr, p->phy_addr);
			list_del(&p->list);
			kfree(p);
			up(&memblk_lock);
			return 0;
		}
	}
	up(&memblk_lock);

	dprint(PLEVEL_ERR, "free memory failed,address=0x%p\n", paddr);

	return -ENXIO;
}

/**
 * dpu_create_kernel - create a kernel ID
 *  the generated ID increased by one on each call
 * @pkernel : kernel structure
 */
static void dpu_create_kernel(struct ioc_kernel_manu_t *pkernel)
{
	static unsigned long dpu_kernel_id = 1;

	spin_lock(&idlock);

	pkernel->kernel_id = dpu_kernel_id++;

	spin_unlock(&idlock);
}

/**
 * dpu_create_task - create a task ID
 *  the generated ID increased by one on each call
 * @pkernel : kernel structure
 */
static void dpu_create_task(struct ioc_task_manu_t *pkernel)
{
	static unsigned long dpu_task_id = 1;

	spin_lock(&taskidlock);

	pkernel->task_id = dpu_task_id++;

	spin_unlock(&taskidlock);
}

/**
 * _dpu_regs_init - dpu registers initialize
 * @channel: the dpu channel [0,DPU_CORE_NUM) need to be initialize,
 *			set all channel if the para is DPU_CORE_NUM
 */
static void _dpu_regs_init(int channel)
{
	uint32_t tmp;
	// reset specific channel : only one dpu now,set the reset reg bit0
	int i = 0;
	int j;
	if ((channel >= 0) && (channel < DPU_CORE_NUM)) {
		tmp = ioread32(&pdpureg->pmu.reset);
		iowrite32(tmp & ~(1 << channel), &pdpureg->pmu.reset);

                iowrite32(0x07070f0f, &pdpureg->ctlreg[channel].hp_ctrl);
		iowrite32(0, &pdpureg->ctlreg[channel].prof_en);
		iowrite32(0, &pdpureg->ctlreg[channel].start);

		iowrite32((1 << channel), &pdpureg->intreg.icr);
		udelay(1); // wait 1us
		iowrite32(0, &pdpureg->intreg.icr);

		iowrite32(tmp | (1 << channel), &pdpureg->pmu.reset);

	} else if (channel == DPU_CORE_NUM) {
                iowrite32(0, &pdpureg->pmu.reset);

                for (i = 0; i < DPU_CORE_NUM; i++) {
			iowrite32(0x07070f0f, &pdpureg->ctlreg[i].hp_ctrl);
			iowrite32(0, &pdpureg->ctlreg[i].prof_en);
			iowrite32(0, &pdpureg->ctlreg[i].start);

			iowrite32(0, &(pdpureg->ctlreg[i].addr_code));
			iowrite32(0, &(pdpureg->ctlreg[i].addr_prof));
			iowrite32(0, &(pdpureg->ctlreg[i].addr_io));
			iowrite32(0, &(pdpureg->ctlreg[i].addr_weight));

			for (j = 0; j < 16; j++)
				iowrite32(0, &(pdpureg->ctlreg[i].com_addr[j]));
		}

		iowrite32(0xFF, &pdpureg->intreg.icr);
		udelay(1); // wait 1us
		iowrite32(0, &pdpureg->intreg.icr);

		iowrite32(0xFFFFFFFF, &pdpureg->pmu.reset);

	}
}

/**
 * _show_debug_info - print information for debug
 *
 */
inline void _show_debug_info(void)
{
	int i;

	dprint(PLEVEL_ERR, "[DPU debug info]\nlevel = %d\n", debuglevel);
	for (i = 0; i < DPU_CORE_NUM; i++) {
		dprint(PLEVEL_ERR, "Core %d schedule  counter: %d\n", i, _run_counter[i]);
		dprint(PLEVEL_ERR, "Core %d interrupt counter: %d\n", i, _int_counter[i]);
	}
}

/**
 * _show_dpu_regs - print dpu registers' value
 *
 */
inline void _show_dpu_regs(void)
{
	int i = 0, j = 0;
	unsigned long flags;

	spin_lock_irqsave(&reglock, flags);

	dprint(PLEVEL_ERR, "[DPU Registers]\n");

	dprint(PLEVEL_ERR, "%-10s\t: 0x%.8x\n", "VER", pdpureg->pmu.version);
	dprint(PLEVEL_ERR, "%-10s\t: 0x%.8x\n", "RST", pdpureg->pmu.reset);
	dprint(PLEVEL_ERR, "%-10s\t: 0x%.8x\n", "ISR", pdpureg->intreg.isr);
	dprint(PLEVEL_ERR, "%-10s\t: 0x%.8x\n", "IMR", pdpureg->intreg.imr);
	dprint(PLEVEL_ERR, "%-10s\t: 0x%.8x\n", "IRSR", pdpureg->intreg.irsr);
	dprint(PLEVEL_ERR, "%-10s\t: 0x%.8x\n", "ICR", pdpureg->intreg.icr);
	dprint(PLEVEL_ERR, "\n");
	for (i = 0; i < DPU_CORE_NUM; i++) {
		dprint(PLEVEL_ERR, "%-8s\t: %d\n", "DPU Core", i);
		dprint(PLEVEL_ERR, "%-8s\t: 0x%.8x\n", "HP_CTL", pdpureg->ctlreg[i].hp_ctrl);
		dprint(PLEVEL_ERR, "%-8s\t: 0x%.8x\n", "ADDR_IO", pdpureg->ctlreg[i].addr_io);
		dprint(PLEVEL_ERR, "%-8s\t: 0x%.8x\n", "ADDR_WEIGHT",
		       pdpureg->ctlreg[i].addr_weight);
		dprint(PLEVEL_ERR, "%-8s\t: 0x%.8x\n", "ADDR_CODE", pdpureg->ctlreg[i].addr_code);
		dprint(PLEVEL_ERR, "%-8s\t: 0x%.8x\n", "ADDR_PROF", pdpureg->ctlreg[i].addr_prof);
		dprint(PLEVEL_ERR, "%-8s\t: 0x%.8x\n", "PROF_VALUE", pdpureg->ctlreg[i].prof_value);
		dprint(PLEVEL_ERR, "%-8s\t: 0x%.8x\n", "PROF_NUM", pdpureg->ctlreg[i].prof_num);
		dprint(PLEVEL_ERR, "%-8s\t: 0x%.8x\n", "PROF_EN", pdpureg->ctlreg[i].prof_en);
		dprint(PLEVEL_ERR, "%-8s\t: 0x%.8x\n", "START", pdpureg->ctlreg[i].start);
#if defined(CONFIG_DPU_v1_3_0)
		for (j = 0; j < 8; j++) {
			dprint(PLEVEL_ERR, "%-8s%d\t: 0x%.8x\n", "COM_ADDR_L", j,
			       pdpureg->ctlreg[i].com_addr[j * 2]);
			dprint(PLEVEL_ERR, "%-8s%d\t: 0x%.8x\n", "COM_ADDR_H", j,
			       pdpureg->ctlreg[i].com_addr[j * 2 + 1]);
		}
#endif
		dprint(PLEVEL_ERR, "\n");
	}

	spin_unlock_irqrestore(&reglock, flags);
}

/**
 * _get_state_str - get dpu state description string
 */
inline char *_get_state_str(int core)
{
	int state = dpudev.state[core];
	if (!(coremask & (1 << core)))
		return "Disable";
	switch (state) {
	case DPU_IDLE:
		return "Idle";
	case DPU_RUNNING:
		return "Running";
	case DPU_DISABLE:
		return "Disable";
	default:
		return "UNDEF";
	}
}
/**
 * _get_available_core - get an available dpu core
 * @ret : return the dpu core number
 * @note: the valid dpu core number range: [0,DPU_CORE_NUM) ;
 *		  invalid if return DPU_CORE_NUM
 */
inline int _get_available_core(void)
{
	int ret, i, dpu_core = DPU_CORE_NUM;
	unsigned long flags;
	do {
		ret = down_interruptible(&dpudev.sem);
	} while (ret < 0);

	spin_lock_irqsave(&corelock, flags);

	for (i = 0; i < DPU_CORE_NUM; i++) {
		if (dpudev.state[i] == DPU_IDLE) {
			dpudev.state[i] = DPU_RUNNING;
			dpudev.intflg[i] = FALSE;
			dpu_core = i;
			break;
		}
	}

	spin_unlock_irqrestore(&corelock, flags);

	return dpu_core;
}
/**
 *  dpu_run -run dpu function
 * @prun : dpu run struct, contains the necessary address info
 *
 */
int dpu_run(struct ioc_kernel_run2_t *prun)
{
	int i, ret = 0;
	unsigned long flags;
	int dpu_core = DPU_CORE_NUM;

	dpu_core = _get_available_core();
	if (dpu_core == DPU_CORE_NUM) {
		// should never get here
		dprint(PLEVEL_ERR, "ERR_CORE_NUMBER!\n");
		spin_lock_irqsave(&corelock, flags);
		for (i = 0; i < DPU_CORE_NUM; i++) {
			dprint(PLEVEL_ERR, "Core%d state:%d,task_id:%ld\n", i, dpudev.state[i],
			       dpudev.task_id[i]);
		}
		spin_unlock_irqrestore(&corelock, flags);
		return -EINTR;
	}

	dpudev.task_id[dpu_core] = prun->handle_id;
	dpudev.pid[dpu_core] = current->pid;
	_run_counter[dpu_core]++;

	spin_lock_irqsave(&reglock, flags);
// extract the address info
#ifdef CONFIG_DPU_v1_1_X
	iowrite32(prun->addr_code >> 12, &(pdpureg->ctlreg[dpu_core].addr_code));
	iowrite32(prun->addr_io >> 12, &(pdpureg->ctlreg[dpu_core].addr_io));
	iowrite32(prun->addr_weight >> 12, &(pdpureg->ctlreg[dpu_core].addr_weight));
	if (profiler) {
		iowrite32((RAM_BASEADDR + PROF_RAM_SIZE * (dpu_core)) >> 12,
			  &(pdpureg->ctlreg[dpu_core].addr_prof));
		iowrite32(0x1, &(pdpureg->ctlreg[dpu_core].prof_en));
	}
	iowrite32(0x1, &(pdpureg->ctlreg[dpu_core].start));
#elif defined CONFIG_DPU_v1_3_0
	iowrite32(prun->addr_code >> 12, &(pdpureg->ctlreg[dpu_core].addr_code));
	iowrite32(prun->addr0, &pdpureg->ctlreg[dpu_core].com_addr[0]);
	iowrite32(prun->addr1, &pdpureg->ctlreg[dpu_core].com_addr[2]);
	iowrite32(prun->addr2, &pdpureg->ctlreg[dpu_core].com_addr[4]);
	iowrite32(prun->addr3, &pdpureg->ctlreg[dpu_core].com_addr[6]);
	iowrite32(prun->addr4, &pdpureg->ctlreg[dpu_core].com_addr[8]);
	iowrite32(prun->addr5, &pdpureg->ctlreg[dpu_core].com_addr[10]);
	iowrite32(prun->addr6, &pdpureg->ctlreg[dpu_core].com_addr[12]);
	iowrite32(prun->addr7, &pdpureg->ctlreg[dpu_core].com_addr[14]);
	for (i = 0; i < 8; i++) // ADDR_H not support now, just write 0
		iowrite32(0, &pdpureg->ctlreg[dpu_core].com_addr[i * 2 + 1]);
	if (profiler) {
		iowrite32((RAM_BASEADDR + PROF_RAM_SIZE * (dpu_core)) >> 12,
			  &(pdpureg->ctlreg[dpu_core].addr_prof));
		iowrite32(0x1, &(pdpureg->ctlreg[dpu_core].prof_en));
	}
	iowrite32(0x1, &(pdpureg->ctlreg[dpu_core].start));
#endif
// record the start time
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	prun->time_start = ktime_get();
#else
	prun->time_start = ktime_get().tv64;
#endif
	prun->core_id = dpu_core;

	dpudev.time_start[dpu_core] = prun->time_start;

	spin_unlock_irqrestore(&reglock, flags);

#ifndef CONFIG_DPU_COMPATIBLE_V1_07
	// wait for the dpu task to be finished
	ret = wait_event_interruptible_timeout(dpudev.waitqueue[dpu_core],
					       dpudev.intflg[dpu_core] == TRUE, timeout * USER_HZ);

	dpudev.intflg[dpu_core] = FALSE;

	if (ret == 0) {
		dprint(PLEVEL_ERR,
		       "[PID %d][taskID %ld]Core %d Run timeout,failed to get finish interrupt!\n",
		       current->pid, prun->handle_id, dpu_core);
		_show_debug_info();

		_show_dpu_regs();

		_dpu_regs_init(dpu_core);

	} else if (ret < 0) {
		dprint(PLEVEL_INFO, "program interrupted by user!\n");
		dprint(PLEVEL_INFO, "[ret=%d][core=%d,status=%d]\n", ret, dpu_core,
		       dpudev.state[dpu_core]);

		_dpu_regs_init(dpu_core);

	} else {
		dprint(PLEVEL_DBG, "[core %i]Task takes: %lld ns\n", dpu_core,
		       dpudev.time_end[dpu_core] - dpudev.time_start[dpu_core]);
		prun->time_end = dpudev.time_end[dpu_core];
	}

	spin_lock_irqsave(&corelock, flags);
	dpudev.state[dpu_core] = DPU_IDLE;
	spin_unlock_irqrestore(&corelock, flags);
	up(&dpudev.sem);
#endif
	return ret > 0 ? 0 : (ret == 0 ? -ETIMEDOUT : ret);
}

#ifdef CONFIG_DPU_COMPATIBLE_V1_07
/**
 * dpu_wait_task_finish - check if dpu finished its job
 * @pkernel : contains task_id info
 * */
int dpu_wait_task_finish(struct ioc_kernel_done_t *pkernel)
{
	int ret, i;
	unsigned long flags;
	int dpu_core = DPU_CORE_NUM;

	spin_lock_irqsave(&corelock, flags);
	for (i = 0; i < DPU_CORE_NUM; i++) {
		if (dpudev.task_id[i] == pkernel->handle_id) {
			dpu_core = i;
			break;
		}
	}
	spin_unlock_irqrestore(&corelock, flags);

	if (dpu_core == DPU_CORE_NUM) {
		_show_debug_info();
		return -EINVAL;
	}
	// wait for the dpu task to be finished
	ret = wait_event_interruptible_timeout(dpudev.waitqueue[dpu_core],
					       dpudev.intflg[dpu_core] == TRUE, timeout * USER_HZ);

	dpudev.intflg[dpu_core] = FALSE;

	if (ret == 0) {
		dprint(PLEVEL_ERR,
		       "[PID %d][taskID %ld]Core %d Run timeout,failed to get finish interrupt!\n",
		       current->pid, pkernel->handle_id, dpu_core);
		_show_debug_info();

		_show_dpu_regs();

		_dpu_regs_init(dpu_core);

	} else if (ret < 0) {
		dprint(PLEVEL_INFO, "program interrupted by user!\n");
		dprint(PLEVEL_INFO, "[ret=%d][core=%d,status=%d]\n", ret, dpu_core,
		       dpudev.state[dpu_core]);

		_dpu_regs_init(dpu_core);

	} else {
		dprint(PLEVEL_DBG, "[core %i]finish time: %lld\n", dpu_core,
		       dpudev.time_end[dpu_core]);
		pkernel->time_end = dpudev.time_end[dpu_core];
	}

	spin_lock_irqsave(&corelock, flags);
	dpudev.state[dpu_core] = DPU_IDLE;
	up(&dpudev.sem);
	spin_unlock_irqrestore(&corelock, flags);

	return 0;
}
#endif

static void get_port_counter(struct port_profile_t *counter)
{
#ifdef PORT_COUNTER
	spin_lock(&portlock);
	// ugly code for a special hardware.
	counter->hp0_read_cnt = ioread32(port_counter + 0);
	counter->hp0_write_cnt = ioread32(port_counter + 1);
	counter->hp1_read_cnt = ioread32(port_counter + 2);
	counter->hp1_write_cnt = ioread32(port_counter + 3);
	counter->hp2_read_cnt = ioread32(port_counter + 4);
	counter->hp2_write_cnt = ioread32(port_counter + 5);
	counter->hp3_read_cnt = ioread32(port_counter + 6);
	counter->hp3_write_cnt = ioread32(port_counter + 7);
	counter->gp_read_cnt = ioread32(port_counter + 8);
	counter->gp_write_cnt = ioread32(port_counter + 9);
	counter->clock_cnt = ioread32(port_counter + 10) + ioread32(port_counter + 11) << 32;
	spin_unlock(&portlock);

#endif
}

/**
 * _flush_cache_range - flush memory range to ensure content is flushed to RAM
 * @pmem: memory fresh structure contains start virtual address and size
 */
void _flush_cache_range(struct ioc_mem_fresh_t *pmem)
{
	dma_sync_single_for_device(dev_handler, pmem->paddr, pmem->size, DMA_BIDIRECTIONAL);
}

/**
 * _invalid_cache_range - invalid memory range to ensure following reading comes from RAM
 * @pmem: memory fresh structure contains start virtual address and size
 */
void _invalid_cache_range(struct ioc_mem_fresh_t *pmem)
{
	dma_sync_single_for_cpu(dev_handler, pmem->paddr, pmem->size, DMA_BIDIRECTIONAL);
}
/**
 * dpu ioctl function
 */
static long dpu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	dprint(PLEVEL_DBG, "[PID %i]name:%s,func:%s\n", current->pid, current->comm, __func__);
	dprint(PLEVEL_INFO, "IOCTL CMD = 0x%x (1:Create;2:Alloc;3:Free;4:Run;6:Done)\n", cmd);

	switch (cmd) {
	case DPU_IOCTL_CREATE_KERNEL: { // create a kernel
		struct ioc_kernel_manu_t t;
		dpu_create_kernel(&t);
		if (copy_to_user((void *)arg, &t, sizeof(struct ioc_kernel_manu_t)))
			return -EINVAL;
		break;
	}
	case DPU_IOCTL_MEM_ALLOC: { // memory alloc
		struct ioc_mem_alloc_t t;
		if (copy_from_user(&t, (void *)arg, sizeof(struct ioc_mem_alloc_t))) {
			return -EINVAL;
		}
		if (t.size == 0)
			return -EINVAL;
		t.addr_phy = dpu_alloc_mem(t.size);
		if (t.addr_phy == 0)
			return -ENOMEM;
		if (copy_to_user((void *)arg, &t, sizeof(struct ioc_mem_alloc_t)))
			return -EINVAL;
		break;
	}
	case DPU_IOCTL_MEM_FREE: { // memory free
		struct ioc_mem_free_t t;
		if (copy_from_user(&t, (void *)arg, sizeof(struct ioc_mem_free_t))) {
			return -EINVAL;
		}
		ret = dpu_free_mem((void *)t.addr_phy);

		break;
	}
	case DPU_IOCTL_RUN: { // run dpu
		struct ioc_kernel_run_t t;
		struct ioc_kernel_run2_t t2;
		if (copy_from_user(&t, (void *)arg, sizeof(struct ioc_kernel_run_t))) {
			return -EINVAL;
		}
#ifdef CONFIG_DPU_v1_1_X
		t2.handle_id = t.handle_id;
		t2.addr_code = t.addr_code;
		t2.addr0 = t.addr_io;
		t2.addr1 = t.addr_weight;
#elif defined(CONFIG_DPU_v1_3_0)
		t2.handle_id = t.handle_id;
		t2.addr_code = t.addr_code;
		t2.addr0 = t.addr0;
		t2.addr1 = t.addr1;
		t2.addr2 = t.addr2;
		t2.addr3 = t.addr3;
		t2.addr4 = t.addr4;
		t2.addr5 = t.addr5;
		t2.addr6 = t.addr6;
		t2.addr7 = t.addr7;
#endif
		ret = dpu_run(&t2);

		t.time_start = t2.time_start;
		t.time_end = t2.time_end;
		t.core_id = t2.core_id;
		if (copy_to_user((void *)arg, &t, sizeof(struct ioc_kernel_run_t)))
			return -EINVAL;

		break;
	}
	case DPU_IOCTL_RUN2: { // run dpu
		struct ioc_kernel_run2_t t;
		if (copy_from_user(&t, (void *)arg, sizeof(struct ioc_kernel_run2_t))) {
			return -EINVAL;
		}
		ret = dpu_run(&t);

		if (copy_to_user((void *)arg, &t, sizeof(struct ioc_kernel_run2_t)))
			return -EINVAL;

		break;
	}
	case DPU_IOCTL_RESET: { // reset dpu

		_dpu_regs_init(DPU_CORE_NUM);

		break;
	}
#ifdef CONFIG_DPU_COMPATIBLE_V1_07
	case DPU_IOCTL_DONE: { // wait for the dpu task done
		struct ioc_kernel_done_t t;
		if (copy_from_user(&t, (void *)arg, sizeof(struct ioc_kernel_done_t))) {
			return -EINVAL;
		}
		ret = dpu_wait_task_finish(&t);
		if ((ret != 0)
			return ret;
		if (copy_to_user((void *)arg, &t, sizeof(struct ioc_kernel_done_t)))
			return -EINVAL;

		break;
	}
#endif
	case DPU_IOCTL_DUMP_STATUS: {
		return -EINVAL;
		break;
	}
	case DPU_IOCTL_CREATE_TASK: { // create taskid
		struct ioc_task_manu_t t;
		dpu_create_task(&t);
		if (copy_to_user((void *)arg, &t, sizeof(struct ioc_task_manu_t)))
			return -EINVAL;
		break;
	}
	case DPU_IOCTL_PORT_PROFILE: { // create taskid
		struct port_profile_t t;
		get_port_counter(&t);
		if (copy_to_user((void *)arg, &t, sizeof(struct port_profile_t)))
			return -EINVAL;
		break;
	}
	case DPU_IOCTL_RUN_SOFTMAX: { // run softmax
		{ // softmax calculation using SMFC
			struct ioc_softmax_t t;
			if (copy_from_user(&t, (void *)arg, sizeof(struct ioc_softmax_t))) {
				return -EINVAL;
			}
			ret = dpu_softmax(&t);
		}
		break;
	}
	case DPU_IOCTL_CACHE_FLUSH: { // flush cache range by physical address
		struct ioc_mem_fresh_t t;
		if (copy_from_user(&t, (void *)arg, sizeof(struct ioc_mem_fresh_t)))
			return -EINVAL;
		_flush_cache_range(&t);
		break;
	}
	case DPU_IOCTL_CACHE_INVALID: { // invalidate cache range by physical address
		struct ioc_mem_fresh_t t;
		if (copy_from_user(&t, (void *)arg, sizeof(struct ioc_mem_fresh_t)))
			return -EINVAL;
		_invalid_cache_range(&t);
		break;
	}
	case DPU_IOCTL_SET_CACHEFLG: { // set cache flag
		if (arg == 0 || arg == 1) {
			cache = arg;
		} else {
			return -EINVAL;
		}
		break;
	}

	case DPU_IOCTL_RUN_FC: {
		struct ioc_fc_t t;
		if (copy_from_user(&t, (void *)arg, sizeof(struct ioc_fc_t))) {
			return -EINVAL;
		}
		ret = dpu_fullconnect(&t);
		break;
	}
	case DPU_IOCTL_RUN_RESIZE: {
		run_resize_t t;
		if (copy_from_user(&t, (void *)arg, sizeof(run_resize_t))) {
			return -EINVAL;
		}
		//ret = dpu_resize(&t);
		break;
	}
	case DPU_IOCTL_CAPS: { // dpu capabilities
		if (copy_to_user((void *)arg, &dpu_caps, sizeof(dpu_caps_t))) {
			return -EINVAL;
		}
		break;
	}
	case DPU_IOCTL_CAPS_CORE: { // dpu capabilities
		int size = (dpu_caps.magic != DPU_CONF_MAGIC)? sizeof(dpu_info_t): sizeof(dpu_configurable_t);
		if (copy_to_user((void *)arg, dpu_caps.p_dpu_info, size * dpu_caps.dpu_cnt)) {
			return -EINVAL;
		}
		break;
	}
	default: {
		dprint(PLEVEL_INFO, "IOCTL CMD NOT SUPPORT!\n");
		ret = -EPERM;
		break;
	}
	}

	return ret;
}
/**
 * dpu interrupt service routine
 * when a task finished, dpu will generate a interrupt,
 * we can look up the IRQ No. to determine the channel
 */
irqreturn_t dpu_isr(int irq, void *data)
{
	int i = 0;
	unsigned long flags;

	spin_lock_irqsave(&reglock, flags);

	// Determine which channel generated the interrupt
	for (i = 0; i < DPU_CORE_NUM; i++) {
		if (irq == dpudev.irq_no[i]) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
			dpudev.time_end[i] = ktime_get(); // ioread32(&pdpureg[i]->prof_tstamp);  //
#else
			dpudev.time_end[i] = ktime_get().tv64;
#endif
			// clear the interrupt
			iowrite32(0, &pdpureg->ctlreg[i].prof_en);
			iowrite32(0, &pdpureg->ctlreg[i].start);
			iowrite32((1 << i), &pdpureg->intreg.icr);
			udelay(1);
			iowrite32(0, &pdpureg->intreg.icr);

			_int_counter[i]++;
			// set the finish flag,record the time,and notify the waiting queue
			dpudev.intflg[i] = TRUE;

			wake_up_interruptible(&dpudev.waitqueue[i]);
		}
	}
	spin_unlock_irqrestore(&reglock, flags);

	return IRQ_HANDLED;
}

/**
 * dpu_open - some initialization
 */
int dpu_open(struct inode *inode, struct file *filp)
{
	dprint(PLEVEL_INFO, "[PID %i]name:%s,func:%s\n", current->pid, current->comm, __func__);

	if (atomic_read(&dpudev.ref_count) == 0) {
		int i;
		int available_core = DPU_CORE_NUM;
		for (i = 0; i < DPU_CORE_NUM; i++) {
			if (coremask & (1 << i)) {
				_dpu_regs_init(i);
				dpudev.state[i] = DPU_IDLE;
			} else {
				dpudev.state[i] = DPU_DISABLE;
				available_core--;
			}
		}
		sema_init(&dpudev.sem, available_core);
	}

	atomic_inc(&dpudev.ref_count);

	return 0;
}
/**
 * dpu_release - dpu close function
 * */
int dpu_release(struct inode *inode, struct file *filp)
{
	struct list_head *plist, *nlist;
	struct memblk_node *p;

	dprint(PLEVEL_INFO, "[PID %i]name:%s,func:%s\n", current->pid, current->comm, __func__);

	if (atomic_dec_and_test(&dpudev.ref_count)) {
		down(&memblk_lock);
		list_for_each_safe (plist, nlist, &head_alloc) {
			p = list_entry(plist, struct memblk_node, list);
			dma_free_coherent(dev_handler, p->size, (void *)p->virt_addr, p->phy_addr);
			list_del(&p->list);
			kfree(p);
		}
		INIT_LIST_HEAD(&head_alloc);
		up(&memblk_lock);

		INIT_LIST_HEAD(&dpudev.tasklist);
	}

	return 0;
}

/**
 * dpu_read -  not SUPPORT now
 */
static ssize_t dpu_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	dprint(PLEVEL_INFO, "[PID %i]name:%s,func:%s\n", current->pid, current->comm, __func__);

	return 0;
}

/**
 * dpu_write - not SUPPORT now
 */
ssize_t dpu_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	dprint(PLEVEL_INFO, "[PID %i]name:%s,func:%s\n", current->pid, current->comm, __func__);

	return 0;
}

/**
 * dpu mmap function
 */
static int dpu_mmap(struct file *file, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;

	if (!cache)
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot)) {
		return -EAGAIN;
	}
	return 0;
}
/*dpu file operation define */
static struct file_operations dev_fops = {

	.owner = THIS_MODULE,
	.unlocked_ioctl = dpu_ioctl,
	.open = dpu_open,
	.release = dpu_release,
	.read = dpu_read,
	.write = dpu_write,
	.mmap = dpu_mmap,
};

/**
 * _dpu_devstate_init - dpu device state initialize
 * @channel: the dpu channel [0,DPU_CORE_NUM) need to be initialize,
 *			 set all channel if the para is DPU_CORE_NUM
 */
static inline void _dpu_devstate_init(int channel)
{
	int i;
	if (channel == DPU_CORE_NUM) {
		dpudev.dev.name = DEVICE_NAME;
		dpudev.dev.minor = MISC_DYNAMIC_MINOR;
		dpudev.dev.fops = &dev_fops;
		dpudev.dev.mode = S_IWUGO | S_IRUGO;

		atomic_set(&dpudev.core, DPU_CORE_NUM);
		sema_init(&dpudev.sem, DPU_CORE_NUM);

		INIT_LIST_HEAD(&dpudev.tasklist);

		for (i = 0; i < DPU_CORE_NUM; i++) {
			init_waitqueue_head(&dpudev.waitqueue[i]);
			dpudev.state[i] = DPU_IDLE;

			dpudev.pid[i] = 0;
			dpudev.task_id[i] = 0;
			dpudev.time_start[i] = 0;
			dpudev.time_end[i] = 0;
		}

	} else if ((channel >= 0) && (channel < DPU_CORE_NUM)) {
		init_waitqueue_head(&dpudev.waitqueue[channel]);
		dpudev.state[channel] = DPU_IDLE;
		dpudev.pid[channel] = 0;
		dpudev.task_id[channel] = 0;
		dpudev.time_start[channel] = 0;
		dpudev.time_end[channel] = 0;
	}
}

//////////////////////////////////////////////////////////
/**
 * proc_write - dpu proc file write function
 *              you can set debug level by "echo levelnum > /proc/dpu_info"
 */
static ssize_t proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos)
{
	int flg_memblk_reset = 0;
	char *kbuf = kzalloc((count + 1), GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;
	if (copy_from_user(kbuf, buffer, count)) {
		kfree(kbuf);
		return -EFAULT;
	}

	sscanf(kbuf, "%d%d", &debuglevel, &flg_memblk_reset);
	dprint(PLEVEL_DBG, "debug lever set to %d", debuglevel);

	if (flg_memblk_reset) {
		INIT_LIST_HEAD(&head_alloc);
	}
	return count;
}
/**
 * proc_show_dpuinfo - dpu proc file show infomation function:
 *	                 type "cat /proc/dpu_info " to get dpu driver
 *information
 */
static int proc_show_dpuinfo(struct seq_file *file, void *v)
{
	int i = 0, j = 0;
	uint32_t MemInUse = 0;
	unsigned long flags;
	struct list_head *plist;
	struct memblk_node *p;

	seq_printf(file, "[DPU Debug Info]\n");
	seq_printf(file, "%-10s\t: %d\n", "Debug level", debuglevel);
	for (i = 0; i < DPU_CORE_NUM; i++) {
		seq_printf(file, "Core %d schedule : %d\n", i, _run_counter[i]);
		seq_printf(file, "Core %d interrupt: %d\n", i, _int_counter[i]);
	}
	seq_printf(file, "\n");

	seq_printf(file, "[DPU Resource]\n");
	spin_lock_irqsave(&corelock, flags);
	for (i = 0; i < DPU_CORE_NUM; i++) {
		seq_printf(file, "%-10s\t: %d\n", "DPU Core", i);
		seq_printf(file, "%-10s\t: %s\n", "State", _get_state_str(i));
		seq_printf(file, "%-10s\t: %d\n", "PID", (dpudev.pid[i]));
		seq_printf(file, "%-10s\t: %ld\n", "TaskID", (dpudev.task_id[i]));
		seq_printf(file, "%-10s\t: %lld\n", "Start", dpudev.time_start[i]);
		seq_printf(file, "%-10s\t: %lld\n", "End", dpudev.time_end[i]);
		seq_printf(file, "\n");
	}
	spin_unlock_irqrestore(&corelock, flags);

	spin_lock_irqsave(&reglock, flags);

	seq_printf(file, "[DPU Registers]\n");
	seq_printf(file, "%-10s\t: 0x%.8x\n", "VER", pdpureg->pmu.version);
	seq_printf(file, "%-10s\t: 0x%.8x\n", "RST", pdpureg->pmu.reset);
	seq_printf(file, "%-10s\t: 0x%.8x\n", "ISR", pdpureg->intreg.isr);
	seq_printf(file, "%-10s\t: 0x%.8x\n", "IMR", pdpureg->intreg.imr);
	seq_printf(file, "%-10s\t: 0x%.8x\n", "IRSR", pdpureg->intreg.irsr);
	seq_printf(file, "%-10s\t: 0x%.8x\n", "ICR", pdpureg->intreg.icr);
	seq_printf(file, "\n");
	for (i = 0; i < DPU_CORE_NUM; i++) {
		seq_printf(file, "%-8s\t: %d\n", "DPU Core", i);
		seq_printf(file, "%-8s\t: 0x%.8x\n", "HP_CTL", pdpureg->ctlreg[i].hp_ctrl);
		seq_printf(file, "%-8s\t: 0x%.8x\n", "ADDR_IO", pdpureg->ctlreg[i].addr_io);
		seq_printf(file, "%-8s\t: 0x%.8x\n", "ADDR_WEIGHT", pdpureg->ctlreg[i].addr_weight);
		seq_printf(file, "%-8s\t: 0x%.8x\n", "ADDR_CODE", pdpureg->ctlreg[i].addr_code);
		seq_printf(file, "%-8s\t: 0x%.8x\n", "ADDR_PROF", pdpureg->ctlreg[i].addr_prof);
		seq_printf(file, "%-8s\t: 0x%.8x\n", "PROF_VALUE", pdpureg->ctlreg[i].prof_value);
		seq_printf(file, "%-8s\t: 0x%.8x\n", "PROF_NUM", pdpureg->ctlreg[i].prof_num);
		seq_printf(file, "%-8s\t: 0x%.8x\n", "PROF_EN", pdpureg->ctlreg[i].prof_en);
		seq_printf(file, "%-8s\t: 0x%.8x\n", "START", pdpureg->ctlreg[i].start);
#if defined(CONFIG_DPU_v1_3_0)
		for (j = 0; j < 8; j++) {
			seq_printf(file, "%-8s%d\t: 0x%.8x\n", "COM_ADDR_L", j,
				   pdpureg->ctlreg[i].com_addr[j * 2]);
			seq_printf(file, "%-8s%d\t: 0x%.8x\n", "COM_ADDR_H", j,
				   pdpureg->ctlreg[i].com_addr[j * 2 + 1]);
		}
#endif
		seq_printf(file, "\n");
	}
	spin_unlock_irqrestore(&reglock, flags);

	seq_printf(file, "[Memory Resource]\n");
	down(&memblk_lock);

	list_for_each(plist, &head_alloc) {
		p = list_entry(plist, struct memblk_node, list);
		MemInUse += p->size;
	}
	MemInUse /= 1024 * 1024;
	seq_printf(file, "%-8s\t: %8d MB\n", "MemInUse", MemInUse);
	if (PLEVEL_DBG >= debuglevel) {
		seq_printf(file, "**************   memory in use  **************\n");
		list_for_each(plist, &head_alloc) {
			p = list_entry(plist, struct memblk_node, list);
			seq_printf(file, "addr:0x%.8lx,size:0x%.8lx,pid:%8d\n", p->virt_addr,
				   p->size, p->pid);
		}
	}
	up(&memblk_lock);

	_show_ext_regs(file, accipmask);
	return 0;
}

static int proc_key_open(struct inode *inode, struct file *file)
{
	single_open(file, proc_show_dpuinfo, NULL);
	return 0;
}
// dpu proc debug file structure
static struct file_operations proc_file_ops = {
	.owner = THIS_MODULE,
	.open = proc_key_open,
	.write = proc_write,
	.read = seq_read,
	.release = single_release,

};

uint32_t field_mask_value(uint32_t val, uint32_t mask)
{
	int i;
	int max_bit = sizeof(uint32_t) * 8;
	int lowest_set_bit = max_bit - 1;

	/* Iterate through each bit of mask */
	for (i = 0; i < max_bit; i++) {
		/* If current bit is set */
		if ((mask >> i) & 1) {
			lowest_set_bit = i;
			break;
		}
	}

	return (val & mask) >> lowest_set_bit;
};

static const char *dts_node_prefix[] = {
	"xilinx,",
	"xilinx, ",
	"Xilinx,",
	"Xilinx, ",
	"deephi,",
	"deephi, ",
	"Deephi,",
	"Deephi, ",
};

struct device_node *dpu_compatible_node(const char *compat)
{
	int idx=0, max=0;
	char dst_node[255];
	struct device_node *pdpu_node = NULL;

	if (strlen(compat)>128) {
		return NULL;
	}

	max = sizeof(dts_node_prefix)/sizeof(char *);
	for (idx=0; idx<max; idx++) {
		memset(dst_node, 0x0, sizeof(dst_node));
		sprintf(dst_node, "%s%s", dts_node_prefix[idx], compat);
		pdpu_node = of_find_compatible_node(NULL, NULL, dst_node);
		if (pdpu_node)
			break;
	}
	return pdpu_node;
};

/**
 * dpu_probe - platform probe method for the dpu driver
 * @pdev:	Pointer to the platform_device structure
 *
 * This function initializes the driver data structures and the hardware.
 *
 * @return:	0 on success and error value on failure
 */
static int dpu_probe(struct platform_device *pdev)
{
	int ret, i;
	void *prop;
	struct device_node *pdpu_node, *dpucore_node;
	struct resource *res;
	unsigned long base_addr_dtsi = 0 ;
	uint32_t signature_length = 0;
	uint32_t signature_field = 0;
	uint32_t signature_temp;
	uint32_t *signature_va;

	uint32_t irqs[DPU_CORE_MAX];
	dpu_info_t dpu_info;

	dprint(PLEVEL_INFO, "[PID %i]name:%s,func:%s\n", current->pid, current->comm, __func__);
	dev_handler = &(pdev->dev);

	dpucore_node = dpu_compatible_node("dpucore");

	pdpu_node = dpu_compatible_node("dpu");
	if (!pdpu_node) {
		dpr_init("Not found DPU device node!\n");
		return -ENXIO;
	} else {
		prop = of_get_property(pdpu_node, "base-addr", NULL);
		if (prop) {
			base_addr_dtsi = of_read_ulong(prop, 1);
		}
		if (base_addr_dtsi) {
			dpr_init("Found DPU signature addr = 0x%lx in device-tree\n", base_addr_dtsi);
			signature_addr = base_addr_dtsi + 0x00F00000;
		}
		if (signature_addr != SIG_BASE_NULL) {
			dpr_init("Checking DPU signature at addr = 0x%lx, \n", signature_addr);
			signature_va =
				ioremap((phys_addr_t)signature_addr, 1 * sizeof(signature_field));
			signature_field = ioread32(signature_va);
		}
		if ((signature_field & SIG_MAGIC_MASK) == SIG_MAGIC) {
			dpu_caps.signature_valid = 1;

			if (1 == field_mask_value(signature_field, SIG_VER_MASK)) {
				dpu_caps.magic = DPU_CONF_MAGIC;
			} else {
				dpu_caps.magic = 0x0;
			}

			signature_length = field_mask_value(signature_field, SIG_SIZE_MASK);
			iounmap(signature_va);
			signature_va = ioremap((phys_addr_t)signature_addr,
					       signature_length * sizeof(signature_field));

			//reserved field checking.
			if (0x0 == dpu_caps.magic) {
				signature_temp = 0;
				for (i = 0; i < VER_MAX_ENTRY; i++) {
					signature_field = ioread32(signature_va + i);
					signature_temp =
						field_mask_value(signature_field, VER_RESERVERD[i]);
					if (signature_temp != 0) {
						dpr_init(
							"Unknown reserved field found in DPU signature at offset: %#X.\n",
							i * 4);
						dpr_init(
							"Try to update DPU driver to the latest version to resolve this issue.\n");
						return -ENXIO;
					}
				}
			}

			// offset 1
			signature_field = ioread32(signature_va + 1);
			sprintf(dpu_caps.hw_timestamp, "20%02d-%02d-%02d %02d:%02d:00",
				field_mask_value(signature_field, YEAR_MASK),
				field_mask_value(signature_field, MONTH_MASK),
				field_mask_value(signature_field, DATE_MASK),
				field_mask_value(signature_field, HOUR_MASK),
				field_mask_value(signature_field, BIT_VER_MASK) * 15);
			dpu_info.base.dpu_freq = field_mask_value(signature_field, FREQ_MASK);

			// offset 2
			signature_field = ioread32(signature_va + 2);
			dpu_caps.irq_base0 = field_mask_value(signature_field, PS_INTBASE0_MASK);
			dpu_caps.irq_base1 = field_mask_value(signature_field, PS_INTBASE1_MASK);

			// offset 3
			signature_field = ioread32(signature_va + 3);
			dpu_caps.hp_width = field_mask_value(signature_field, HP_WIDTH_MASK);
			dpu_caps.data_width = field_mask_value(signature_field, DATA_WIDTH_MASK);
			dpu_caps.bank_group = field_mask_value(signature_field, BANK_GROUP_MASK);
			dpu_info.base.dpu_arch = field_mask_value(signature_field, DPU_ARCH_MASK);
			dpu_info.dpu_target = field_mask_value(signature_field, DPU_TARGET_MASK);
			dpu_caps.dpu_cnt = field_mask_value(signature_field, DPU_CORENUM_MASK);
			if (dpu_caps.hp_width >= DPU_HP_WIDTH_RESERVE) {
				dpr_init("Invalid hp width '%d' found in DPU signature.\n",
					 dpu_caps.hp_width);
				return -ENXIO;
			}
			if (dpu_caps.data_width >= DPU_DATA_WIDTH_RESERVE) {
				dpr_init("Invalid data width '%d' found in DPU signature.\n",
					 dpu_caps.data_width);
				return -ENXIO;
			}
			if (dpu_caps.bank_group >= DPU_BANK_GROUP_RESERVE ||
			    DPU_BANK_GROUP_1 == dpu_caps.bank_group) {
				dpr_init("Invalid bank group '%d' found in DPU signature.\n",
					 dpu_caps.bank_group);
				return -ENXIO;
			}

			// offset 4
			signature_field = ioread32(signature_va + 4);
			for (i = 0; i < 8; i++) {
				signature_temp = field_mask_value(signature_field, 0xF << (4 * i));
				irqs[i] = signature_temp & 0x8 ?
						  (signature_temp & 0x7) + dpu_caps.irq_base1 :
						  (signature_temp & 0x7) + dpu_caps.irq_base0;
			}

			// offset 5
			signature_field = ioread32(signature_va + 5);
			for (i = 0; i < 8; i++) {
				signature_temp = field_mask_value(signature_field, 0xF << (4 * i));
				irqs[8 + i] = signature_temp & 0x8 ?
						      (signature_temp & 0x7) + dpu_caps.irq_base1 :
						      (signature_temp & 0x7) + dpu_caps.irq_base0;
			}

			if (dpu_caps.dpu_cnt > 0){
			} else {
				dpr_init(
					"Error of no DPU core found in current configuration of DPU IP!\n");
				return -ENXIO;
			}

			// offset 6
			signature_field = ioread32(signature_va + 6);
			dpu_caps.avgpool.version = field_mask_value(signature_field, AVGPOOL_MASK);
			dpu_caps.conv_depthwise.version =
				field_mask_value(signature_field, CONV_DEPTHWISE_MASK);
			dpu_caps.relu_leaky.version =
				field_mask_value(signature_field, RELU_LEAKY_MASK);
			dpu_caps.relu_p.version = field_mask_value(signature_field, RELU_P_MASK);

			// offset 7
			signature_field = ioread32(signature_va + 7);
			dpu_caps.serdes_nonlinear.version =
				field_mask_value(signature_field, SERDES_NONLINEAR_MASK);

			// offset 9
			signature_field = ioread32(signature_va + 9);
			dpu_caps.hdmi.enable = field_mask_value(extension, DPU_EXT_HDMI);
			dpu_caps.hdmi.version = field_mask_value(signature_field, HDMI_VER_MASK);
			dpu_caps.hdmi.valid = field_mask_value(signature_field, HDMI_VLD_MASK);
			dpu_caps.hdmi.enable &= dpu_caps.hdmi.valid;
			signature_temp = field_mask_value(signature_field, HDMI_IRQ_MASK);
			dpu_caps.hdmi.irq = signature_temp & 0x8 ?
						    (signature_temp & 0x7) + dpu_caps.irq_base1 :
						    (signature_temp & 0x7) + dpu_caps.irq_base0;

			dpu_caps.bt1120.enable = field_mask_value(extension, DPU_EXT_BT1120);
			dpu_caps.bt1120.version =
				field_mask_value(signature_field, BT1120_VER_MASK);
			dpu_caps.bt1120.valid = field_mask_value(signature_field, BT1120_VLD_MASK);
			dpu_caps.bt1120.enable &= dpu_caps.bt1120.valid;
			signature_temp = field_mask_value(signature_field, BT1120_IRQ_MASK);
			dpu_caps.bt1120.irq = signature_temp & 0x8 ?
						      (signature_temp & 0x7) + dpu_caps.irq_base1 :
						      (signature_temp & 0x7) + dpu_caps.irq_base0;

			dpu_caps.fullconnect.enable =
				field_mask_value(extension, DPU_EXT_FULLCONNECT);
			dpu_caps.fullconnect.version =
				field_mask_value(signature_field, FC_VER_MASK);
			dpu_caps.fullconnect.valid = field_mask_value(signature_field, FC_VLD_MASK);
			dpu_caps.fullconnect.enable &= dpu_caps.fullconnect.valid;
			signature_temp = field_mask_value(signature_field, FC_IRQ_MASK);
			dpu_caps.fullconnect.irq =
				signature_temp & 0x8 ? (signature_temp & 0x7) + dpu_caps.irq_base1 :
						       (signature_temp & 0x7) + dpu_caps.irq_base0;

			dpu_caps.softmax.enable = field_mask_value(extension, DPU_EXT_SOFTMAX);
			dpu_caps.softmax.version =
				field_mask_value(signature_field, SOFTMAX_VER_MASK);
			dpu_caps.softmax.valid =
				field_mask_value(signature_field, SOFTMAX_VLD_MASK);
			dpu_caps.softmax.enable &= dpu_caps.softmax.valid;
			signature_temp = field_mask_value(signature_field, SOFTMAX_IRQ_MASK);
			dpu_caps.softmax.irq = signature_temp & 0x8 ?
						       (signature_temp & 0x7) + dpu_caps.irq_base1 :
						       (signature_temp & 0x7) + dpu_caps.irq_base0;

			// offset 10
			signature_field = ioread32(signature_va + 10);
			dpu_caps.resize.enable = field_mask_value(extension, DPU_EXT_RESIZE);
			dpu_caps.resize.version =
				field_mask_value(signature_field, RESIZE_VER_MASK);
			dpu_caps.resize.valid = field_mask_value(signature_field, RESIZE_VLD_MASK);
			dpu_caps.resize.enable &= dpu_caps.resize.valid;
			signature_temp = field_mask_value(signature_field, RESIZE_IRQ_MASK);
			dpu_caps.resize.irq = signature_temp & 0x8 ?
						      (signature_temp & 0x7) + dpu_caps.irq_base1 :
						      (signature_temp & 0x7) + dpu_caps.irq_base0;


			if (0x0 == dpu_caps.magic) {
				dpu_caps.p_dpu_info =
					kzalloc(sizeof(dpu_info_t) * dpu_caps.dpu_cnt, GFP_ATOMIC);
				if (!dpu_caps.p_dpu_info) {
					dpr_init("kzalloc fail!\n");
					return -ENXIO;
				} else {
					int idx;
					for (idx = 0; idx < dpu_caps.dpu_cnt; idx++) {
						(*(dpu_caps.p_dpu_info + idx)).base.dpu_arch =
							dpu_info.base.dpu_arch;
						(*(dpu_caps.p_dpu_info + idx)).base.dpu_freq =
							dpu_info.base.dpu_freq;
						(*(dpu_caps.p_dpu_info + idx)).dpu_target =
								dpu_info.dpu_target;
						(*(dpu_caps.p_dpu_info + idx)).irq = irqs[idx];

						if ((*(dpu_caps.p_dpu_info + idx)).base.dpu_arch >
						    DPU_ARCH_RESERVE) {
							dpr_init(
								"Unknown DPU arch type '%d' found in DPU signature.\n",
								(*(dpu_caps.p_dpu_info + idx)).base.dpu_arch);
							dpr_init(
								"Try to update DPU driver to the latest version to resolve this issue.\n");
							return -ENXIO;
						}
						if ((*(dpu_caps.p_dpu_info + idx)).dpu_target >
						    DPU_TARGET_RESERVE) {
							dpr_init(
								"Unknown DPU target type '%d' found in DPU signature.\n",
								(*(dpu_caps.p_dpu_info + idx))
									.dpu_target);
							dpr_init(
								"Try to update DPU driver to the latest version to resolve this issue.\n");
							return -ENXIO;
						}
					}
				}
			} else if (DPU_CONF_MAGIC == dpu_caps.magic) {
				dpu_configurable_t * p_conf_t;
				p_conf_t = kzalloc(sizeof(dpu_configurable_t) * dpu_caps.dpu_cnt, GFP_ATOMIC);

				switch (dpu_info.base.dpu_arch)
				{
					case 1:
						dpu_info.base.dpu_arch = 1024;
						break;
					case 2:
						dpu_info.base.dpu_arch = 1152;
						break;
					case 3:
						dpu_info.base.dpu_arch = 4096;
						break;
					case 4:
						dpu_info.base.dpu_arch = 256;
						break;
					case 5:
						dpu_info.base.dpu_arch = 512;
						break;
					case 6:
						dpu_info.base.dpu_arch = 800;
						break;
					case 7:
						dpu_info.base.dpu_arch = 1600;
						break;
					case 8:
						dpu_info.base.dpu_arch = 2048;
						break;
					case 9:
						dpu_info.base.dpu_arch = 2304;
						break;
					case 10:
						dpu_info.base.dpu_arch = 8192;
						break;
					case 11:
						dpu_info.base.dpu_arch = 3136;
						break;
					case 12:
						dpu_info.base.dpu_arch = 288;
						break;
					default:
						dpr_init("Unknown DPU arch type '%d' found in DPU signature.\n", dpu_info.base.dpu_arch);
						dpr_init("Try to update DPU driver to the latest version to resolve this issue.\n");
						return -ENXIO;
				}
				switch (dpu_info.dpu_target)
				{
					case 1:
						dpu_info.dpu_target = 0x113;
						break;
					case 2:
						dpu_info.dpu_target = 0x130;
						break;
					case 3:
						dpu_info.dpu_target = 0x131;
						break;
					case 4:
						dpu_info.dpu_target = 0x132;
						break;
					case 5:
						dpu_info.dpu_target = 0x133;
						break;
					case 6:
						dpu_info.dpu_target = 0x134;
						break;
					case 7:
						dpu_info.dpu_target = 0x135;
						break;
					case 8:
						dpu_info.dpu_target = 0x140;
						break;
					case 9:
						dpu_info.dpu_target = 0x141;
						break;
					case 10:
						dpu_info.dpu_target = 0x142;
						break;
					case 11:
						dpu_info.dpu_target = 0x136;
						break;
					case 12:
						dpu_info.dpu_target = 0x137;
						break;
					case 13:
						dpu_info.dpu_target = 0x138;
						break;
					default:
						dpr_init("Unknown DPU target type '%d' found in DPU signature.\n", dpu_info.dpu_target);
						dpr_init("Try to update DPU driver to the latest version to resolve this issue.\n");
						return -ENXIO;
				}

				if (!p_conf_t) {
					dpr_init("kzalloc fail!\n");
					p_conf_t = NULL;
					return -ENXIO;
				} else {
					int idx;
					for (idx = 0; idx < dpu_caps.dpu_cnt; idx++) {
						(*(p_conf_t + idx)).base.dpu_arch = dpu_info.base.dpu_arch;
						(*(p_conf_t + idx)).base.dpu_freq = dpu_info.base.dpu_freq;

						(*(p_conf_t + idx)).sys.sys_ip_type = 0x1;
						(*(p_conf_t + idx)).sys.sys_regmap_ver = 0x1;

						(*(p_conf_t + idx)).sub_version.ver_target = dpu_info.dpu_target;

						(*(p_conf_t + idx)).arch.arch_hp_bw = 1<<(dpu_caps.hp_width+4);
						(*(p_conf_t + idx)).arch.arch_data_bw = 8*dpu_caps.data_width;
						(*(p_conf_t + idx)).arch.arch_img_bkgrp = dpu_caps.bank_group;
						(*(p_conf_t + idx)).arch.arch_pp = 0x4;
						(*(p_conf_t + idx)).arch.arch_icp = 0xc;
						(*(p_conf_t + idx)).arch.arch_ocp = 0xc;

						(*(p_conf_t + idx)).ram.ram_depth_mean = (0x1+1)*16;
						(*(p_conf_t + idx)).ram.ram_depth_bias = (0x3+1)*512;
						(*(p_conf_t + idx)).ram.ram_depth_wgt  = (0x3+1)*512;
						(*(p_conf_t + idx)).ram.ram_depth_img  = (0x3+1)*512;

						signature_field = ioread32(signature_va + 7);
						(*(p_conf_t + idx)).load.load_augm_enable = field_mask_value(signature_field, LOAD_AUG_MASK);
						(*(p_conf_t + idx)).load.load_img_mean_enable = field_mask_value(signature_field, LOAD_IMG_MEAN_MASK);

						(*(p_conf_t + idx)).conv.conv_leakyrelu_enable = dpu_caps.relu_leaky.version & 0x1;;
						(*(p_conf_t + idx)).conv.conv_relu6_enable = (dpu_caps.relu_leaky.version & 0x2) >> 1;
						(*(p_conf_t + idx)).conv.conv_wr_parallel = 0x1; // no check

						(*(p_conf_t + idx)).pool.pool_average_enable = dpu_caps.avgpool.version;

						(*(p_conf_t + idx)).elew.elew_parallel = 0x1; // no check

						(*(p_conf_t + idx)).dwcv.dwcv_alu_mode_enable = 0x0; // no check
						(*(p_conf_t + idx)).dwcv.dwcv_relu6_enable = (dpu_caps.relu_leaky.version & 0x2) >> 1;
						(*(p_conf_t + idx)).dwcv.dwcv_parallel = dpu_caps.conv_depthwise.version;

						(*(p_conf_t + idx)).misc.misc_wr_parallel = 0x1; // no check
					}
				}
				dpu_caps.p_dpu_info = (dpu_info_t *)p_conf_t;
			}

			dpr_init("DPU signature checking done!\n");
			dpu_caps.reg_base = DPU_BASE;
			dpu_caps.reg_size = DPU_SIZE;
			DPU_CORE_NUM = dpu_caps.dpu_cnt;

		} else if (base_addr_dtsi == signature_addr) {
			dpr_init("Invalid 'signature-addr' value specified in DPU device tree, please check.\n");
			return -ENXIO;
		} else {
			dpr_init("DPU signature NOT found, fallback to device-tree.\n");

			of_property_read_u32(pdpu_node, "core-num", &DPU_CORE_NUM);
			dpu_caps.dpu_cnt = DPU_CORE_NUM;

			if (dpu_caps.dpu_cnt > 0) {
				dpu_caps.p_dpu_info =
					kzalloc(sizeof(dpu_info_t) * dpu_caps.dpu_cnt, GFP_ATOMIC);
				if (!dpu_caps.p_dpu_info) {
					dpr_init("kzalloc fail!\n");
					return -ENXIO;
				} else {
					int idx;
					for (idx = 0; idx < dpu_caps.dpu_cnt; idx++) {
						(*(dpu_caps.p_dpu_info + idx)).dpu_target =
							DPU_TARGET_V1_1_3; //default to target:1.1.3 due to massively deployed
					}
				}
			} else {
				dpr_init(
					"Error of no DPU core found in current configuration of DPU IP!\n");
				return -ENXIO;
			}
		}
	}

	if ((DPU_CORE_NUM == 0) || (DPU_CORE_NUM > MAX_CORE_NUM)) {
		dpr_init("Core number %d invalid!\n", DPU_CORE_NUM);
		return -EINVAL;
	}
	RAM_SIZE = PROF_RAM_SIZE * DPU_CORE_NUM;
	RAM_BASEADDR_VIRT = dma_alloc_coherent(dev_handler, RAM_SIZE, &RAM_BASEADDR, GFP_KERNEL);

	// map the dpu Register
	if (dpu_caps.signature_valid) {
		res = kzalloc(sizeof(struct resource), GFP_ATOMIC);
		res->start = dpu_caps.reg_base;
		res->end = dpu_caps.reg_base + dpu_caps.reg_size - 1;
	} else {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	}
	if (res) {
		pdpureg = (DPUReg *)ioremap(res->start, res->end - res->start + 1);
		if (!pdpureg) {
			dpr_init("Map DPU registers error!\n");
			return -ENXIO;
		}
		if (dpu_caps.signature_valid) {
			kfree(res);
		}
	} else {
		dpr_init("Not found DPU reg resources!\n");
	}

	_dpu_regs_init(DPU_CORE_NUM);
	_dpu_devstate_init(DPU_CORE_NUM);

	// memory structure init
	sema_init(&memblk_lock, 1);
	INIT_LIST_HEAD(&head_alloc);

	spin_lock_init(&tasklstlock);
	spin_lock_init(&idlock);
	spin_lock_init(&taskidlock);
	spin_lock_init(&corelock);
	spin_lock_init(&reglock);

	// register interrupt service routine for DPU
	for (i = 0; i < DPU_CORE_NUM; i++) {
		dpudev.irq_no[i] = dpucore_node? irq_of_parse_and_map(dpucore_node, i): platform_get_irq(pdev, i);

		if (dpudev.irq_no[i] < 0) {
			dprint(PLEVEL_ERR, "IRQ resource not found for DPU core %d\n", i);
			return dpudev.irq_no[i];
		}

		ret = request_irq(dpudev.irq_no[i], (irq_handler_t)dpu_isr, 0, "dpu_isr", NULL);
		if (ret != 0) {
			dpr_init("Request IRQ %d failed!\n", dpudev.irq_no[i]);
			return ret;
		} else {
		}
	}

	init_ext(pdpu_node); // initialize extent modules
	// create the proc file entry
	proc_file = proc_create(PROCFNAME, 0644, NULL, &proc_file_ops);

	// Register the dpu device
	return misc_register(&dpudev.dev);
}

/**
 * dpu_remove - platform remove method for the dpu driver
 * @pdev:	Pointer to the platform_device structure
 *
 * This function is called if a device is physically removed from the system or
 * if the driver module is being unloaded. It frees all resources allocated to
 * the device.
 *
 * @return:	0 on success and error value on failure
 */
static int dpu_remove(struct platform_device *pdev)
{
	int i;

	dprint(PLEVEL_INFO, "[PID %i]name:%s,func:%s\n", current->pid, current->comm, __func__);

	if (dpu_caps.p_dpu_info) {
		kfree(dpu_caps.p_dpu_info);
	}
	if (RAM_BASEADDR_VIRT) {
		dma_free_coherent(dev_handler, RAM_SIZE, (void *)RAM_BASEADDR_VIRT, RAM_BASEADDR);
	}
	misc_deregister(&dpudev.dev);

	remove_proc_entry(PROCFNAME, NULL);

	exit_ext(); // clear extend mdoules

	for (i = 0; i < DPU_CORE_NUM; i++)
		free_irq(dpudev.irq_no[i], NULL);
	iounmap(pdpureg);
	return 0;
}

static const struct of_device_id dpu_dt_ids[] = { { .compatible = "deephi, dpu" },
						{ .compatible = "deephi,dpu" },
						{ .compatible = "xilinx, dpu" },
						{ .compatible = "xilinx,dpu" },
						{ /* end of table */ } };

static struct platform_driver dpu_drv = {
	.driver = {
		.name = "dpu",
		.of_match_table = dpu_dt_ids,
	},
	.probe = dpu_probe,
	.remove = dpu_remove,
};

/*==========================================================*/
/**
 * dpu initialize function
 */
static int __init dpu_init(void)
{
	dprint(PLEVEL_INFO, "[PID %i]name:%s,func:%s\n", current->pid, current->comm, __func__);

	return platform_driver_register(&dpu_drv);
}

/**
 * dpu uninstall function
 */
static void __exit dpu_exit(void)
{
	dprint(PLEVEL_INFO, "[PID %i]name:%s,func:%s\n", current->pid, current->comm, __func__);

	platform_driver_unregister(&dpu_drv);
}
/*==========================================================*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xilinx");
module_init(dpu_init);
module_exit(dpu_exit);
