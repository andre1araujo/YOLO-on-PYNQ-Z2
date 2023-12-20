/*
 * Copyright (C) 2019 Xilinx, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 */
#include "dpuext.h"
#include <linux/delay.h>
#include "dpucore.h"

extern uint debuglevel;
extern uint accipmask;
extern uint timeout;

extern dpu_caps_t dpu_caps;

run_smfc_t run_smfc;
run_resize_t run_resize;

/**
 * init_ext - initialize dpu extension modules
 * @pnode  : the dpu device tree node
 *
 * @return : 0 if OK; otherwise error number
 */
int init_ext(struct device_node *pdpunode)
{
	int ret = 0;
	struct device_node *node;

	dprint(PLEVEL_INFO, "[PID %i]name:%s,func:%s\n", current->pid, current->comm, __func__);

	node = dpu_compatible_node("smfc");
	if (node && (dpu_caps.softmax.valid || dpu_caps.fullconnect.valid)) {
		uint32_t reg_base, reg_size;
		dpr_init("Init SMFC IP...\n");

		// register smfc interrupt isr
		run_smfc.irqno = irq_of_parse_and_map(node, 0);
		if (run_smfc.irqno < 0) {
			dpr_init("SMFC IRQ res not found!\n");
			return run_smfc.irqno;
		}
		ret = request_irq(run_smfc.irqno, (irq_handler_t)dpu_ext_isr, 0, "dpu_smfc", NULL);
		if (ret != 0) {
			dpr_init("Request SMFC IRQ %d failed!\n", run_smfc.irqno);
			return ret;
		} else {
			dpr_init("Request SMFC IRQ %d successful.", run_smfc.irqno);
		}

		// map smfc register
		// of_property_read_u32_index(node, "reg", 0, &reg_base);
		// of_property_read_u32_index(node, "reg", 1, &reg_size);
		reg_base = DPU_EXT_SMFC_BASE;
		reg_size = DPU_EXT_SMFC_SIZE;
		run_smfc.regs = (ioremap(reg_base, reg_size));
		if (!run_smfc.regs) {
			dpr_init("Map SMFC registers error!\n");
			return -EINVAL;
		}

		init_waitqueue_head(&run_smfc.wq);
		spin_lock_init(&run_smfc.lck_reg);
		sema_init(&run_smfc.sem, 1);
		run_smfc.timeout = timeout * USER_HZ;

		accipmask |= (dpu_caps.softmax.valid ? DPU_EXT_SOFTMAX : 0);
		accipmask |= (dpu_caps.fullconnect.valid ? DPU_EXT_FULLCONNECT : 0);
		dpr_init("Init SMFC IP done\n");
	} else {
		dpu_caps.fullconnect.enable = 0;
		dpu_caps.softmax.enable = 0;
	}

	// initialize resize IP
	node = dpu_compatible_node("resize");
	if (node && dpu_caps.resize.valid) {
		uint32_t reg_base, reg_size;

		dpr_init("init Resize IP...\n");

		// register smfc interrupt isr
		run_resize.irqno = irq_of_parse_and_map(node, 0);
		if (run_resize.irqno < 0) {
			dpr_init("Resize IRQ res not found!\n");
			return run_resize.irqno;
		}
		ret = request_irq(run_resize.irqno, (irq_handler_t)dpu_ext_isr, 0, "dpu_resize",
				  NULL);
		if (ret != 0) {
			dpr_init("Request Resize IRQ %d failed!\n", run_resize.irqno);
			return ret;
		} else {
			dpr_init("Request Resize IRQ %d successful.", run_resize.irqno);
		}

		// map smfc register
		// of_property_read_u32_index(node, "reg", 0, &reg_base);
		// of_property_read_u32_index(node, "reg", 1, &reg_size);
		reg_base = DPU_EXT_RESIZE_BASE;
		reg_size = DPU_EXT_RESIZE_SIZE;
		run_resize.regs = (ioremap(reg_base, reg_size));
		if (!run_resize.regs) {
			dpr_init("Map Resize registers error!\n");
			return -EINVAL;
		}

		init_waitqueue_head(&run_resize.wq);
		spin_lock_init(&run_resize.lck_reg);
		sema_init(&run_resize.sem, 1);
		run_resize.timeout = timeout * USER_HZ;

		accipmask |= DPU_EXT_RESIZE;
	} else {
		dpu_caps.resize.enable = 0;
	}

	return ret;
}

/**
 * exit_ext - called when unloading extend modules
 */
