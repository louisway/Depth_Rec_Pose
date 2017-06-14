#include <iostream>
#include <vector>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <utility>
#include <algorithm>
//#include <unordered_map>
using namespace std;
using namespace cv;
std::pair<int, int> Find_Pa(std::vector< std::vector<std::pair<int, int> > > &ParMat, std::pair<int,int> curpair){
     std::pair<int,int> pa_pair = ParMat[curpair.first][curpair.second];
     if(pa_pair == curpair || pa_pair == make_pair(-1,-1)) return curpair;
     std::pair<int,int> head_pair = Find_Pa(ParMat, pa_pair);
     ParMat[curpair.first][curpair.second] = head_pair;
     return head_pair;
}

void element_union(std::vector< std::vector<std::pair<int, int> > > &ParMat, std::pair<int,int> uppair, std::pair<int,int> thispair){
    std::pair<int, int> new_head = make_pair(std::min(uppair.first, thispair.first) , std::min(uppair.second, thispair.second));
    ParMat[uppair.first][uppair.second] = new_head;
    ParMat[thispair.first][thispair.second] = new_head;
     
}
int search_head(std::vector<std::pair<int,int> > ranvec,std::pair<int,int> parent_pair){
      int result = -1;
      for(int i = 0;i < ranvec.size();++i) {
          if(ranvec[i].first == parent_pair.first && ranvec[i].second == parent_pair.second) {
                result = i;
                break;
          }  
      }
      return result;
}

void iterFind_union(std::vector< std::vector<float> >&vec, std::vector<cv::Rect > &Rectstore ) {
   //find union
    std::pair <int , int> ini_pair(-1,-1);
    std::vector< std::vector<std::pair<int, int> > > ParMat(vec.size() , std::vector<std::pair<int,int> > (vec[0].size(), ini_pair) );
    for ( int h = 0; h < vec.size(); ++h) {
        for ( int w = 0; w < vec[h].size();++w) {
            if(vec[h][w] == 0.0) continue;
            ParMat[h][w] = make_pair(h,w);
            if(w != 0){
                 if (ParMat[h][w-1] != ini_pair) {
                    std::pair<int, int> leftPair = Find_Pa(ParMat , make_pair(h,w-1)); 
                     ParMat[h][w] = leftPair;
                 } 
            } 
            if(h != 0){
                if(ParMat[h-1][w] != ini_pair) {
                     std::pair<int,int> upPair = Find_Pa(ParMat , make_pair(h-1, w));
                     std::pair<int,int> thisPair = Find_Pa(ParMat,make_pair(h,w));
                     element_union(ParMat , upPair , thisPair);       
                }
            } 
            
        } 
    }
   //after find all union
   std::vector<std::pair<int,int> > ranvec;
   for(int h = 0;h < vec.size(); ++h){
       for(int w = 0 ; w < vec[h].size() ; ++w){
           if(vec[h][w] == 0.0) continue;
           std::pair<int,int> parent_pair = Find_Pa(ParMat , make_pair(h,w));
           //std::unordered_map<std::pair<int, int>, cv::Rect>::iterator got = rangeMap.find(parent_pair);
            int pos = search_head(ranvec, parent_pair);
           if(pos == -1){

             cv::Rect thisRect(parent_pair.second , parent_pair.first, 1, 1);
             Rectstore.push_back(thisRect);   
             ranvec.push_back(parent_pair);   
           } 
           else {
            int width = w - parent_pair.second + 1;
            int height = h - parent_pair.first + 1;
            Rectstore[pos].width = std::max(Rectstore[pos].width , width);
            Rectstore[pos].height = std::max(Rectstore[pos].height, height);
           }
       } 
   } 
  
}
void iterFind( std::vector< vector<float > > &vec , cv::Rect &recRecord , int wpos , int hpos ) {
  
    if ( vec[ hpos ][ wpos ] == 0.0 ) {
        return;
    }
 
    if (hpos >= vec.size() || hpos < 0) {
        return;
    }
    if (wpos >= vec[hpos].size() || wpos < 0) {
        return;
    }
    if ( wpos < recRecord.x ) {
        recRecord.width = recRecord.width + recRecord.x - wpos;
        recRecord.x = wpos;
    }
    else{
        int new_width = wpos - recRecord.x ;
        if ( new_width > recRecord.width ) {
            recRecord.width = new_width;
        }
    }
    if ( hpos < recRecord.y ) {
        recRecord.height = recRecord.height + recRecord.y - hpos;
        recRecord.y = hpos;
    }
    else {
        int new_height = hpos - recRecord.y;
        if ( new_height > recRecord.height ) {
            recRecord.height = new_height;
        }
    }
    vec[ hpos ][ wpos ] = 0.0;
    iterFind( vec , recRecord , wpos + 1 , hpos );
    iterFind( vec , recRecord , wpos - 1 , hpos);
    iterFind( vec , recRecord , wpos , hpos + 1);
    iterFind( vec , recRecord , wpos , hpos - 1);
    return;
}

std::vector< vector< float > > mat2vec( cv::Mat mat ) {
    std::vector < vector< float > > vec;
    for ( int i = 0 ; i < mat.rows ; ++i) {
        float * Mi = mat.ptr<float>(i);
        std::vector<float> vect(Mi , Mi + mat.cols);
        vec.push_back(vect);
    } 
    return vec;
} 

std::vector< cv::Rect > findConnectComponent( cv::Mat cmat , int scale = 150) {
    std::vector< cv::Rect > RectStore;
    std::vector< vector< float > > vec = mat2vec(cmat);
    iterFind_union(vec, RectStore);
    /*
    for ( int h = 0 ; h < vec.size() ; ++h ) {
        for ( int w = 0 ; w < vec[h].size() ; ++w ) {
            cv::Rect rec(w , h , 0 , 0 );
            iterFind( vec , rec , w , h );
            if ( rec.width * rec.height < scale ) {
                continue;
            } 
            RectStore.push_back( rec );
        }
    }*/ 
    return  RectStore;
}
