The volume sensor uses YOLO to identify people in its view.

To use it, please visit YOLO's site and download the training set of yolov3, named yolov3.weights. We didnt upload it to git beacuse its a big file.

Please also make sure to install the following:

pip install numpy

pip install argparse

pip install cv2

pip install firebase_admin

pip install urlib

To run the code, use: python rsp.py --dir/-d <path_to_volumeSensor_dir> [-c confidence] [-t threshold]  
