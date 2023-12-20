/*
 * Copyright (C) 2019 Xilinx, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 */
#ifndef _DPU_DEF_H_
#define _DPU_DEF_H_

#define DPU_EXT_HDMI (1 << 1)
#define DPU_EXT_BT1120 (1 << 2)
#define DPU_EXT_FULLCONNECT (1 << 3)
#define DPU_EXT_SOFTMAX (1 << 4)
#define DPU_EXT_RESIZE (1 << 5)

#define SIG_BASE_NULL 0X00000000
#ifdef SIG_BASE_ADDR
#define SIG_BASE SIG_BASE_ADDR
#else
#define SIG_BASE SIG_BASE_NULL
#endif

extern unsigned long signature_addr;
#define SIG_BASE_MASK 0XFF000000
#define DPU_BASE ((signature_addr & SIG_BASE_MASK) + 0x0000)
#define DPU_SIZE 0X00000700
#define DPU_EXT_SMFC_BASE ((signature_addr & SIG_BASE_MASK) + 0x0700)
#define DPU_EXT_SMFC_SIZE 0X00000041
#define DPU_EXT_RESIZE_BASE ((signature_addr & SIG_BASE_MASK) + 0x1000)
#define DPU_EXT_RESIZE_SIZE 0X000001C0

#define SIG_SIZE_MASK 0XFF000000
#define SIG_VER_MASK 0X00FF0000
#define SIG_MAGIC_MASK 0X0000FFFF
/*dpu signature magic number*/
#define SIG_MAGIC 0X4450

#define BIT_VER_MASK 0XC0000000
#define HOUR_MASK 0X3E000000
#define DATE_MASK 0X01F00000
#define MONTH_MASK 0X000F0000
#define YEAR_MASK 0X0000F800
#define FREQ_MASK 0X000007FE
#define ENCRYPT_MASK 0X00000001

#define PS_INTBASE1_MASK 0X0000FF00
#define PS_INTBASE0_MASK 0X000000FF

#define HP_WIDTH_MASK 0XF0000000
#define DATA_WIDTH_MASK 0X0F000000
#define BANK_GROUP_MASK 0X00F00000
#define DPU_ARCH_MASK 0X000F0000
#define DPU_TARGET_MASK 0X0000FF00
#define DPU_HP_INTERACT_MASK 0X000000F0
#define DPU_CORENUM_MASK 0X0000000F

#define AVGPOOL_MASK 0XFFFF0000
#define CONV_DEPTHWISE_MASK 0X0000FF00
#define RELU_LEAKY_MASK 0X000000F0
#define RELU_P_MASK 0X0000000F

#define LOAD_AUG_MASK         0X00000F00
#define LOAD_IMG_MEAN_MASK    0X000000F0
#define SERDES_NONLINEAR_MASK 0X0000000F

#define SOFTMAX_IRQ_MASK 0XF0000000
#define SOFTMAX_VER_MASK 0X0E000000
#define SOFTMAX_VLD_MASK 0X01000000
#define FC_IRQ_MASK 0X00F00000
#define FC_VER_MASK 0X000E0000
#define FC_VLD_MASK 0X00010000
#define BT1120_IRQ_MASK 0X0000F000
#define BT1120_VER_MASK 0X00000E00
#define BT1120_VLD_MASK 0X00000100
#define HDMI_IRQ_MASK 0X000000F0
#define HDMI_VER_MASK 0X0000000E
#define HDMI_VLD_MASK 0X00000001

#define RESIZE_IRQ_MASK 0X000000F0
#define RESIZE_VER_MASK 0X0000000E
#define RESIZE_VLD_MASK 0X00000001

#define AUTH_EN_MASK 0X10000000
#define CHIP_PART_MASK 0X0F000000
#define BOARD_HWVER_MASK 0X00F00000
#define BOARD_TYPE_MASK 0X000F0000
#define BOARD_NUM_MASK 0X0000FFFF

