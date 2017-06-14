# -*- coding: utf-8 -*-
"""
This package contains code for the "CRF-RNN" semantic image segmentation method, published in the
ICCV 2015 paper Conditional Random Fields as Recurrent Neural Networks. Our software is built on
top of the Caffe deep learning library.

Contact:
Shuai Zheng (szheng@robots.ox.ac.uk), Sadeep Jayasumana (sadeep@robots.ox.ac.uk), Bernardino Romera-Paredes (bernard@robots.ox.ac.uk)

Supervisor:
Philip Torr (philip.torr@eng.ox.ac.uk)

For more information about CRF-RNN, please vist the project website http://crfasrnn.torr.vision.
"""

caffe_root = '../caffe/'
import sys, getopt
sys.path.insert(0, caffe_root + 'python')

import os
import cPickle
import logging
import numpy as np
import pandas as pd
from PIL import Image as PILImage
#import Image
import cStringIO as StringIO
import caffe
import matplotlib.pyplot as plt


def tic():
    #http://stackoverflow.com/questions/5849800/tic-toc-functions-analog-in-python
    #Homemade version of matlab tic and toc functions
    import time
    global startTime_for_tictoc
    startTime_for_tictoc = time.time()

def toc():
    import time
    if 'startTime_for_tictoc' in globals():
        print "Elapsed time is " + str(time.time() - startTime_for_tictoc) + " seconds."
    else:
        print "Toc: start time not set"


def getpallete(num_cls):
        # this function is to get the colormap for visualizing the segmentation mask
        n = num_cls
        pallete = [0]*(n*3)
        for j in xrange(0,n):
                lab = j
                pallete[j*3+0] = 0
                pallete[j*3+1] = 0
                pallete[j*3+2] = 0
                i = 0
                while (lab > 0):
                        pallete[j*3+0] |= (((lab >> 0) & 1) << (7-i))
                        pallete[j*3+1] |= (((lab >> 1) & 1) << (7-i))
                        pallete[j*3+2] |= (((lab >> 2) & 1) << (7-i))
                        i = i + 1
                        lab >>= 3
        return pallete

def crfasrnn_segmenter(model_file, pretrained_file, gpudevice, inputs):
    if gpudevice >= 0:
        #Do you have GPU device? NO GPU is -1!
        has_gpu = 1
        #which gpu device is available?
        gpu_device=gpudevice#assume the first gpu device is available, e.g. Titan X
    else:
        has_gpu = 0
    if has_gpu==1:
        caffe.set_device(gpu_device)
        caffe.set_mode_gpu()
    else:
        caffe.set_mode_cpu()


    net = caffe.Net(model_file, pretrained_file, caffe.TEST)

    input_ = np.zeros((len(inputs),
        500, 500, inputs[0].shape[2]),
        dtype=np.float32)
    for ix, in_ in enumerate(inputs):
        input_[ix] = in_

    # Segment
    caffe_in = np.zeros(np.array(input_.shape)[[0,3,1,2]],
                        dtype=np.float32)
    for ix, in_ in enumerate(input_):
        caffe_in[ix] = in_.transpose((2, 0, 1))
    tic()
    out = net.forward_all(**{net.inputs[0]: caffe_in})
    toc()
    predictions = out[net.outputs[0]]

    return predictions[0].argmax(axis=0).astype(np.uint8)

def run_crfasrnn(inputfile, outputfile, gpudevice):
    MODEL_FILE = 'TVG_CRFRNN_new_deploy.prototxt'
    PRETRAINED = 'TVG_CRFRNN_COCO_VOC.caffemodel'
    IMAGE_FILE = 'final.png'

    input_image = 255 * caffe.io.load_image(IMAGE_FILE)
    input_image = resizeImage(input_image)

    width = input_image.shape[0]
    height = input_image.shape[1]
    maxDim = max(width,height)

    image = PILImage.fromarray(np.uint8(input_image))
    image = np.array(image)

    pallete = getpallete(256)

    mean_vec = np.array([103.939, 116.779, 123.68], dtype=np.float32)
    reshaped_mean_vec = mean_vec.reshape(1, 1, 3)

    # Rearrange channels to form BGR
    im = image[:,:,::-1]
    # Subtract mean
    #im = im - reshaped_mean_vec

    # Pad as necessary
    cur_h, cur_w, cur_c = im.shape
    pad_h = 250 - cur_h
    pad_w = 250 - cur_w
    im = np.pad(im, pad_width=((0, pad_h), (0, pad_w), (0, 0)), mode = 'constant', constant_values = 0)
    # Get predictions
    #segmentation = net.predict([im])
    segmentation  = crfasrnn_segmenter(MODEL_FILE,PRETRAINED,gpudevice,[im])

    segmentation2 = segmentation[0:cur_h, 0:cur_w]
    output_im = PILImage.fromarray(segmentation2)
    output_im.putpalette(pallete)
    output_im.save(outputfile)

