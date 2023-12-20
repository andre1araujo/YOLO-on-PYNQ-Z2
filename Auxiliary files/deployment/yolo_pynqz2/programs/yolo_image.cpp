#include <algorithm>
#include <vector>
#include <atomic>
#include <queue>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <zconf.h>
#include <thread>
#include <sys/stat.h>
#include <dirent.h>
#include <iomanip>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <math.h>
#include <arm_neon.h>
#include <opencv2/opencv.hpp>
#include <dnndk/n2cube.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
using namespace std;
using namespace cv;
using namespace std::chrono;
//confidence and threshold
#define CONF 0.5
#define NMS_THRE  0.1
//dpu kernel info
#define YOLOKERNEL "yolo"
#define INPUTNODE "conv2d_1_convolution"
vector<string>outputs_node= {"conv2d_59_convolution", "conv2d_67_convolution", "conv2d_75_convolution"};
//yolo parameters
const int classification = 80;
const int anchor = 3;
vector<float> biases { 116,90,  156,198,  373,326, 30,61,  62,45,  59,119, 10,13,  16,30,  33,23};

vector<string> class_names = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
    "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench",
    "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra",
    "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove",
    "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork",
    "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli",
    "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant",
    "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard",
    "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book",
    "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
};

// ANSI escape codes for text colors
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

class image {
	public:
    int w;
    int h;
    int c;
    float *data;
    image(int ww, int hh, int cc, float fill):w(ww),h(hh),c(cc){
        data = new float[h*w*c];
        for(int i = 0; i < h*w*c; ++i) data[i] = fill;
    };
   void free(){delete[] data;};
};

void detect(vector<vector<float>> &boxes, vector<float> result, int channel, int height, int weight, int num, int sh, int sw);
image load_image_cv(const cv::Mat& img);
image letterbox_image(image im, int w, int h);

void get_output(int8_t* dpuOut, int sizeOut, float scale, int oc, int oh, int ow, vector<float>& result) {
    vector<int8_t> nums(sizeOut);
    memcpy(nums.data(), dpuOut, sizeOut);
    for(int a = 0; a < oc; ++a){
        for(int b = 0; b < oh; ++b){
            for(int c = 0; c < ow; ++c) {
                int offset = b * oc * ow + c * oc + a;
                result[a * oh * ow + b * ow + c] = nums[offset] * scale;
            }
        }
    }
}

void set_input_image(DPUTask* task, const Mat& img, const char* nodename) {
    Mat img_copy;
    int height = dpuGetInputTensorHeight(task, nodename);
    int width = dpuGetInputTensorWidth(task, nodename);
    int size = dpuGetInputTensorSize(task, nodename);
    int8_t* data = dpuGetInputTensorAddress(task, nodename);
    //cout<<"set_input_image height:"<<height<<" width:"<<width<<" size"<<size<<endl;

    image img_new = load_image_cv(img);
    image img_yolo = letterbox_image(img_new, width, height);
    vector<float> bb(size);
    for(int b = 0; b < height; ++b)
        for(int c = 0; c < width; ++c)
            for(int a = 0; a < 3; ++a)
                bb[b*width*3 + c*3 + a] = img_yolo.data[a*height*width + b*width + c];

    float scale = dpuGetInputTensorScale(task, nodename);
    //cout<<"scale: "<<scale<<endl;
    for(int i = 0; i < size; ++i) {
        data[i] = int(bb.data()[i]*scale);
        if(data[i] < 0) data[i] = 127;
    }
    img_new.free();
    img_yolo.free();
}


inline float sigmoid(float p) {
    return 1.0 / (1 + exp(-p * 1.0));
}

inline float overlap(float x1, float w1, float x2, float w2) {
    float left = max(x1 - w1 / 2.0, x2 - w2 / 2.0);
    float right = min(x1 + w1 / 2.0, x2 + w2 / 2.0);
    return right - left;
}

inline float cal_iou(vector<float> box, vector<float>truth) {
    float w = overlap(box[0], box[2], truth[0], truth[2]);
    float h = overlap(box[1], box[3], truth[1], truth[3]);
    if(w < 0 || h < 0) return 0;
    float inter_area = w * h;
    float union_area = box[2] * box[3] + truth[2] * truth[3] - inter_area;
    return inter_area * 1.0 / union_area;
}

