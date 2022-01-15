# Display Array Animated GIF Firmware

![displayArrayGIF_Animation](https://savageelectronics.com/wp-content/uploads/2022/01/AnimatedGIFApp.gif)

For this Animated GIF project, you will need to add some libraries to your Arduino IDE, this app was written by Larry Bank to test his awesome library capable of showing GIFs. 

```c++
 Animated GIF
 BB SPI LCD
 ```
 
 To add this libraries open your Arduino IDE and click in "Tools>Manage Libraries"

![displayArrayGIF_manageLib](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_AnimatedGIFLib.png)

Search and install each library as shown below.

![displayArrayGIF_manageLib_AnimatedGIF](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_AnimatedGIF.png)

![displayArrayGIF_manageLib_BBSPILCD](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_bbSPILCD.png)

After installing all the libraries you should now be able to compile the code, so prepare your ESP32-S2 to load the app, dont forget to use Huge app as partition scheme.

![displayArrayGIF_verified](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_AnimatedGIFVerified.png)

If you are using the displayArray controller remmber that it does not have a USB to Serial converter so you will need to put it in Boodtloader mode by pressing reset and boot at the same time, then release the reset button and after 3 seconds release also the boot button and your controller will be in bootloader mode. 

![displayArrayCtlrBootMode](https://savageelectronics.com/wp-content/uploads/2022/01/bootMode.gif)

Now press the upload button to load your app in to your ESP32-S2.

![displayArrayGIF_uploading](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_AnimatedGIFUpload.png)

For the displayArray controller you will have this message at the end but dont worry your app has been loaded sucessfully and you just need to press the restart button on the controller board. 

![displayArrayGIF_uploadErr](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_AnimatedGIFApp.png)






