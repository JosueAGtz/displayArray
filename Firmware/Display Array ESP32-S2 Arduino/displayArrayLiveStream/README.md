# Display Array Youtube Livestream Notifier Firmware

For this Youtube Notifier project, you will need to add some libraries to your Arduino IDE. 

```c++
 Adafruit Neopixel
 Youtube API
 Arduino JSON
 BB SPI LCD
 ```
 
 To add this libraries open your Arduino IDE and click in "Tools>Manage Libraries"

![displayArrayYoutube_manageLib](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_liveStreamLib.png)

Search and install each library as shown below.

![displayArrayYoutube_manageLib_Neopixel](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_neoPixel.png)

![displayArrayYoutube_manageLib_YoutubeAPI](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_liveStreamAPI.png)

![displayArrayYoutube_manageLib_ArduinoJSON](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_ArduinoJSON.png)

![displayArrayYoutube_manageLib_BBSPILCD](https://savageelectronics.com/wp-content/uploads/2022/01/Arduino_bbSPILCD.png)

After installing all the libraries you should now be able to compile the code, so prepare your ESP32-S2 to load the app, dont forget to use Huge app as partition scheme.


## Troubleshooting

You might encounter a little problem with Youtube Livestream Arduino Library as me, this happens because the Youtube page is not always in english and that depends on your location, as I live in Mexico the youtube.com page is always in spanish and that it is why you might need to search for other word rather than "watching".

I have used the word " EN VIVO " -> "{\"text\":\"EN VIVO\"}". to get the livestream in the file YoutubeLiveStream.cpp file.

```c++
bool YouTubeLiveStream::scrapeIsChannelLive(const char *channelId, char *videoIdOut, int videoIdOutSize){
    char command[100];
    sprintf(command, youTubeChannelUrl, channelId);

    bool channelIsLive = true;

    #ifdef YOUTUBE_DEBUG
    Serial.print("url: ");
    Serial.println(command);
    #endif

    int statusCode = makeGetRequest(command, YOUTUBE_HOST, "*/*", YOUTUBE_ACCEPT_COOKIES_COOKIE);
    if(statusCode == 200) {
        if (!client->find("{\"text\":\"EN VIVO\"}"))  
        {
            #ifdef YOUTUBE_DEBUG
            Serial.println(F("Channel doesn't seem to be live"));
            #endif

            channelIsLive = false;
```
You can check what is the better word for you by opening the youtube.com page in an incognito window in your explorer and search in the code source.

## Youtube Livestream Notifier

![displayArrayNotLive](https://savageelectronics.com/wp-content/uploads/2021/07/IMG_7319-scaled.jpg)

![displayArrayLive](https://savageelectronics.com/wp-content/uploads/2021/07/IMG_7323-scaled.jpg)

