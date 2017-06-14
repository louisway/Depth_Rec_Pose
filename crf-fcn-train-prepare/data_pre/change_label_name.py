import os
import numpy as np

files = os.listdir("./labels")

for filename in files:
    new_name = "./labels/img_" + filename.split("_")[1] + ".png"
    os.rename("./labels/" + filename,new_name)
