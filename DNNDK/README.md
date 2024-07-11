# DNNDK
This is the part where we compress the YOLOv3 model to fit the PYNQ-Z2 limitations / DPU requirements. It is divided in 3 phases as you might have noticed by the folders:

 1. **Model conversion** - This part is where we convert the original YOLOv3 model in Darknet to Tensorflow. Tensorflow is a more well known Framework for Neural Networks and is one which is supported by DNNDK v3.1.
 2. **Quantization** - It represents a process of compression of the YOLOv3 model. The Xilinx tool will reduce the Network parameters, so they obey the board's limitations. The process is done with a quantization script and needs a dataset to adjust the Neural Network parameters.
 3. **Compilation** - It's the last part of this chapter and it involves the process of converting the quantized YOLOv3 Tensorflow model to a binary file with instructions the DPU understands. In other words, using DNNDK, we will create a compilation script that converts the quantized model in a file that will interact with the DPU later on. This file basically describes to the DPU the Neural Network (the one we compressed).

## Requirements
To do the process from here, you need to have **DNNDK v3.1 installed on an Ubuntu Operating System**. The OS must be Ubuntu 18.04 or 16.06 for this to work. I would also recommend you do the installation on an anaconda virtual environment. The environment must be based on python 3.6, and you need to install other dependencies. 
It's safe to say you should consult the [Model optimization and compilation](https://andre-araujo.gitbook.io/yolo-on-pynq-z2/model-optimization-and-compilation) chapter on the Gitbook tutorial for the details. On the other hand, you can consult the [DNNDK v3.1 User Guide](https://docs.xilinx.com/v/u/1.6-English/ug1327-dnndk-user-guide) as it has everything also well explained.

**DNNDK v3.1 download:** https://www.xilinx.com/member/forms/download/xef.html?filename=xilinx_dnndk_v3.1_190809.tar.gz

**Anaconda installation:**

    cd /tmp
    sudo apt-get install wget
    wget https://repo.anaconda.com/archive/Anaconda3-2020.02-Linux-x86_64.sh
    bash Anaconda3-2020.02-Linux-x86_64.sh

**DNNDK setup:**

    conda create -n decent pip python=3.6
    source activate decent
    cd Downloads/xilinx_dnndk_v3.1/host_x86/decent-tf/ubuntu18.04/
    pip install ./tensorflow-1.12.0-cp36-cp36m-linux_x86_64.whl
    pip install numpy opencv-python==4.6.0.66 scikit-learn==0.24.2 scipy progressbar2 keras==2.2.4
    pip install pillow
    pip install 'h5py==2.10.0' --force-reinstall
    sudo apt install libgoogle-glog-dev
    sudo apt-get install graphviz
    cd ~
    cd Downloads/xilinx_dnndk_v3.1/host_x86
Change "install.sh" file on line 4 with this: `support_board=(ZCU102 ZCU104 ZedBoard Ultra96 PynqZ2)`

    chmod +x install.sh
    sudo ./install.sh PynqZ2
With DNNDK installed you can proceed to the Model conversion.

