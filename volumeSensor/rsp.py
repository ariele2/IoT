import firebase_admin
from firebase_admin import db, credentials, initialize_app
import numpy as np
import argparse
import time
import cv2
import os
import threading
import queue
import time
import datetime
import ntplib
from time import ctime
from urllib.request import urlopen



def get_input(message, channel):
    response = input(message)
    channel.put(response)


def input_with_timeout(message, timeout):
    channel = queue.Queue()
    message = message + " [{} sec timeout] ".format(timeout)
    thread = threading.Thread(target=get_input, args=(message, channel))
    # by setting this as a daemon thread, python won't wait for it to complete
    thread.daemon = True
    thread.start()

    try:
        response = channel.get(True, timeout)
        return response
    except queue.Empty:
        pass
    return None
	
debug_Mode = True
time_between_frame = 5
exit_command = False
time_to_exit = 5
if exit_command:
	command = input_with_timeout("Commands:", time_to_exit)
	time.sleep(time_to_exit)
	if( command == 'exit'):
		print (command)
		exit(0)
	print (command)


def get_current_time():
	res = urlopen('http://just-the-time.appspot.com/')
	result = res.read().strip()
	result_str = result.decode('utf-8')
	day = result_str[8:10]
	month = result_str[5:7]
	year = result_str[2:4]
	hour = datetime.datetime.strptime(result_str[11:], "%H:%M:%S")
	hour += datetime.timedelta(hours=3)
	hour = hour.strftime("%H:%M:%S")
	to_ret = day + "-" + month + "-" + year + " " + hour
	return to_ret


def set_system_time():
    res = urlopen('http://just-the-time.appspot.com/')
    result = res.read().strip()
    result_str = result.decode('utf-8')
    day = result_str[8:10]
    month = result_str[5:7]
    year = result_str[2:4]
    times = datetime.datetime.strptime(result_str[11:], "%H:%M:%S")
    times += datetime.timedelta(hours=3)
    times = times.strftime("%H:%M:%S")
    os.system("sudo date -s \'" + year + "-" + month + "-" + day + " " + times + "\'") 


def checkScheduler(curr_time):
	scheduler_ref = db.reference("/scheduler")
	scheduler_data = scheduler_ref.get()
	print(f"scheduler_data: {scheduler_data}")
	if scheduler_data:
		s, e = list(scheduler_data[0].items())[0]
		s_p = datetime.datetime.strptime(s, "%d-%m-%y %H:%M:%S")
		e_p = datetime.datetime.strptime(e, "%d-%m-%y %H:%M:%S")
		if curr_time > e_p:
			scheduler_data.pop(0)
			scheduler_ref.set(scheduler_data)
			if action_ref.get() != 'off':
				action_ref.set("off")
				print(f"Turning system off!")
		elif curr_time > s_p and action_ref.get() != 'on':
			action_ref.set("on")
			print(f"Turning system on!")


cred_obj = firebase_admin.credentials.Certificate("iotprojdb-firebase-adminsdk-q9c5k-113a48d6a7.json")
firebase_path = 'https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/'
default_app = firebase_admin.initialize_app(cred_obj, {'databaseURL':firebase_path})


ap = argparse.ArgumentParser()
ap.add_argument("-i", "--image", required=False,
	help="path to input image")
ap.add_argument("-y", "--yolo", required=True,
	help="base path to YOLO directory")
ap.add_argument("-c", "--confidence", type=float, default=0.5,
	help="minimum probability to filter weak detections")
ap.add_argument("-t", "--threshold", type=float, default=0.3,
	help="threshold when applying non-maxima suppression")
args = vars(ap.parse_args())

# load the COCO class labels our YOLO model was trained on
labelsPath = os.path.sep.join([args["yolo"], "coco.names"])
LABELS = open(labelsPath).read().strip().split("\n")
# initialize a list of colors to represent each possible class label
np.random.seed(42)
COLORS = np.random.randint(0, 255, size=(len(LABELS), 3),
	dtype="uint8")

if debug_Mode:
	print("[DEBUG]: COLORS = ", COLORS)

# derive the paths to the YOLO weights and model configuration
weightsPath = os.path.sep.join([args["yolo"], "yolov3.weights"])
configPath = os.path.sep.join([args["yolo"], "yolov3.cfg"])
# load our YOLO object detector trained on COCO dataset (80 classes)
if debug_Mode:
	print("[INFO] loading YOLO from disk...")
net = cv2.dnn.readNetFromDarknet(configPath, weightsPath)