vector<vector<float>> apply_nms(vector<vector<float>>& boxes,int classes, const float thres) {
    vector<pair<int, float>> order(boxes.size());
    vector<vector<float>> result;
    for(int k = 0; k < classes; k++) {
        for (size_t i = 0; i < boxes.size(); ++i) {
            order[i].first = i;
            boxes[i][4] = k;
            order[i].second = boxes[i][6 + k];
        }
        sort(order.begin(), order.end(),
             [](const pair<int, float> &ls, const pair<int, float> &rs) { return ls.second > rs.second; });
        vector<bool> exist_box(boxes.size(), true);
        for (size_t _i = 0; _i < boxes.size(); ++_i) {
            size_t i = order[_i].first;
            if (!exist_box[i]) continue;
            if (boxes[i][6 + k] < CONF) {
                exist_box[i] = false;
                continue;
            }
            //add a box as result
            result.push_back(boxes[i]);
            //cout << "i = " << i<<" _i : "<< _i << endl;
            for (size_t _j = _i + 1; _j < boxes.size(); ++_j) {
                size_t j = order[_j].first;
                if (!exist_box[j]) continue;
                float ovr = cal_iou(boxes[j], boxes[i]);
                if (ovr >= thres) exist_box[j] = false;
            }
        }
    }
    return result;
}


static float get_pixel(image m, int x, int y, int c)
{
    assert(x < m.w && y < m.h && c < m.c);
    return m.data[c*m.h*m.w + y*m.w + x];
}

static void set_pixel(image m, int x, int y, int c, float val)
{
    if (x < 0 || y < 0 || c < 0 || x >= m.w || y >= m.h || c >= m.c) return;
    assert(x < m.w && y < m.h && c < m.c);
    m.data[c*m.h*m.w + y*m.w + x] = val;
}

static void add_pixel(image m, int x, int y, int c, float val)
{
    assert(x < m.w && y < m.h && c < m.c);
    m.data[c*m.h*m.w + y*m.w + x] += val;
}

image resize_image(image im, int w, int h)
{
    image resized(w, h, im.c,0);   
    image part(w, im.h, im.c,0);
    int r, c, k;
    float w_scale = (float)(im.w - 1) / (w - 1);
    float h_scale = (float)(im.h - 1) / (h - 1);
    for(k = 0; k < im.c; ++k){
        for(r = 0; r < im.h; ++r){
            for(c = 0; c < w; ++c){
                float val = 0;
                if(c == w-1 || im.w == 1){
                    val = get_pixel(im, im.w-1, r, k);
                } else {
                    float sx = c*w_scale;
                    int ix = (int) sx;
                    float dx = sx - ix;
                    val = (1 - dx) * get_pixel(im, ix, r, k) + dx * get_pixel(im, ix+1, r, k);
                }
                set_pixel(part, c, r, k, val);
            }
        }
    }
    for(k = 0; k < im.c; ++k){
        for(r = 0; r < h; ++r){
            float sy = r*h_scale;
            int iy = (int) sy;
            float dy = sy - iy;
            for(c = 0; c < w; ++c){
                float val = (1-dy) * get_pixel(part, c, iy, k);
                set_pixel(resized, c, r, k, val);
            }
            if(r == h-1 || im.h == 1) continue;
            for(c = 0; c < w; ++c){
                float val = dy * get_pixel(part, c, iy+1, k);
                add_pixel(resized, c, r, k, val);
            }
        }
    }
    part.free();
    return resized;
}

image load_image_cv(const cv::Mat& img) {
    int h = img.rows;
    int w = img.cols;
    int c = img.channels();
    image im(w, h, c,0);

    unsigned char *data = img.data;

    for(int i = 0; i < h; ++i){
        for(int k= 0; k < c; ++k){
            for(int j = 0; j < w; ++j){
                im.data[k*w*h + i*w + j] = data[i*w*c + j*c + k]/255.;
            }
        }
    }

    //bgr to rgb
    for(int i = 0; i < im.w*im.h; ++i){
        float swap = im.data[i];
        im.data[i] = im.data[i+im.w*im.h*2];
        im.data[i+im.w*im.h*2] = swap;
    }
    return im;
}

image letterbox_image(image im, int w, int h)
{
    int new_w = im.w;
    int new_h = im.h;
    if (((float)w/im.w) < ((float)h/im.h)) {
        new_w = w;
        new_h = (im.h * w)/im.w;
    } else {
        new_h = h;
        new_w = (im.w * h)/im.h;
    }
    image resized = resize_image(im, new_w, new_h);
    image boxed(w, h, im.c, .5);
    
    int dx = (w-new_w)/2;
    int dy = (h-new_h)/2;
    for(int k = 0; k < resized.c; ++k){
        for(int y = 0; y < new_h; ++y){
            for(int x = 0; x < new_w; ++x){
                float val = get_pixel(resized, x,y,k);
                set_pixel(boxed, dx+x, dy+y, k, val);
            }
        }
    }
    resized.free();
    return boxed;
}


