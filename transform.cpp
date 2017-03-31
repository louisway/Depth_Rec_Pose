/*******************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

*******************************************************************************/

#include "rsglwidget.h"
#include "rsdevice.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <sstream>
#include <iomanip>
#ifdef WINDOWS
#include <direct.h>
#endif


#ifdef MIPI
#include "motionthread.h"
MotionThread * pMotionThread;
MotionFisheyeFrame frontFisheyeFrame;
#endif

#define SHADER_PROG_VERTEX 0
#define SHADER_PROG_TEXCOORD 1

#define CV_PI 3.1415926535897932384626433832795
#define TIME_SCALE 1000000lld
#define PRT 31.25f   // Period time 31.25ns (32MHz)

int m_zlr_last_timestamp, m_third_last_timestamp;
std::vector<uint8_t> rgb;
int g_zlr_fps, g_third_fps, g_fisheye_fps, m_zlr_num_frames, m_zlr_next_time, m_third_num_frames, m_third_next_time;
std::mutex g_fequeue_mutex;
typedef unsigned char byte;
int maxdepth = 4000;



RSGLWidget::RSGLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      clearColor(Qt::black),
      xRot(0),
      yRot(0),
      zRot(0),
      m_xRot(0),
      m_yRot(0),
      m_zRot(0),
      mshaderprog(0)
{
    memset(textures, 0, sizeof(textures));
    g_zlr_fps = 0;
    g_third_fps = 0;
    g_fisheye_fps = 0;
    m_zlr_num_frames = 0;
    m_zlr_next_time = 1000;
    m_zlr_last_timestamp = -1;
    m_third_num_frames = 0;
    m_third_next_time = 1000;
    m_third_last_timestamp = -1;
#ifdef MIPI
    frontFisheyeFrame.data = new uint8_t[640*480];
#endif

    for (int i = 0; i < 7; i++)
    {
        framedata2[i] = nullptr;
    }
#ifndef UBT1604
    mPlatCamFrameBuf = nullptr;
#endif
}

RSGLWidget::~RSGLWidget()
{
    makeCurrent();
    vbo.destroy();
    for (int i = 0; i < 7; ++i)
        delete textures[i];
    delete mshaderprog;
    doneCurrent();
#ifndef UBT1604
    if (mPlatCamFrameBuf!=nullptr)
    {
        delete mPlatCamFrameBuf;
        mPlatCamFrameBuf = nullptr;
    }
#endif

#ifdef MIPI
    delete frontFisheyeFrame.data;
#endif
}

QSize RSGLWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize RSGLWidget::sizeHint() const
{
    return QSize(200, 200);
}

static void qNormalizeAngle(int &angle)
{
    while (angle < 0)
        angle += 360 * 16;
    while (angle > 360 * 16)
        angle -= 360 * 16;
}

void RSGLWidget::setRecordFile(bool recordfile)
{
    mRecordFile = recordfile;
}

/////////////////////// yuv nv21 to rgb
int R = 0;
int G = 1;
int B = 2;

class RGB{
public: int r, g, b;
};

RGB yuvTorgb(char Y, char U, char V){
    RGB rgb = {};
    rgb.r = (int)((Y&0xff) + 1.4075 * ((V&0xff)-128));
    rgb.g = (int)((Y&0xff) - 0.3455 * ((U&0xff)-128) - 0.7169*((V&0xff)-128));
    rgb.b = (int)((Y&0xff) + 1.779 * ((U&0xff)-128));
    rgb.r =(rgb.r<0? 0: rgb.r>255? 255 : rgb.r);
    rgb.g =(rgb.g<0? 0: rgb.g>255? 255 : rgb.g);
    rgb.b =(rgb.b<0? 0: rgb.b>255? 255 : rgb.b);
    return rgb;
}

void NV21ToRGB(uchar * src, uchar* dst,int width, int height){
    int numOfPixel = width * height;
    int positionOfV = numOfPixel;

    for(int i=0; i<height; i++){
        int startY = i*width;
        int step = i/2*width;
        int startV = positionOfV + step;
        for(int j = 0; j < width; j++){
            int Y = startY + j;
            int V = startV + j/2;
            int U = V + 1;
            int index = Y*3;
            RGB tmp = yuvTorgb(src[Y], src[U], src[V]);
            dst[index+R] = tmp.r;
            dst[index+G] = tmp.g;
            dst[index+B] = tmp.b;
        }
    }
}

