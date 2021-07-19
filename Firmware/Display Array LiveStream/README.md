# Display Array Youtube Livestream Notifier

This sketch requires Youtube livestrem Arduino Library and Arduino JSON, For more info go to:

https://github.com/witnessmenow/youtube-livestream-arduino

for bb_spi_lcd go to:

https://github.com/bitbank2/bb_spi_lcd


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
You can check what is the better word for you by opening the youtube.com page in an incognito window in your explorer and search in the codesoruce.

## Youtube Livestream Notifier

![displayArrayNotLive](https://savageelectronics.com/wp-content/uploads/2021/07/IMG_7319-scaled.jpg)

![displayArrayLive](https://savageelectronics.com/wp-content/uploads/2021/07/IMG_7323-scaled.jpg)

