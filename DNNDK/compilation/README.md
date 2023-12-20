## Compilation
This part is very simple, and basically we let the program convert the resulting quantized YOLO model to a file for the DPU. A love letter if you will.
For that, we need to execute the compilation script called ***compile.sh***.  It's worth mentioning that the program needs a file describing the DPU, so it generates the file based on the DPU architecture. That file I already included on the folder so no need to worry. But, if you are a masochist, you can create it as described on the tutorial.
It's all set for the compilation. You change the working directory to the compilation folder and then execute the compilation script:

    cd ~
    cd /home/<user>/Desktop/YOLO-on-PYNQ-Z2/DNNDK/compilation
    ./compile.sh
The process might take a **few minutes** and in the end there will be a folder called ***compile***. On this folder there will be information files and the **DPU hybrid executable**.
What we want to do here is to copy those files to the respective folders on the ***Deployment***. As I couldn't place empty folders on Github, you will have to create them yourself.
Go to the Deployment directory and create 3 folders with these names: ***"model"*** , ***"info"*** and ***"objects"*** .

    cp /compile/dpu_yolo.elf /home/<user>/Desktop/YOLO-on-PYNQ-Z2/Deployment/yolo_pynqz2/model
    cp /compile/yolo_kernel.info /home/<user>/Desktop/YOLO-on-PYNQ-Z2/Deployment/yolo_pynqz2/info
    cp /compile/yolo_kernel_graph.jpg /home/<user>/Desktop/YOLO-on-PYNQ-Z2/Deployment/yolo_pynqz2/info
Don't forget... yes, the username on the path's. Change it from user to your respective username.
After creating the folders, we want to move the contents of the compile folder to the respective locations described here:

 - dpu_yolo.elf -> model folder
 - yolo_kernel.info -> info folder
 - yolo_kernel_graph.jpg -> info folder

The code block above does exactly that but you can also do it manually.