void NV12ToRGB(uchar * src, uchar * dst, int width, int height){
        int numOfPixel = width * height;
        int positionOfU = numOfPixel;

        for(int i=0; i<height; i++){
            int startY = i*width;
            int step = i/2*width;
            int startU = positionOfU + step;
            for(int j = 0; j < width; j++){
                int Y = startY + j;
                int U = startU + j/2;
                int V = U + 1;
                int index = Y*3;
                RGB tmp = yuvTorgb(src[Y], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
            }
        }
    }

void YV16ToRGB(uchar * src, uchar * dst, int width, int height){
        int numOfPixel = width * height;
        int positionOfU = numOfPixel;
        int positionOfV = numOfPixel/2 + numOfPixel;
        for(int i=0; i<height; i++){
            int startY = i*width;
            int step = i*width/2;
            int startU = positionOfU + step;
            int startV = positionOfV + step;
            for(int j = 0; j < width; j++){
                int Y = startY + j;
                int U = startU + j/2;
                int V = startV + j/2;
                int index = Y*3;
                RGB tmp = yuvTorgb(src[Y], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
            }
        }
    }

void YV12ToRGB(uchar * src, uchar * dst, int width, int height){
        int numOfPixel = width * height;
        int positionOfV = numOfPixel;
        int positionOfU = numOfPixel/4 + numOfPixel;

        for(int i=0; i<height; i++){
            int startY = i*width;
            int step = (i/2)*(width/2);
            int startV = positionOfV + step;
            int startU = positionOfU + step;
            for(int j = 0; j < width; j++){
                int Y = startY + j;
                int V = startV + j/2;
                int U = startU + j/2;
                int index = Y*3;
                RGB tmp = yuvTorgb(src[Y], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
            }
        }
    }

void YUY2ToRGB(uchar * src, uchar * dst, int width, int height){
        int lineWidth = 2*width;
        for(int i=0; i<height; i++){
            int startY = i*lineWidth;
            for(int j = 0; j < lineWidth; j+=4){
                int Y1 = j + startY;
                int Y2 = Y1+2;
                int U = Y1+1;
                int V = Y1+3;
                int index = (Y1>>1)*3;
                RGB tmp = yuvTorgb(src[Y1], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
                index += 3;
                tmp = yuvTorgb(src[Y2], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
            }
        }
    }

void UYVYToRGB(uchar * src, uchar * dst, int width, int height){
        int numOfPixel = width * height;
        int lineWidth = 2*width;
        for(int i=0; i<height; i++){
            int startU = i*lineWidth;
            for(int j = 0; j < lineWidth; j+=4){
                int U = j + startU;
                int Y1 = U+1;
                int Y2 = U+3;
                int V = U+2;
                int index = (U>>1)*3;
                RGB tmp = yuvTorgb(src[Y1], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
                index += 3;
                tmp = yuvTorgb(src[Y2], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
            }
        }
    }

void NV16ToRGB(uchar * src, uchar * dst, int width, int height){
        int numOfPixel = width * height;
        int positionOfU = numOfPixel;

        for(int i=0; i<height; i++){
            int startY = i*width;
            int step = i*width;
            int startU = positionOfU + step;
            for(int j = 0; j < width; j++){
                int Y = startY + j;
                int U = startU + j/2;
                int V = U + 1;
                int index = Y*3;
                RGB tmp = yuvTorgb(src[Y], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
            }
        }
    }

void NV61ToRGB(uchar * src, uchar * dst, int width, int height){
        int numOfPixel = width * height;
        int positionOfV = numOfPixel;

        for(int i=0; i<height; i++){
            int startY = i*width;
            int step = i*width;
            int startV = positionOfV + step;
            for(int j = 0; j < width; j++){
                int Y = startY + j;
                int V = startV + j/2;
                int U = V + 1;
                int index = Y*3;
                RGB tmp = yuvTorgb(src[Y], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
            }
        }
    }

void YVYUToRGB(uchar * src, uchar * dst, int width, int height){
        int numOfPixel = width * height;
        int lineWidth = 2*width;
        for(int i=0; i<height; i++){
            int startY = i*lineWidth;
            for(int j = 0; j < lineWidth; j+=4){
                int Y1 = j + startY;
                int Y2 = Y1+2;
                int V = Y1+1;
                int U = Y1+3;
                int index = (Y1>>1)*3;
                RGB tmp = yuvTorgb(src[Y1], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
                index += 3;
                tmp = yuvTorgb(src[Y2], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
            }
        }
    }

void VYUYToRGB(uchar * src, uchar * dst, int width, int height){
        int numOfPixel = width * height;
        int lineWidth = 2*width;
        for(int i=0; i<height; i++){
            int startV = i*lineWidth;
            for(int j = 0; j < lineWidth; j+=4){
                int V = j + startV;
                int Y1 = V+1;
                int Y2 = V+3;
                int U = V+2;
                int index = (U>>1)*3;
                RGB tmp = yuvTorgb(src[Y1], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
                index += 3;
                tmp = yuvTorgb(src[Y2], src[U], src[V]);
                dst[index+R] = tmp.r;
                dst[index+G] = tmp.g;
                dst[index+B] = tmp.b;
            }
        }
    }
///////////////////////

// Produce an RGB image from a depth image by computing a cumulative histogram of depth values and using it to map each pixel between a near color and a far color
void RSGLWidget::ConvertDepthToRGBUsingHistogramImprove(const uint16_t depthImage[], int width, int height, const uint8_t nearColor[3], const uint8_t farColor[3], uint8_t rgbImage[])
{
    const int depth_max = 5000, depth_min = 300;

    uint16_t H, V;
    double S;

    // Produce RGB image by using the histogram to interpolate between two colors
    auto rgb = rgbImage;
    for (int i = 0; i < width * height; i++)
    {
        uint16_t d = depthImage[i];

        if (d >= depth_max)
            d = depth_max - 1;

        if (d > depth_min) // For valid depth values (depth > 0)
        {
            float proportion = (float)d/(float)depth_max;
            H = proportion * 360;
            if (proportion < 0.2)
                S = proportion*4;
            else if (proportion < 0.6)
                S = proportion*1.5;
            else
                S = proportion;

            V = 255 - ((d>>3) & 0x00ff);

            if (S == 0)                       //HSL values = 0 ÷ 1
            {
                *rgb++ = V;                   //RGB results = 0 ÷ 255
                *rgb++ = V;
                *rgb++ = V;
            }
            else
            {
                H /= 60;
                uint8_t i = (int) H;
                uint8_t f = H - i;
                uint8_t a = V * (1 - S);
                uint8_t b = V * (1 - S * f);
                uint8_t c = V * (1 - S * (1 - f));
                switch(i)
                {
                    case 0:
                        *rgb++ = V;
                        *rgb++ = c;
                        *rgb++ = a;
                        break;
                    case 1:
                        *rgb++ = b;
                        *rgb++ = V;
                        *rgb++ = a;
                        break;
                    case 2:
                        *rgb++ = a;
                        *rgb++ = V;
                        *rgb++ = c;
                        break;
                    case 3:
                        *rgb++ = a;
                        *rgb++ = b;
                        *rgb++ = V;
                        break;
                    case 4:
                        *rgb++ = c;
                        *rgb++ = a;
                        *rgb++ = V;
                        break;
                    case 5:
                        *rgb++ = V;
                        *rgb++ = a;
                        *rgb++ = b;
                        break;
                }
            }
        }
        else // Use black pixels for invalid values (depth == 0)
        {
            *rgb++ = 0;
            *rgb++ = 0;
            *rgb++ = 0;
        }
    }
}

static unsigned char clampbyte(int value)
{
    return value < 0 ? 0 : value > 255 ? 255 : value;
}

void RSGLWidget::YUY2ToRGB8(const unsigned char * src, int width, int height, unsigned char * dst)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; x += 2)
        {
            int y0 = src[0], u0 = src[1], y1 = src[2], v0 = src[3];

            int c = y0 - 16, d = u0 - 128, e = v0 - 128;
            dst[0] = clampbyte((298 * c + 409 * e + 128) >> 8);           // red
            dst[1] = clampbyte((298 * c - 100 * d - 208 * e + 128) >> 8); // green
            dst[2] = clampbyte((298 * c + 516 * d + 128) >> 8);           // blue

            c = y1 - 16;
            dst[3] = clampbyte((298 * c + 409 * e + 128) >> 8);           // red
            dst[4] = clampbyte((298 * c - 100 * d - 208 * e + 128) >> 8); // green
            dst[5] = clampbyte((298 * c + 516 * d + 128) >> 8);           // blue

            src += 4;
            dst += 6;
        }
    }
}

void RSGLWidget::BGRA8ToRGB8(const unsigned char * src, int width, int height, unsigned char * dst)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; x ++)
        {
            dst[0]=src[2];
            dst[1]=src[1];
            dst[2]=src[0];

            src+=4;
            dst+=3;
        }
    }
}

void RSGLWidget::hsb2rgb(int hsb_h,int rgb[3]) {//hsb_s=100%,hsb_b=100%,0<hsb_h<256;

    for(int offset=240,i=0;i<3;i++,offset-=120) {
        float x=abs((hsb_h+offset)%360-240);
        if(x<=60) rgb[i]=255;
        else if(60<x && x<120) rgb[i]=((1-(x-60)/60)*255);
        else rgb[i]=0;
    }
    return ;
}

void RSGLWidget::ConvertDepthToRGBUsingHistogramhsb(const uint16_t depthImage[], int width, int height,  uint8_t rgbImage[])
{
    // Produce a cumulative histogram of depth values
    int histogram[256 * 256] = {1};
    for (int i = 0; i < width * height; ++i)
    {
        if (auto d = depthImage[i]) ++histogram[d];
    }
    for (int i = 1; i < 256 * 256; i++)
    {
        histogram[i] += histogram[i - 1];
    }

    // Remap the cumulative histogram to the range 0..256
    for (int i = 1; i < 256 * 256; i++)
    {
        histogram[i] = (histogram[i] << 8) / histogram[256 * 256 - 1];
    }

    // Produce RGB image by using the histogram to interpolate between two colors
    auto rgb = rgbImage;
    int color[3];
    for (int i = 0; i < width * height; i++)
    {
        if (uint16_t d = depthImage[i]) // For valid depth values (depth > 0)
        {
            auto t = histogram[d]; // Use the histogram entry (in the range of 0..256) to interpolate between nearColor and farColor
            hsb2rgb(t,color);
            *rgb++ = color[0];
            *rgb++ = color[1];
            *rgb++ = color[2];
        }
        else // Use black pixels for invalid values (depth == 0)
        {
            *rgb++ = 0;
            *rgb++ = 0;
            *rgb++ = 0;
        }
    }
}

void RSGLWidget::ConvertDepthToRGBUsingHistogram(const uint16_t depthImage[], int width, int height, const uint8_t nearColor[3], const uint8_t farColor[3], uint8_t rgbImage[])
{
    // Produce a cumulative histogram of depth values
    int histogram[256 * 256] = {1};
    for (int i = 0; i < width * height; ++i)
    {
        if (auto d = depthImage[i]) ++histogram[d];
    }
    for (int i = 1; i < 256 * 256; i++)
    {
        histogram[i] += histogram[i - 1];
    }

    // Remap the cumulative histogram to the range 0..256
    for (int i = 1; i < 256 * 256; i++)
    {
        histogram[i] = (histogram[i] << 8) / histogram[256 * 256 - 1];
    }

    // Produce RGB image by using the histogram to interpolate between two colors
    auto rgb = rgbImage;
    for (int i = 0; i < width * height; i++)
    {
        if (uint16_t d = depthImage[i]) // For valid depth values (depth > 0)
        {
            auto t = histogram[d]; // Use the histogram entry (in the range of 0..256) to interpolate between nearColor and farColor
            *rgb++ = ((256 - t) * nearColor[0] + t * farColor[0]) >> 8;
            *rgb++ = ((256 - t) * nearColor[1] + t * farColor[1]) >> 8;
            *rgb++ = ((256 - t) * nearColor[2] + t * farColor[2]) >> 8;
        }
        else // Use black pixels for invalid values (depth == 0)
        {
            *rgb++ = 0;
            *rgb++ = 0;
            *rgb++ = 0;
        }
    }
}

void RSGLWidget::make_depth_histogramhsb(uint8_t rgb_image[640*480*3], const uint16_t depth_image[], int width, int height)
{
    static uint32_t histogram[0x10000];
    int i, d, f;
    int color[3];
    memset(histogram, 0, sizeof(histogram));

    for(i = 0; i < width*height; ++i) ++histogram[depth_image[i]];
    for(i = 2; i < 0x10000; ++i) histogram[i] += histogram[i-1]; // Build a cumulative histogram for the indices in [1,0xFFFF]
    for(i = 0; i < width*height; ++i)
    {
        if(d = depth_image[i])
        {
            f = histogram[d] * 255 / histogram[0xFFFF]; // 0-255 based on histogram location
            hsb2rgb(f,color);
            rgb_image[i*3 + 0] = color[0];
            rgb_image[i*3 + 1] = color[1];
            rgb_image[i*3 + 2] = color[2];
        }
        else
        {
            rgb_image[i*3 + 0] = 20;
            rgb_image[i*3 + 1] = 5;
            rgb_image[i*3 + 2] = 0;
        }
    }
}

void RSGLWidget::make_depth_histogram(uint8_t rgb_image[640*480*3], const uint16_t depth_image[], int width, int height)
{
    static uint32_t histogram[0x10000];
    int i, d, f;
    memset(histogram, 0, sizeof(histogram));

    for(i = 0; i < width*height; ++i) ++histogram[depth_image[i]];
    for(i = 2; i < 0x10000; ++i) histogram[i] += histogram[i-1]; // Build a cumulative histogram for the indices in [1,0xFFFF]
    for(i = 0; i < width*height; ++i)
    {
        if(d = depth_image[i])
        {
            f = histogram[d] * 255 / histogram[0xFFFF]; // 0-255 based on histogram location
            rgb_image[i*3 + 0] = 255 - f;
            rgb_image[i*3 + 1] = 0;
            rgb_image[i*3 + 2] = f;
        }
        else
        {
            rgb_image[i*3 + 0] = 20;
            rgb_image[i*3 + 1] = 5;
            rgb_image[i*3 + 2] = 0;
        }
    }
}

void RSGLWidget::make_luminance2rgb(uint8_t rgb_image[640*480*3], const uint8_t luminance_image[], int width, int height)
{
    int i;

    for(i = 0; i < width*height; ++i)
    {
        rgb_image[i*3 + 0] = luminance_image[i];
        rgb_image[i*3 + 1] = luminance_image[i];
        rgb_image[i*3 + 2] = luminance_image[i];
    }
}

void RSGLWidget::make_luminance2rgb(uint8_t rgb_image[640*480*3], const uint16_t luminance_image[], int width, int height)
{
    int i;

    for(i = 0; i < width*height; ++i)
    {
        rgb_image[i*3 + 0] = luminance_image[i]>>8;
        rgb_image[i*3 + 1] = luminance_image[i]>>8;
        rgb_image[i*3 + 2] = luminance_image[i]>>8;
    }
}

void RSGLWidget::setXRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != m_xRot) {
        m_xRot = angle;
    }
}

void RSGLWidget::setYRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != m_yRot) {
        m_yRot = angle;
    }
}

void RSGLWidget::setZRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != m_zRot) {
        m_zRot = angle;
    }
}

