# Modal LSM data collection code
This code manages 3 modes for long battery life, wifi web server, and data collection.

# Dependencies
This module depends on a modified LSM6DSV16X library, forked at ...
It should be cloned or submoduled, and the dependency added in the top level CMakeLists.txt.
The library has been modified to make reg_ctrl protected, and add a CMakeLists.txt component
definition file.

### Important - notes on light sleep and flash leakage
https://esp32.com/viewtopic.php?t=41048

### TODO
Since light sleep interferes with USB, it would be great if we could establish
a console connection over TCP and maintain it through brief intervals of light
sleep.  This would both demonstrate wifi feasibility, and provide a mechanism
for debugging while using light sleep.

We want to support roughly 150 hours of active time - when people are in the 
tower and interacting with the device.  Allocating 10% of battery life for this
means about 250mAH for 150 hours of operation, or about 1.6mA average power
consumption.  We can likely support something like 0.5% duty cycle running the
RF modems for light web interactions (not downloads).

### ESP-IDF stuff
The esp-idf is installed on my local machine in ~/esp/v*/esp-idf/

To make idf.py available, run 
```sh
. ~/esp/v5.5.1/esp-idf/export.sh
```

To configure IDF run
```sh
idf.py menuconfig
```

We want to use the Arduino library for LSM6dsv.  Arduino component was incompatible for a while,
but esp-idf v5.5.1 is compatible with Arduino 3.3.3
So, in idf_component.yml, we want
```yml
 espressif/arduino-esp32: ^3.3.3
```
And we get this by running
```bash
idf.py add-dependency "espressif/arduino-esp32^3.3.3"
```



