#include <iostream>
#include <vector>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
using namespace std;
using namespace cv;

void iterFind( std::vector< vector<float > > &vec , cv::Rect &recRecord , int wpos , int hpos) {
    if (hpos >= vec.size() || hpos < 0) {
        return;
    }
    if (wpos >= vec[hpos].size() || wpos < 0) {
        return;
    }
    if ( vec[ hpos ][ wpos ] == 0.0 ) {
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
    iterFind( vec , recRecord , wpos + 1 , hpos);
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
    for ( int h = 0 ; h < vec.size() ; ++h ) {
        for ( int w = 0 ; w < vec[h].size() ; ++w ) {
            cv::Rect rec(w , h , 0 , 0 );
            iterFind( vec , rec , w , h );
            if ( rec.width * rec.height < scale ) {
                continue;
            } 
            RectStore.push_back( rec );
        }
    } 
    return  RectStore;
}