#define DPU_CORE_MAX (16)

//according to ver_reg release @2018072422
#define VER_MAX_ENTRY (12)

#define MAX_REG_NUM 32
#define DPU_IOCTL_MAGIC 'D'

/* create a new DPU kernel */
#define DPU_IOCTL_CREATE_KERNEL _IOWR(DPU_IOCTL_MAGIC, 1, struct ioc_kernel_manu_t *)
/* allocate DPU memory */
#define DPU_IOCTL_MEM_ALLOC _IOWR(DPU_IOCTL_MAGIC, 2, struct ioc_mem_alloc_t *)
/* free DPU memory */
#define DPU_IOCTL_MEM_FREE _IOWR(DPU_IOCTL_MAGIC, 3, struct ioc_mem_free_t *)
/* start DPU */
#define DPU_IOCTL_RUN _IOWR(DPU_IOCTL_MAGIC, 4, struct ioc_session_submit_t *)
/* reset DPU */
#define DPU_IOCTL_RESET _IOWR(DPU_IOCTL_MAGIC, 5, int)
/* check if task finished (DISCARDED after dnndk v1.08 release ) */
#define DPU_IOCTL_DONE _IOWR(DPU_IOCTL_MAGIC, 6, struct ioc_task_done_t *)
/* dump the values of dpu registers */
#define DPU_IOCTL_DUMP_STATUS _IOWR(DPU_IOCTL_MAGIC, 7, struct ioc_status_t *)
/* init dpu registers */
#define DPU_IOCTL_INIT _IOWR(DPU_IOCTL_MAGIC, 8, int)
/* create a new DPU task */
#define DPU_IOCTL_CREATE_TASK _IOWR(DPU_IOCTL_MAGIC, 9, struct ioc_task_manu_t *)
/* clean/flush cache range to ensure content written into RAM */
#define DPU_IOCTL_CACHE_FLUSH _IOWR(DPU_IOCTL_MAGIC, 10, struct ioc_cache_ctrl_t *)
/* invalid cache range to ensure following reading got from RAM instead of cache */
#define DPU_IOCTL_CACHE_INVALID _IOWR(DPU_IOCTL_MAGIC, 11, struct ioc_cache_ctrl_t *)
/* invalid cache range to ensure following reading got from RAM instead of cache */
#define DPU_IOCTL_PORT_PROFILE _IOWR(DPU_IOCTL_MAGIC, 12, struct port_profile_t *)
/* softmax request */
#define DPU_IOCTL_RUN_SOFTMAX _IOWR(DPU_IOCTL_MAGIC, 13, struct ioc_sm_t *)
/* set cache flag (0:default,disable ; 1: enable) */
#define DPU_IOCTL_SET_CACHEFLG _IOWR(DPU_IOCTL_MAGIC, 14, int)
/* run FC */
#define DPU_IOCTL_RUN_FC _IOWR(DPU_IOCTL_MAGIC, 15, struct ioc_fc_t *)
/* run resize */
#define DPU_IOCTL_RUN_RESIZE _IOWR(DPU_IOCTL_MAGIC, 16, struct ioc_resize_t *)
/* alloc share memory */
#define DPU_IOCTL_ALLOC_SHM _IOWR(DPU_IOCTL_MAGIC, 17, struct ioc_mem_alloc_t *)
/* free share memory */
#define DPU_IOCTL_FREE_SHM _IOWR(DPU_IOCTL_MAGIC, 18, struct ioc_mem_free_t *)
/* get dpu capabilities info */
#define DPU_IOCTL_CAPS _IOWR(DPU_IOCTL_MAGIC, 19, dpu_caps_t *)
/* start DPU for ABIversion after v1.0 */
#define DPU_IOCTL_RUN2 _IOWR(DPU_IOCTL_MAGIC, 20, struct ioc_session_submit_new_t *)
/* get dpu core info */
#define DPU_IOCTL_CAPS_CORE _IOWR(DPU_IOCTL_MAGIC, 21, dpu_info_t *)


