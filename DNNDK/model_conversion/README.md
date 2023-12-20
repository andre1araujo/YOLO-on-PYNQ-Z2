## Model conversion
Firstly we need to convert the YOLO model from Darknet to Tensorflow because DNNDK v3.1 doesn't support Darknet. On your Ubuntu Operating System, you should open the model_conversion folder from the project root:

    cd ~
    cd /home/<user>/Desktop/YOLO-on-PYNQ-Z2/DNNDK/model_conversion

Then I need you to go to the [YOLO website](https://pjreddie.com/darknet/yolo/) and **download the YOLOv3 weights**. You can refer to the YOLOv3-416 weights.
When the download is complete, you should add the weights file to the ***"model_conversion"*** folder. 


![enter image description here](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_6.png?raw=true)


### Convert from Darknet to Keras
The conversion process is not direct, so we need to first convert the Darknet model to Keras, which is a Framework related to Tensorflow. For this first conversion we will be using the ***convert.py*** script from the github repository [keras-yolo3](https://github.com/qqwweee/keras-yolo3). The conversion is done as described on the code block below. Don't forget to change the path according to your username.
**Note:** Github doesn't upload empty folders so you will need to create a folder called ***model_data*** to store the model files.

    python convert.py yolov3.cfg yolov3.weights /home/<user>/Desktop/YOLO-on-PYNQ-Z2/DNNDK/model_conversion/model_data/yolo.h5

We need to point the weights and configuration file of the YOLOv3 model and then the script will convert to Keras and leave the output on the ***model_data*** folder. This file will have the **".h5" extension**. Note that the script needs the ".cfg" and the ".weights" files to work. All of them are on the YOLO website for you to download, but I already included the configuration file on the repository.


### Convert from Keras to Tensorflow
With the YOLO model in Keras, is now time to convert it to Tensorflow. It is very simple as well. We will use Wu-Tianze's script called ***keras_to_tensorflow.py*** and all we need to do is point the input Keras file and the output folder. For this I decided to leave the output Tensorflow model file on the ***model_data*** folder. The following code does exactly that. Don't forget to change the path according to your username.

    python keras_to_tensorflow.py --input_model=/home/<user>/Desktop/YOLO-on-PYNQ-Z2/DNNDK/model_conversion/model_data/yolo.h5 --output_model=/home/<user>/Desktop/YOLO-on-PYNQ-Z2/DNNDK/model_conversion/model_data/yolo.pb

The result will be a file with the **".pb" extension**. This is our YOLOv3 model now in Tensorflow format. The next step is to quantize this model so let's **copy this file to the quantization folder** using this command or doing it manually.

    cp /model_data/yolo.pb /home/<user>/Desktop/YOLO-on-PYNQ-Z2/DNNDK/quantization
