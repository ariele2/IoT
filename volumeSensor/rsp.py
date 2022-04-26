# import firebase_admin
# from firebase_admin import db, credentials, initialize_app
  
# import numpy as np
# import argparse
# import time
import cv2
# import os

vid = cv2.VideoCapture(0)

while(True):
	ret, frame = vid.read()
	cv2.imshow("frame", frame)
	if cv2.waitKey(1) and 0xFF == ord('q'):
		break

vid.release()
cv2.destroyAllWindows()

# cred_obj = firebase_admin.credentials.Certificate("iotprojdb-firebase-adminsdk-q9c5k-113a48d6a7.json")
# firebase_path = 'https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/'
# default_app = firebase_admin.initialize_app(cred_obj, {'databaseURL':firebase_path})
# ref = db.reference("/test/volume")

# ap = argparse.ArgumentParser()
# ap.add_argument("-i", "--image", required=False,
# 	help="path to input image")
# ap.add_argument("-y", "--yolo", required=True,
# 	help="base path to YOLO directory")
# ap.add_argument("-c", "--confidence", type=float, default=0.5,
# 	help="minimum probability to filter weak detections")
# ap.add_argument("-t", "--threshold", type=float, default=0.3,
# 	help="threshold when applying non-maxima suppression")
# args = vars(ap.parse_args())

# # load the COCO class labels our YOLO model was trained on
# labelsPath = os.path.sep.join([args["yolo"], "coco.names"])
# LABELS = open(labelsPath).read().strip().split("\n")
# # initialize a list of colors to represent each possible class label
# np.random.seed(42)
# # COLORS = np.random.randint(0, 255, size=(len(LABELS), 3),
# # 	dtype="uint8")

# # print("[DEBUG]: COLORS = ", COLORS)

# # derive the paths to the YOLO weights and model configuration
# weightsPath = os.path.sep.join([args["yolo"], "yolov3.weights"])
# configPath = os.path.sep.join([args["yolo"], "yolov3.cfg"])
# # load our YOLO object detector trained on COCO dataset (80 classes)
# print("[INFO] loading YOLO from disk...")
# net = cv2.dnn.readNetFromDarknet(configPath, weightsPath)

# # initialize camera
# cam = cv2.VideoCapture(0)
# fps = cam.get(cv2.CAP_PROP_FPS) 
# frame_count = cam.get(cv2.CAP_PROP_FRAME_COUNT)
# frame_number = 0
# cam.set(cv2.CAP_PROP_POS_FRAMES, frame_number)
# print("[DEBUG] FPS = ", fps, ", frame_count = ", frame_count)
# (W,H) = (None, None)
# prev_time = 0
# # make a while loop that works every 30 secs
# while(True):
# 	curr_time = time.time()
# 	# res, image = cam.read()
# 	# cv2.imshow("Image", image)
# 	if curr_time - prev_time > 20:
# 		res, image = cam.read()
# 		if not res:	#didn't capture an image
# 			print("There is an error with the camera - couldn't capture images")
# 			break
# 		# load our input image and grab its spatial dimensions
# 		if W is None or H is None:
# 			(H, W) = image.shape[:2]

# 		# determine only the *output* layer names that we need from YOLO
# 		ln = net.getLayerNames()
# 		ln = [ln[i - 1] for i in net.getUnconnectedOutLayers()]
# 		# construct a blob from the input image and then perform a forward
# 		# pass of the YOLO object detector, giving us our bounding boxes and
# 		# associated probabilities
# 		blob = cv2.dnn.blobFromImage(image, 1 / 255.0, (416, 416),
# 			swapRB=True, crop=False)
# 		net.setInput(blob)
# 		start = time.time()
# 		layerOutputs = net.forward(ln)
# 		end = time.time()
# 		# show timing information on YOLO
# 		print("[INFO] YOLO took {:.6f} seconds".format(end - start))

# 		# initialize our lists of detected bounding boxes, confidences, and
# 		# class IDs, respectively
# 		boxes = []
# 		confidences = []
# 		classIDs = []

# 		# loop over each of the layer outputs
# 		for output in layerOutputs:
# 			# loop over each of the detections
# 			for detection in output:
# 				# extract the class ID and confidence (i.e., probability) of
# 				# the current object detection
# 				scores = detection[5:]
# 				classID = np.argmax(scores)
# 				confidence = scores[classID]
# 				# filter out weak predictions by ensuring the detected
# 				# probability is greater than the minimum probability
# 				if confidence > args["confidence"]:
# 					# scale the bounding box coordinates back relative to the
# 					# size of the image, keeping in mind that YOLO actually
# 					# returns the center (x, y)-coordinates of the bounding
# 					# box followed by the boxes' width and height
# 					box = detection[0:4] * np.array([W, H, W, H])
# 					(centerX, centerY, width, height) = box.astype("int")
# 					# use the center (x, y)-coordinates to derive the top and
# 					# and left corner of the bounding box
# 					x = int(centerX - (width / 2))
# 					y = int(centerY - (height / 2))
# 					# update our list of bounding box coordinates, confidences,
# 					# and class IDs
# 					boxes.append([x, y, int(width), int(height)])
# 					confidences.append(float(confidence))
# 					classIDs.append(classID)
# 		print("[DEBUG] Finished for output in layerOutputs")
# 		# apply non-maxima suppression to suppress weak, overlapping bounding
# 		# boxes
# 		idxs = cv2.dnn.NMSBoxes(boxes, confidences, args["confidence"],
# 			args["threshold"])

		
# 		# # ensure at least one detection exists
# 		# if len(idxs) > 0:
# 		# 	# loop over the indexes we are keeping
# 		# 	print("[DEBUG] len(idxs) = ", len(idxs))
# 		# 	print("[DEBUG] ClassIDs = ", classIDs)
# 		# 	for i in idxs.flatten():
# 		# 		# extract the bounding box coordinates
# 		# 		(x, y) = (boxes[i][0], boxes[i][1])
# 		# 		(w, h) = (boxes[i][2], boxes[i][3])
# 		# 		# draw a bounding box rectangle and label on the image
# 		# 		print("[DEBUG] ClassID = ", classID)
# 		# 		color = [int(c) for c in COLORS[classIDs[i]]]
# 		# 		cv2.rectangle(image, (x, y), (x + w, y + h), color, 2)
# 		# 		text = "{}: {:.4f}".format(LABELS[classIDs[i]], confidences[i])
# 		# 		cv2.putText(image, text, (x, y - 5), cv2.FONT_HERSHEY_SIMPLEX,
# 		# 			0.5, color, 2)
# 		prev_time = curr_time
# 		# show the output image
# 		print("Found ", len(idxs), "People")
# 		cv2.imshow("Image"+str(curr_time), image)
# 		cv2.waitKey(1)
# 		cv2.destroyAllWindows()

# cam.release()