void RSGLWidget::cleanup()
{
}

void RSGLWidget::initializeGL()
{
    wait_first_valid_frame = false;

    initGL();

    initShader();

    bindShader();
}

void RSGLWidget::paintGL()
{

    glClearColor(clearColor.redF(), clearColor.greenF(), clearColor.blueF(), clearColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 m;
    if(!g_stream_pointcloud)
    {
        m.ortho(-0.5f, +0.5f, +0.5f, -0.5f, 4.0f, 15.0f);
        m.translate(0.0f, 0.0f, -10.0f);
    }
    if(g_stream_pointcloud)
    {
        if(g_switch_dsapi_librs)//dsapi
        {
            m.perspective(45.0f, (float)g_widget_w/g_widget_h, 0.1f, 4000.0f);
            m.lookAt(QVector3D(0,0,0),QVector3D(0,0,1),QVector3D(0,-1,0));
            m.translate(0.0f, 0.0f, 1000.0f);//set rotate center
            m.rotate(m_xRot / 16.0f, 1.0f, 0.0f, 0.0f);
            m.rotate(m_yRot / 16.0f, 0.0f, 1.0f, 0.0f);
            m.rotate(m_zRot / 16.0f, 0.0f, 0.0f, 1.0f);
            m.translate(0.0f, 0.0f, -1000.0f);// reset draw center
        }
        else
        {
            m.perspective(60.0f, (float)g_widget_w/g_widget_h, 0.01f, 40.0f);
            m.lookAt(QVector3D(0,0,0),QVector3D(0,0,1),QVector3D(0,-1,0));
            m.translate(0.0f, 0.0f, 2.0f);//set rotate center
            m.rotate(m_xRot / 16.0f, 1.0f, 0.0f, 0.0f);
            m.rotate(m_yRot / 16.0f, 0.0f, 1.0f, 0.0f);
            m.rotate(m_zRot / 16.0f, 0.0f, 0.0f, 1.0f);
            m.translate(0.0f, 0.0f, -2.0f);// reset draw center
        }
    }

    mshaderprog->setUniformValue("matrix", m);
    mshaderprog->enableAttributeArray(SHADER_PROG_VERTEX);
    mshaderprog->enableAttributeArray(SHADER_PROG_TEXCOORD);
    mshaderprog->setAttributeBuffer(SHADER_PROG_VERTEX, GL_FLOAT, 0, 3, 5 * sizeof(GLfloat));
    mshaderprog->setAttributeBuffer(SHADER_PROG_TEXCOORD, GL_FLOAT, 3 * sizeof(GLfloat), 2, 5 * sizeof(GLfloat));

    if (startGetFrame)
    {
        if (g_stream_depth || g_stream_left || g_stream_right || g_stream_color || g_stream_pointcloud || g_stream_fisheye)
        {
            // Get once frames, get once, can get many kinds of images
            if (!pauseFlag){
                if (g_pRSDevice->wait_frames())
                    wait_first_valid_frame = true;
            }

            int thisFrameNumZLR = 0;
            int thisFrameNumRGB = 0;
            int thisFrameNumFISH = 0;

            if (g_stream_depth || g_stream_pointcloud){
                thisFrameNumZLR =g_pRSDevice->get_frame_num(rs_depth);
                update_ui_framenum(thisFrameNumZLR);
            }
            else if (g_stream_left)
            {
                thisFrameNumZLR =g_pRSDevice->get_frame_num(rs_left);
                update_ui_framenum(thisFrameNumZLR);
            }
            else if (g_stream_right)
            {
                thisFrameNumZLR =g_pRSDevice->get_frame_num(rs_right);
                update_ui_framenum(thisFrameNumZLR);
            }

            if (g_stream_color || g_stream_pointcloud){
                thisFrameNumRGB = g_pRSDevice->get_frame_num(rs_color);
                update_ui_framenum(thisFrameNumRGB);
            }

            if (g_stream_fisheye){
                thisFrameNumFISH = g_pRSDevice->get_frame_num(rs_fisheye);
            }


            if( thisFrameNumZLR != lastFrameNumZLR)
            {
                lastFrameNumZLR = thisFrameNumZLR;
                fpsFrameCntZLR++;
                QDateTime now_zlr = QDateTime::currentDateTime();
                qint64 nowMSec = now_zlr.toMSecsSinceEpoch();
                qint64 lastMSec = lastTimeZLR.toMSecsSinceEpoch();
                qint64 diff = nowMSec - lastMSec;

                if ( diff >= 1000) {
                    g_zlr_fps = fpsFrameCntZLR*1000/diff;
                    lastTimeZLR = QDateTime::currentDateTime();
                    fpsFrameCntZLR = 0;
                }
            }

            if( thisFrameNumRGB != lastFrameNumRGB)
            {
                lastFrameNumRGB = thisFrameNumRGB;
                fpsFrameCntRGB++;
                QDateTime now_rgb = QDateTime::currentDateTime();
                qint64 nowMSec = now_rgb.toMSecsSinceEpoch();
                qint64 lastMSec = lastTimeRGB.toMSecsSinceEpoch();
                qint64 diff = nowMSec - lastMSec;
                is_new_rgb_frame=true;
                if ( diff >= 1000) {
                    g_third_fps = fpsFrameCntRGB*1000/diff;
                    lastTimeRGB = QDateTime::currentDateTime();
                    fpsFrameCntRGB = 0;
                }
            }

            if( thisFrameNumFISH != lastFrameNumFISH)
            {
                lastFrameNumFISH = thisFrameNumFISH;
                fpsFrameCntFISH++;
                QDateTime now_fish = QDateTime::currentDateTime();
                qint64 nowMSec = now_fish.toMSecsSinceEpoch();
                qint64 lastMSec = lastTimeFISH.toMSecsSinceEpoch();
                qint64 diff = nowMSec - lastMSec;

                if ( diff >= 1000) {
                    g_fisheye_fps = fpsFrameCntFISH*1000/diff;
                    lastTimeFISH = QDateTime::currentDateTime();
                    fpsFrameCntFISH = 0;
                }
            }
        }

        if (wait_first_valid_frame)
        {

        // draw frame
        if (g_stream_color){
            getFrameData(FrameTextType[1]);
        }
        if (g_stream_depth){
            getFrameData(FrameTextType[0]);
        }
        if (g_stream_left){
            getFrameData( FrameTextType[2]);
        }
        if (g_stream_right){
            getFrameData(FrameTextType[3]);
        }
        if (g_stream_fisheye){
            getFrameData(FrameTextType[4]);
        }
        if (g_stream_platcam){
            getFrameData(FrameTextType[6]);
        }
        if (g_stream_pointcloud){
            getFrameDataPC();
        }
        }
    }

    update();
}

void RSGLWidget::resizeGL(int width, int height)
{
    g_widget_w=width;
    g_widget_h=height;
    int side = qMin(width, height);
    glViewport((width - side) / 2, (height - side) / 2, side, side);
    if(x>g_widget_h)
        x=g_widget_h-1;
    if(y>g_widget_w)
        y=g_widget_w-1;

}

void RSGLWidget::mousePressEvent(QMouseEvent *event)
{
    m_lastPos = event->pos();
    x=event->y();
    y=event->x();
}

void RSGLWidget::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->x() - m_lastPos.x();
    int dy = event->y() - m_lastPos.y();

    if (event->buttons() & Qt::LeftButton) {
        setXRotation(m_xRot + 8 * dy);
        setYRotation(m_yRot + 8 * dx);
    } else if (event->buttons() & Qt::RightButton) {
        setXRotation(m_xRot + 8 * dy);
        setZRotation(m_zRot + 8 * dx);
    }
    m_lastPos = event->pos();
}

