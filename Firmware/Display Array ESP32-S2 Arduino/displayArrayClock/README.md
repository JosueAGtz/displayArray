# Display Array Clock Firmware

![displayArrayClock](https://savageelectronics.com/wp-content/uploads/2022/01/displayArrayClockapp-scaled.jpg)

For this clock project, you will need to add some libraries to your Arduino IDE. 

```c++
 Adafruit Neopixel
 NTP Client
 BB SPI LCD
 ```
 
 To add this libraries open your Arduino IDE and click in "Tools>Manage Libraries"

![displayArrayClock_manageLib](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_manageLib.png)

Search and install each library as shown below.

![displayArrayClock_manageLib_Neopixel](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_neoPixel.png)

![displayArrayClock_manageLib_NTPClient](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_ntpClient.png)

![displayArrayClock_manageLib_BBSPILCD](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_bbSPILCD.png)

After installing all the libraries you should now be able to compile the code, so prepare your ESP32-S2 to load the app, dont forget to use Huge app as partition scheme.

![displayArrayClock_verified](https://savageelectronics.com/wp-content/uploads/2022/01/displayArray_Verified.png)

If you are using the displayArray controller remmber that it does not have a USB to Serial converter so you will need to put it in Boodtloader mode by pressing reset and boot at the same time, then release the reset button and after 3 seconds release also the boot button and your controller will be in bootloader mode. 

Now press the upload button to load your app in to your ESP32-S2.

![displayArrayClock_uploading](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_Uploading.png)

For the displayArray controller you will have this message at the end but dont worry your app has been loaded sucessfully and you just need to press the restart button on the controller board. 

![displayArrayClock_uploadErr](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_UploadErr.png)

Now its time to configure your clock as you prefer by selecting your favorite Clock Theme, your Flag and Timezone, I use the Find and replace option to change all the clock theme definitions and click Replace all.

![displayArrayClock_Themerep](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_displayArray_Theme.png)

Set your Wifi Credentials

![displayArrayClock_WifiCredentials](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_displayArray_Credentials.png)

And for the time zone just set it in seconds, for example Mexico is GTM-6 that would be minus 6 hours so that will be -6 * 3600, as there are 3600 seconds in an hour. 

![displayArrayClock_timezpnesetting](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_displayArray_Timezone.png)








