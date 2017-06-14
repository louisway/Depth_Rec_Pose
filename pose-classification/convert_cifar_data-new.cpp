//
// This script converts the CIFAR dataset to the leveldb format used
// by caffe to perform classification.
// Usage:
//    convert_cifar_data input_folder output_db_file
// The CIFAR dataset could be downloaded at
//    http://www.cs.toronto.edu/~kriz/cifar.html

#include <fstream>  // NOLINT(readability/streams)
#include <string>
#include <sstream>
#include <caffe/caffe.hpp>

#include "boost/scoped_ptr.hpp"
#include "glog/logging.h"
#include "google/protobuf/text_format.h"
#include "stdint.h"

#include "caffe/proto/caffe.pb.h"
#include "caffe/util/db.hpp"
 
#include <dirent.h>
using caffe::Datum; using boost::scoped_ptr;
using std::string;
namespace db = caffe::db;


const int kCIFARSize = 224;
const int kCIFARSize_width = 224;
const int kCIFARSize_height = 224; 
const int kCIFARImageNBytes = 50176;
const int kCIFARBatchSize = 10000;
const int kCIFARTrainBatches = 5;

void read_image(std::ifstream* file, int* label, char* buffer) {
  char label_char;
  file->read(&label_char, 1);
  *label = label_char;
  file->read(buffer, kCIFARImageNBytes);
  return;
}


