# Configuring Arduino IDE for ESP32-S2
![displayArray](https://savageelectronics.com/wp-content/uploads/2021/07/DisplayArray-Sideview.png)

In order to configure the Arduino IDE for the ESP32-S2 it is necessary to add the Espressif board configuration files.

### Espressif Repository for ESP32-S2

```c++
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json
```

## Steps to install Arduino ESP32 support on Windows
### Tested on 32 and 64 bit Windows 10 machines

1. Download and install the latest Arduino IDE ```Windows Installer``` from [arduino.cc](https://www.arduino.cc/en/Main/Software)
2. Download and install Git from [git-scm.com](https://git-scm.com/download/win)
3. Start ```Git GUI``` and run through the following steps:
    - Select ```Clone Existing Repository```

        ![Step 1](https://savageelectronics.com/wp-content/uploads/2021/07/win-gui-1.png)

    - Select source and destination
        - Sketchbook Directory: Usually ```C:/Users/[YOUR_USER_NAME]/Documents/Arduino``` and is listed underneath the "Sketchbook location" in Arduino preferences.
        - Source Location: ```https://github.com/espressif/arduino-esp32.git```
        - Target Directory: ```[ARDUINO_SKETCHBOOK_DIR]/hardware/espressif/esp32```
        - Click ```Clone``` to start cloning the repository

            ![Step 2](https://savageelectronics.com/wp-content/uploads/2021/07/win-gui-2.png)
            ![Step 3](https://savageelectronics.com/wp-content/uploads/2021/07/win-gui-3.png)
    - open a `Git Bash` session pointing to ```[ARDUINO_SKETCHBOOK_DIR]/hardware/espressif/esp32``` and execute ```git submodule update --init --recursive``` 
    - Open ```[ARDUINO_SKETCHBOOK_DIR]/hardware/espressif/esp32/tools``` and double-click ```get.exe```

        ![Step 4](https://savageelectronics.com/wp-content/uploads/2021/07/win-gui-4.png)

    - When ```get.exe``` finishes, you should see the following files in the directory

        ![Step 5](https://savageelectronics.com/wp-content/uploads/2021/07/win-gui-5.png)

4. Plug your ESP32 board and wait for the drivers to install (or install manually any that might be required)
5. Start Arduino IDE
6. Select your board in ```Tools > Board``` menu
7. Select the COM port that the board is attached to
8. Compile and upload (You might need to hold the boot button while uploading)

    ![Arduino IDE Example](https://savageelectronics.com/wp-content/uploads/2021/07/arduino-ide.png)