const static uint32_t VER_RESERVERD[VER_MAX_ENTRY] = {
	0x00000000,
	0x00000000,
	0xFFFF0000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0xFFFFFFF0,
	0xFFFFFFFF,
	0x00000000,
	0xFFFF0000,
	0xE0000000
};

typedef enum {
	DPU_HP_WIDTH_0 = 0,
	DPU_HP_WIDTH_1 = 1,
	DPU_HP_WIDTH_2 = 2,
	DPU_HP_WIDTH_3 = 3,
	DPU_HP_WIDTH_4 = 4,
	DPU_HP_WIDTH_5 = 5,
	DPU_HP_WIDTH_6 = 6,
	DPU_HP_WIDTH_7 = 7,
	DPU_HP_WIDTH_RESERVE = 8
} dpu_hp_width_t;

typedef enum {
	DPU_DATA_WIDTH_0 = 0,
	DPU_DATA_WIDTH_1 = 1,
	DPU_DATA_WIDTH_2 = 2,
	DPU_DATA_WIDTH_RESERVE = 3
} dpu_data_width_t;

typedef enum {
	DPU_BANK_GROUP_0 = 0,
	DPU_BANK_GROUP_1 = 1, /* Invalid value for DPU bank group */
	DPU_BANK_GROUP_2 = 2,
	DPU_BANK_GROUP_3 = 3,
	DPU_BANK_GROUP_4 = 4,
	DPU_BANK_GROUP_RESERVE = 5
} dpu_bank_group_t;

/*DPU target list*/
typedef enum {
	DPU_TARGET_UNKNOWN = 0, /* without dpu target info */
	DPU_TARGET_V1_1_3 = 1,
	DPU_TARGET_V1_3_0 = 2,
	DPU_TARGET_V1_3_1 = 3,
	DPU_TARGET_V1_3_2 = 4,
	DPU_TARGET_V1_3_3 = 5,
	DPU_TARGET_V1_3_4 = 6,
	DPU_TARGET_V1_3_5 = 7,
	DPU_TARGET_V1_4_0 = 8,
	DPU_TARGET_V1_4_1 = 9,
	DPU_TARGET_V1_4_2 = 10,
	DPU_TARGET_V1_3_6 = 11,
	DPU_TARGET_V1_3_7 = 12,
	DPU_TARGET_RESERVE = 13
} dpu_target_t;

/*DPU arch list*/
typedef enum {
	DPU_ARCH_UNKNOWN = 0, /* without dpu arch info */
	DPU_ARCH_B1024F = 1,
	DPU_ARCH_B1152F = 2,
	DPU_ARCH_B4096F = 3,
	DPU_ARCH_B256F = 4,
	DPU_ARCH_B512F = 5,
	DPU_ARCH_B800F = 6,
	DPU_ARCH_B1600F = 7,
	DPU_ARCH_B2048F = 8,
	DPU_ARCH_B2304F = 9,
	DPU_ARCH_B8192F = 10,
	DPU_ARCH_B3136F = 11,
	DPU_ARCH_B288F = 12,
	DPU_ARCH_RESERVE = 13
} dpu_arch_t;

typedef enum {
	DPU_HP_CONNECT_0 = 0,
	DPU_HP_CONNECT_1 = 1,
	DPU_HP_CONNECT_2 = 2,
	DPU_HP_CONNECT_3 = 3,
	DPU_HP_CONNECT_4 = 4,
	DPU_HP_CONNECT_RESERVE = 5
} dpu_hp_connect_t;

typedef struct {
	uint32_t dpu_arch;
	uint32_t dpu_freq;
	float peak_perf;
} dpu_info_base_t;

/** @Descriptor for DPU core
 */
typedef struct {
	dpu_info_base_t base;
	uint32_t dpu_target;
	uint32_t irq;
}dpu_info_t;

