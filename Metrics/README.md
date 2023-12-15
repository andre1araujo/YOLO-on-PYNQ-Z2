# Metrics
The objective here is to run the YOLO object detector on a large set of images and obtain a folder with the detections for each image on a specific format. The detections consist on information about the bounding box, class and probability for each object on each image. The format will be the one on the next image:

![enter image description here](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_17.png?raw=true)

These text files are called annotations, and they will be used later on a program to be compared with the ground truth annotations. In other words, the detections made by the YOLO on the PYNQ-Z2 will be compared to the exact perfect results, so we can evaluate its accuracy. 
We will use a program on the **yolo_pynqz2_data** folder to achieve the objective. The detections will be stored on text files with the same name as the image it refers to. 
 
![enter image description here](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_14.png?raw=true)
 

## COCO validation Dataset
First, on the ***yolo_pynqz2_data*** folder, you have to add a folder named **images** and other named **labels**. The images' folder will have the COCO validation dataset images which is about 5000 images. You can download the validation dataset images and annotations on the [Downloads page](https://cocodataset.org/#download) on the COCO website as "**2017 val images**". Alternatively, use these links:

 - COCO validation images: http://images.cocodataset.org/zips/val2017.zip
 - COCO annotations: http://images.cocodataset.org/annotations/annotations_trainval2017.zip

You need uncompress the *2017 val images* to the **images** folder on *yolo_pynqz2_data* you just created. Make sure that in the end the packages looks like this:

> **images** 
>>  *5000 images*
>
> **infos** 
> >  yolo_kernel.info\
> > yolo_kernel_graph.jpg
>
> **labels** \
> **model**
>> dpu_yolo.elf
>
> **objects**
>>yolo_data.o
>
> **programs**
>> yolo_data.cpp
>
> **makefile**
> **monitor.py**

## Detections for YOLO on the PYNQ-Z2

When everything is ready, you power up the PYNQ-Z2 just like on the [Deployment](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/tree/main/Deployment) and open MobaXTerm. You now **copy yolo_pynqz2_data** to the left part of the application, so you can work with the Terminal. This process can take some time because there are a lot of images.

![enter image description here](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_12.png?raw=true)

There is a script called ***yolo_data.cpp*** which is already defined to perform inference on all images and register the detections on text files for each one. The text files will have the same name as the corresponding image. All we have to do is compile the code and then execute the ***monitor.py*** script which assures that if the DPU fails, it will start the process again where it left.

    cd yolo_pynqz2_data
    make
    python monitor.py

Now you just wait until all of the annotations are generated. In the end there should be 5000 text files with annotations for each object of the image on the labels' folder. I need you to copy that folder to the Windows Desktop as we will be needing it later.

If you were not able to do generate the detections for the model, you can access the Auxiliary files under the Metrics folder and use the labels_yolo folder as it has all the 5000 text files with the annotations.

Finally, if you want to try out this but for the Tiny yolo object detector, there is also a folder on the Auxiliary files you can use to generate the detections.

## Metrics Program
I used a program called **[review_object_detection_metrics](https://github.com/rafaelpadilla/review_object_detection_metrics)**, developed by Rafael Padilla on Github. This program as the advantage of having a User Interface (UI) so the process is more easy to handle in general. You can use it on Ubuntu or Windows, but I used it on the first option as I already had Anaconda installed.

### Installation
---
The installation process is well documented on the respective Github page, but I will briefly go through the process because there was a little problem.
First of all, [clone the Github repository](https://github.com/rafaelpadilla/review_object_detection_metrics/archive/refs/heads/main.zip) with the program and place it somewhere only we know. I am just kidding, you put it on the Desktop for example.
To use this program you need a Virtual Environment based on python 3.9. There is also a file called environment.yml which contains the packages to be installed. We need to change this file as there were some incompatibilities with OpenCV and QT. We will install those packages separately. When removing those from the file, it will end up like this:

```
# conda env create -n <env_name> --file <this_file>.yml
channels:
  - conda-forge
  - defaults
dependencies:
  - ipython
  - jupyter
  - matplotlib
  - notebook
  - numpy=1.19
  - pandas=1.1
  - pip
  - pytest=6.1
  - python==3.9
  - pip:
    - awscli==1.18.180
    - chardet==3.0.4
    - click==7.1.2
    - flake8==3.8.4
    - python-dotenv==0.15.0
    - pyyaml==5.3.1
    - sphinx==3.3.1
```

Then, we create the virtual environment based on python 3.9 and install the dependencies with the environment.yml file. After that we will install the other two libraries separately with compatible versions.

    conda env create -n metrics_env --file {path/to/repository/location}/environment.yml --verbose
    conda activate metrics_env
    pip install opencv-python==4.5.5.62 pyqt5==5.12 --verbose

After the environment is set, you just need to start the program by accessing the folder of the repository and then paste two other commands.

    cd {path/to/repository/location}
    python setup.py install
    python run.py
    
A window should appear on the screen. That is the program UI we will interact with.

### Using the program to obtain metrics
---
Now, let's use the program to obtain some metrics. If you want to know more about the existing metrics and what they mean, I wrote a little document introducing that topic. You can consult it on the [Metrics Context chapter on Gitbook](https://andre-araujo.gitbook.io/yolo-on-pynq-z2/metrics/metrics-context).

I advise you create a file to store everything to stay organized. For example, you can have a folder with the following organization:

![enter image description here](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_15.png?raw=true)

The **ground truth images** should refer to the COCO validation images and the **annotations** to the file instances_val2017.json. The **labels_yolo** should be the labels' folder you obtained from the PYNQ-Z2, the **results** should be an empty folder to store the final results and the **classes.txt** file you can find on the repository.

The program UI is divided in 3 parts: the top part is to define the paths to the ground truth Dataset, the middle part is for the detections and the lower part is to define the output metrics. 
The ground truth Dataset is the COCO validation dataset image and annotations. The annotations indicate exactly where the bounding boxes on each image should be, and the file is the ***instances_val2017.json*** you downloaded from the COCO website.
Then the detections are relative to the text files you obtained on the PYNQ-Z2 **labels folder**.  There is a place to select the format of the annotations, which is the **"class_id, left, top, width, height (absolute)"** we defined on the start of the PYNQ-Z2 script. Also, we will need to indicate the COCO classes. We are using all the 80 classes but the option is there just in case you use a more restricted set of classes.
Finally, at the bottom there is a place to select the output folder and also a variety of options in terms of metrics. I chose all of them as it doesn't take long to obtain them. The IoU threshold can stay at 50% as it is a very reasonable value.
In the end, the interface should look like this:

![enter image description here](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_13.png?raw=true)

Now you just click on **RUN** and wait a minute until the metrics are obtained. At the end a new window will pop up, and you should copy the contents to a text file, so you don't lose them!