void RSGLWidget::makeObject()
{
    static const int coords[4][3] = {
        { +1, -1, -1 }, { -1, -1, -1 }, { -1, +1, -1 }, { +1, +1, -1 }
    };

    static const int Texcoords[4][2] = {
        { 1, 0}, { 0, 0}, { 0, 1}, { 1, 1}
    };

    QVector<GLfloat> vertData;
    for (int j = 0; j < 4; ++j) {
        // vertex position
        vertData.append(0.5 * coords[j][0]);
        vertData.append(0.5 * coords[j][1]);
        vertData.append(0.5 * coords[j][2]);
        // texture coordinate
        vertData.append(Texcoords[j][0]);
        vertData.append(Texcoords[j][1]);
    }

    vbo.create();
    vbo.bind();
    vbo.allocate(vertData.constData(), vertData.count() * sizeof(GLfloat));
}

void RSGLWidget::makeObjectMuti()
{
    static const int coords[4][4][3] = {
       { { 0, 0, -1 }, { -1, 0, -1 }, { -1, +1, -1 }, { 0, +1, -1 } },
       { { +1, 0, -1 }, { 0, 0, -1 }, { 0, +1, -1 }, { +1, +1, -1 } },
       { { 0, -1, -1 }, { -1, -1, -1 }, { -1, 0, -1 }, { 0, 0, -1 } },
       { { +1, -1, -1 }, { 0, -1, -1 }, { 0, 0, -1 }, { +1, 0, -1 } }
    };

    static const int Texcoords[4][2] = {
        { 1, 0}, { 0, 0}, { 0, 1}, { 1, 1}
    };

    QVector<GLfloat> vertData;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            // vertex position
            vertData.append(0.5 * coords[i][j][0]);
            vertData.append(0.5 * coords[i][j][1]);
            vertData.append(0.5 * coords[i][j][2]);
            // texture coordinate
            vertData.append(Texcoords[j][0]);
            vertData.append(Texcoords[j][1]);
        }
    }

    vbo.create();
    vbo.bind();
    vbo.allocate(vertData.constData(), vertData.count() * sizeof(GLfloat));
}

