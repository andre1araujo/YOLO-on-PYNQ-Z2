/*
-- (c) Copyright 2019 Xilinx, Inc. All rights reserved.
--
-- This file contains confidential and proprietary information
-- of Xilinx, Inc. and is protected under U.S. and
-- international copyright and other intellectual property
-- laws.
--
-- DISCLAIMER
-- This disclaimer is not a license and does not grant any
-- rights to the materials distributed herewith. Except as
-- otherwise provided in a valid license issued to you by
-- Xilinx, and to the maximum extent permitted by applicable
-- law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
-- WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
-- AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
-- BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
-- INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
-- (2) Xilinx shall not be liable (whether in contract or tort,
-- including negligence, or under any other theory of
-- liability) for any loss or damage of any kind or nature
-- related to, arising under or in connection with these
-- materials, including for any direct, or any indirect,
-- special, incidental, or consequential loss or damage
-- (including loss of data, profits, goodwill, or any type of
-- loss or damage suffered as a result of any action brought
-- by a third party) even if such damage or loss was
-- reasonably foreseeable or Xilinx had been advised of the
-- possibility of the same.
--
-- CRITICAL APPLICATIONS
-- Xilinx products are not designed or intended to be fail-
-- safe, or for use in any application requiring fail-safe
-- performance, such as life-support or safety devices or
-- systems, Class III medical devices, nuclear facilities,
-- applications related to the deployment of airbags, or any
-- other applications that could lead to death, personal
-- injury, or severe property or environmental damage
-- (individually and collectively, "Critical
-- Applications"). Customer assumes the sole risk and
-- liability of any use of Xilinx products in Critical
-- Applications, subject only to applicable laws and
-- regulations governing limitations on product liability.
--
-- THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
-- PART OF THIS FILE AT ALL TIMES.
*/

#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <fcntl.h>
#include <fcntl.h>

const char eth_show_ip[] = "10.10.21.160";
int m_sockClient;
sockaddr_in m_servaddr;

// Header files for DNNDK API
#include <dnndk/dnndk.h>

// DPU input & output Node name for DenseBox
#define NODE_INPUT "L0"
#define NODE_CONV "pixel_conv"
#define NODE_OUTPUT "bb_output"

#define IMAGE_SCALE (0.02)
#define CONFIDENCE_THRESHOLD (0.65)
#define IOU_THRESHOLD (0.3)

using namespace std;
using namespace std::chrono;
using namespace cv;

typedef pair<int, Mat> pairImage;

class PairComp {  // An auxiliary class for sort the image pair according to its
                  // index
    public:
    bool operator()(const pairImage &n1, const pairImage &n2) const {
        if (n1.first == n2.first) return n1.first > n2.first;
        return n1.first > n2.first;
    }
};

/**
 * @brief NMS - Discard overlapping boxes using NMS
 *
 * @param box - input box vector
 * @param nms - IOU threshold
 *
 * @ret - output box vector after discarding overlapping boxes
 */
vector<vector<float>> NMS(const vector<vector<float>> &box, float nms) {
    size_t count = box.size();
    vector<pair<size_t, float>> order(count);
    for (size_t i = 0; i < count; ++i) {
        order[i].first = i;
        order[i].second = box[i][4];
    }

    sort(order.begin(), order.end(), [](const pair<int, float> &ls, const pair<int, float> &rs) {
        return ls.second > rs.second;
    });

    vector<int> keep;
    vector<bool> exist_box(count, true);
    for (size_t _i = 0; _i < count; ++_i) {
        size_t i = order[_i].first;
        float x1, y1, x2, y2, w, h, iarea, jarea, inter, ovr;
        if (!exist_box[i]) continue;
        keep.push_back(i);
        for (size_t _j = _i + 1; _j < count; ++_j) {
            size_t j = order[_j].first;
            if (!exist_box[j]) continue;
            x1 = max(box[i][0], box[j][0]);
            y1 = max(box[i][1], box[j][1]);
            x2 = min(box[i][2], box[j][2]);
            y2 = min(box[i][3], box[j][3]);
            w = max(float(0.0), x2 - x1 + 1);
            h = max(float(0.0), y2 - y1 + 1);
            iarea = (box[i][2] - box[i][0] + 1) * (box[i][3] - box[i][1] + 1);
            jarea = (box[j][2] - box[j][0] + 1) * (box[j][3] - box[j][1] + 1);
            inter = w * h;
            ovr = inter / (iarea + jarea - inter);
            if (ovr >= nms) exist_box[j] = false;
        }
    }

    vector<vector<float>> result;
    result.reserve(keep.size());
    for (size_t i = 0; i < keep.size(); ++i) {
        result.push_back(box[keep[i]]);
    }

    return result;
}