//------------------------------------------------------------------

void correct_region_boxes(vector<vector<float>>& boxes, int n, int w, int h, int netw, int neth, int relative = 0) {
    int new_w=0;
    int new_h=0;
    if (((float)netw/w) < ((float)neth/h)) {
        new_w = netw;
        new_h = (h * netw)/w;
    } else {
        new_h = neth;
        new_w = (w * neth)/h;
    }
    for (int i = 0; i < n; ++i){
        boxes[i][0] =  (boxes[i][0] - (netw - new_w)/2./netw) / ((float)new_w/(float)netw);
        boxes[i][1] =  (boxes[i][1] - (neth - new_h)/2./neth) / ((float)new_h/(float)neth);
        boxes[i][2] *= (float)netw/new_w;
        boxes[i][3] *= (float)neth/new_h;
    }
}

void deal(DPUTask* task, Mat& img, int sw, int sh)
{
    vector<vector<float>> boxes;
    for (int i = 0; i < outputs_node.size(); i++) {
        string output_node = outputs_node[i];
        int channel = dpuGetOutputTensorChannel(task, output_node.c_str());
        int width = dpuGetOutputTensorWidth(task, output_node.c_str());
        int height = dpuGetOutputTensorHeight(task, output_node.c_str());

        int sizeOut = dpuGetOutputTensorSize(task, output_node.c_str());
        int8_t* dpuOut = dpuGetOutputTensorAddress(task, output_node.c_str());
        float scale = dpuGetOutputTensorScale(task, output_node.c_str());
        vector<float> result(sizeOut);
        boxes.reserve(sizeOut);
        get_output(dpuOut, sizeOut, scale, channel, height, width, result);
        detect(boxes, result, channel, height, width, i, sh, sw);
    }
    correct_region_boxes(boxes, boxes.size(), img.cols, img.rows, sw, sh);
    vector<vector<float>> res = apply_nms(boxes, classification, NMS_THRE);

    float h = img.rows;
    float w = img.cols;
    for (size_t i = 0; i < res.size(); ++i) {
        float xmin = (res[i][0] - res[i][2] / 2.0) * w;
        float ymin = (res[i][1] - res[i][3] / 2.0) * h;
        float xmax = (res[i][0] + res[i][2] / 2.0) * w;
        float ymax = (res[i][1] + res[i][3] / 2.0) * h;
        int cls = static_cast<int>(res[i][4]);
        string class_name = class_names[cls];
        float conf = res[i][5];

        // Draw bounding box
        rectangle(img, Point(xmin, ymin), Point(xmax, ymax), Scalar(255, 0, 0), 2);

        // Display class name and confidence on top of the bounding box
        string text = class_name + ": " + to_string(conf);
        int baseline = 0;
        Size textSize = getTextSize(text, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        
        // Create a blue background rectangle behind the text
        rectangle(img, Point(xmin, ymin - textSize.height - 5), Point(xmin + textSize.width, ymin), Scalar(255, 0, 0), FILLED);
        
        // Put the text on top of the blue background
        putText(img, text, Point(xmin, ymin - 5), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 255), 1, LINE_AA);

        std::cout << "-----------------------------" << std::endl;
        std::cout << "Class name: " <<  class_name << "     [ID]:" << cls << std::endl;
    }

    //cout << "Deal: DONE!" << endl;
}

