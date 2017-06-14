#include <iostream>
#include <map> 
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"

using namespace std;
using namespace cv;

void Cal_pixel_freq(const cv::Mat& img , std::map<int , int > &freq_map , int &pixel_count , int &aver_pixel_value) {
    for ( int i = 0 ; i < img.rows ; ++i) {
         
        for ( int j = 0; j < img.cols; ++j) {
            
            int num = img.at<int>(cv::Point(j , i));
            if (num == 0.0) {
               continue;
            }
            pixel_count++;
            aver_pixel_value += num; 
            if (freq_map.find(num) == freq_map.end()) {
               freq_map[num] = 1;
            }
            else {
               freq_map[num]++;
            }
        }
    }
   aver_pixel_value = aver_pixel_value / pixel_count; 
} 

void Depth_Process(cv::Mat& img , float left_ratio , float right_ratio) {
    std::map<int , int> freq_pixel;
    int aver_pixel_value = 0;
    int pixel_stat  = 0; 
    Cal_pixel_freq( img , freq_pixel , pixel_stat , aver_pixel_value);
    int min_pixel = 0;
    int max_pixel = 0;
    float uniform_ratio = 0;
    int min_num = pixel_stat * left_ratio;
    int max_num = pixel_stat - pixel_stat * right_ratio;
    int counts = 0;
    for ( std::map<int , int>::iterator it = freq_pixel.begin(); it != freq_pixel.end(); ++it) {
      if (counts < min_num) {
          counts = counts + it->second;
          if (counts >= min_num) {
            min_pixel = it->first;
          } 
      } 
      else {
          if(counts < max_num) {
            counts = counts + it->second;
            if (counts >= max_num) {
               max_pixel = it->first;
               break;
            }
          } 
      }
    }
    
    uniform_ratio = (max_pixel - min_pixel) /255;
    for ( int i = 0; i < img.rows; ++i ) {
        for ( int j = 0; j < img.cols; ++j) {
            int num = img.at<int>(cv::Point(j , i));
            if (num < min_pixel || num > max_pixel) {
               num = 0;
            }
            else {
               num = num - min_pixel;
               num = num/uniform_ratio;
            }
           img.at<int>(cv::Point(j , i)) = num;
        } 
    }
    
} 