/**
 * @brief softmax_2 - 2-class softmax calculation
 *
 * @param input   - vector of input data
 * @param output  - output vecotr
 *
 * @return none
 */
void softmax_2(const vector<float> &input, vector<float> &output) {
    for (size_t n = 0; n < input.size(); n += 2) {
        float sum = 0;
        for (auto i = n; i < n + 2; i++) {
            output[i] = exp(input[i]);
            sum += output[i];
        }
        for (auto i = n; i < n + 2; i++) {
            output[i] /= sum;
        }
    }
}

/**
 * @brief runDenseBox - Run DPU Task for Densebox
 *
 * @param task - pointer to a DPU Task
 * @param img  - input image in OpenCV's Mat format
 *
 * @return none
 */
void runDenseBox(DPUTask *task, Mat &img) {
    DPUTensor *conv_in_tensor = dpuGetInputTensor(task, NODE_INPUT);
    int inHeight = dpuGetTensorHeight(conv_in_tensor);
    int inWidth = dpuGetTensorWidth(conv_in_tensor);

    float scale_w = (float)img.cols / (float)inWidth;
    float scale_h = (float)img.rows / (float)inHeight;

    dpuSetInputImage2(task, NODE_INPUT, img);

    dpuRunTask(task);

    DPUTensor *conv_out_tensor = dpuGetOutputTensor(task, NODE_CONV);
    int tensorSize = dpuGetTensorSize(conv_out_tensor);
    DPUTensor *conv_out_tensor_2 = dpuGetOutputTensor(task, NODE_OUTPUT);
    int tensorSize_2 = dpuGetTensorSize(conv_out_tensor_2);
    int outHeight_2 = dpuGetTensorHeight(conv_out_tensor_2);
    int outWidth_2 = dpuGetTensorWidth(conv_out_tensor_2);
    vector<float> pixel(tensorSize);
    vector<float> conf(tensorSize);
    vector<float> bb(tensorSize_2);

    //output data format convert
    dpuGetOutputTensorInHWCFP32(task, NODE_CONV, pixel.data(), tensorSize);
    dpuGetOutputTensorInHWCFP32(task, NODE_OUTPUT, bb.data(), tensorSize_2);

    //2-classes softmax
    softmax_2(pixel, conf);

    // get original face boxes
    vector<vector<float>> boxes;
    for (int i = 0; i < outHeight_2; i++) {
        for (int j = 0; j < outWidth_2; j++) {
            int position = i * outWidth_2 + j;
            vector<float> box;
            if (conf[position * 2 + 1] > 0.55) {
                box.push_back(bb[position * 4 + 0] + j * 4);
                box.push_back(bb[position * 4 + 1] + i * 4);
                box.push_back(bb[position * 4 + 2] + j * 4);
                box.push_back(bb[position * 4 + 3] + i * 4);
                box.push_back(conf[position * 2 + 1]);
                boxes.push_back(box);
            }
        }
    }

    // Discard overlapping boxes using NMS
    vector<vector<float>> res = NMS(boxes, 0.35);

    // Draw detected face boxes to image
    for (size_t i = 0; i < res.size(); ++i) {
        float xmin = std::max(res[i][0] * scale_w, 0.0f);
        float ymin = std::max(res[i][1] * scale_h, 0.0f);
        float xmax = std::min(res[i][2] * scale_w, (float)img.cols);
        float ymax = std::min(res[i][3] * scale_h, (float)img.rows);

        rectangle(img, Point(xmin, ymin), Point(xmax, ymax), Scalar(0, 255, 0), 1, 1, 0);
    }
}

/*
 * @brief faceDetection - Entry of face detection using Densebox
 *
 * @param kernel - point to DPU Kernel
 *
 * @return none
 */
