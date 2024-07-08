# DPU implementation
This folder contains information relative to the DPU creation on Vivado and the Petalinux part. These topics can be explored in more detail on the [DPU implementation](https://andre-araujo.gitbook.io/yolo-on-pynq-z2/dpu-implementation) chapter on the gitbook tutorial.

## Vivado files
The **pynqz2_dpu** folder refers to the Vivado project files. This was the result of the implementation of the DPU TRD 3.0 and the most important part is the project file ***pynqz2_dpu.xsa*** that will be used on the Petalinux project as a base.

**NOTE:** As the project folder was too big for the repository, you can now access it using this [google drive link](https://drive.google.com/drive/folders/1Tga_d2qfZRlQrvJvwPAHon4qpM60h9UP?usp=sharing).

## Vivado Project
Here you will create and design the DPU Hardware. We will follow the **B1152 architecture** as it is the one that respects the PYNQ-Z2 limitations. 
To make this project you will need to have **Vivado 2020.1** installed as well as the **PYNQ-Z2 board files**. This is quite simple to do, and you can follow the [gitbook tutorial](https://andre-araujo.gitbook.io/yolo-on-pynq-z2/dpu-implementation/creating-the-dpu-3.0#installing-vivado) to see the details of the installation.

For now, I can't replicate the Vivado project as I don't have free space on the disk. You can consult the [Aiotlab DPU tutorial](http://www.aiotlab.org/teaching/fpga/DPU%20on%20PYNQ-Z2_Vivado.pdf) **starting on page 14** as it displays all the steps in a really good way. I will make my own process as soon as possible as it is not right to steal his screenshots.

## Petalinux Files
There are 3 folders on this part. The **SD card** folder is here in case you want to skip the Petalinux part, as I made available the SD card files. If you do skip this "long" part, you should jump to the SD card image title here. The **zynq7020_dnndk_v3.1** folder is the DNNDK package for the board, which should be added to the SD card on top of the other partition files. The last folder, **recipes-modules** was used for the Petalinux part on a configuration.


## Petalinux project
To start the Petalinux project from scratch, you will have to host a Virtual Machine (VM) with Ubuntu 16.04 or 18.04 and install Petalinux version 2019.2. All of this is documented on the gitbook tutorial, but you can also refer to the [Petalinux 2019.2 User Guide](https://docs.xilinx.com/v/u/2019.2-English/ug1144-petalinux-tools-reference-guide).

The goal here is to obtain a SD card image that includes the Board's Linux Operating System with the necessary libraries and packages. The image will have to also include the DPU information, so it can be laid out on the Zynq-7020 FPGA.

---
### Project Setup and first configurations

To start, you should create a folder to serve as a directory for the project on your Desktop. You can name it something like ***dpu_integration*** and then open that directory. Ah, yes, before that, don't forget to start petalinux!

    source /opt/petalinux/settings.sh
    cd Desktop/dpu_integration
Then we need to create the project itself based on the Zynq template (for FPGA's based on the Zynq platform) and name it something like ***pynqz2_dpu***.

    petalinux-create --type project --template zynq --name pynqz2_dpu
    cd pynqz2_dpu/

   After entering the newly created Petalinux project we need to configure it to be based on a particular Hardware Layout. In other words, we have to associate the project with the DPU Hardware we created on Vivado. This association is enabled with the specific ***pynqz2_dpu.xsa*** file on the Vivado project directory. With that said, it's probably a good idea to include the Vivado project on the Virtual Machine Desktop.

    petalinux-config --get-hw-description=/home/<user>/Desktop/Vivado/pynqz2_dpu

Now an interesting menu will be displayed, and you can navigate it with the arrows as indicated on top bar. Here you will need to access these paths and Enable/ Disable the options (**enable -y   disable -n**):

 - Image Packaging Configuration->Root filesystem type **Enable** (EXT4 (SD card))
 - DTG Settings->Kernel Bootargs->**Disable** generate boot args automatically
 
On this last one, paste this on ***user set kernel bootargs***:

    console=ttyPS0,115200 root=/dev/mmcblk0p2 rw earlyprintk quiet rootfstype=ext4 rootwait cma=256M
---
### Messing around with Files

The most important configurations are complete now. The next steps involve some weird files configuration and other stuff. I don't really know what I am doing here, I'm just sticking to the [tutorial on Aiotlab](http://www.aiotlab.org/teaching/fpga/DPU%20on%20PYNQ-Z2%20Petalinux.pdf).

  You need to configure the system-user.dtsi file. You can use the command `gedit` to edit the content of the file with ease.

    gedit project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi

Then you have to clear the file and paste the next code. I suspect this is one of the configurations for the system to accept the DPU Hardware. It's like a blood transfusion, your body will refuse the new blood if it's not the same type, so you configure the body to accept it. Except that doesn't happen of course. I might remove this line...

```
/include/ "system-conf.dtsi"
&amba {
xlnk {
compatible = "xlnx,xlnk-1.0";
};
};
&amba{
dpu{
#address-cells = <1>;
#size-cells = <1>;
compatible = "xilinx,dpu";
base-addr = <0x4f000000>; //CHANGE THIS ACCORDING TO YOUR DESIGN
dpucore {
compatible = "xilinx,dpucore";
interrupt-parent = <&intc>;
interrupts = <0 29 4>; //CHANGE THIS ACCORDING TO YOUR DESIGN
core-num = <0x1>; //CHANGE THIS ACCORDING TO YOUR DESIGN
};
};
};
//usb device tree
/{ 
usb_phy0: usb_phy@0 {
compatible = "ulpi-phy";
#phy-cells = <0>;
reg = <0xe0002000 0x1000>;
view-port = <0x0170>;
drv-vbus;
};
};
&usb0 {
dr_mode = "host";
usb-phy = <&usb_phy0>;
};
```

The next step is to add a file to a directory of the Petalinux project. You need to pass the file ***recipes-modules*** from the Github repository to the VM, and then you need to copy it and paste on the path ***project-spec/meta-user*** on the Petalinux project.
Once you have the file on the VM Desktop, for example, you can copy to the desired directory with this code:

    cp -rv ~/Desktop/recipes-modules/ project-spec/meta-user/

With that done, now you need to add some lines of code to some specific files. 

 - project-spec/meta-user/conf/user-rootfsconfig  -> Add `echo "CONFIG_dpu"`to the first line.
 - project-spec/meta-user/conf/petalinuxbsp.conf -> Add `echo 'IMAGE_INSTALL_append = "dpu"'` on the first line.

---
### Menu Configurations
The messing around with files is done for now, so it's time for more configurations using the fancy Petalinux menu. 

What will be done now is the configuration of the OS packages. In other words, you will tell the Linux OS for the PYNQ-Z2 which packages it will have from "factory". This includes very basic libraries to be able to work with code or to have certain access to functions. To configure the rootfs you paste this command:

    cd Desktop/dpu_integration/pynqz2_dpu
    petalinux-config -c rootfs

Next is a list of packages you have to enable on this menu:
-   modules -> dpu
-   Filesystem Packages -> admin -> sudo, sudo-dev
-   Filesystem Packages —> misc —> packagegroup-petalinux-self-hosted step
-   Petalinux Package Groups → packagegroup-petalinux-python-modules, -dev
-   Petalinux Package Groups → packagegroup-petalinux-x11, -dev
-   Filesystem Packages -> console -> utils -> pkgconfig -> pkgconfig, pkgconfig dev
-   Filesystem Packages -> libs -> gtk+3 -> gtk+3, gtk+3-demo, gtk+3-dev, gtk+3-dbg
-   Petalinux Package Groups -> petalinuxgroup-petalinux-opencv -> opencv, opencv-dev

After enabling all the packages, you can save and exit.

Next step is the kernel configuration. You want to enable USB support and the rest stays the same. To access the configuration menu you type this:

    petalinux-config -c kernel
    
And then you enable USB support on this path:

 - Device Drivers -> USB support -> **Enable** USB announce new devices

Then you, exit and finish the kernel configuration using the following command:

    petalinux-build -c kernel -x finish

Lastly, there is one more file you need to configure. When I say configure, I mean you clear it and paste some fancy code. To access the file and edit it you use the `gedit` command again for simplicity:

    gedit project-spec/meta-user/recipes-kernel/linux/linux-xlnx/devtool-fragment.cfg

You should paste this code and the save the changes on the **save** button.

```
CONFIG_USB_OTG=y
# CONFIG_USB_OTG_FSM is not set
# CONFIG_USB_ZERO_HNPTEST is not set
CONFIG_MEDIA_USB_SUPPORT=y
CONFIG_USB_VIDEO_CLASS=y
CONFIG_USB_VIDEO_CLASS_INPUT_EVDEV=y
CONFIG_USB_GSPCA=m
CONFIG_V4L_PLATFORM_DRIVERS=y
CONFIG_VIDEO_ADV7604=y
CONFIG_USB_HID=y
CONFIG_USB_OHCI_LITTLE_ENDIAN=y
CONFIG_USB_SUPPORT=y
CONFIG_USB_COMMON=y
CONFIG_USB_ARCH_HAS_HCD=y
CONFIG_USB=y
CONFIG_USB_ANNOUNCE_NEW_DEVICES=y
CONFIG_USB_DEFAULT_PERSIST=y
CONFIG_USB_EHCI_HCD=y
CONFIG_USB_EHCI_ROOT_HUB_TT=y
CONFIG_USB_EHCI_PCI=y
CONFIG_USB_EHCI_HCD_PLATFORM=y
CONFIG_USB_ACM=m
CONFIG_USB_PRINTER=m
CONFIG_USB_WDM=m
CONFIG_USB_TMC=m
CONFIG_USB_STORAGE=y
CONFIG_USB_CHIPIDEA=y
CONFIG_USB_CHIPIDEA_OF=y
CONFIG_USB_CHIPIDEA_PCI=y
CONFIG_USB_CHIPIDEA_HOST=y
CONFIG_USB_PHY=y
CONFIG_NOP_USB_XCEIV=y
CONFIG_AM335X_CONTROL_USB=y
CONFIG_AM335X_PHY_USB=y
CONFIG_USB_GPIO_VBUS=y
CONFIG_USB_ULPI=y
CONFIG_USB_ULPI_VIEWPORT=y
```
---
### Finishing Touches
The configurations are finally over. You can breathe now!
Now let's build the project on Petalinux. The program will do it's magic, and we just wait some minutes depending on your VM performance.

    petalinux-build

Then you instruct Petalinux to create the SD card files based on the project:

    petalinux-package --boot --force --fsbl images/linux/zynq_fsbl.elf --fpga images/linux/*.bit --u-boot

And finally you can save all the configurations you have done on a .bsp file, so you don't lose this. Obviously you don't have to do this and nor is it important as it's all documented here. Note that you can name it ***pynq-z2-dpu*** as on the example or something you desire.

    petalinux-package --bsp -p ./ -o pynq-z2-dpu.bsp

---

### SD Card files
After the Petalinux project is done, on the path ***dpu_integration/pynqz2_dpu/images/linux/*** you will find the necessary files to create the SD card image. Alternatively, if you miss the Petalinux project you can use the files on the ***SD card*** folder of this repository.
You should format a Micro SD card and assign two partitions. This can be done with tools like AOMEI Partition Assistant for example. Here are the details for each one:

 - **1st partition:** Called "BOOT"; Format FAT32; 128MB in size;
 - **2nd partition:** Called "rootfs"; Format EXT4; rest of the card space;
 
 On the first one, you should add the files ***BOOT.BIN*** and ***image.ub*** and on the second partition you should add the file ***rootfs.tar.gz*** uncompressed.
 
 **1s Partition:**

    cp ./images/linux/BOOT.BIN /media/<user>/BOOT/
    cp ./images/linux/image.ub /media/<user>/BOOT/

**2nd Partition:**

    sudo tar xzf ./images/linux/rootfs.tar.gz -C /media/<user>/rootfs/

---
### DNNDK Package

Then you should add the DNNDK package ***zynq7020_dnndk_v3.1*** to the image OS as done with this command:

    sudo cp -r ../zynq7020_dnndk_v3.1/* /media/<user>/rootfs/home/root/

This will allow future communication with the DPU using the DNNDK API, so it's very important you add to the system. You can also add it at the Deployment phase using the MobaXTerm interface!