void RSGLWidget::makeObjectPointCloud()
{
    static const int coords[4][3] = {
        { +1, -1, -1 }, { -1, -1, -1 }, { -1, +1, -1 }, { +1, +1, -1 }
    };

    static const int Texcoords[4][2] = {
        { 1, 0}, { 0, 0}, { 0, 1}, { 1, 1}
    };

    QVector<GLfloat> vertData;
    for (int j = 0; j < 4; ++j) {
        // vertex position
        vertData.append(0.4 * coords[j][0]);
        vertData.append(0.4 * coords[j][1]);
        vertData.append(0.4 * coords[j][2]);
        // texture coordinate
        vertData.append(Texcoords[j][0]);
        vertData.append(Texcoords[j][1]);
    }

    vbo.create();
    vbo.bind();
    vbo.allocate(vertData.constData(), vertData.count() * sizeof(GLfloat));
}


void RSGLWidget::getFrameDataPC()
{

    static float *buffer[5]={nullptr};
    StreamType tex_stream = rs_color;
    static char * framedata3 = nullptr;
    switch(g_texture_index)//switch texture type
    {
    case 0:
        tex_stream=rs_left;
        break;
    case 1:
        tex_stream=rs_depth;
        break;
    default:break;
    }

    int pc_w= g_pRSDevice->get_stream_width(rs_depth);//depth width and height
    int pc_h= g_pRSDevice->get_stream_height(rs_depth);
    int tex_w = g_pRSDevice->get_stream_width(tex_stream); //texture width and height
    int tex_h = g_pRSDevice->get_stream_height(tex_stream);

    //when texture changed, need reset texture buffer size
    if(g_texture_change==1)
    {
        g_texture_change=0;
        setReadyTexture(tex_w, tex_h, 5);
        if(framedata3!=nullptr)
            delete [] framedata3;
        framedata3=new char[tex_h*tex_w*3];
        for(int i=0;i<5;i++)
        {
            if(buffer[i]!=nullptr)
                delete [] buffer[i];
            buffer[i]=new float[pc_w*pc_h];
        }
    }



    ////////////////////////////////////get texture data and bind texture


    if((tex_stream==rs_color && is_new_rgb_frame) || tex_stream!=rs_color)
    {
        g_pRSDevice->get_frame_data_rgb(tex_stream,framedata3);

        //for sr300, color is small than depth, so if depth cood over range, need set texture to white,
        //for issue 125
        for(int i=tex_h-2;i<tex_h;i++)
        {
            for(int j=tex_w-2;j<tex_w;j++)
            {
                framedata3[(i*tex_w+j)*3]=255;
                framedata3[(i*tex_w+j)*3+1]=255;
                framedata3[(i*tex_w+j)*3+2]=255;
            }
        }
    }
    if(tex_stream==rs_color)
        is_new_rgb_frame=false;

    textures[5]->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8,framedata3);
    textures[5]->bind();


    ////////////////////////////////////////////////// draw vertex and coord
    QVector<GLfloat> vertDataPC;

    g_pRSDevice->get_frame_data_pc(tex_stream,buffer);

    int index=0;

    if(g_geometry==0)//points
    {
        for(int y=0; y<pc_h; ++y)
        {
            for(int x=0; x<pc_w; ++x)
            {
                index=y*pc_w+x;

                if (buffer[2][index]>0)
                {
                    // vertex position
                    vertDataPC.append(buffer[0][index]);
                    vertDataPC.append(buffer[1][index]);
                    vertDataPC.append(buffer[2][index]);
                    //texture coord
                    vertDataPC.append(buffer[3][index]);
                    vertDataPC.append(buffer[4][index]);

                }
            }
        }
        vbo.allocate(vertDataPC.constData(), vertDataPC.count() * sizeof(GLfloat));
        glDrawArrays(GL_POINTS, 0, vertDataPC.count()/5);

    }

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

    if(g_geometry==1)//mesh
    {
        int dy[6]={-1,0,0,-1,0,-1};
        int dx[6]={-1,-1,0,-1,0,0};
        for(int y=1; y<pc_h; ++y)
        {
            for(int x=1; x<pc_w; ++x)
            {
                auto d0 = buffer[2][(y - 1) * pc_w + (x - 1)];
                auto d1 = buffer[2][(y - 1) * pc_w + (x - 0)];
                auto d2 = buffer[2][(y - 0) * pc_w + (x - 0)];
                auto d3 = buffer[2][(y - 0) * pc_w + (x - 1)];
                auto minD = min(min(d0, d1), min(d2, d3));
                auto maxD = max(max(d0, d1), max(d2, d3));
                if (d0>0 && d1>0 && d2>0 && d3>0 && maxD - minD < 50)
                {
                    for(int k=0;k<6;k++)//draw triangles , so a mesh need two triangles, so 6 points
                    {
                        index = (y+dy[k])*pc_w+x+dx[k];
                        // vertex position
                        vertDataPC.append(buffer[0][index]);
                        vertDataPC.append(buffer[1][index]);
                        vertDataPC.append(buffer[2][index]);
                        //texture coord
                        vertDataPC.append(buffer[3][index]);
                        vertDataPC.append(buffer[4][index]);

                    }
                }
            }
        }
        vbo.allocate(vertDataPC.constData(),vertDataPC.count()*sizeof(GLfloat));
        glDrawArrays(GL_TRIANGLES, 0, vertDataPC.count()*sizeof(GLfloat)/5);
    }
}


void RSGLWidget::setGetFrameFlag(bool flag)
{
    startGetFrame = flag;
    if(flag==false)
    {
        for(int i=0;i<7;i++)
        {
            if(framedata2[i]!=nullptr)
            {
                delete framedata2[i];
                framedata2[i]=nullptr;
            }
        }
    }
}

void RSGLWidget::setPauseFlag(bool value)
{
    pauseFlag = value;
}

void RSGLWidget::setReadyTexture(int width, int height, int textType)
{
    textures[textType] = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textures[textType]->create();
    textures[textType]->setSize(width, height);
    textures[textType]->setFormat(QOpenGLTexture::TextureFormat::RGB8_UNorm);
    textures[textType]->allocateStorage();
}