void detect(vector<vector<float>> &boxes, vector<float> result, int channel, int height, int width, int num, int sh, int sw) 
{
    {
	int conf_box = 5 + classification;
        float swap[height * width][anchor][conf_box];
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                for (int c = 0; c < channel; ++c) {
                    int temp = c * height * width + h * width + w;
                    swap[h * width + w][c / conf_box][c % conf_box] = result[temp];
                }
            }
        }

        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                for (int c = 0; c < anchor; ++c) {
                    float obj_score = sigmoid(swap[h * width + w][c][4]);
                    if (obj_score < CONF)
                        continue;
                    vector<float> box;
                    box.push_back((w + sigmoid(swap[h * width + w][c][0])) / width);
                    box.push_back((h + sigmoid(swap[h * width + w][c][1])) / height);
                    box.push_back(exp(swap[h * width + w][c][2]) * biases[2 * c + anchor * 2 * num] / float(sw));
                    box.push_back(exp(swap[h * width + w][c][3]) * biases[2 * c + anchor * 2 * num + 1] / float(sh));
                    box.push_back(-1);                   // class
                    box.push_back(obj_score);                   // this class's conf
                    for (int p = 0; p < classification; p++) {
                        box.push_back(obj_score * sigmoid(swap[h * width + w][c][5 + p]));
                    }
                    boxes.push_back(box);
                }
            }
        }
    }

}
int main(const int argc, const char** argv) {
        dpuOpen();
        DPUKernel *kernel = dpuLoadKernel(YOLOKERNEL);
        DPUTask *task = dpuCreateTask(kernel, 0);
        int sh = dpuGetInputTensorHeight(task, INPUTNODE);
        int sw = dpuGetInputTensorWidth(task, INPUTNODE);
	if(argc < 2)
  {
 	    cout<<"You should use like: ./yolo dog.jpg";
 	    return -1;
 	}
    	  //string name(argv[1]);
	      //Mat img = imread(name);
	      //set_input_image(task,  img,INPUTNODE);
        //dpuRunTask(task);
	      //deal(task, img, sw, sh);
        //cv::imshow("yolo-v3", img);
        //cv::waitKey(0);
		
   
        //Start capture time count
        std::chrono::steady_clock::time_point start_capture_time = std::chrono::steady_clock::now();
        string name(argv[1]);
        Mat img = imread(name);
        //Stop capture time count and display
        std::chrono::steady_clock::time_point end_capture_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> capture_duration = end_capture_time - start_capture_time;
   
   
        // Define the desired window size (416x416)
        int windowWidth = 640;
        int windowHeight = 480;
         
   
        //Start Inference time count
        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
        //Start setImage time count
        std::chrono::steady_clock::time_point start_setImage_time = std::chrono::steady_clock::now();
        //---------------------------------------------------------------------------------------------------------
        set_input_image(task,  img,INPUTNODE);
        //---------------------------------------------------------------------------------------------------------
        //Stop SetImage time count
        std::chrono::steady_clock::time_point end_setImage_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> setImage_duration = end_setImage_time - start_setImage_time;
        //Start DPUTASK time count
        std::chrono::steady_clock::time_point start_DPUTASK_time = std::chrono::steady_clock::now();
        //---------------------------------------------------------------------------------------------------------
        dpuRunTask(task);
        //---------------------------------------------------------------------------------------------------------
        //Stop DPUTASK time count
        std::chrono::steady_clock::time_point end_DPUTASK_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> DPUTASK_duration = end_DPUTASK_time - start_DPUTASK_time;
        //Start deal time count
        std::chrono::steady_clock::time_point start_deal_time = std::chrono::steady_clock::now();
        //---------------------------------------------------------------------------------------------------------
        deal(task, img, sw, sh);
        //---------------------------------------------------------------------------------------------------------
        //Stop deal time count
        std::chrono::steady_clock::time_point end_deal_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> deal_duration = end_deal_time - start_deal_time;
        //Stop Inference time count and display
        std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> inference_duration = end_time - start_time;
        
        // Resize the image to fit the window size without maintaining aspect ratio
        //cv::resize(img, img, cv::Size(windowWidth, windowHeight));


        //  Create a window and display the resized image
        cv::namedWindow("yolo-v3", cv::WINDOW_NORMAL);  // WINDOW_NORMAL allows resizing the window
        cv::resizeWindow("yolo-v3", windowWidth, windowHeight);
        cv::imshow("yolo-v3", img);

        //Blank line
        std::cout << std::endl;
        //Display Inference time
        std::cout << "Total time:              " << ANSI_COLOR_YELLOW << inference_duration.count() << ANSI_COLOR_RESET << " seconds" << std::endl;
        //Display set_image time
        std::cout << "___set_input_image time: " << setImage_duration.count() << " seconds" << std::endl;
        //Display DPU TASK time
        std::cout << "___DPU task time:        " << DPUTASK_duration.count() << " seconds" << std::endl;
        //Display deal time
        std::cout << "___deal time:            " << deal_duration.count() << " seconds" << std::endl;
        
        cv::waitKey(0);
        cv::destroyAllWindows();
   
        dpuDestroyTask(task);
        dpuDestroyKernel(kernel);
        dpuClose();
        return 0;
}