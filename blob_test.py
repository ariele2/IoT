import sensor, image, pyb, os, time, math

thresholds = [(0, 34)]

sensor.reset()
sensor.set_pixformat(sensor.GRAYSCALE)
sensor.set_framesize(sensor.QVGA)
sensor.skip_frames(time = 2000)
sensor.set_auto_gain(False) # must be turned off for color tracking
sensor.set_auto_whitebal(False) # must be turned off for color tracking
clock = time.clock()

extra_fb = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.GRAYSCALE)

print("About to save background image...")
sensor.skip_frames(time = 2000) # Give the user time to get ready.
extra_fb.replace(sensor.snapshot())
print("Saved background image - Now frame differencing!")
first = True

while(True):
    clock.tick()
    img = sensor.snapshot()
    img.difference(extra_fb)
    if not first:
        prev_blob_cx = blob.cx()
        print ("prev_blob_cx: ", prev_blob_cx)
    sensor.skip_frames(time = 2000)
    blobs = img.find_blobs(thresholds, roi=(50,40,214,162), pixels_threshold=200)
    if blobs:
        blob = img.find_blobs(thresholds, roi=(50,40,214,162), pixels_threshold=150)[0]
        img.draw_rectangle(blob.rect())
        img.draw_cross(blob.cx(), blob.cy())
        print("Coordinates of blob: x:", blob.x(), " y: ", blob.y(), " w: ", blob.w(), " h: ", blob.h())
        print("Pixels of blob: ", blob.pixels())
        print("Centroid of blob - x: ", blob.cx(), " y: ", blob.cy())
        print("Perimeter of blob: ", blob.perimeter())
        print("Area of blob: ", blob.area())
        print("Elipse can be drawn for(x,y,radius_x,radius_y, rotation): ", blob.enclosed_ellipse())
        print("-----------------------------------")
        if not first and blob.cx() - prev_blob_cx >= 6:
            print ("Found an object moving right")
        elif not first and blob.cx() - prev_blob_cx <= -6:
            print ("Found and object moving left")
            #if blob.elongation() > 0.5:
                #img.draw_edges(blob.min_corners(), color=(255,0,0))
                #img.draw_line(blob.major_axis_line(), color=(0,255,0))
                #img.draw_line(blob.minor_axis_line(), color=(0,0,255))
            ## These values are stable all the time.
            #img.draw_rectangle(blob.rect())
            #img.draw_cross(blob.cx(), blob.cy())
            ## Note - the blob rotation is unique to 0-180 only.
            #img.draw_keypoints([(blob.cx(), blob.cy(), int(math.degrees(blob.rotation())))], size=20)
        if first:
           first = False
    print(clock.fps())
    print("==============================")
