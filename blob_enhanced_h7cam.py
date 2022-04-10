import sensor, image, pyb, os, time, math
from utime import ticks_add, ticks_diff
from pyb import UART


thresholds = [(0, 34)]

sensor.reset()
sensor.set_pixformat(sensor.GRAYSCALE)
sensor.set_framesize(sensor.QVGA)
sensor.skip_frames(time = 2000)
sensor.set_auto_gain(False) # must be turned off for color tracking
sensor.set_auto_whitebal(False) # must be turned off for color tracking
clock = time.clock()

#initialize uart
uart = UART(3, 9600, timeout_char=1000)
uart.init(9600, bits=8, parity=None, stop=1, timeout_char=1000)

extra_fb = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.GRAYSCALE)

print("About to save background image...")
sensor.skip_frames(time = 2000) # Give the user time to get ready.
extra_fb.replace(sensor.snapshot())
print("Saved background image - Now frame differencing!")
counter = 0
first_l = first_r = True
left_out = right_in = False

while (first_l or first_r):
    img = sensor.snapshot()
    img.difference(extra_fb)
    sensor.skip_frames(time = 350)
    blobs_r = img.find_blobs(thresholds, roi=(260,18,9,130), pixels_threshold=200)
    blobs_l = img.find_blobs(thresholds, roi=(140,18,9,130), pixels_threshold=200)
    print("waiting for input")
    if blobs_r and first_r:
        blob_r = blobs_r[0]
        prev_blob_r_cx = blob_r.cx()
        first_r = False
        print("right block initialization complete.")
    if blobs_l and first_l:
        blob_l = blobs_l[0]
        prev_blob_l_cx = blob_l.cx()
        first_l = False
        print("left block initialization complete.")

deadline_in = deadline_out = 0

while(True):
    clock.tick()
    img = sensor.snapshot()
    img.difference(extra_fb)
    sensor.skip_frames(time = 280)
    blobs_r = img.find_blobs(thresholds, roi=(250,18,10,190), pixels_threshold=150)
    blobs_l = img.find_blobs(thresholds, roi=(140,18,10,190), pixels_threshold=150)
    if blobs_r:
        print("if blobs_r")
        prev_blob_r_cx = blob_r.cx()
        blob_r = blobs_r[0]
        img.draw_rectangle(blob_r.rect())
        img.draw_cross(blob_r.cx(), blob_r.cy())
        print("blob_r.cx() - prev_blob_r_cx: ", blob_r.cx() - prev_blob_r_cx)
        if blob_r.cx() - prev_blob_r_cx > 0:
            print("----------------------------------")
            print ("Right block - found an object moving out")
            if (left_out or (blob_r.cx() - prev_blob_r_cx >= 2)) and \
                ticks_diff(deadline_out, time.ticks_ms()):
                counter -= 1
            left_out = False
            #uart.write("r")
        elif blob_r.cx() - prev_blob_r_cx < 0:
            print("----------------------------------")
            print ("Right block - found and object moving in")
            right_in = True
            template = img
            deadline_in = ticks_add(time.ticks_ms(), 800)
            print("deadline_in: ", time.ticks_ms())
            #uart.write("l")

    if blobs_l:
        print("if blobs_l")
        prev_blob_l_cx = blob_l.cx()
        blob_l = blobs_l[0]
        img.draw_rectangle(blob_l.rect())
        img.draw_cross(blob_l.cx(), blob_l.cy())
        print("blob_l.cx() - prev_blob_l_cx: ", blob_l.cx() - prev_blob_l_cx)
        if blob_l.cx() - prev_blob_l_cx > 0:
            template = img
            print("----------------------------------")
            print ("Left block - found an object moving out")
            left_out = True
            deadline_out = ticks_add(time.ticks_ms(), 800)
            #uart.write("r")
        elif blob_l.cx() - prev_blob_l_cx < 0:
            print("----------------------------------")
            print ("Left block - found and object moving in")
            print("curr_time: ", time.ticks_ms())
            if (right_in or (blob_l.cx() - prev_blob_l_cx <= -2)) and \
               ticks_diff(deadline_in, time.ticks_ms()) > 0:
                counter += 1
            right_in = False
            #uart.write("l")
    if ticks_diff(deadline_in, time.ticks_ms()) <= 0:
        right_in = False
    if ticks_diff(deadline_out, time.ticks_ms()) <= 0:
        left_out = False
    print("Currently inside: ", counter)
    print ("right_in: ", right_in, "left_out: ", left_out)
    print(clock.fps())
    print("==============================")
