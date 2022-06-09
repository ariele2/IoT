# IoT Smart Building project

1. Can monitor the amount of people in certain room/area (with defined enterences).
2. Can monitor the amount of people sitting in a retricted zone withing the room/area.
3. Can monitor the amount of people entering/exiting the room/area.
4. There are 2 GUIs for the app: 

      a. Web App - https://smartbuilding.pythonanywhere.com/
      
      b. UNITY

# The project consists of:
1. OpenMV camera. 

   a. We developed a really easy and relaible algorithm to detect objects movement directions to determine if an object is moving in or out of the room/area.
   
   b. The camera is connected to an ESP32 board that uploads the data to a realtime-firebase database.
   
2. Sonar sensors.

   a. Each sensor is placed on one work station, the sensor determines if an object (person) is sitting on the work station.
   
   b. We developed a really easy and relaible algorithm to detect if someone is sitting next to the workstation.
   
   c. The sensors are connected to an ESP32 board that uplaods the data to a realtime-firebase database.
   
3. Volume detector with Raspberry Pie + YOLO

   a. YOLO - a reliable algorithm to detect persons in an imgae frame.
   
   b. The Raspberry Pie is connected to a camera that catches frames of a hall, then calculates with YOLO the amount of people it sees, then reports to the database.
   
4. WebApp.

   a. Start/Stop the system.
   
   b. Monitor system's health - information of errornus or unconnected data from a sensor.
   
   c. Sensors details - get and set the sensor location, description and a picture of the sensor. The webapp allows the user to update these fields.
   
   d. Scheduler - the user can schedule a time range in which the system will turn on.
  