void RSGLWidget::setFrameParameter(int textType)
{
    FrameTextType[textType] = textType;
    depth_scale = g_pRSDevice->get_depth_scale();
}

#ifndef UBT1604
void RSGLWidget::glGrabPlatCamFrame(QVideoFrame * value)
{
    //QVideoFrame cloneFrame(* value);
    if (mPlatCamFrameBuf != nullptr)
    {
        delete mPlatCamFrameBuf;
        mPlatCamFrameBuf = nullptr;
    }
    mPlatCamFrameBuf = new QVideoFrame( *value);
}
#endif

void RSGLWidget::setFisheyeParameter(QThread *pmd)
{
#ifdef MIPI
    pMotionThread = (MotionThread*)pmd;
    fisheyeWidth = pMotionThread->getFisheyeFrameWidth();
    fisheyeHeight = pMotionThread->getFisheyeFrameHeight();
#endif
}


//#ifndef ANDROID

void RSGLWidget::save_fisheye_data()
{
#ifdef ANDROID
    return;
#endif
    const char *fisheyefile = "./fisheye_timestamps.txt";
    StreamType type = rs_fisheye;
    int width = g_pRSDevice->get_stream_width(type);
    int height = g_pRSDevice->get_stream_height(type);

    if ((!mFisheyeIsRecording) && mRecordFile)
    {
        mFisheyeIsRecording = true;
        fisheyeof.open(fisheyefile);
        fisheye_cnt = 1;
    }

    // generate fisheye pgm file
    if (mFisheyeIsRecording && mRecordFile)
    {
        double frameTS; // unit is ms
//        frameTS = g_pRSDevice->get_frame_num(rs_fisheye);
        frameTS = g_pRSDevice->get_frame_timestamp(rs_fisheye);

        char feimagefir[50] = "./fisheye/";
#ifdef WINDOWS
        _mkdir(feimagefir);

#else
        mkdir(feimagefir, 77777);
#endif
        char feimagefile[50];
        sprintf(feimagefile, "./fisheye/image_%05d.pgm", fisheye_cnt);

        feimageof.open(feimagefile, std::ios::binary);

        // write header
        char headerbuf[100];
        sprintf(headerbuf, "P5\n%d %d\n255\n", width, height);
        feimageof.write(headerbuf, strlen(headerbuf));

        // write content
        const char *pChunk = reinterpret_cast<const char *>(g_pRSDevice->get_frame_data(type));
        static const int CHUNK_SIZE = 153600;
        int w = width, h = height;
        for (int numChunk = 0; numChunk * CHUNK_SIZE < w*h; numChunk++)
        {
            feimageof.write(&pChunk[0], CHUNK_SIZE);
            pChunk += CHUNK_SIZE;
        }
        feimageof.close();

        // write fisheye timestap file
        char buf[100];
        sprintf(buf, "fisheye/image_%05d.pgm %.5f\n", fisheye_cnt, frameTS);
        int size = strlen (buf);
        fisheyeof.write(buf, size);

        fisheye_cnt ++;
    }

    // close fisheye  timestap file
    if (mFisheyeIsRecording && (!mRecordFile))
    {
        mFisheyeIsRecording = false;
        fisheyeof.close();
    }
}
//#endif
void RSGLWidget::getFrameData( int textType)
{
    int width = 0, height = 0;
    char  msg[100]="null";
    int pos_x=0,pos_y=0;
    bool isshow=false;
    StreamType stream=rs_color;
    switch(textType)
    {
    case 0:
        stream = rs_depth;
        break;
    case 1:
        stream = rs_color;
        break;
    case 2:
        stream = rs_left;
        break;
    case 3:
        stream = rs_right;
        break;
    case 4:
        stream = rs_fisheye;
        break;
    case 6:
        stream = rs_platcam;
        break;
    default:
//        qDebug()<< "qt" << textType;
        break;
    }

    if (stream == rs_platcam)
    {
#ifndef UBT1604
        width = mPlatCamFrameBuf->width();
        height = mPlatCamFrameBuf->height();
#endif
    }
    else
    {
        width = g_pRSDevice->get_stream_width(stream);
        height = g_pRSDevice->get_stream_height(stream);
    }

    if(framedata2[textType]==nullptr)
    {
        framedata2[textType]=new char[width*height*3];
    }

    const void * framedata;

    if (stream == rs_platcam)
    {
#ifndef UBT1604
        if(mPlatCamFrameBuf->map(QAbstractVideoBuffer::ReadOnly))
        {
            NV21ToRGB(mPlatCamFrameBuf->bits(), (uchar*)framedata2[textType], width, height);
            mPlatCamFrameBuf->unmap();
        }
#endif
    }
    else
    {
        framedata = g_pRSDevice->get_frame_data(stream);
    }

    if (textType == 6)
    {
        isshow=ispixview(framedata2[textType],width,height,textType,pos_x,pos_y,msg);
    }
    else if(textType<2 || textType>3)
    {
        isshow=ispixview(framedata,width,height,textType,pos_x,pos_y,msg);
    }
    else if(g_pRSDevice->get_lr_fromat()==8)
    {
        isshow=ispixview(framedata,width,height,textType,pos_x,pos_y,msg,1);
    }
    else if(g_pRSDevice->get_lr_fromat()==16)
    {
        isshow=ispixview(framedata,width,height,textType,pos_x,pos_y,msg,2);
    }

    if(((stream==rs_color && is_new_rgb_frame) || stream!=rs_color) && (stream!=rs_platcam))
    {
        g_pRSDevice->get_frame_data_rgb(stream,framedata2[textType]);
    }

    if(stream==rs_color)
        is_new_rgb_frame=false;

    //draw context msg
    if(isshow)
    {
        drawstring(msg,reinterpret_cast<unsigned char *>(framedata2[textType]),width,height,pos_x+3,pos_y+5,3,0xff000000);
        drawpos(reinterpret_cast<unsigned char *>(framedata2[textType]),width,height,pos_x+3,pos_y+3,0xffff00);
    }
    drawstring(getinfo(textType,width,height,msg),reinterpret_cast<unsigned char *>(framedata2[textType]),width,height,20,20,3,0xff0000);

    ///////bind texture

    textures[textType]->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, framedata2[textType]);
    textures[textType]->bind();

    if (textType == 4)  // save fisheye data
    {
#ifndef MIPI
        save_fisheye_data();
#endif
    }

    // Draw arrays
    if (streamCount == 1)   // singel view
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    else    // muti view
    {
        if (textType == 4)  // fisheye image
        {
            if (!g_stream_depth)    // replace others image
            {
                textType = 0;
                glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
            }
            else if (!g_stream_color)    // replace others image
            {
                textType = 1;
                glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
            }
            else if (!g_stream_left)    // replace others image
            {
                textType = 2;
                glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
            }
            else if (!g_stream_right)    // replace others image
            {
                textType = 3;
                glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
            }
        }
        else if (textType == 6) // platform camera
        {
            if (g_stream_fisheye)
            {
                int showPosFlag = 0;

                if (!g_stream_depth)    // replace others image
                {
                    if (showPosFlag == 1)
                    {
                        textType = 0;
                        glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
                    }
                    showPosFlag++;
                }

                if (!g_stream_color)    // replace others image
                {
                    if (showPosFlag == 1)
                    {
                        textType = 1;
                        glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
                    }
                    showPosFlag++;
                }

                if (!g_stream_left)    // replace others image
                {
                    if (showPosFlag == 1)
                    {
                        textType = 2;
                        glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
                    }
                    showPosFlag++;
                }

                if (!g_stream_right)    // replace others image
                {
                    if (showPosFlag == 1)
                    {
                        textType = 3;
                        glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
                    }
                    showPosFlag++;
                }
            }
            else
            {
                if (!g_stream_depth)    // replace others image
                {
                    textType = 0;
                    glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
                }
                else if (!g_stream_color)    // replace others image
                {
                    textType = 1;
                    glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
                }
                else if (!g_stream_left)    // replace others image
                {
                    textType = 2;
                    glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
                }
                else if (!g_stream_right)    // replace others image
                {
                    textType = 3;
                    glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
                }
            }
        }
        else
            glDrawArrays(GL_TRIANGLE_FAN, textType*4, 4);
    }
}

