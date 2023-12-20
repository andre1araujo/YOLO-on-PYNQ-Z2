# makefile for DPU driver (outside the linux kernel tree)
# a typical command for build the driver:
#		make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- DPU_TARGET=1.3 KERNEL_SRC=/path/to/your/kernel
obj-m:=dpu.o
dpu-objs:=dpucore.o dpuext.o

SRC :=$(shell pwd)

MCFLAGS :=
ifeq ($(DPU_TARGET),1.1)
	MCFLAGS +=-DCONFIG_DPU_v1_1_X
else
	MCFLAGS +=-DCONFIG_DPU_v1_3_0
endif

ifeq ($(ARCH),arm)
	MCFLAGS +=-DSIG_BASE_ADDR=0X4FF00000 -DCACHE_OFF
endif
ifeq ($(ARCH),arm64)
	MCFLAGS +=-DSIG_BASE_ADDR=0X8FF00000
endif

# check the compiler version
GCCV1 := $(shell $(CROSS_COMPILE)gcc -dumpversion | cut -f1 -d. )
GCCV2 := $(shell $(CROSS_COMPILE)gcc -dumpversion | cut -f2 -d. )
GCCV3 := $(shell expr $(GCCV1) \* 100 + $(GCCV2))

DT_FLAG := $(shell expr `echo $(GCCV3)` \>= 409)
ifeq ($(DT_FLAG),1)
	MCFLAGS +=-Wno-error=date-time -Wno-date-time
endif

all:
	$(MAKE) CFLAGS_MODULE='$(MCFLAGS)' -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) CFLAGS_MODULE='$(MCFLAGS)' -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
