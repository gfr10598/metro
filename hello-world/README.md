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



# _Sample project_

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This is the simplest buildable example. The example is used by command `idf.py create-project`
that copies the project to user specified path and set it's name. For more information follow the [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)



## How to use example
We encourage the users to use the example as a template for the new projects.
A recommended way is to follow the instructions on a [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project).

## Example folder contents

The project **sample_project** contains one source file in C language [main.c](main/main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt`
files that provide set of directives and instructions describing the project's source files and targets
(executable, library, or both). 

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md                  This is the file you are currently reading
```
Additionally, the sample project contains Makefile and component.mk files, used for the legacy Make based build system. 
They are not used or needed when building with CMake and idf.py.
