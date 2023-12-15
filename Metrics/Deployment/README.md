<<<<<<< HEAD
version https://git-lfs.github.com/spec/v1
oid sha256:4d5ddad0aceb65c7eff752e48267134286462b86626381a2fb559a007bd3edbb
size 4399
=======
# Deployment
This chapter is about the deployment of the YOLOv3 object detector on the PYNQ-Z2. This folder contains files related to YOLOv3 but you can find Tiny YOLO files on the Auxiliary files of the repository. We will focus on the yolo_pynqz2 folder, and you can later repeat the same process for the Tiny YOLO if you desire. 

First, you should set up the board inserting the Micro SD card with the right image on the board and plug all the necessary cables. For instance, I recommend you use a power regulator to feed the board as the YOLO algorithm is quite demanding on power. You should also establish USB and Ethernet connections.

## Requirements
For this part you will need the PYNQ-Z2 board, obviously, a micro SD card with the [DPU image](https://drive.google.com/file/d/1ETyM51KSWX_h1DVq9ptHPIux89oTrNPy/view?usp=drive_link), Ethernet cable (RJ45), USB to micro USB cable and a power regulator. The YOLO algorithm is quite demanding, so that is why you should use a power regulator to feed the board.
All the necessary material can be consulted on the [Hardware requirements](https://andre-araujo.gitbook.io/yolo-on-pynq-z2/requirements#hardware).
## Board setup
This information can be exploited in more detail on the [Board setup chapter](https://andre-araujo.gitbook.io/yolo-on-pynq-z2/deployment-on-pynq-z2/board-setup) of the tutorial. Essentially, we will need to provide power to the board with the coaxial plug, establish communication via USB and also Ethernet. 
Here are the basic connections:
![enter image description here](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_9.png?raw=true)

 1. Set the Boot jumper to the _SD_ position. This will make the board load the Operating System on the micro SD card;
 2. Set the Power jumper to _REG_, so the board will receive power from the power regulator and not the USB cable;
 3. Insert the Micro SD card with the custom image (the one we flashed with the DPU). Remember to insert it from the back as described on the image.
 4. Connect the micro USB cable between your Host computer and the board.
 5. Connect the Ethernet cable (RJ45) between the Laptop and the board. Don't forget to use the adapter if your computer doesn't have the RJ45 port;
 6. Connect the power adapter's coaxial port on the PYNQ-Z2 to give it the necessary power;
 7. Turn on the PYNQ-Z2 and the boot sequence should correspond to a red LED turning immediately on representing that the board has power. Then the Yellow/ Green LED called _Done_ will turn on showing that the Zynq service is ready.

### Serial communication

For the USB communication (also known as Serial communication) you can use programs like Putty. You just have to identify the correct COM port and assign the 115200 baud rate. **The user is "root" and password is also "root"**. With this done you have access to the board, and can do what you want but because the interface is quite inadequate, I suggest you establish an Ethernet communication. Before I talk about it, while you have the Serial COM open, I suggest you assign an IP address for the board, like `ifconfig eth0 192.168.2.99`. 

### Ethernet communication

We want to have a better interface to work with, so I suggest you install MobaXTerm and connect the application to the PYNQ-Z2 board. For this you should obviously assign the computer an IP address on the same range as the board (192.168.2.1 for example). On XTerm after the connection is established, we need to activate the DNNDK library:

    cd zynq7020_dnndk_v3.1
    ./install.sh



## Execute YOLO inference
When everything is set, we can access the YOLO folder and execute the Makefile, so the code is compiled, and the libraries are made available. This file is associated with the c++ script "yolo_image.cpp" on the *programs* folder. You should check out the code and play with it a little bit to understand how it works.
After the compilation is complete you will see an executable file called "yolo_image" that appears on the interface on the left side. We want to execute it and feed the program the test image "dog.jpg".

    cd ~
    cd yolo_pynqz2
    make
    ./yolo_image dog.jpg

The result should be a window with the image with the detections. The program might crash the window, but you should be able to restart it with the same command. After this you can test any image you want.
**Have fun! :)**

>>>>>>> origin/main
