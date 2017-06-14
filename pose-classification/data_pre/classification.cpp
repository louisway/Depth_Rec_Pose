//#ifdef USE_OPENCV
#include <caffe/caffe.hpp>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/opencv.hpp"
#include <algorithm>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>
//#endif //USE_OPENCV

//#ifdef USE_OPENCV
using namespace caffe;  // NOLINT(build/namespaces)
using std::string;


class Classifier {
 public:
  Classifier(const string& model_file,
             const string& trained_file,
             bool isGpu,
             int device 
            );

  cv::Mat Classify( const cv::Mat& img , bool unify);

 private:
  void SetMean(const string& mean_file);

  std::vector<float> Predict(const cv::Mat& img );

  void WrapInputLayer(std::vector<cv::Mat>* input_channels);

  cv::Mat Preprocess(const cv::Mat& img);
  
  void MatUniform(cv::Mat& img , float threshold);
  void readImage(const cv::Mat& img , Blob<float>& input_blob);

 private:
  shared_ptr<Net<float> > net_;
  int num_channels_;
  int compress_size;
  int original_height;
  int original_weight;
  float transfer_ratio;
  int new_height;
  Blob<float> input_blob;
  float img_to_net_scale;
  TransformationParameter input_xform_param;
};

Classifier::Classifier(const string& model_file,
                       const string& trained_file, bool isGpu, int device = 0
                       ):input_blob() , img_to_net_scale(1.0) , original_height(0) , original_weight(0) , transfer_ratio(0.0) , new_height(0){

  if (!isGpu){
  Caffe::set_mode(Caffe::CPU);
  }
  else {
  Caffe::set_mode(Caffe::GPU);
  Caffe::SetDevice(device); 
  }

  /* Load the network. */
  net_.reset(new Net<float>(model_file,caffe::TEST));
  net_->CopyTrainedLayersFrom(trained_file);
  Blob<float>* input_layer = net_->input_blobs()[0];

  num_channels_ = input_layer->channels();
  compress_size = input_layer->width();
  input_xform_param.set_scale(img_to_net_scale);
  input_blob.Reshape(input_layer->num() , input_layer->channels() , input_layer->height() , input_layer->width()); 
  
}

cv::Mat Classifier::Classify( const cv::Mat& img , bool unify = false) {
  cv::Mat pre_Mat = Preprocess(img); 
/*preprocess*/
  std::vector<float> output = Predict(pre_Mat);
  cv::Mat out_Mat = cv::Mat(output);
  out_Mat = out_Mat.reshape(1, compress_size);
/*postprocess*/
  cv::Rect rec(0 , 0 , compress_size , new_height);
  cv::Mat crop_Mat = out_Mat(rec);
  cv::Size resilient_size( original_weight , original_height);
  cv::Mat resilient_Mat; 
  cv::resize(crop_Mat , resilient_Mat , resilient_size); 
  if (unify) {
     MatUniform(resilient_Mat , 1.0);
  }
 
  return resilient_Mat; 
}

void Classifier::readImage(const cv::Mat& img, Blob<float> &input_blob){
    float* transformed_data = input_blob.mutable_cpu_data();
    int height = img.rows;
    int width = img.cols;
    int top_index;
    for ( int h = 0; h < height; ++h){
         const ushort* ptr = img.ptr<ushort>(h);
         int img_index = 0;
         for(int w = 0; w < width;++w){
             top_index = h * width + w;
             float pixel = static_cast<float>(ptr[img_index++]);
             transformed_data[top_index] = pixel;
         }
    }
}
std::vector<float> Classifier::Predict(const cv::Mat& img) {
  Blob<float>* input_layer = net_->input_blobs()[0];
  input_layer->Reshape(1, num_channels_,
                    compress_size, compress_size);
/* Forward dimension change to all layers. */
  net_->Reshape();

  std::vector<cv::Mat> input_channels;
  //non-warp
  //DataTransformer<float> input_xformer(input_xform_param , TEST);
  //input_xformer.Transform( img  , &input_blob);
  readImage(img, input_blob);
  std::vector<Blob<float>*> input;
  input.push_back(&input_blob);

  //net_->ForwardPrefilled();
  std::vector<Blob<float>*> out = net_->Forward(input);

/* Copy the output layer to a std::vector */
  //Blob<float>* output_layer = net_->output_blobs()[0];

  const float* begin = out[0]->cpu_data();
  //const float* begin = output_layer->cpu_data();
  int width = input_layer->width();
  int height = input_layer->height();
  int offset = width * height;
  int layer_channel = out[0]->channels();
  std::vector<float> argMat;
  for(int i = 0; i < offset; i++) {
    int maxPos = 0;
    float maxValue = begin[i]; 
    for(int j = 0; j < layer_channel; j++){
       float mid = begin[i + j * offset];
       if (mid >= maxValue ){
           maxPos = j;
           maxValue = mid;
       }
   } 
   argMat.push_back(maxPos*1.0);
  } 
  return argMat;
}

/* Wrap the input layer of the network in separate cv::Mat objects
 * (one per channel). This way we save one memcpy operation and we
 * don't need to rely on cudaMemcpy2D. The last preprocessing
 * operation will write the separate channels directly to the input
 * layer. */
void Classifier::WrapInputLayer(std::vector<cv::Mat>* input_channels) {
  Blob<float>* input_layer = net_->input_blobs()[0];

  int width = input_layer->width();
  int height = input_layer->height();
  float* input_data = input_layer->mutable_cpu_data();
  for (int i = 0; i < input_layer->channels(); ++i) {
    cv::Mat channel(height, width, CV_32FC1, input_data);
    input_channels->push_back(channel);
    input_data += width * height;
  }
}

cv::Mat Classifier::Preprocess(const cv::Mat& img)
{
  original_height = img.rows;
  original_weight = img.cols;
  transfer_ratio = (original_weight * 1.0) / compress_size;
  new_height = original_height / transfer_ratio; 
  cv::Size reshape_size(compress_size , new_height);
  cv::Mat downsample;
  cv::resize( img , downsample , reshape_size ); 
  cv::Mat padded_Mat;
  cv::copyMakeBorder(downsample , padded_Mat , 0 , compress_size - new_height, 0 , 0 , cv::BORDER_CONSTANT , 0);
  return padded_Mat;
}

void Classifier::MatUniform(cv::Mat& img , float threshold ) {
    for ( int y = 0 ; y < img.rows ; y++ ) {
        for ( int x = 0 ; x < img.cols ; x++ ) {
            float color = img.at<float>(cv::Point(x,y));
            if (color > 0.0) {
                color = threshold;
            } 
            img.at<float>(cv::Point(x,y)) = color;
        } 
    }
}
//#endif
