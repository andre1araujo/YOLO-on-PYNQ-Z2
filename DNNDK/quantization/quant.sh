### 
# @Author: Sauron Wu
 # @GitHub: wutianze
 # @Email: 1369130123qq@gmail.com
 # @Date: 2019-09-19 12:44:02
 # @LastEditors  : Please set LastEditors
 # @LastEditTime : 2019-12-20 10:22:51
 # @Description: 
 ###
#!/bin/bash


# activate DECENT_Q Python3.6 virtual environment
#conda activate decent_q1

# generate calibraion images and list file
#python generate_images.py

# remove existing files
rm -rf ./quantize_results


# run quantization
echo "#####################################"
echo "QUANTIZE"
echo "#####################################"
decent_q quantize \
  --input_frozen_graph ./yolo.pb \
  --input_nodes input_1 \
  --input_shapes ?,416,416,3 \
  --output_nodes "conv2d_59/BiasAdd,conv2d_67/BiasAdd,conv2d_75/BiasAdd" \
  --method 1 \
  --input_fn graph_input_fn.calib_input \
  --gpu 0 \
  --calib_iter 100

echo "#####################################"
echo "QUANTIZATION COMPLETED"
echo "#####################################"

