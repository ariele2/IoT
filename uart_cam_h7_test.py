# Untitled - By: ariel - Tue Apr 5 2022

import sensor, image, time
from pyb import UART
import pyb

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.skip_frames(time = 2000)

uart = UART(1, 9600, timeout_char=1000)
uart.init(9600, bits=8, parity=None, stop=1, timeout_char=1000)
counter = 0
clock = time.clock()

while(True):
    clock.tick()
    msg = "hello" + str(counter)
    counter +=1
    print("Sending...")
    uart.write(msg)
    sensor.skip_frames(time = 4000)
    pyb.delay(300)
    print("Recieveing...")
    print(uart.read())
    print(clock.fps())