void write_train_dataset(const string& input_folder, const string& output_folder,const string& db_type, const string& istrain){
    scoped_ptr<db::DB> train_db(db::GetDB(db_type));
    train_db->Open(output_folder + "cpose_" + "data" + "_" + db_type, db::NEW);
    scoped_ptr<db::Transaction> txn_data(train_db->NewTransaction());

    scoped_ptr<db::DB> label_db(db::GetDB(db_type));
    label_db->Open(output_folder + "cpose_" + "label_" + db_type, db::NEW);
    scoped_ptr<db::Transaction> txn_label(label_db->NewTransaction());
    int count = 1;
    
    int key = 0;
    int* label = &key;
    Datum datum_label;
    datum_label.set_channels(1);
    datum_label.set_height(1);
    datum_label.set_width(1);
    
    Datum datum;
    float str_buffer[kCIFARImageNBytes];
    char len[kCIFARImageNBytes];
    datum.set_channels(1);
    datum.set_height(kCIFARSize_height);
    datum.set_width(kCIFARSize_width);
    datum.clear_float_data();
    
    LOG(INFO) <<"Writing " +  istrain + " data";
    //read from crunch 
    std::string path = input_folder;
    
    DIR* crun_dirp = opendir((path+"image-crunch/").c_str());
    
    struct dirent *crun_dp ;
    
    *label = 0;
    std::cout<< "crunch label: " << *label << std::endl;
    while( (crun_dp = readdir(crun_dirp)) != NULL){
      datum.clear_float_data();
      std::string filename(crun_dp->d_name);
      if(filename == "." || filename == "..") continue;
      std::cout << count<<" : " << filename << " ;";
      FILE *fp = NULL;
      fp = fopen(((path+ "image-crunch/" )+ filename).c_str(),"rb");
      
      fread(str_buffer, sizeof(float), kCIFARImageNBytes, fp);
      
      datum_label.set_data(label,1);//+
      for(int m =0; m < kCIFARImageNBytes ;++m){
       datum.add_float_data(str_buffer[m]);
      }
      //datum.set_data(str_buffer, kCIFARImageNBytes);
      int length = snprintf(len, kCIFARImageNBytes, "%05d", count);
      //string key_str = caffe::format_int(count,8);
      string out;
      string out_label;
      CHECK(datum_label.SerializeToString(&out_label));
      CHECK(datum.SerializeToString(&out));
      /*if(count%10 == 0){
       txn_test->Put(string(str_buffer,length),out);
       
       std::stringstream test_save;
       std::string test_filename;
       test_save <<count<<".bin";
       test_save>>test_filename;
    
       std::cout<<test_filename<<std::endl;
       char *cha;
       fread(cha, sizeof(char), kCIFARImageNBytes,fp);
       fclose(fp);
       
       FILE* test_fp = NULL;
       test_fp = fopen(test_filename.c_str(),"wb");
       fwrite(cha,sizeof(char), kCIFARImageNBytes,test_fp);
              std::cout<<"read c" << std::endl;
       fclose(test_fp);
       free(cha);
      }
      else{
      txn->Put(string(str_buffer, length), out);
      fclose(fp);
      }*/
      txn_data->Put(string(len, length), out);
      txn_label->Put(string(len, length), out_label);
      fclose(fp);
      count++;
  
    }
    std::cout << "        crunch finish      " << std::endl; 
    //read from lying
    crun_dirp = opendir((path+"image-lying/").c_str());
    *label = 1;
    std::cout<< "lying label: " << *label << std::endl;
    while( (crun_dp = readdir(crun_dirp)) != NULL){
      datum.clear_float_data();
      std::string filename(crun_dp->d_name);
      if(filename == "." || filename == "..") continue;
      std::cout << count<<" : " << filename << " ;";
      FILE *fp = NULL;
      fp = fopen(((path+ "image-lying/" )+ filename).c_str(),"rb");
      fread(str_buffer, sizeof(float), kCIFARImageNBytes, fp);
     
      datum_label.set_data(label,1);
      //datum.set_data(str_buffer, kCIFARImageNBytes);
      for(int m =0; m < kCIFARImageNBytes ;++m){
       datum.add_float_data(str_buffer[m]);
      }
      //int length = snprintf(str_buffer, kCIFARImageNBytes, "%05d", count);
      int length = snprintf(len, kCIFARImageNBytes, "%05d", count);
      string out;
      string out_label;
      CHECK(datum_label.SerializeToString(&out_label));
      CHECK(datum.SerializeToString(&out));
      /*if(count%10 == 0){
       txn_test->Put(string(str_buffer,length),out);
       
       std::stringstream test_save;
       std::string test_filename;
       test_save <<count<<".bin";
       test_save>>test_filename;
    
       std::cout<<test_filename<<std::endl;
       char *cha;
       fread(cha, sizeof(char), kCIFARImageNBytes,fp);
       fclose(fp);
       
       FILE* test_fp = NULL;
       test_fp = fopen(test_filename.c_str(),"wb");
       fwrite(cha,sizeof(char), kCIFARImageNBytes,test_fp);
              std::cout<<"read c" << std::endl;
       fclose(test_fp);
       free(cha);
      }
      else{
      txn->Put(string(str_buffer, length), out);
      fclose(fp);
      }*/
      //txn_data->Put(string(str_buffer, length), out);
      //txn_label->Put(string(str_buffer, length), out_label);
      txn_data->Put(string(len, length), out);
      txn_label->Put(string(len, length), out_label);
      fclose(fp);
      count++;
  
    }
    std::cout << "            lying finish       " <<std::endl;
   //read from stand 
    crun_dirp = opendir((path+"image-stand/").c_str());
    *label = 2;
    std::cout<< "stand label: " << *label << std::endl;
    while( (crun_dp = readdir(crun_dirp)) != NULL){
       datum.clear_float_data();
      std::string filename(crun_dp->d_name);
      if(filename == "." || filename == "..") continue;
      std::cout << count<<" : " << filename << " ;";
      FILE *fp = NULL;
      fp = fopen(((path+ "image-stand/" )+ filename).c_str(),"rb");
      fread(str_buffer, sizeof(float), kCIFARImageNBytes, fp);
      datum_label.set_data(label,1);
      //datum.set_data(str_buffer, kCIFARImageNBytes);
       for(int m =0; m < kCIFARImageNBytes ;++m){
        datum.add_float_data(str_buffer[m]);
       }
      //int length = snprintf(str_buffer, kCIFARImageNBytes, "%05d", count);
      int length = snprintf(len, kCIFARImageNBytes, "%05d", count);
      string out;
      string out_label;
      CHECK(datum_label.SerializeToString(&out_label));
      CHECK(datum.SerializeToString(&out));
      /*if(count%10 == 0){
       txn_test->Put(string(str_buffer,length),out);
       
       std::stringstream test_save;
       std::string test_filename;
       test_save <<count<<".bin";
       test_save>>test_filename;
    
       std::cout<<test_filename<<std::endl;
       char *cha;
       fread(cha, sizeof(char), kCIFARImageNBytes,fp);
       fclose(fp);
       
       FILE* test_fp = NULL;
       test_fp = fopen(test_filename.c_str(),"wb");
       fwrite(cha,sizeof(char), kCIFARImageNBytes,test_fp);
              std::cout<<"read c" << std::endl;
       fclose(test_fp);
       free(cha);
      }
      else{
      txn->Put(string(str_buffer, length), out);
      fclose(fp);
      }*/
     txn_data->Put(string(len, length), out);
      txn_label->Put(string(len, length), out_label);
      fclose(fp);
      count++;
    }
   
   txn_data->Commit();
   train_db->Close();
   txn_label->Commit();
   label_db->Close();
}

int main(int argc, char** argv) {
  if (argc != 4) {
    printf("This script converts the CIFAR dataset to the leveldb format used\n"
           "by caffe to perform classification.\n"
           "Usage:\n"
           "    convert_cifar_data input_folder output_folder db_type\n"
           "Where the input folder should contain the binary batch files.\n"
           "The CIFAR dataset could be downloaded at\n"
           "    http://www.cs.toronto.edu/~kriz/cifar.html\n"
           "You should gunzip them after downloading.\n");
  } else {
    google::InitGoogleLogging(argv[0]);
    std::cout << argv[0] << argv[1] << argv[2] << argv[3] <<std::endl;
    //convert_dataset(string(argv[1]), string(argv[2]), string(argv[3]));
    write_train_dataset(string(argv[1]), string(argv[2]), string(argv[3]),"train");
  }
  return 0;
}
