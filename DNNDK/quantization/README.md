## Quantization
Now it's time for the model quantization. I have created a quantization script with everything set according to the necessary specifications. For more details you already know... visit the gitbook tutorial...

For this part the program needs a **dataset** to perform the quantization of the model. A dataset is a set of **images and annotations** about the objects on each image. No, they are not annotations like the ones you had on your 7th grade maths book but more like **information about the location, type and size of the object**.

For the dataset I recommend you use the **COCO Dataset** as it is pretty complete, gathering more than 100k images with a lot of scenarios, so the model gets a happy load of different data. The quantization tool will adjust the YOLO Neural Network parameters according to the images and annotations from the dataset, so that is why we need it here.

The download of the COCO dataset can be done on the official website on the [Downloads tab](https://cocodataset.org/#download). You want to download the "2017 Train images [118k/18GB]" and also the "Train/Val annotations [241MB]". You can also use the following links.
**COCO Dataset:**
 - [images](http://images.cocodataset.org/zips/train2017.zip)
 - [annotations](http://images.cocodataset.org/annotations/annotations_trainval2017.zip)
 
After the long download (about 20GB), I recommend you store the images on the **yolo_dataset/images** folder and the annotations json file on the **yolo_dataset/labels** folder.
![enter image description here](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_8.png?raw=true)

Next step is to open the ***graph_input_fn.py*** script and change the path according to your username. You want to lead that script to the COCO dataset images.
![enter image description here](https://github.com/andre1araujo/YOLO-on-PYNQ-Z2/blob/main/images/Demo_image_7.png?raw=true)

So, with this, all you have to do now is execute the quantization script:

    cd ~
    cd /home/<user>/Desktop/YOLO-on-PYNQ-Z2/DNNDK/quantization
    ./quant.sh
Don't forget to change the username!
The process might take **12 hours** because the script only tells the program to use the CPU. If you have GPU I would recommend you check the [DNNDK v3.1 User Guide](https://docs.xilinx.com/v/u/1.6-English/ug1327-dnndk-user-guide) as you will need to do the installation based on that. On the script you should enable GPU usage on the part that says gpu, obviously...
After the quantization is finished, there will be a ***"quantize_results"*** folder and there will be two ".pb" files. That extension refers to models in Tensorflow, and we will only need the ***deploy_model.pb*** so you should copy it to the compilation folder to use on the next part.

    cp /quantize_results/deploy_model.pb /home/<user>/Desktop/YOLO-on-PYNQ-Z2/DNNDK/compilation