/*
 * @Descriptor for DPU external IP such as softmax, full-connect and resize etc.
 */
typedef struct {
	int valid; /*whether this extension/IP exists in BOOT.BIN, 0: not exist; 1: exist*/
	int enable; /*whether use this extension: 0: not use; 1: use*/
	int version; /*ip verson info*/
	int irq; /*interupt info*/
} dpu_extension_info_t;

/** @Descriptor for DPU function such as depthwise conv and serdes etc.
 */
typedef struct {
	int version; /*version info; it means the function do not exist when version is 0.*/
} dpu_function_info_t;

/*
 * @Descriptor for DPU capability
 */
#define DPU_CONF_MAGIC  0x434F4E46
typedef struct {
	uint32_t magic;  /* 0x434F4E46: "CONF" for configurable IP, for previous version, it's 0 */
	uint32_t cache;
	char hw_timestamp[32]; /*the time BOOT.BIN was release by hardware team*/
	uint32_t signature_valid; /*whether BOOT.BIN has a vaild signature, 0: invalid, 1: valid*/
	uint32_t dpu_cnt; /*DPU core avaiablity: [0, 1, ... dpu_cnt-1]*/
	dpu_info_t *p_dpu_info; /*p_dpu_info point to an arrry, each enrty contains a DPU*/

	uint32_t hp_width;
	uint32_t data_width;
	uint32_t bank_group;
	uint32_t reg_base; /*register base address for all DPUs*/
	uint32_t reg_size; /*register size covering all DPUs*/
	uint32_t irq_base0;
	uint32_t irq_base1; /*irq_base0 and irq_base1 are used to caculate irqs for IPs in BOOT.BIN*/

	dpu_extension_info_t hdmi;
	dpu_extension_info_t bt1120;
	dpu_extension_info_t fullconnect;
	dpu_extension_info_t softmax;
	dpu_extension_info_t resize;

	dpu_function_info_t relu_p;
	dpu_function_info_t relu_leaky;
	dpu_function_info_t conv_depthwise;
	dpu_function_info_t avgpool;
	dpu_function_info_t serdes_nonlinear;
} dpu_caps_t;

typedef struct {
	uint32_t sys_ip_type;
	uint32_t sys_regmap_ver;
} dpu_conf_sys_t;

typedef struct {
	uint32_t ver_target;
} dpu_conf_sub_version_t;

typedef struct {
	uint32_t arch_hp_bw;
	uint32_t arch_data_bw;
	uint32_t arch_img_bkgrp;
	uint32_t arch_pp;
	uint32_t arch_icp;
	uint32_t arch_ocp;
} dpu_conf_arch_t;

typedef struct {
	uint32_t ram_depth_mean;
	uint32_t ram_depth_bias;
	uint32_t ram_depth_wgt;
	uint32_t ram_depth_img;
} dpu_conf_ram_t;

typedef struct {
	uint32_t load_augm_enable;
	uint32_t load_img_mean_enable;
}dpu_conf_load_t;

typedef struct {
	uint32_t conv_leakyrelu_enable;
	uint32_t conv_relu6_enable;
	uint32_t conv_wr_parallel;
}dpu_conf_conv_t;

typedef struct {
	uint32_t pool_average_enable;
} dpu_conf_pool_t;

typedef struct {
	uint32_t elew_parallel;
} dpu_conf_elew_t;

typedef struct {
	uint32_t dwcv_alu_mode_enable;
	uint32_t dwcv_relu6_enable;
	uint32_t dwcv_parallel;
} dpu_conf_dwcv_t;

typedef struct {
	uint32_t misc_wr_parallel;
} dpu_misc_t;

typedef struct {
	dpu_info_base_t base;
	dpu_conf_sys_t sys;
	dpu_conf_sub_version_t sub_version;
	dpu_conf_arch_t arch;
	dpu_conf_ram_t  ram;
	dpu_conf_load_t load;
	dpu_conf_conv_t conv;
	dpu_conf_pool_t pool;
	dpu_conf_elew_t elew;
	dpu_conf_dwcv_t dwcv;
	dpu_misc_t      misc;
} dpu_configurable_t;