void faceDetection(DPUKernel *kernel) {
    mutex mtxQueueInput;                                               // mutex of input queue
    mutex mtxQueueShow;                                                // mutex of display queue
    queue<pairImage> queueInput;                                       // input queue
    priority_queue<pairImage, vector<pairImage>, PairComp> queueShow;  // display queue

    VideoCapture camera(0);
    if (!camera.isOpened()) {
        cerr << "Open camera error!" << endl;
        exit(-1);
    }

    camera.set(CV_CAP_PROP_FRAME_WIDTH, 640);
    camera.set(CV_CAP_PROP_FRAME_HEIGHT, 360);
    camera.set(CV_CAP_PROP_FPS, 20);

    // We create three different threads to do face detection:
    // 1. Reader thread  : Read images from camera and put it to the input queue;
    //
    // 2. Worker threads : Each worker thread repeats the following 3 steps util
    // no images:
    // (1) get an image from input queue;
    // (2) process it using DenseBox model;
    // (3) put the processed image to the display queue.
    //
    // 3. Display thread : Get output image from queueShow and display it.

    // 1. Reader thread
    atomic<bool> bReading(true);
    thread reader([&]() {
        // image index of input video
        int idxInputImage = 0;
        while (true) {
            Mat img;
            camera >> img;
            if (img.empty()) {
                cerr << "Fail to read image from camera!" << endl;
                camera.release();
                break;
            }
            mtxQueueInput.lock();
            queueInput.push(make_pair(idxInputImage++, img));
            if (queueInput.size() >= 100) {
                mtxQueueInput.unlock();
                cout << "[Warning]input queue size is " << queueInput.size() << endl;
                // Sleep for a moment
                sleep(2);
            } else {
                mtxQueueInput.unlock();
            }
        }
        bReading.store(false);
    });

    // 2. Worker thread
    constexpr int workerNum = 2;
    thread workers[workerNum];
    atomic<int> workerAlive(workerNum);
    for (auto i = 0; i < workerNum; i++) {
        workers[i] = thread([&]() {
            // Create DPU Tasks from DPU Kernel
            DPUTask *task = dpuCreateTask(kernel, 0);

            while (true) {
                pair<int, Mat> pairIndexImage;
                mtxQueueInput.lock();
                if (queueInput.empty()) {
                    mtxQueueInput.unlock();
                    if (bReading.load())
                        continue;
                    else
                        break;
                } else {
                    // Get an image from input queue
                    pairIndexImage = queueInput.front();
                    queueInput.pop();
                }
                mtxQueueInput.unlock();
                // Process the image using DenseBox model
                runDenseBox(task, pairIndexImage.second);
                mtxQueueShow.lock();
                // Put the processed iamge to show queue
                queueShow.push(pairIndexImage);
                mtxQueueShow.unlock();
            }

            // Destroy DPU Tasks & free resources
            dpuDestroyTask(task);

            workerAlive--;
        });
    }

    // 3. Display thread;
    atomic<int> idxShowImage(0);  // next frame index to be display
    thread show([&]() {
        while (true) {
            mtxQueueShow.lock();
            if (queueShow.empty()) {  // no image in display queue
                mtxQueueShow.unlock();
                if (workerAlive.load() == 0) {
                    cout << "Face Detection End." << endl;
                    break;
                } else {
                    usleep(10000);  // Sleep for a moment
                }
            } else if (idxShowImage.load() == queueShow.top().first) {
                //cv::imshow("Face Detection @Xilinx DPU", queueShow.top().second);  // Display image
                
                std::vector<uchar> data_encode;
                std::vector<int> quality;
                quality.push_back(CV_IMWRITE_JPEG_QUALITY);
                quality.push_back(50);
                imencode(".jpg", queueShow.top().second, data_encode,quality);
                char encodeImg[65535];
                
                int nSize = data_encode.size();
                for (int i = 0; i < nSize; i++)
                {
                    encodeImg[i] = data_encode[i];
                }
                sendto(m_sockClient, encodeImg, nSize, 0, (const sockaddr*)& m_servaddr, sizeof(m_servaddr));
                memset(&encodeImg, 0, sizeof(encodeImg));
                
                idxShowImage++;
                queueShow.pop();
                mtxQueueShow.unlock();
                if (waitKey(1) == 'q') {
                    bReading = false;
                    exit(0);
                }
            } else {
                mtxQueueShow.unlock();
            }
        }
    });

    // Release thread resources.
    if (reader.joinable()) reader.join();
    if (show.joinable()) show.join();
    for (auto &w : workers) {
        if (w.joinable()) w.join();
    }
}

/*
 * @brief main - Entry of DenseBox neural network sample.
 *
 * @note This sample presents how to implement an face detection appliacation
 *       on DPU using DenseBox model.
 *
 */
int main(void) {
    if ((m_sockClient = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }
    
    memset(&m_servaddr, 0, sizeof(m_servaddr));
    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_addr.s_addr = inet_addr(eth_show_ip);
    m_servaddr.sin_port = htons(0x1234);
    bind(m_sockClient, (sockaddr*)&m_servaddr, sizeof(m_servaddr));

    // Attach to DPU driver and prepare for running
    dpuOpen();

    // Load DPU Kernel for DenseBox neural network
    DPUKernel *kernel = dpuLoadKernel("densebox");

    // Doing face detection.
    faceDetection(kernel);

    // Destroy DPU Kernel & free resources
    dpuDestroyKernel(kernel);

    // Dettach from DPU driver & release resources
    dpuClose();

    return 0;
}
