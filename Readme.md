# Tiny 5.8GHz Diversity VRX for EV100 Googles

![Virtual PCB](Images/PCB_KiCad1.jpg)
![Virtual PCB](Images/PCB_KiCad2.jpg)

PCB can be ordered here: https://oshpark.com/shared_projects/ZRMoFzeY

## Proof of concept
- Diversity bench test: https://www.youtube.com/watch?v=jwpYg-zWEIs
- Band/channel change and auto-search test: https://www.youtube.com/watch?v=jH8NeLfK5f4

![EV100 Inside](Images/tVRX_fitting.jpg)

## Calibration
- Power OFF VTX.
- Power ON Googles and then press button on tVRX.
- LEDs will make 5 short (100ms) blinks indicating, that lowest video signal detection in progress.
- When LEDs will stay lit, then power ON VTX.
- After about 2 seconds LEDs will make 5 long (900ms) blinks indicating that highest video signal detection in progress.
- When LEDs turn OFF for 2 seconds, then Calibration is done.
- Now tVRX will exit from Calibration mode and go to normal state.

Discussion on RCG: https://www.rcgroups.com/forums/showthread.php?3051872-DIY-Tiny-Diversity-VRX-for-EV100-Googles