typedef unsigned long kernel_handle_t;
typedef unsigned long task_handle_t;

struct ioc_kernel_manu_t {
	kernel_handle_t kernel_id; /* the handle of DPU kernel (RETURNED) */
};

struct ioc_task_manu_t {
	task_handle_t task_id; /* the handle of DPU task (RETURNED) */
};

struct ioc_mem_alloc_t {
	kernel_handle_t kernel_id; /* the handle of DPU kernel */
	unsigned long size; /* size of memory space to be allocated */
	unsigned long
		addr_phy; /* suv the start pyhsical address of allocated DPU memory (RETURNED) */
};

struct ioc_mem_free_t {
	kernel_handle_t kernel_id; /* the handle of DPU kernel */
	unsigned long addr_phy; /* the start pyhsical address of allocated DPU memory */
};

struct ioc_session_submit_t {
	task_handle_t task_id; /* the handle of DPU task */
	unsigned long addr_io; /* the address to memory region of DPU input/output */
	unsigned long addr_wb; /* the address to memory region of DPU weight/bias */
	unsigned long addr_code; /* the address to memory region of DPU code */

	signed long long time_start; /* the start timestamp before running (RETURNED) */
	signed long long time_end; /* the end timestamp after running (RETURNED) */
	int coreID; /* DPU core ID which Task runs on (RETURNED) */
};

#define REG_NUM 8

/* ioctl session for ABI version after v1.0 */
struct ioc_session_submit_new_t {
	task_handle_t task_id; /* the handle of DPU task */
	signed long long time_start; /* the start timestamp before running (RETURNED) */
	signed long long time_end; /* the end timestamp after running (RETURNED) */
	int coreID; /* the core id of the task*/
	uint32_t addr_code; /* the address for DPU code */

        /* For ABIv1.0
	 *     reg0 store addr for input/output
	 *     ret1 store addr for weights/bias
	 * For ABI after v1.0
	 *     reg assigned by dnnc
	 */
	uint32_t regs[REG_NUM]; /* DPU regs */
};

struct ioc_kernel_run_t {
#ifdef CONFIG_DPU_v1_1_X
	task_handle_t handle_id; /* the handle of DPU task */
	unsigned long addr_io; /* the address for DPU code */
	unsigned long addr_weight; /* the address for DPU weight */
	unsigned long addr_code; /* the address for DPU bias */
	long long time_start; /* the start timestamp before running (RETURNED) */
	long long time_end; /* the end timestamp after running (RETURNED) */
	int core_id; /* the core id of the task*/
#elif defined(CONFIG_DPU_v1_3_0)
	task_handle_t handle_id; /* the handle of DPU task */
	uint32_t addr_code; /* the address for DPU code */
	uint32_t addr0; /* address reg0 */
	uint32_t addr1; /* address reg1 */
	uint32_t addr2; /* address reg2 */
	uint32_t addr3; /* address reg3 */
	uint32_t addr4; /* address reg4 */
	uint32_t addr5; /* address reg5 */
	uint32_t addr6; /* address reg6 */
	uint32_t addr7; /* address reg7 */
	long long time_start; /* the start timestamp before running (RETURNED) */
	long long time_end; /* the end timestamp after running (RETURNED) */
	int core_id; /* the core id of the task*/
#endif
};

