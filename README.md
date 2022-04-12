# IoT
Smart Building project.

An application that:
1. Can monitor the amount of people in certain room/area (with defined enterences).
2. Can monitor the amount of people sitting in a retricted zone withing the room/area.
3. There is a GUI for the app,  delivered with UNITY.

project consists of:
1. OpenMV camera. 
   1.1. We developed a really easy and relaible algorithm to detect objects movement directions to determine if an object is moving in or out of the room/area.
   1.2. The camera will be connected to an ESP32 board that will upload the data to a realtime-firebase database.
2. Sonar sensors.
   2.1. One sensor will be placed on one work station, the sensor will determine if ab object is sitting on the work station.
   2.2. We developed a really easy and relaible algorithm to detect if someone is sitting next to the workstation.
   2.3. The sensors will report to an ESP32 board that will uplaod the data to a realtime-firebase database.
3. Volume detector with Raspberry Pie + YOLO
   3.1. 