# initialize camera
set_system_time()
(W,H) = (None, None)
prev_time = 0
action_ref = db.reference('/action')
data_ref = db.reference('/data')
real_data_ref = db.reference('/real_data')
caller_ref = db.reference('/call_id')
sensorID = "V-01"
# make a while loop that works every 30 secs
while(True):
	curr_machine_time = time.time()
	curr_time = datetime.datetime.strptime(get_current_time(), "%d-%m-%y %H:%M:%S")
	# validate that the system is on
	action_data = action_ref.get()

	checkScheduler(curr_time)

	while action_data == 'off':
		curr_time = datetime.datetime.strptime(get_current_time(), "%d-%m-%y %H:%M:%S")
		checkScheduler(curr_time)
		time.sleep(5)
		action_data = action_ref.get()
		print('System is off!')

	if curr_machine_time - prev_time > time_between_frame:
		cam = cv2.VideoCapture(0)
		res, image = cam.read()
		if not res:	#didn't capture an image
			print("There is an error with the camera - couldn't capture images")
			break
		# load our input image and grab its spatial dimensions
		if W is None or H is None:
			(H, W) = image.shape[:2]

		# determine only the *output* layer names that we need from YOLO
		ln = net.getLayerNames()
		ln = [ln[i - 1] for i in net.getUnconnectedOutLayers()]
		# construct a blob from the input image and then perform a forward
		# pass of the YOLO object detector, giving us our bounding boxes and
		# associated probabilities
		blob = cv2.dnn.blobFromImage(image, 1 / 255.0, (256, 256),	# changed from 416 to 256 reduces time *2
			swapRB=True, crop=False)
		net.setInput(blob)
		start = time.time()
		layerOutputs = net.forward(ln)
		end = time.time()
		# show timing information on YOLO
		if debug_Mode:
			print("[INFO] YOLO took {:.6f} seconds".format(end - start))

		# initialize our lists of detected bounding boxes, confidences, and
		# class IDs, respectively
		boxes = []
		confidences = []
		classIDs = []

		# loop over each of the layer outputs
		for output in layerOutputs:
			# loop over each of the detections
			for detection in output:
				# extract the class ID and confidence (i.e., probability) of
				# the current object detection
				scores = detection[5:]
				classID = np.argmax(scores)
				confidence = scores[classID]
				# filter out weak predictions by ensuring the detected
				# probability is greater than the minimum probability
				if confidence > args["confidence"]:
					# scale the bounding box coordinates back relative to the
					# size of the image, keeping in mind that YOLO actually
					# returns the center (x, y)-coordinates of the bounding
					# box followed by the boxes' width and height
					box = detection[0:4] * np.array([W, H, W, H])
					(centerX, centerY, width, height) = box.astype("int")
					# use the center (x, y)-coordinates to derive the top and
					# and left corner of the bounding box
					x = int(centerX - (width / 2))
					y = int(centerY - (height / 2))
					# update our list of bounding box coordinates, confidences,
					# and class IDs
					boxes.append([x, y, int(width), int(height)])
					confidences.append(float(confidence))
					classIDs.append(classID)
		if debug_Mode:
			print("[DEBUG] Finished for output in layerOutputs")
		# apply non-maxima suppression to suppress weak, overlapping bounding
		# boxes
		idxs = cv2.dnn.NMSBoxes(boxes, confidences, args["confidence"],
			args["threshold"])

		count_error = 0
		# ensure at least one detection exists
		if len(idxs) > 0:
			# loop over the indexes we are keeping 
			if debug_Mode:
				print("[DEBUG] len(idxs) = ", len(idxs))
				print("[DEBUG] ClassIDs = ", classIDs)
			for i in idxs.flatten():
				# extract the bounding box coordinates
				(x, y) = (boxes[i][0], boxes[i][1])
				(w, h) = (boxes[i][2], boxes[i][3])
				# draw a bounding box rectangle and label on the image
				if debug_Mode:
					print("[DEBUG] ClassID = ", classID)
				if classIDs[i] <= 2:
					color = [int(c) for c in COLORS[classIDs[i]]]
					cv2.rectangle(image, (x, y), (x + w, y + h), color, 2)
					text = "{}: {:.4f}".format(LABELS[classIDs[i]], confidences[i])
					cv2.putText(image, text, (x, y - 5), cv2.FONT_HERSHEY_SIMPLEX,
						0.5, color, 2)

					if confidences[i] > 0.9 and debug_Mode:
						print(confidences[i])
				else:
					count_error = count_error + 1
		prev_time = curr_machine_time
		# show the output image
		num_of_pepole = len(idxs) - count_error 
		print("Found ", num_of_pepole , "People")
		time_str = get_current_time()  
		print("time_str: ", time_str)
		call_id = caller_ref.get()[sensorID]
		insert_data = {time_str + " " + sensorID: {"value":str(num_of_pepole), "callID":str(call_id)}}
		insert_real_data = {sensorID:{"value":str(num_of_pepole), "callID":str(call_id), "time":time_str}}
		data_ref.update(insert_data) 
		caller_ref.update({sensorID:call_id+1})
		real_data_ref.update(insert_real_data)
		if debug_Mode:
			cv2.imshow("Image"+str(curr_machine_time), image)
			cv2.waitKey(2000)
			cv2.destroyAllWindows()
		cam.release()