struct ioc_kernel_run2_t {
	task_handle_t handle_id; /* the handle of DPU task */
	long long time_start; /* the start timestamp before running (RETURNED) */
	long long time_end; /* the end timestamp after running (RETURNED) */
	int core_id; /* the core id of the task*/
	uint32_t addr_code; /* the address for DPU code */
	union {
		uint32_t addr_io; /* address for DPU IO */
		uint32_t addr0; /* address reg0 */
	};
	union {
		uint32_t addr_weight; /* address for DPU weight */
		uint32_t addr1; /* address reg1 */
	};
	uint32_t addr2; /* address reg2 */
	uint32_t addr3; /* address reg3 */
	uint32_t addr4; /* address reg4 */
	uint32_t addr5; /* address reg5 */
	uint32_t addr6; /* address reg6 */
	uint32_t addr7; /* address reg7 */
};

struct ioc_status_t {
	unsigned long reg[MAX_REG_NUM]; /* the values for DPU control registers (RETURNED) */
};

struct port_profile_t {
	unsigned int port_hp0_read_byte; /* bytes read in HP0 port (128-bit) */
	unsigned int port_hp0_write_byte; /* bytes write in HP0 port (128-bit) */

	unsigned int port_hp1_read_byte; /* bytes read in HP1 port (128-bit) */
	unsigned int port_hp1_write_byte; /* bytes write in HP1 port (128-bit) */

	unsigned int port_hp2_read_byte; /* bytes read in HP2 port (128-bit) */
	unsigned int port_hp2_write_byte; /* bytes write in HP2 port (128-bit) */

	unsigned int port_hp3_read_byte; /* bytes read in HP3 port (128-bit) */
	unsigned int port_hp3_write_byte; /* bytes write in HP3 port (128-bit) */

	unsigned int port_gp_read_byte; /* bytes read in GP port (32-bit for DPU inst) */
	unsigned int port_gp_write_byte; /* bytes write in GP port (32-bit for write profiler) */

	unsigned long long dpu_cycle; /* the start timestamp before running (RETURNED) */
};

struct ioc_cache_ctrl_t {
	unsigned long addr_phy; /* physical address of memory range */
	unsigned long size; /* size of memory range */
};

struct ioc_softmax_t {
	uint32_t width; /* width dimention of Tensor */
	uint32_t height; /* height dimention of Tensor */
	uint32_t input; /* physical address of input Tensor */
	uint32_t output; /* physical address of output Tensor */
	uint32_t scale; /* quantization info of input Tensor */
	uint32_t offset; /* offset value for input Tensor */
};

struct ioc_fc_t {
	uint32_t src; /* src address */
	uint32_t dst; /* target address */
	uint32_t input_dim; /* input dimension */
	uint32_t output_dim; /* output dimension */
	uint32_t batch_dim; /* batch dimension */
	uint32_t start_addr_w; /* start address of weights */
	uint32_t end_addr_w; /* end address of weights */
	uint32_t relu_enable; /* relu enable [0:disable,1:enable] */
};

struct ioc_resize_t {
	uint32_t channel;
	uint32_t src_height;
	uint32_t src_width;
	uint32_t dst_height;
	uint32_t dst_width;
	uint32_t scale_w;
	uint32_t scale_h;
	uint32_t src_addr;
	uint32_t dst_addr;
	uint32_t addr_stride;
	uint32_t src_bias;
	uint32_t dst_bias;
	uint32_t method;
};

struct ioc_mem_fresh_t {
	unsigned long paddr; /* the physical start address*/
	unsigned long size; /* region size*/
};

enum dpu_mem_type_t {
	MEM_CODE = (1 << 1),
	MEM_DATA = (1 << 2),
	MEM_BIAS = (1 << 3),
	MEM_WEIGHT = (1 << 4),
	MEM_INPUT = (1 << 5),
	MEM_OUTPUT = (1 << 6),
	MEM_PROF = (1 << 7)
};

struct dpu_mem_blk_t {
	kernel_handle_t kernel_id; /* the handle of DPU kernel */
	unsigned int size; /* size of memory space to be allocated */
	enum dpu_mem_type_t type; /* type of DPU memory: MEM_CODE, MEM_DATA, MEM_IO */
	unsigned long addr_phy; /* the start pyhsical address of allocated DPU memory */
};
#endif
