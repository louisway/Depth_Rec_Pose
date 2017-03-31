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


int main() {

    //read image this define the range of the file. if file name is 10c.bin to 50c.bin then start = 10 and end = 50 
    int start = 200;
    int end = 440;
  
 // flag to save the image
    bool picture_save = true; 

// flag to show the image    
    bool picture_show = true; 
  
    //load network
    string model_file = "TVG_CRFRNN_new_deploy.prototxt";
    string trained_file = "TVG_CRFRNN_COCO_VOC.caffemodel";
    bool uniform_flag = true; // if you want to unifrom the output of classifier with 0 or 1
    Classifier classifier(model_file, trained_file);

    //set the input file size
    int width = 1280;
    int height = 720;
    
    //set Gaussian size for Gaussian blur
    int Gau_size = 21;
    
    // set the minmum area limit for the findconnectedComponent
    int min_area = 200;
   
    //set the curtail ratio for the Depth process
    float  left_cut_ratio = 0.05; 
    float right_cut_ratio = 0.001;

 
   // loop to deal with every pair in the data folder 
    for (int i = start ; i <= end  ; ++i) {
    // assign file name for color_img 
        std::stringstream ss;
        std::string filename;
        ss<<"./data/"<<i<<"c.bin";
        ss >> filename ;
        char *filenames = new char[30];
        std::strcpy(filenames , filename.c_str());


    //transform bin to rgb opencv mat; 
        cv::Mat img = yuv_bin_rgb(filenames , width , height);

    // assign file name for depth_img
        std::stringstream dss;
        dss << "./data/" << i << "d.bin";
        std::string depth_filename;
        dss >> depth_filename;
        char *depth_filenames = new char[30];
        std::strcpy(depth_filenames , depth_filename.c_str()); 
   
    //transform depth bin to opencv mat 
        cv::Mat img_depth = Depth_bin2mat(depth_filenames, width, height);

   
    //Gaussian smooth comment it if not useful 
        GaussianBlur(img_depth , img_depth , cv::Size( Gau_size , Gau_size) , 0, 0); 
    
    //input the rgb mat into the net to classify
        cv::Mat output_Mat = classifier.Classify(img , uniform_flag);
   

    //find the ConnectComponent area with a threshold to limit the size(height * width) of the area 
        std::vector < cv::Rect > rectRecord = findConnectComponent(output_Mat , min_area);
    
   //once got the ConnectedComponent in rectRecord, use it to crop depth in depth img
        for ( int j = 0 ; j < rectRecord.size(); ++j) {

    //remove background
              cv::Mat inte_Mat;
              output_Mat(rectRecord[j]).convertTo(inte_Mat , CV_32S);
              cv::Mat result; 
              img_depth(rectRecord[j]).convertTo(result , CV_32S);
              result = result.mul(inte_Mat);
              Depth_Process(result , left_cut_ratio , right_cut_ratio); 
    
    //embeding
              cv::Mat bg_Mat( height , width , CV_32SC1 , cv::Scalar(0));
              int x = (width - result.cols)/2;
              int y = (height - result.rows)/2; 
              result.copyTo(bg_Mat(cv::Rect(x , y , result.cols, result.rows)));
              result.convertTo(result , CV_8U);
              bg_Mat.convertTo(bg_Mat , CV_8U);

   //save generate image'
            if (picture_save) {
               std::stringstream savefilestream;
               std::string savefilename;
               savefilestream << "./result/" << i << "_" << j << ".png";
               savefilestream >> savefilename;
               cv::imwrite(savefilename , bg_Mat); 
             } 
    //show the img
            if ( picture_show ){
              cv::namedWindow("Display Window" , CV_WINDOW_AUTOSIZE);
              cv::imshow("Display Window" , bg_Mat );
              cv::waitKey(0);
              cv::imshow("Display Window" , img_depth );
              cv::waitKey(0);
              cv::imshow("Display Window" , img );
              cv::waitKey(0);
           } 
    }
    }
    return 1; 
   
}
