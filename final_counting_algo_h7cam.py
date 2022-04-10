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

while(True):
    clock.tick()
    img = sensor.snapshot()
    img.difference(extra_fb)
    left_frame = 0
    #get ten frames
    print("getting frames... ")
    i = 0
    r_frames = []
    r_frames.append(left_frame)
    dist_frames = []
    deadline_in = ticks_add(time.ticks_ms(), 1000)
    while i<15 and ticks_diff(deadline_in, time.ticks_ms()) > 0:
        blobs_r = img.find_blobs(thresholds, roi=(154,25,120,110), pixels_threshold=150)
        if blobs_r:
            blob_r = blobs_r[0]
            r_frames.append(blob_r.cx())
            img.draw_rectangle(blob_r.rect())
            img.draw_cross(blob_r.cx(), blob_r.cy())
            i += 1
            print("i: ", i)
            deadline_in = ticks_add(time.ticks_ms(), 1000)
        sensor.skip_frames(time = 7)

    for i in range(len(r_frames)):
        if i < len(r_frames) - 1 and abs(r_frames[i+1]-r_frames[i])<7:
            dist_frames.append(r_frames[i+1]-r_frames[i])
    if len(dist_frames)>= 3:
        print("captured: ", r_frames)
        print("calculated: ", dist_frames, "; sum = ", sum(dist_frames))
        if sum(dist_frames) >= 4:
            counter -= 1
        elif sum(dist_frames) <= -4:
            counter += 1
    sensor.skip_frames(time = 1000)

    print("Currently inside: ", counter)
    print(clock.fps())
    print("==============================")
