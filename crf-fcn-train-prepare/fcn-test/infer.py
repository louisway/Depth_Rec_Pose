import numpy as np
from PIL import Image
from scipy import misc
import cv2

import caffe
def tic():
    import time
    global startTime_for_tictoc
    startTime_for_tictoc = time.time()
def toc():
    import time
    if 'startTime_for_tictoc' in globals():
        print "Elapsed time is " + str(time.time() - startTime_for_tictoc) + " seconds."
    else:
        print "Toc:start time not set"
 
def getpallete(num_class):
    n =num_class
    pallete = [0]*(n*3)
    for j in xrange(0,n):
        lab = j
        pallete[j*3+0] = 0
        pallete[j*3+1] = 0
        pallete[j*3+1] = 0
        i = 0
        while(lab > 0):
            pallete[j*3+0] |= (((lab >> 0) & 1) << (7-i))
            pallete[j*3+1] |= (((lab >> 1) & 1) << (7-i))

            pallete[j*3+2] |= (((lab >> 2) & 1) << (7-i))
            i = i + 1
            lab >>= 3
    return pallete 
# load image, switch to BGR, subtract mean, and make dims C x H x W for Caffe
# load image, switch to BGR, subtract mean, and make dims C x H x W for Caffe
pic_name = '44'
#img_gray = Image.open(pic_name + '_gray.png')
im = misc.imread(pic_name+'_depth.png')
in_ = np.array(im, dtype=np.int16)
in_ = np.expand_dims(in_ , axis = 0)
# load net
net = caffe.Net('deploy.prototxt', 'fcn.caffemodel', caffe.TEST)
# shape for input (data blob is N x C x H x W), set data
net.blobs['data'].reshape(1, *in_.shape)
net.blobs['data'].data[...] = in_
# run net and take argmax for prediction
tic()
out = net.forward()
toc()
predictions = out[net.outputs[0]] 
segmentation = predictions[0].argmax(axis = 0).astype(np.uint8)
#out = net.forward_all(**{net.inputs[0]:caffe_in})
#out = net.blobs['score'].data[0].argmax(axis=0)
print segmentation.shape
output_im = Image.fromarray(segmentation)
pallete = getpallete(256)
output_im.putpalette(pallete)
#img_gray.paste(output_im,(0,0))
outputname = 'output_' + pic_name + '.png';
output_im.save(outputname)
#img_gray.save('mask' + outputname)
print im.size
