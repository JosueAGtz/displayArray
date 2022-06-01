# Display Array Clock using 3MB SPIFFS
![displayArray](https://savageelectronics.com/wp-content/uploads/2021/07/DisplayArray-Sideview.png)

This code uses the ESP32-S2 Stick internal flash memory for the APP(1MB) and SPIFFS(3MB) making posible just to copy paste your PNG theme files directly to the data folder in your Arduino project and uploaded directly using the SPIFFS Uploader for the ESP32-S2.

In order to use the SPIFFS you should have the SPIFFS tool for ESP32-S2 installed, you can download the tool from: https://github.com/etherfi/arduino-esp32fs-plugin-esp32s2/releases and copy the folder in C:\\Program Files\Arduino\tools like the image below:

![displayArray_SPIFFSTOOL](https://savageelectronics.com/wp-content/uploads/2022/05/ESP32_SPIFFS_TOOL.png)

Now that you have installed the SPIFFS Tool you should see in your arduino IDE the option to upload your data to your ESP32-S2.

![displayArray_SPIFFSTOOL](https://savageelectronics.com/wp-content/uploads/2022/05/SPIFFS_dataUpload.png)

The files that you want to upload shoud be contained in a directory data next to your Arduino Sketch, and remember to select the proper partition table for the app and data, in this case 1MB for the APP and 3MB for the data.

![displayArray_SPIFFSTOOL](https://savageelectronics.com/wp-content/uploads/2022/05/displayArray_SPIFFSdata.png)

This sketch uses the bb_spi_lcd, PNGdec and NTPClient libraries, so be sure to copy those folders in to your Arduino libraries folder.
