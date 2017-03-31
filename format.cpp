#include <iostream> 
#include <fstream>
#include <string>
#include <stdio.h>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"


using namespace std;
using namespace cv;

int R = 0;
int G = 1;
int B = 2;

struct RGB{
int r , g , b;
};

RGB yuvTorgb(char Y, char U, char V){
    RGB rgb = {};
    rgb.r = (int)((Y & 0xff) + 1.4075 * ((V & 0xff) - 128));
    rgb.g = (int)((Y & 0xff) - 0.3455 * ((U & 0xff) - 128) - 0.7169 * ((V & 0xff)-128));
    rgb.b = (int)((Y & 0xff) + 1.779 * ((U & 0xff) - 128));
    rgb.r = (rgb.r < 0 ? 0: rgb.r > 255 ? 255 : rgb.r); 
    rgb.g = (rgb.g < 0 ? 0: rgb.g > 255 ? 255 : rgb.g); 
    rgb.b = (rgb.b < 0 ? 0: rgb.b > 255 ? 255 : rgb.b); 
    return rgb;
}

void UYVYToRGB(char * src, vector<vector<uchar> > &rgb_img, int width, int height ){
    int numofPixel = width * height;
    int lineWidth = 2*width;
    vector<uchar> R;
    vector<uchar> G;
    vector<uchar> B;
    for(int i = 0; i < height ; i++){
        int startU = i * lineWidth; 
        for (int j = 0; j < lineWidth; j+=4){
            int U = j + startU;
            int Y1 = U + 1;
            int Y2 = U + 3;
            int V = U + 2;
            RGB tmp = yuvTorgb(src[Y1], src[U], src[V]);
            R.push_back(tmp.r);
            G.push_back(tmp.g);
            B.push_back(tmp.b);
            tmp = yuvTorgb(src[Y2] , src[U], src[V]);
            R.push_back(tmp.r);
            G.push_back(tmp.g);
            B.push_back(tmp.b);
        }
    }
    rgb_img.push_back(R);
    rgb_img.push_back(G);
    rgb_img.push_back(B);
}

cv::Mat Color_bin2mat(const char* binfile , int width , int height , int img_channel , int bytes) {
    cv::Mat img;
    FILE *fp = NULL;
    char *imagedata = NULL;
    int framesize = width * height * img_channel * bytes;
    fp = fopen(binfile , "rb");
    imagedata = (char*) malloc (sizeof(char)* framesize);
    fread(imagedata , sizeof(char) , framesize , fp);
    img.create(height , width , CV_8UC3);
    memcpy(img.data , imagedata , framesize);
    free(imagedata);
    fclose(fp);
    return img;
}

cv::Mat Depth_bin2mat(const char* binfile , int width , int height ){
    cv::Mat mat;
    FILE *fp = NULL;
    char *imagedata = NULL;
    int framesize = width * height * 2;
    fp = fopen(binfile , "rb");
    imagedata = (char*)malloc (sizeof(char)*framesize);
    fread(imagedata , sizeof(char) , framesize , fp);
    mat.create(height, width , CV_16UC1);
    memcpy(mat.data , imagedata , framesize);
    free(imagedata);
    fclose(fp);
    return mat; 
}
cv::Mat yuv_bin_rgb(const char* binfile , int width , int height ){
    FILE *fp = NULL;
    char *data = NULL;
    int framsize = width * height * 2;
    data = (char*) malloc (sizeof(char)*framsize);
    fp = fopen(binfile , "rb");
    fread(data , sizeof(char) , framsize , fp); 
    vector<vector<uchar> > img_rgb;
    UYVYToRGB(data , img_rgb , width , height); 
    free(data);
    fclose(fp);
    cv::Mat R_mat = cv::Mat(img_rgb[0]);
    R_mat = R_mat.reshape(1 , height );
    cv::Mat G_mat = cv::Mat(img_rgb[1]);
    G_mat = G_mat.reshape(1 , height );
    cv::Mat B_mat = cv::Mat(img_rgb[2]);
    B_mat = B_mat.reshape(1 , height );
    cv::Mat mat;
    std::vector<cv::Mat> channels;
    channels.push_back(B_mat);
    channels.push_back(G_mat);
    channels.push_back(R_mat);
    cv::merge(channels , mat);
    mat.convertTo(mat , CV_8UC3);
    return mat;
}