void exit_ext(void)
{
	if (accipmask & (DPU_EXT_SOFTMAX | DPU_EXT_FULLCONNECT)) {
		// clean smfc moudle
		iounmap(run_smfc.regs);
		free_irq(run_smfc.irqno, NULL);
	}
	if (accipmask & DPU_EXT_RESIZE) {
		// clean resize moudle
		iounmap(run_resize.regs);
		free_irq(run_resize.irqno, NULL);
	}
}
/**
 * dpu_ext_isr - dpu extension modules isr
 * @irq  : interrupt number
 * @data : additional data
 */
irqreturn_t dpu_ext_isr(int irq, void *data)
{
	dprint(PLEVEL_DBG, "ext_isr, irq = %d\n", irq);
	if (irq == run_smfc.irqno) {
		if (accipmask & (DPU_EXT_SOFTMAX | DPU_EXT_FULLCONNECT)) {
			run_smfc.intflg = TRUE;
			// clear smfc interrupt
			iowrite32(1, &run_smfc.regs->clr);
			iowrite32(0, &run_smfc.regs->clr);

			wake_up_interruptible(&run_smfc.wq);
		}
	} else if (irq == run_resize.irqno) {
		if (accipmask & DPU_EXT_RESIZE) {
			run_resize.intflg = TRUE;
			// clear resize interrupt
			iowrite32(0, &run_resize.regs->start);
			iowrite32(1, &run_resize.regs->clr);
			iowrite32(0, &run_resize.regs->clr);

			wake_up_interruptible(&run_resize.wq);
		}
	}
	return IRQ_HANDLED;
}

/**
 * _show_ext_regs - show dpu extension module registers
 * @mask  : extension module's mask
 */
inline void _show_ext_regs(struct seq_file *file, u32 mask)
{
	int idx = 0;
	int msg_len = 65536;
	char *msg = kzalloc(msg_len, GFP_KERNEL);

	if (!msg) {
		dpr_init("kzalloc fail in _show_ext_regs !\n");
		return;
	}

	if (mask & (DPU_EXT_SOFTMAX | DPU_EXT_FULLCONNECT)) {
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"[SMFC Registers]\n");
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "DONE", run_smfc.regs->done);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "SM_LEN_X", run_smfc.regs->sm_len_x);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "SM_LEN_Y", run_smfc.regs->sm_len_y);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "SRC", run_smfc.regs->src);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "DST", run_smfc.regs->dst);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "SCALE", run_smfc.regs->scale);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "SM_OFFSET", run_smfc.regs->sm_offset);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "CLR", run_smfc.regs->clr);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "START", run_smfc.regs->start);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "FC_INPUT_CHANNEL",
				run_smfc.regs->fc_input_channel);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "FC_OUTPUT_CHANNEL",
				run_smfc.regs->fc_output_channel);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "FC_BATCH", run_smfc.regs->fc_batch);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "FC_WEIGHT_START",
				run_smfc.regs->fc_weight_start);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "FC_WEIGHT_END", run_smfc.regs->fc_weight_end);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "CALC_MOD", run_smfc.regs->calc_mod);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "DST_ADDR_SEL", run_smfc.regs->dst_addr_sel);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "FC_RELU_EN", run_smfc.regs->fc_relu_en);
	}
	if (mask & DPU_EXT_RESIZE) {
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"[RESIZE Registers]\n");
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "CTRL", run_resize.regs->ctrl);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "CHANNEL", run_resize.regs->channel);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "SRC_SIZE", run_resize.regs->src_size);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "DST_SIZE", run_resize.regs->dst_size);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "SCALE_W", run_resize.regs->scale_w);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "SCALE_H", run_resize.regs->scale_h);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "SRC_ADDR", run_resize.regs->src_addr);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "DST_ADDR", run_resize.regs->dst_addr);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "ADDR_STRIDE", run_resize.regs->addr_stride);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "METHOD", run_resize.regs->method);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "START", run_resize.regs->start);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "IRSR", run_resize.regs->done_irs);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "CLR", run_resize.regs->clr);
		idx += snprintf(msg + idx, ((msg_len - idx) > 0 ? (msg_len - idx) : 0),
				"%-8s\t: 0x%.8x\n", "BIAS", run_resize.regs->bias);
	}

	if (file) {
		seq_printf(file, msg);
	} else {
		dprint(PLEVEL_ERR, "%s", msg);
	}

	kfree(msg);
}

/**
 * dpu_softmax - softmax calculation acceleration using SMFC IP
 * @para : softmax parameter structure
 *
 * @return: 0 if successful; otherwise -errno
 */
