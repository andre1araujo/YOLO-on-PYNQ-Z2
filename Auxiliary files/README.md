# Auxiliary Files
This folder contains some resulting files in case you skipped any of the steps. The organization is based on the name of the folders, so you might have noticed that the DPU implementation is not included here. That's because that folder already contains all the necessary files in case you skip that part. The rest of the chapters are described here:

## Model conversion
There are two files, one called ***yolo.h5*** and other ***yolo.pb***. These are the YOLO models in Keras and Tensorflow respectively.
Also, there is a folder called **Tiny** that contains the Tiny YOLO model in Keras and in Tensorflow in case you want to experiment with them. 

**NOTE**: This folder is no longer available on the github repository. You can now access it through [here](https://drive.google.com/drive/folders/1wvNceFlJtY_OgCthoCUySj9VA7MOkLdq?usp=sharing) if you need it.

## Quantization
Here are the files that come as a result from the quantization process. The files are stored on a folder called **quantize_results** as they would originally. The most important file is the ***deploy_model.pb*** which is needed on the compilation process.

**NOTE**: This folder is no longer available on the github repository. You can now access it through [here](https://drive.google.com/drive/folders/1Z9f7k2nSqBmhzujIGrHJilvVg2uyP-CT?usp=sharing) if you need it.

## Compilation
In case you couldn't execute the compilation process, there is folder **compile** with the resulting files. Here there are two informational files which are not important but also there is the ***dpu_yolo.elf*** file which is the resulting binary file for the DPU. This last one is very important and should be placed on the **yolo_pynqz2 folder** of the [Deployment](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/Deployment) as the model file.

## Deployment
There are two different folders: one relative to the **YOLOv3 model** and other to the **Tiny YOLO model**. Once again, we focus on the YOLO model, but I included the other one in case you want to test it.
The ***yolo_pynqz2*** folder is completed as it should be so it's here in case you just want to skip the DNNDK process.

Each folder contains executables relative to the **inference on a single image** but also the **real-time object detector using a USB webcam**. You can learn how to change the Makefile to compile either of those programs on the [Deployment chapter](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/Deployment#execute-yolo-in-real-time), in case you want to have fun with the code.

## Metrics
Here there are 2 folders and one executable file. The folder **labels_yolo** refers to the detections for the YOLOv3 object detector. It contains 5000 text files with annotations relative to the COCO validation dataset.
The **tiny_yolo_pynqz2_data** refers to the set of files necessary to run the program that generates the annotations for the COCO validation dataset. This folder is added on the PYNQ-Z2 environment just like the yolo_pynqz2_data but in this case it has the Tiny YOLO and not the YOLOv3.
Finally, the **yolo** executable file is there in case the compilation process fails for some reason. You can include it directly on the yolo_pynqz2_data folder.