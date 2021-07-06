# Display Array Hardware

With this files you should be able to modify the board as need, also you can fabricate your own board by using the Gerber files and BOM. 

### Printed Circuit Board
![displayArrayPCB](https://savageelectronics.com/wp-content/uploads/2021/06/DisplayArrayPCB-1024x345.png)

### Schematic
![displayArraySCH](https://savageelectronics.com/wp-content/uploads/2021/06/DisplayArrayCAD-1024x401.png)

### Display Array Board
![displayArrayFront](https://savageelectronics.com/wp-content/uploads/2021/06/IMG_7236.jpg)
![displayArrayBack](https://savageelectronics.com/wp-content/uploads/2021/06/IMG_7241.jpg)

# Display Array Controller Hardware

The controller board integrates an ESP32-S2 for its integrated USB Bootloader making it a very compact board with a powerful microcontroller with wifi and Bluetooth capabilities.

This board connects directly to the SPI Display Array Board in its back, it also has an RGB LED and Audio Amplifier for color and audible notifications. The hardware is more than capable to manage 6 displays at a decent frame rate and has enough memory for more than 5 different timezones and clock themes.

This controller has also take into account the possibility of an 8 display board and different display controller detection by the reading of one GPIO, Low logic state for ST7735, and High logic state for ST7789.

### Printed Circuit Board
![displayArrayCtrlPCB](https://savageelectronics.com/wp-content/uploads/2021/06/ControllerPCB-1024x531.png)

### Display Array Controller Board
![displayArrayCtrl](https://savageelectronics.com/wp-content/uploads/2021/06/IMG_7244.jpg)
![displayArrayCtrlMount](https://savageelectronics.com/wp-content/uploads/2021/06/IMG_7262.jpg)
