import sensor, image, pyb, os, time, math

#thresholds = [(11, 100, -128, 127, -128, 127)]
thresholds = [(0,255)]

sensor.reset()
sensor.set_pixformat(sensor.GRAYSCALE)
sensor.set_framesize(sensor.QVGA)
sensor.skip_frames(time = 2000)
sensor.set_auto_gain(False) # must be turned off for color tracking
sensor.set_auto_whitebal(False) # must be turned off for color tracking
clock = time.clock()
#sensor.set_auto_exposure(True, exposure_us=10000) # shutter module

extra_fb = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.GRAYSCALE)

print("About to save background image...")
sensor.skip_frames(time = 2000) # Give the user time to get ready.
extra_fb.replace(sensor.snapshot())
print("Saved background image - Now frame differencing!")


while(True):
    clock.tick()
    img = sensor.snapshot()
    img.difference(extra_fb)
    sensor.skip_frames(time = 1000)

    for blob in img.find_blobs(thresholds, pixels_threshold=200):
        # These values depend on the blob not being circular - otherwise they will be shaky.
        print("Coordinates of blob: x:", blob.x(), " y: ", blob.y(), " w: ", blob.w(), " h: ", blobl.h())
        print("Pixels of blob: ", blob.pixels())
        print("Centroid of blob - x: ", blob.cx(), " y: ", blob.cy())
        print("Perimeter of blob: ", blob.perimeter())
        print("Area of blob: ", blob.area())
        print("Densitiy of blob: ", blob.density())
        print("Hist bins of blob - x: ", blob.x_hist_bins(), " y: ", blob.y_hist_bins())
        print("Elipse can be drawn for(x,y,radius_x,radius_y, rotation): ", blob.enclosed_ellipse())
        #if blob.elongation() > 0.5:
            #img.draw_edges(blob.min_corners(), color=(255,0,0))
            #img.draw_line(blob.major_axis_line(), color=(0,255,0))
            #img.draw_line(blob.minor_axis_line(), color=(0,0,255))
        ## These values are stable all the time.
        #img.draw_rectangle(blob.rect())
        #img.draw_cross(blob.cx(), blob.cy())
        ## Note - the blob rotation is unique to 0-180 only.
        #img.draw_keypoints([(blob.cx(), blob.cy(), int(math.degrees(blob.rotation())))], size=20)
    print(clock.fps())
