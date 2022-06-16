# DRGDice
A digital dice for Deep Rock Galactic (the board game) and Dungeons and Dragons

# Requirements
- THIS library: https://github.com/JoaoLopesF/SPFD5408
  - Do not use the Arduino Library Manager version!
- Adafruit TFTLCD library BEFORE version 1.5.4
  - Versions after 1.5.4 dropped support for this display.
  - Moreover, recent versions of this library is way too big for Uno with this version of code. (MCUFriend version)
  - Please use that version before I rewrite the code to be smaller.
- Arduino Uno
- 2.4 inch TFT LCD display with SPFD5408 controller (ones that don't get recognized by the stock Adafruit "graphicstest" sketch)
  - ILI9341 display is supported with the older version IF it's not a MCUFRIEND module (identified with a row of yellow header pins and 3.3V regulator on the board) 
  - For MCUFriend display module, please use the respective version, and apply the patch as instructed.

# Pictures
![IMG1655027158](https://user-images.githubusercontent.com/11834016/173227442-b04c39ca-d5ab-4efb-86fc-5715bd658269.png)

![IMG1655027181](https://user-images.githubusercontent.com/11834016/173227446-298b5b9c-4520-4c84-96a5-997e22e4a5d3.png)

![IMG1655346198](https://user-images.githubusercontent.com/11834016/173977717-dbbcf5f0-aa95-4ad3-bb9e-fa495be2e1a7.png)
