import sensor, image, pyb, os, time, math
from utime import ticks_add, ticks_diff
from pyb import UART

# thresholds are for the background grayscale to identify an object
thresholds = [(0, 34)]
# the frames number between each moving object.
NUM_OF_FRAMES = 15
# the time interval between each frame, in ms.
FRAME_TIME_INTERVAL = 7
# the maximum distance movement between each frame (so we wont capture noisy movements)
MAX_FRAME_DIST = 7
# the limit that if we passed it, it means there was a movement to certain direction (in pixels)
MOVEMENT_LIMIT = 4

sensor.reset()
sensor.set_pixformat(sensor.GRAYSCALE)  # do not change
sensor.set_framesize(sensor.QVGA)   # do not change
sensor.skip_frames(time = 2000)
sensor.set_auto_gain(False) # must be turned off for color tracking
sensor.set_auto_whitebal(False) # must be turned off for color tracking
clock = time.clock()

#initialize uart
uart = UART(3, 9600, timeout_char=1000) # send data from port 3
uart.init(9600, bits=8, parity=None, stop=1, timeout_char=1000)

extra_fb = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.GRAYSCALE)

# capture background image to compare movement to
print("About to save background image...")
sensor.skip_frames(time = 2000) # Give the user time to get ready.
extra_fb.replace(sensor.snapshot())
print("Saved background image - Now frame differencing!")
counter = 0


while(True):
    clock.tick()
    img = sensor.snapshot()
    img.difference(extra_fb)
    # get ten frames
    print("getting frames... ")
    frame_num = 0
    captured_frames_cx = [0]*1 # initialize the array with 0
    dist_frames = []
    # add a 1 second delay between each capturing of NUM_OF_FRAMES,
    deadline_in = ticks_add(time.ticks_ms(), 1000)
    while frame_num<NUM_OF_FRAMES and ticks_diff(deadline_in, time.ticks_ms()) > 0:
        blobs = img.find_blobs(thresholds, roi=(154,25,120,110), pixels_threshold=150)
        if blobs:
            captured_blob = blobs[0]
            # add the central x of the blob to the captured frames cx array
            captured_frames_cx.append(captured_blob.cx())
            img.draw_rectangle(captured_blob.rect())
            img.draw_cross(captured_blob.cx(), captured_blob.cy())
            frame_num += 1
            print("fram_num: ", frame_num)
            # reset the delay if we found a moving object and didnt captured enough frames yet
            deadline_in = ticks_add(time.ticks_ms(), 1000)
        # time interval between each individual frame
        sensor.skip_frames(time = FRAME_TIME_INTERVAL)

    # create an array of distances between consecutive frames
    for i in range(len(captured_frames_cx)):
        # we want to ignore noisy capturing, we set the noise to 7
        if i < len(captured_frames_cx) - 1 and \
                abs(captured_frames_cx[i+1]-captured_frames_cx[i])<MAX_FRAM_DIST:
            dist_frames.append(captured_frames_cx[i+1]-captured_frames_cx[i])
    if len(dist_frames)>= 3:    # need at least 3 elements in the array of frames to be relaiable
        print("captured: ", captured_frames_cx)
        print("calculated: ", dist_frames, "; sum = ", sum(dist_frames))
        if sum(dist_frames) >= MOVEMENT_LIMIT:
            print("Someone entered!")
            counter -= 1
        elif sum(dist_frames) <= -MOVEMENT_LIMIT:
            print("Someone exited!")
            counter += 1
    sensor.skip_frames(time = 1000)

    print("Currently inside: ", counter)
    print(clock.fps())
    print("==============================")