bool RSGLWidget::ispixview(const void * framedata,int width,int height,int textType,int &pos_x,int &pos_y,char * msg,int format)
{

    int textpos;
    int mousepos=0;
    if(g_pixelview)
    {
        if(streamCount==1)//get pos
        {
            pos_x = x * height / g_widget_h;
            pos_y = y * width / g_widget_w;
        }
        else//get pos
        {
            if(x>g_widget_h/2 && y<g_widget_w/2)//mouse pos
                mousepos=0;
            if(x>g_widget_h/2 && y>g_widget_w/2)
                mousepos=1;
            if(x<g_widget_h/2 && y<g_widget_w/2)
                mousepos=2;
            if(x<g_widget_h/2 && y>g_widget_w/2)
                mousepos=3;
            textpos=textType;
            if (textType == 4)  // fisheye image
            {
                if (!g_stream_depth)    // replace others image
                {
                    textpos = 0;
                }
                else if (!g_stream_color)    // replace others image
                {
                    textpos = 1;
                }
                else if (!g_stream_left)    // replace others image
                {
                    textpos = 2;
                }
                else if (!g_stream_right)    // replace others image
                {
                    textpos = 3;
                }
            }
            else if (textType == 6) // platform camera
            {
                if (g_stream_fisheye)
                {
                    int showPosFlag = 0;

                    if (!g_stream_depth)    // replace others image
                    {
                        if (showPosFlag == 1)
                        {
                            textpos = 0;
                        }
                        showPosFlag++;
                    }

                    if (!g_stream_color)    // replace others image
                    {
                        if (showPosFlag == 1)
                        {
                            textpos = 1;
                        }
                        showPosFlag++;
                    }

                    if (!g_stream_left)    // replace others image
                    {
                        if (showPosFlag == 1)
                        {
                            textpos = 2;
                        }
                        showPosFlag++;
                    }

                    if (!g_stream_right)    // replace others image
                    {
                        if (showPosFlag == 1)
                        {
                            textpos = 3;
                        }
                        showPosFlag++;
                    }
                }
                else
                {
                    if (!g_stream_depth)    // replace others image
                    {
                        textpos = 0;
                    }
                    else if (!g_stream_color)    // replace others image
                    {
                        textpos = 1;
                    }
                    else if (!g_stream_left)    // replace others image
                    {
                        textpos = 2;
                    }
                    else if (!g_stream_right)    // replace others image
                    {
                        textpos = 3;
                    }
                }
            }

            if(textpos==mousepos)// mouse and texture at the same area
            {
                switch(textpos)
                {
                case 0:
                    pos_x = (x-g_widget_h / 2) * height *2 /g_widget_h;
                    pos_y = y * width *2 /g_widget_w;
                    break;
                case 1:
                    pos_x = (x-g_widget_h / 2) * height *2 /g_widget_h;
                    pos_y = (y-g_widget_w / 2) * width *2 /g_widget_w;
                    break;
                case 2:
                    pos_x = x * height *2 /g_widget_h;
                    pos_y = y * width *2 /g_widget_w;
                    break;
                case 3:
                    pos_x = x * height *2 /g_widget_h;
                    pos_y = (y-g_widget_w / 2) * width *2 /g_widget_w;
                    break;
                }
            }
        }
        if(textpos==mousepos || streamCount==1 )
        {
            unsigned int temp1,temp2,temp3,temp4,f;
            switch(textType)
            {
            case 0:
                temp1=((reinterpret_cast<const unsigned short*>(framedata))[pos_x*width+pos_y]);
                if (g_device_type == SR300)
                    temp1 = temp1*depth_scale*1000;
                sprintf(msg,"(%d,%d)Depth:%u",pos_x,pos_y,temp1);
                break;
            case 1:
                f=g_pRSDevice->get_color_format();
                if(f==0)
                {
                    temp1=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*3]);
                    temp2=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*3+1]);
                    temp3=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*3+2]);
                    sprintf(msg,"(%d,%d)RGB:(%u,%u,%u)",pos_x,pos_y,temp1,temp2,temp3);
                }
                else if(f==1)
                {
                    temp1=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*4]);
                    temp2=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*4+1]);
                    temp3=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*4+2]);
                    temp4=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*4+3]);
                    sprintf(msg,"(%d,%d)BGRA:(%u,%u,%u,%u)",pos_x,pos_y,temp1,temp2,temp3,temp4);
                }
                else if( f==2)
                {
                    temp1=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*2]);
                    temp2=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*2+1]);
                    temp3=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*2+2]);
                    temp4=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*2+3]);
                    sprintf(msg,"(%d,%d)YUYV:(%u,%u,%u,%u)",pos_x,pos_y,temp1,temp2,temp3,temp4);
                }
                break;
            case 2:
            case 3:
                if(format==2)
                    temp1=((reinterpret_cast<const uint16_t *>(framedata))[pos_x*width+pos_y]);
                else
                    temp1=((reinterpret_cast<const uint8_t *>(framedata))[pos_x*width+pos_y]);
                sprintf(msg,"(%d,%d)Infrared:%u",pos_x,pos_y,temp1);
                break;
            case 4:
                temp1=((reinterpret_cast<const unsigned char*>(framedata))[pos_x*width+pos_y]);
                sprintf(msg,"(%d,%d)Fisheye:%u",pos_x,pos_y,temp1);
                break;
            case 6:
                temp1=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*3]);
                temp2=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*3+1]);
                temp3=((reinterpret_cast<const unsigned char*>(framedata))[(pos_x*width+pos_y)*3+2]);
                sprintf(msg,"(%d,%d)PlatCam:(%u,%u,%u)",pos_x,pos_y,temp1,temp2,temp3);
                break;
            }
            return true;
        }
    }
    return false;
}