def modify(inputfile, outputfile, gpudevice, scala = 250):
    MODEL_FILE = 'deploy.prototxt'
    PRETRAINED = 'fcn.caffemodel'
    IMAGE_FILE = 'input1.jpg'
    #IMAGE_FILE = 'final.png'

    input_image = 255 * caffe.io.load_image(IMAGE_FILE)
    print type(input_image)
    o_maxDim = max(input_image.shape[0] , input_image.shape[1])
    print o_maxDim 
    input_image = resizeImage(input_image, scala)
     
    #test bin
    #file_name = 'test.bin'
    #image = open(file_name, 'rb')
    #rawdata = image.read()
    #imgSize = (640, 480)
    #mg = PILImage.frombytes('RGB', imgSize , rawdata)
    #b , g , r = mg.split()
    #mg = PILImage.merge("RGB" , (r , g , b))
    #input_image.show()
    #input_image = np.asarray(mg) 
    #o_maxDim = max(input_image.shape[0] , input_image.shape[1])
    #test bin 
    #input_image = resizeImage(input_image, scala)

    width = input_image.shape[0]
    height = input_image.shape[1]
    maxDim = max(width,height)
    
    image = PILImage.fromarray(np.uint8(input_image))
    image = np.array(image)

    pallete = getpallete(256)
    mean_vec = np.array([103.939,116.779,123.68], dtype = np.float32)
    reshaped_mean_vec = mean_vec.reshape(1 , 1 , 3)
   
    #im = image[:,:,::-1]
    im = image[:,:,:]
    #im = im - reshaped_mean_vec

    cur_h, cur_w, cur_c = im.shape
    pad_h = scala - cur_h
    pad_w = scala - cur_w
    im = np.pad(im, pad_width = ((0, pad_h), (0, pad_w), (0, 0)),mode = 'constant', constant_values = 0)
    test_image = PILImage.fromarray(im)
    test_image.show()
    if gpudevice >= 0:
        has_gpu = 1
        gpu_device = gpudevice
    else:
        has_gpu = 0
    if has_gpu == 1:
        caffe.set_device(gpu_device)
        caffe.set_mode_cpu()
    else:
        caffe.set_mode_cpu()
    inputs = [im]
    #tic()
    net = caffe.Net(MODEL_FILE, PRETRAINED,caffe.TEST)
    print "Time to load:"
    #toc()
    input_ = np.zeros((len(inputs), scala, scala, inputs[0].shape[2]),dtype = np.float32)

    for ix, in_ in enumerate(inputs):
        input_[ix] = in_
 
    caffe_in = np.zeros(np.array(input_.shape)[[0,3,1,2]],dtype = np.float32)
    for ix, in_ in enumerate(input_):
        caffe_in[ix] = in_.transpose((2 , 0 , 1))
    #tic()
    out = net.forward_all(**{net.inputs[0]:caffe_in})
    print "time to process one picture:" 
    #toc()
    predictions = out[net.outputs[0]]
    print "predictions.shape"
    print predictions.shape
    #tic()
    segmentation = predictions[0].argmax(axis = 0)
    segmentation = segmentation.astype(np.uint8)
    #toc()
    print segmentation.shape
    segmentation2 = segmentation[0:cur_h, 0:cur_w]
    segmentation2 = resizeImage(segmentation2 , o_maxDim) 
    output_im = PILImage.fromarray(segmentation2)
    output_im.putpalette(pallete)
    output_im.show()
    import time
    output_im.save(outputfile) 
     


def resizeImage(rz_image,rz_scala = 250):
        rz_scala = rz_scala*1.0
        rz_width = rz_image.shape[0]
        rz_height = rz_image.shape[1]
        print rz_width, rz_height
        #rz_maxDim = max(rz_width,rz_height)
        #print rz_maxDim 
        #if maxDim>scala:
        if rz_height > rz_width:
            rz_ratio = float(rz_scala/rz_height)
        else:
            rz_ratio = rz_float(rz_scala/rz_width)
        print rz_ratio
        rz_image = PILImage.fromarray(np.uint8(rz_image))
        rz_image = rz_image.resize((int(rz_height * rz_ratio), int(rz_width * rz_ratio)),resample=PILImage.BILINEAR)
        rz_image = np.array(rz_image)
        return rz_image

def main(argv):
   inputfile = '/home/input.png'
   outputfile = 'output.png'
   gpu_device = -1 # use -1 to run only on CPU, use 0-3[7] to run on GPU
   try:
      opts, args = getopt.getopt(argv,'hi:o:g:',["ifile=","ofile=","gpu="])
   except getopt.GetoptError:
      print 'crfasrnn_demo.py -i <inputfile> -o <outputfile> -g <gpu_device>'
      sys.exit(2)
   for opt, arg in opts:
      if opt == '-h':
         print 'crfasrnn_demo.py -i <inputfile> -o <outputfile> -g <gpu_device>'
         sys.exit()
      elif opt in ("-i", "ifile"):
         inputfile = arg
      elif opt in ("-o", "ofile"):
         outputfile = arg
      elif opt in ("-g", "gpudevice"):
         gpu_device = int(arg)
   print 'Input file is "', inputfile
   print 'Output file is "', outputfile
   print 'GPU_DEVICE is "', gpu_device
   #run_crfasrnn(inputfile,outputfile,gpu_device)
   tic() 
   modify(inputfile , outputfile , gpu_device , 500)
   toc()


if __name__ == "__main__":
    main(sys.argv[1:])
