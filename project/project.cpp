#include <iostream>
#include <cstring>
#include <sstream>
#include <string>
#include "classification.cpp"
#include "process.cpp"
#include "format.cpp" 
#include "stat.cpp"
#include "opencv2/imgproc/imgproc.hpp"
using namespace std;
using namespace cv;

void result_Reshape(cv::Mat &img , int height , int weight , float embed_ratio) {
   int img_height = img.rows;
   int img_weight = img.cols;
   //lstd::cout <<img_height << "" << img_weight << std::endl;
   if (img_height > img_weight) {
       if ( img_height < height ) return;
       float ratio = (float)height/img_height;
       int resized_height = height*embed_ratio;
       int resized_weight = ratio * img_weight*embed_ratio;
       cv::Size resize_size( resized_weight , resized_height);
       cv::resize(img , img , resize_size);
       return ; 
    }
   else {
       if (img_weight < weight) return ;
       float ratio = (float)weight/img_weight;
       int resized_height = ratio* img_height * embed_ratio;
       int resized_weight = weight * embed_ratio;
       cv::Size resize_size(resized_weight, resized_height);
       cv::resize(img, img , resize_size);
       return ;
   } 
} 
int main(){
   string model_file = "deploy_parse.prototxt";
   string trained_file = "fcn.caffemodel";
   Classifier classifier(model_file, trained_file, 0, 0);
   string pose_model_file = "deploy_pose.prototxt";
   string pose_trained_file = "classify.caffemodel";
   pose_Classifier pose_classifier(pose_model_file,pose_trained_file,0,0);
   
   std::string classes[] = {"crunch","lying","stand"};
   //input file size
   int width = 640; 
   int height = 480;

   int min_area = 1000;
   int count = 0;
    //output file size
    int out_width = 224;
    int out_height =224; 

    
    int step = 11;
    int num_count =0;
    int crunch_count = 0;
    int stand_count = 0;
    int lying_count = 0;
    std::string src_folder = "./stand/";
    //int pos = 0;
    int num_length = 1500;
    for(int i =1; i < num_length; i+=step){
    std::cout<< "process " <<i <<" th image" << std::endl;
    std::stringstream ss;
    std::string filename;
    ss<< src_folder;
    ss<<"d_"<< i <<".bin";
    ss >> filename ;
    FILE *img_file = fopen(filename.c_str(),"r");
    if( !img_file) {std::cout<< "no such file" <<std::endl; continue;} 
    fclose(img_file);
    cv::Mat img_depth = Depth_bin2mat(filename.c_str(), width, height);
    cv::Mat output_Mat = classifier.Classify(img_depth , false);
    std::vector < cv::Rect > rectRecord = findConnectComponent(output_Mat , min_area);
    if (rectRecord.size()>1)
     std::cout << rectRecord.size() << std::endl;
    for (int j = 0;j < rectRecord.size();++j){
         if( rectRecord[j].width < 100 && rectRecord[j].height < 100 ) continue;
         num_count++;
         cv::Mat inte_Mat;
         cv::Mat bg_Mat(out_height, out_width, CV_16UC1,cv::Scalar(0));
         inte_Mat = img_depth(rectRecord[j]);
         result_Reshape(inte_Mat,out_height,out_width,0.9);
         int x = (out_width - inte_Mat.cols)/2;
         int y = (out_height - inte_Mat.rows)/2; 
         
         inte_Mat.copyTo(bg_Mat(cv::Rect(x,y,inte_Mat.cols, inte_Mat.rows)));
         std::pair<int,float> pose_alys = pose_classifier.Classify(bg_Mat);
         std::cout << "people in pic: " << i << " block: " << j << " is " << classes[pose_alys.first] << " with prob: " << pose_alys.second <<std::endl;
         std::cout << "square: " << rectRecord[j] << std::endl;
         std::cout<< "area: " << rectRecord[j].width * rectRecord[j].height << std::endl;
         if (pose_alys.first == 0) crunch_count++;
         if (pose_alys.first == 1) lying_count++;
         if (pose_alys.first == 2) stand_count++;
         //cv::imwrite(savefilename, bg_Mat); 
         
         /*img_depth.convertTo(img_depth,CV_16U);
         cv::rectangle(img_depth, rectRecord[j],Scalar(10),1,8,0);
         cv::namedWindow("Display Window", CV_WINDOW_AUTOSIZE);
         cv::imshow("Display Window", img_depth);
         cv::waitKey(0);*/
    }
    std::cout << "Total: "<<num_count<<std::endl;
    std::cout << "crunch: "<<crunch_count<<std::endl;
    std::cout << "lying: "<<lying_count<<std::endl;
    std::cout << "stand: "<<stand_count<<std::endl;
 }
    
}
