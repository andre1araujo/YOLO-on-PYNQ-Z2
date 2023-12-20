""" (c) Copyright 2019 Xilinx, Inc. All rights reserved.
--
-- This file contains confidential and proprietary information
-- of Xilinx, Inc. and is protected under U.S. and
-- international copyright and other intellectual property
-- laws.
--
-- DISCLAIMER
-- This disclaimer is not a license and does not grant any
-- rights to the materials distributed herewith. Except as
-- otherwise provided in a valid license issued to you by
-- Xilinx, and to the maximum extent permitted by applicable
-- law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
-- WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
-- AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
-- BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
-- INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
-- (2) Xilinx shall not be liable (whether in contract or tort,
-- including negligence, or under any other theory of
-- liability) for any loss or damage of any kind or nature
-- related to, arising under or in connection with these
-- materials, including for any direct, or any indirect,
-- special, incidental, or consequential loss or damage
-- (including loss of data, profits, goodwill, or any type of
-- loss or damage suffered as a result of any action brought
-- by a third party) even if such damage or loss was
-- reasonably foreseeable or Xilinx had been advised of the
-- possibility of the same.
--
-- CRITICAL APPLICATIONS
-- Xilinx products are not designed or intended to be fail-
-- safe, or for use in any application requiring fail-safe
-- performance, such as life-support or safety devices or
-- systems, Class III medical devices, nuclear facilities,
-- applications related to the deployment of airbags, or any
-- other applications that could lead to death, personal
-- injury, or severe property or environmental damage
-- (individually and collectively, "Critical
-- Applications"). Customer assumes the sole risk and
-- liability of any use of Xilinx products in Critical
-- Applications, subject only to applicable laws and
-- regulations governing limitations on product liability.
--
-- THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
-- PART OF THIS FILE AT ALL TIMES.
"""

from ctypes import *
import cv2
import numpy as np
from dnndk import n2cube, dputils
import os
import threading
import time
import sys

l = threading.Lock()

def RunDPU(kernel, img, count):
    """
    DPU run function
    kernel: dpu kernel
    img: image to be run
    count : test rounds count
    """
    """Create DPU Tasks from DPU Kernel"""
    task = n2cube.dpuCreateTask(kernel, 0)
    while count < 1000:
        """Load image to DPU"""
        dputils.dpuSetInputImage2(task, KERNEL_CONV_INPUT, img)
        
        """Get input Tesor"""
        tensor = n2cube.dpuGetInputTensor(task, KERNEL_CONV_INPUT)
        
        """Model run on DPU"""
        n2cube.dpuRunTask(task)
        
        """Get the output tensor size from FC output"""
        size = n2cube.dpuGetOutputTensorSize(task, KERNEL_FC_OUTPUT)
        
        """Get the output tensor channel from FC output"""
        channel = n2cube.dpuGetOutputTensorChannel(task, KERNEL_FC_OUTPUT)
        
        softmax = [0 for i in range(size)]
        
        """Get FC result"""
        conf = n2cube.dpuGetOutputTensorAddress(task, KERNEL_FC_OUTPUT)
        n2cube.dpuGetTensorData(conf, softmax, size)
        
        """Get output scale of FC"""
        outputScale = n2cube.dpuGetOutputTensorScale(task, KERNEL_FC_OUTPUT)
        
        """Run softmax"""
        n2cube.dpuRunSoftmax(conf, softmax, channel, size // channel, outputScale)
        
        l.acquire()
        count = count + threadnum
        l.release()

    """Destroy DPU Tasks & free resources"""
    n2cube.dpuDestroyTask(task)

global threadnum
threadnum = 0
KERNEL_CONV = "inception_v1_0"
KERNEL_CONV_INPUT = "conv1_7x7_s2"
KERNEL_FC_OUTPUT = "loss3_classifier"

"""
brief Entry for runing GoogLeNet neural network
"""
def main(argv):

    """Attach to DPU driver and prepare for runing"""
    n2cube.dpuOpen()

    """Create DPU Kernels for GoogLeNet"""
    kernel = n2cube.dpuLoadKernel(KERNEL_CONV)

    image_path = "./../common/image_224_224/"
    
    listimage = os.listdir(image_path)
    
    path = os.path.join(image_path, listimage[0])
    
    print("Loading  %s" %listimage[0])
    
    img = cv2.imread(path)
    
    threadAll = []
    global threadnum
    threadnum = int(argv[1])
    print("Input thread number is: %d" %threadnum)
    
    time1 = time.time()
    
    for i in range(int(threadnum)):
        t1 = threading.Thread(target=RunDPU, args=(kernel, img, i))
        threadAll.append(t1)
    for x in threadAll:
        x.start()
    for x in threadAll:
        x.join()
    
    time2 = time.time()
    
    timetotal = time2 - time1
    fps = float(1000 / timetotal)
    print("%.2f FPS" %fps)

    """Destroy DPU Tasks & free resources"""
    rtn = n2cube.dpuDestroyKernel(kernel)

    """Dettach from DPU driver & release resources"""
    n2cube.dpuClose()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("please input thread number.")
    else :
        main(sys.argv)