int dpu_softmax(struct ioc_softmax_t *para)
{
	int ret = 0;

	while (down_interruptible(&run_smfc.sem) < 0) {
		dprint(PLEVEL_DBG, "Interrupted when acquiring semaphore of smfc\n");
	}
	// write softmax parameters
	iowrite32(para->width, &run_smfc.regs->sm_len_x);
	iowrite32(para->height, &run_smfc.regs->sm_len_y);

	iowrite32(para->input, &run_smfc.regs->src);
	iowrite32(para->output, &run_smfc.regs->dst);

	iowrite32(para->scale, &run_smfc.regs->scale);
	iowrite32(para->offset, &run_smfc.regs->sm_offset);

	iowrite32(0, &run_smfc.regs->calc_mod); // softmax mode

	// start calculation
	iowrite32(1, &run_smfc.regs->start);
	iowrite32(0, &run_smfc.regs->start);

	ret = wait_event_interruptible_timeout(run_smfc.wq, run_smfc.intflg == TRUE,
					       run_smfc.timeout);
	run_smfc.intflg = FALSE;
	up(&run_smfc.sem);

	if (ret == 0) {
		dprint(PLEVEL_ERR, "softmax timeout!\n");
		_show_ext_regs(NULL, DPU_EXT_SOFTMAX);
	}

	return ret > 0 ? 0 : (ret == 0 ? -ETIMEDOUT : ret);
}

/**
 * dpu_fullconnect - fullconnect calculation acceleration using SMFC IP
 * @para : fullconnect parameter structure
 *
 * @return: 0 if successful; otherwise -errno
 */
int dpu_fullconnect(struct ioc_fc_t *para)
{
	int ret = 0;

	while (down_interruptible(&run_smfc.sem) < 0) {
		dprint(PLEVEL_DBG, "Interrupted when acquiring semaphore of smfc\n");
	}

	iowrite32(para->src, &run_smfc.regs->src);
	iowrite32(para->dst, &run_smfc.regs->dst);

	iowrite32(para->input_dim, &run_smfc.regs->fc_input_channel);
	iowrite32(para->output_dim, &run_smfc.regs->fc_output_channel);

	iowrite32(para->batch_dim, &run_smfc.regs->fc_batch);
	iowrite32(para->relu_enable, &run_smfc.regs->fc_relu_en);

	iowrite32(para->start_addr_w, &run_smfc.regs->fc_weight_start);
	iowrite32(para->end_addr_w, &run_smfc.regs->fc_weight_end);

	iowrite32(1, &run_smfc.regs->calc_mod); // fc mode

	// start calculation
	iowrite32(1, &run_smfc.regs->start);
	iowrite32(0, &run_smfc.regs->start);

	ret = wait_event_interruptible_timeout(run_smfc.wq, run_smfc.intflg == TRUE,
					       run_smfc.timeout);
	run_smfc.intflg = FALSE;

	up(&run_smfc.sem);

	if (ret == 0) {
		dprint(PLEVEL_ERR, "fc timeout!\n");
		_show_ext_regs(NULL, DPU_EXT_SOFTMAX);
	}
	return ret > 0 ? 0 : (ret == 0 ? -ETIMEDOUT : ret);
}

#if 0 //resize not enable now
/**
 * dpu_resize - resize calculation acceleration using RESIZE IP
 * @para : resize parameter structure
 *
 * @return: 0 if successful; otherwise -errno
 */
int dpu_resize(run_resize_t *para)
{
	int ret = 0;
	u32 size = 0;

	while (down_interruptible(&run_resize.sem) < 0) {
		dprint(PLEVEL_DBG, "Interrupted when acquiring semaphore of resize\n");
	}

	// write resize parameters
	iowrite32(para->channel, &run_resize.regs->channel);
	size = ((para->src_height & 0xFFFF) << 16) | (para->src_width & 0xFFFF);
	iowrite32(size, &run_resize.regs->src_size);
	size = ((para->dst_height & 0xFFFF) << 16) | (para->dst_width & 0xFFFF);
	iowrite32(size, &run_resize.regs->dst_size);

	iowrite32(para->scale_w, &run_resize.regs->scale_w);
	iowrite32(para->scale_h, &run_resize.regs->scale_h);
	iowrite32(para->src_addr, &run_resize.regs->src_addr);
	iowrite32(para->dst_addr, &run_resize.regs->dst_addr);
	iowrite32(para->addr_stride, &run_resize.regs->addr_stride);
	size = ((para->dst_bias & 0xFF) << 8) | (para->src_bias & 0xFF);
	iowrite32(size, &run_resize.regs->bias);
	iowrite32(para->method, &run_resize.regs->method);

	// start calculation
	iowrite32(1, &run_resize.regs->start);

	ret = wait_event_interruptible_timeout(run_resize.wq, run_resize.intflg == TRUE,
					       run_resize.timeout);
	run_resize.intflg = FALSE;
	up(&run_resize.sem);

	if (ret == 0) {
		dprint(PLEVEL_ERR, "resize timeout!\n");
		_show_ext_regs(NULL, DPU_EXT_RESIZE);
	}

	return ret > 0 ? 0 : (ret == 0 ? -ETIMEDOUT : ret);
}
#endif