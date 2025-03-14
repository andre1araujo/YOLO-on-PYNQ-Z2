# :warning: Disclaimer
> This repository is followed by a [step-by-step tutorial on Gitbook](https://andre-araujo.gitbook.io/yolo-on-pynq-z2/) that you can read that explains the whole process. 
> The explanations might contain information that is not 100% correct. I am still learning a lot and this project combines all the things I have learned during its development. Please be sure to notify me about any information you consider incorrect or misleading on the tutorial on GitBook or on the README files of the repository. Thank you!


---
# <img src="https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_19.png?raw=true" alt="Image Description" width="280"/> YOLO-on-PYNQ-Z2
This project consists on implementing a **YOLOv3 object detector** on a **PYNQ-Z2** PLD, making use of the Zynq-7020 to accelerate the inference and provide accurate results.



![Project scheme](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_10.png?raw=true)

In the end, the user should be able to obtain the detection (bounding boxes, class and probability) of an input image.
**On average the inference time of the YOLOv3 on the PYNQ-Z2 is about 0,45 seconds!**

![Sample image](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_11.png?raw=true)

## Examples

If you follow this tutorial you will end up with a YOLO detection algorithm capable of detecting 80 different types of objects. But, you can also make your own specialized object detector for fewer classes. Here you can see two exciting works: one from **Mainul** for **face detection** and the other from **Vinil** and **Bigin** for **PCB defect detection**:

![Sample detections](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/YOLO_samples.png)


## Tutorial
- If you want to **follow the whole development process** to implement the YOLOv3 on the board, I recommend you check out [my tutorial on gitbook](https://andre-araujo.gitbook.io/yolo-on-pynq-z2/)! It's pretty in depth, but you should be able to toss out any excessive information. In case you **miss any of the steps** the **Auxiliary files folder** contains the necessary files for each part of the project.

- If you want, you can also **follow the tutorials presented on each folder** as they are more straight to the point and might assume you have previous knowledge.

- Finally, if you just want to **execute the YOLOv3 on your PYNQ-Z2** you can download the SD card image [here](https://drive.google.com/file/d/1ETyM51KSWX_h1DVq9ptHPIux89oTrNPy/view?usp=drive_link) and follow the process on the [Requirements chapter](https://andre-araujo.gitbook.io/yolo-on-pynq-z2/requirements) of the gitbook tutorial.

## Project organization
This project involves 3 steps to get to the final result. The steps are in chronological order, so you first go to the [DPU implementation folder](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/DPU%20implementation) and follow the instructions, then to the [DNNDK](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/DNNDK) folder and lastly to the [Deployment](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/Deployment) folder. There is also a [Metrics](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/Metrics) folder if you want to obtain the accuracy of your model.
Here is a little description of the topics:

 **1. DPU implementation** - Here we create a Hardware layout capable of processing various types of Neural Networks (including YOLO) called the DPU. The DPU is Xilinx's property (You can check the [DPU Product Guide](https://docs.xilinx.com/r/3.3-English/pg338-dpu/Customizing-and-Generating-the-Core-in-Zynq-7000-Devices)) and here we will adopt the best architecture for the PYNQ-Z2 limitations. Then we will create a SD card image with the DPU and other Software configurations.

[<img src="https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_3.png?raw=true" alt="Image Description" width="300"/>](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/DPU%20implementation)


 **2. DNNDK** - The YOLO model will be compressed and optimized to fit the PYNQ-Z2 board as the DPU has it's limitations and can't use the raw model. To do this we will use Xilinx tool DNNDK v3.1. This program will allow quantization of the model and also compilation, resulting on a file that can communicate with the DPU telling it to form the compressed YOLO Network.

[<img src="https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_4.png?raw=true" alt="Image Description" width="300"/>](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/DNNDK)
 

 **3. Deployment** - This is the part where we will set up the board in order to see the results. The files will be organized and compiled resulting on an executable file that is able to run YOLOv3 inference on a desired image.

[<img src="https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_5.png?raw=true" alt="Image Description" width="400"/>](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/Deployment)

**4. Metrics** - You have the YOLOv3 model working on the PYNQ-Z2 and now you want to know the accuracy of the model. On this chapter we will go trough all the steps to do that evaluation so at the end you can see if the detections are accurate enough for your applications. 

[<img src="https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_16.png?raw=true" alt="Image Description" width="400"/>](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/Metrics)

## Development flow
So, how do we tie everything down to make the YOLOv3 object detector work on the PYNQ-Z2? 
It's simple, we **design the DPU** with the architecture that fits the PYNQ-Z2 limitations, include that information and the **Operating System on the SD card** so when we boot the board, the DPU is implemented on the Zynq-7020 chip and the OS is ready with the required packages. Then we **compress the YOLOv3 model** using the **DNNDK v3.1** tool and join the resulting file with a c++ script that can interact with the DPU and do the post-processing of the image. Those files are the included on the OS and after proceeding with the **board setup and start-up**, we compile the code and execute the YOLO executable associating an image to get detections on.

![Project development scheme](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_2.png?raw=true)

## Help
If you have any doubts or suggestions, express them on the [issues tab](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/issues). I will try to answer everything as soon as possible.

You can also contact me by email at: a21185@alunos.ipca.pt

## Acknowledgements
This project was made possible with the help of my **professor adviser**, my **colleagues** from the Engineering course and also other **creators**.
First, I want to thank **Wu-Tianze** for the inspiration he gave me with his project. All of this wouldn't be possible without his work guidelines and his help. Thanks to **Jin-Chen** for his work on Aiotlab which was really helpful on this project, particularly the first part. Thanks to the **community** for all the information on the forums and all that shared curiosity.
Finally, I want to ****Thank you** for visiting this project!** 

Special thanks to [Aryan](https://github.com/MaRcOsss1) for the help on updating the tutorial so it is kept up to date!

Wu-Tianze's work: https://github.com/wutianze/dnndk-pynqz2

Jin-chen's work: http://www.aiotlab.org/teaching/fpga.html

PYNQ forum: https://discuss.pynq.io/

## Feedback

> **If you find my work helpfull in any way consider these actions:**
> * Give the project a nice star :star:
> * Give some feedback on the GitBook tutorial :smiley:
> * Contact me via e-mail :mailbox_with_mail:


---
**Work to be done until product is finished:**
- [ ] ~~Write chapter regarding the DPU 3.0 development on Vivado;~~
- [X] Write metrics chapter (if not **lazy**);
- [X] Create README files for each folder on the repository, introducing everything and making a compressed tutorial;
- [X] Update the repository to match the needs of someone willing to follow the whole process;
- [X] Include a folder with extra files in case the user gets lost halfway through the process;
- [X] Correct my grammar mistakes (probably);
- [X] Create and add some interesting images, so the user's eyes get happy and fulfilled;
- [X] Make a fancy introductory README file and **don't forget to thank everyone at the end!**;
- [X] Add e-mail contact; 
