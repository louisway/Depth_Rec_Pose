#include <iostream>
#include <cstring>
#include <sstream>
#include <fstream>
#include <string>
#include "format.cpp" 
#include "opencv2/imgproc/imgproc.hpp"
using namespace std;
using namespace cv;

int main(){
int width = 640; 
int height = 480;
int start = 1;
int end = 1480;
std::string src_folder="./standing/";
std::string dest_folder="./jpg-stand/";
std::string result = "png-crunch/";
for(int i = start;i <= end;++i){
std::stringstream ss;
std::string filename;
ss << src_folder<< i << "d"<<".bin" ;
ss >> filename ;
FILE *img_file = fopen(filename.c_str(),"r");
if( !img_file) {std::cout<< "no such file" <<std::endl; continue;} 
fclose(img_file);
cv::Mat img_depth = Depth_bin2mat(filename.c_str(), width, height);
std::stringstream savefilestream;
std::string savefilename;
savefilestream << dest_folder << "output_"<<i<<".jpg";
savefilestream >> savefilename;
cv::imwrite(savefilename , img_depth); 
}
return 0;
}
