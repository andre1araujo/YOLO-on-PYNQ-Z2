## (c) Copyright 2019 Xilinx, Inc. All rights reserved.
##
## This file contains confidential and proprietary information
## of Xilinx, Inc. and is protected under U.S. and
## international copyright and other intellectual property
## laws.
##
## DISCLAIMER
## This disclaimer is not a license and does not grant any
## rights to the materials distributed herewith. Except as
## otherwise provided in a valid license issued to you by
## Xilinx, and to the maximum extent permitted by applicable
## law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
## WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
## AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
## BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
## INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
## (2) Xilinx shall not be liable (whether in contract or tort,
## including negligence, or under any other theory of
## liability) for any loss or damage of any kind or nature
## related to, arising under or in connection with these
## materials, including for any direct, or any indirect,
## special, incidental, or consequential loss or damage
## (including loss of data, profits, goodwill, or any type of
## loss or damage suffered as a result of any action brought
## by a third party) even if such damage or loss was
## reasonably foreseeable or Xilinx had been advised of the
## possibility of the same.
##
## CRITICAL APPLICATIONS
## Xilinx products are not designed or intended to be fail-
## safe, or for use in any application requiring fail-safe
## performance, such as life-support or safety devices or
## systems, Class III medical devices, nuclear facilities,
## applications related to the deployment of airbags, or any
## other applications that could lead to death, personal
## injury, or severe property or environmental damage
## (individually and collectively, "Critical
## Applications"). Customer assumes the sole risk and
## liability of any use of Xilinx products in Critical
## Applications, subject only to applicable laws and
## regulations governing limitations on product liability.
##
## THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
## PART OF THIS FILE AT ALL TIMES.

PROJECT   =   resnet50

CXX       :=   g++
OBJ       :=   main.o

# linking libraries of OpenCV
LDFLAGS   =   $(shell pkg-config --libs opencv)

# linking libraries of DNNDK
LDFLAGS   +=  -lhineon -ln2cube -ldputils

CUR_DIR =   $(shell pwd)
SRC     =   $(CUR_DIR)/src
BUILD   =   $(CUR_DIR)/build
VPATH   =   $(SRC)
MODEL   =   $(CUR_DIR)/model/dpu_resnet50_0.elf

ARCH    =   $(shell uname -m | sed -e s/arm.*/armv71/ -e s/aarch64.*/aarch64/)
CFLAGS :=   -O2 -Wall -Wpointer-arith -std=c++11 -ffast-math
ifeq ($(ARCH),armv71)
    CFLAGS += -mcpu=cortex-a9 -mfloat-abi=hard -mfpu=neon
endif
ifeq ($(ARCH),aarch64)
    CFLAGS += -mcpu=cortex-a53
endif

.PHONY: all clean

all: $(BUILD) $(PROJECT)

$(PROJECT) : $(OBJ)
	$(CXX) $(CFLAGS) $(addprefix $(BUILD)/, $^) $(MODEL) -o $@ $(LDFLAGS)

%.o : %.cc
	$(CXX) -c $(CFLAGS) $< -o $(BUILD)/$@

clean:
	$(RM) -rf $(BUILD)/*.o $(BUILD)
	$(RM) $(PROJECT)

$(BUILD) :
	-mkdir -p $@
