#!/usr/bin/env python
# Martin Kersner, m.kersner@gmail.com
# 2016/01/18 

import numpy as np

def pascal_classes():
  classes = {'background' : 0 , 'person' : 1 }

  return classes

def pascal_palette():
  palette = {(  0,   0,   0) : 0} 
              

  return palette

def palette_demo():
  palette_list = pascal_palette().keys()
  palette = ()
  
  for color in palette_list:
    palette += color

  return palette

def convert_from_color_segmentation(arr_3d):
  arr_2d = np.zeros((arr_3d.shape[0], arr_3d.shape[1]), dtype=np.uint8)
  palette = pascal_palette()

  # slow!
  for i in range(0, arr_3d.shape[0]):
    for j in range(0, arr_3d.shape[1]):
      key = (arr_3d[i,j,0], arr_3d[i,j,1], arr_3d[i,j,2])
      arr_2d[i, j] = palette.get(key, 1) # default value if key was not found is 0
  return arr_2d

def get_id_classes(classes):
  all_classes = pascal_classes()
  id_classes = [all_classes[c] for c in classes]
  return id_classes

def strstr(str1, str2):
  if str1.find(str2) != -1:
    return True
  else:
    return False

def create_lut(class_ids, max_id=256):
  # Index 0 is the first index used in caffe for denoting labels.
  # Therefore, index 0 is considered as default.
  lut = np.zeros(max_id, dtype=np.uint8)

  new_index = 1
  for i in class_ids:
    lut[i] = new_index
    new_index += 1

  return lut