void RSGLWidget::drawpos(unsigned char * data,int w,int h,int x,int y,int color)
{
    int r,g,b;
    r=(color>>16)& 0xff;
    g=(color>>8)& 0xff;
    b=color& 0xff;
    double factor_w, factor_h;

    calc_factor(factor_w,factor_h,w,h);

    for(int i=-3*factor_h;i<=3*factor_h;i++)
    {
        for(int j=-factor_w/1.5;j<=factor_w/1.5;j++)
        {
            if(x+i<0 || x+i>h || y+j<0 || y+j>w)
                continue;

            int pos=(x+i)*w+(y+j);
            data[3*pos]=r;
            data[3*pos+1]=g;
            data[3*pos+2]=b;
        }


    }
    for(int j=-3*factor_w;j<=3*factor_w;j++)
    {
        for(int i=-factor_h/1.5;i<=factor_h/1.5;i++)
        {
            if(x+i<0 || x+i>h || y+j<0 || y+j>w)
                continue;

            int pos=(x+i)*w+(y+j);
            data[3*pos]=r;
            data[3*pos+1]=g;
            data[3*pos+2]=b;
        }

    }
}

void RSGLWidget::calc_factor(double & factor_w,double & factor_h,int w,int h)//calculate font size or factor
{

    if(w<350)
        factor_w=1;
    else if(w<500)
        factor_w=1;
    else if(w<700)
        factor_w=1.5;
    else if(w<1000)
        factor_w=3;
    else
        factor_w=4;

    if(streamCount>1)
        factor_w*=1.5;
    factor_w*=640.0/g_widget_w;

    if(h<250)
        factor_h=1;
    else if(h<400)
        factor_h=1;
    else if(h<500)
        factor_h=1.5;
    else if(h<800)
        factor_h=3;
    else
        factor_h=4;

    if(streamCount>1)
        factor_h*=1.5;
    factor_h*=480.0/g_widget_h;


    if(factor_h<1  )
        factor_h=1;
    if(factor_w<1 )
        factor_w=1;

}

void RSGLWidget::initGL()
{
    initializeOpenGLFunctions();

    // count how many streaming enable
    streamCount=howManyImageEnable();

    if (streamCount == 1)   // singel view
        makeObject();
    else    // muti view
        makeObjectMuti();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
}

#include <QFile>
void RSGLWidget::drawstring(char * str,unsigned char * data,int w,int h,int x,int y,int mode,int color)//draw text to image data
{
    //mode:1 : gray,3:rgb else : invoild

    int len=strlen(str);
    int ch_w=8,ch_h=18;
    int r,g,b,inverse;
    int m=0;
    double factor_w, factor_h;

    if(mode!=1 && mode!=3)
        return;

    calc_factor(factor_w,factor_h,w,h);

    if(!mapinitflag)
    {
        QFile qfile(":/prefix1/Images/font18.raw");
        qfile.open(QFile::OpenModeFlag::ReadOnly);

        for(int i=0;i<ch_h;i++)
        {
            qfile.read(reinterpret_cast<char *>(textmap[i]),ch_w*95);
        }
        mapinitflag=true;
    }


    if(mode==3)//rgb
    {
        inverse=(color>>24)& 0xff;
        r=(color>>16)& 0xff;
        g=(color>>8)& 0xff;
        b=color& 0xff;
    }

    for(int n=0;n<len;n++)
    {
        if(str[n]=='\n')
        {
            m=1;//flag
            break;
        }
    }

    if(m==0 && x+ch_h*factor_h>h)
    {
        x=h-ch_h*factor_h;
    }
    if(m==0 && y+len*ch_w*factor_w>w)
    {
        y=w-len*ch_w*factor_w;
    }

    m=0;//num
    for(int n=0;n<len;n++,m++)
    {
        if(str[n]=='\n')
        {
            x+=ch_h*factor_h+4;
            m=-1;
            continue;
        }
        int index=str[n]-32;
        for(int i=0;i<ch_h*factor_h;i++)
        {
            for(int j=0;j<ch_w*factor_w;j++)
            {
                if(x+i>h || y+j+m*ch_w>w)
                    continue;

                int pos=(x+i)*w+(y+j+m*ch_w*factor_w);//mode=0
                if(textmap[(int)(i/factor_h)][(int)(index*ch_w+j/factor_w)]<10)
                {
                    if(mode==1)
                        data[pos]=0xff;//black
                    else
                    {
                        if(inverse!=0)
                        {
                            data[3*pos]=255-data[3*pos];
                            data[3*pos+1]=255- data[3*pos+1];
                            data[3*pos+2]= 255-data[3*pos+2];
                        }
                        else
                        {
                            data[3*pos]=r;
                            data[3*pos+1]=g;
                            data[3*pos+2]=b;
                        }
                    }
                }

            }
        }
    }


}

void RSGLWidget::bindShader()
{
    mshaderprog->bindAttributeLocation("vertex", SHADER_PROG_VERTEX);
    mshaderprog->bindAttributeLocation("texCoord", SHADER_PROG_TEXCOORD);
    mshaderprog->link();

    mshaderprog->bind();
    mshaderprog->setUniformValue("texture", 0);
}

char * RSGLWidget::getinfo(int texttype,int w,int h,char *str)//get msg for draw at left-up
{
    if(str==nullptr)
        return nullptr;

    else if(texttype>=0 && texttype<=3)
    {
        char streamname[4][10]={"Depth","Color","Infrared","Infrared2"};
        const char * color_format[]={"RGB8","BGRA8","YUY2"};

        if(texttype==1)
            sprintf(str,"%s:%dfps\n%dX%d\n%s" , streamname[texttype], g_third_fps, w, h,color_format[g_pRSDevice->get_color_format()]);
        else
            sprintf(str,"%s:%dfps\n%dX%d" , streamname[texttype], g_zlr_fps, w, h);

    }
    else if(texttype==4)
    {
        double frameTS=0; // unit is ms
        frameTS = g_pRSDevice->get_frame_timestamp(rs_fisheye);
        sprintf(str, " Fisheye:%dfps\n %.0f ms", g_fisheye_fps, frameTS);
    }
    else if(texttype==6)
    {
        //fps
//        sprintf(str, "PlatCam:%dfps", 0);
        sprintf(str, "PlatCam", 0);
    }

    return str;
}

void RSGLWidget::initShader()
{

    QOpenGLShader *v_sd = new QOpenGLShader(QOpenGLShader::Vertex, this);

    const char *v_source =
                "uniform  mediump  mat4  matrix;\nvarying  mediump  vec4  texc;\n"
                "attribute  mediump  vec4  texCoord;\nattribute  highp  vec4  vertex;\n"
                "void  main( void )\n"
                "{\n    texc  =  texCoord; \n"
                "    gl_Position  =  matrix  *  vertex;\n}\n";

    v_sd->compileSourceCode(v_source);


    QOpenGLShader *f_sd = new QOpenGLShader(QOpenGLShader::Fragment, this);

    const char *f_source =
                "varying  mediump  vec4  texc;\nuniform  sampler2D  texture;\n"
                "void  main( void )\n{\n    gl_FragColor  =  texture2D( texture,  texc.st );\n}\n";

    f_sd->compileSourceCode(f_source);

    mshaderprog = new QOpenGLShaderProgram;
    mshaderprog->addShader(v_sd);
    mshaderprog->addShader(f_sd);
}

void RSGLWidget::set_func(std::function<void(int)> process)
{
    update_ui_framenum=process;
}
