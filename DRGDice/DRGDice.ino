/*
   Deep Rock Galactic Dice

   By Blackbeard Softworks 2022
   ---
   This specific version of the program was made for LCDs using SPFD5408 controller IC.
   For newer ILI9341 and other displays, please use the other version.
   The LCD shield that use SPFD5408 and ILI9341 look almost identical,
   so try this first and if you get a white or corrupted screen, try ILI9341 version.

   NOTE: You cannot use the SPFD5408 library shown in Arduino Library Manager, as this will result in one axis always being flipped.
   Please download the library from https://github.com/JoaoLopesF/SPFD5408 .

   If you get a black screen or corrupted images, please format your SD card using FAT32 filesystem.

   Changelog:
   V1.0 - Initial release (SD Read test)
   v1.1 - Added menu graphics
   v1.2 - Added submenus
   v1.3 - Added support for navigation via side buttons
          SD card support temporarily removed
   v1.4 - Added menu support
          Debug option improved
   v1.5 -

   HOME: 196, -8
   BOOK: 156, -8
   MSG:  119, -8
   CAM:  78,  -8
   SCR:  35,  -8
*/
String versionNr = "Version 1849 Revision 14";
// Let's try to randomize the dice rolls...
long seed;
const long interval = 5000; // Change this to change how often the seed should update
unsigned long prevTimer = 0;
// Is it in debug mode?
bool debug = false;
// Disable images to speed up loading? (This automatically gets enabled if SD card init fails)
bool noimg = false;
// Button position
#define HOME 830
#define BOOK 690
#define MSG 550
#define CAM 423
#define SCR 283

#include <SPFD5408_Adafruit_GFX.h>    // Core graphics library
#include <SPFD5408_Adafruit_TFTLCD.h> // Hardware-specific library
#include <SPFD5408_TouchScreen.h>     // Touch library
#include <SD.h>
#include <SPI.h>


#if defined(__SAM3X8E__)
#undef __FlashStringHelper::F(string_literal)
#define F(string_literal) string_literal
#endif

// Firmware version, used for the info screen
#define VERSION 1000

#define SENSIBILITY 300
#define MINPRESSURE 10
#define MAXPRESSURE 1000


// If the axis are flipped, please swap X and Y pins in group.
#define YP A2 // Must be an analog pin
#define XM A1 // Must be an analog pin
#define YM 6
#define XP 7

// You probably don't have to touch this, but just in case...
// This is the same as SD_SS pin on some modules.
#define SD_CS 10

// Change this value using the spfd5408_calibrate sketch.
// To be replaced with EEPROM based calibration in the future release.
#define TS_MINX 190
#define TS_MINY 120
#define TS_MAXX 915
#define TS_MAXY 838

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// If you get white or corrupted screen, change the pins listed here according to your module's pinout
#define LCD_CS A3
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
// optional
#define LCD_RESET A4

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define ORANGE  0xFCA0
#define SILVER  0xA510

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

// What menu are we on?
/*
   0: Main menu (default)
   1: Coin toss
   2: d6
   3: DnD
   4: DRG
   5: Info
*/
int menuIndex = 0;
int diceCount = 1;

uint16_t scrWidth = tft.width() - 1;
uint16_t scrHeight = tft.height() - 1;

bool throwCoin() {
  int a = random(0, 10);
  if (a == 0 || a == 2 || a == 4 || a == 6 || a == 8 ) {
    return true;
  }
  else if (a == 1 || a == 3 || a == 5 || a == 7 || a == 9) {
    return false;
  }
  else {
    return false;
  }
  return false;
}

int throwDice() {
  int a = random(1, 7);
  return a;
}


int rollDnD(int which = 0, int count = 1) {
  /*
     which parameter:
     0: d4
     1: d6
     2: d8
     3: d10
     4: d12
     5: d26
     6: d20

     count parameter:
     Repeat the action n times and add up the numbers, then return it
  */
  int maximum = 5;
  switch (which) {
    case 0:
      maximum = 4 + 1;
      break;
    case 1:
      maximum = 6 + 1;
      break;
    case 2:
      maximum = 8 + 1;
      break;
    case 3:
      maximum = 10 + 1;
      break;
    case 4:
      maximum = 12 + 1;
      break;
    case 5:
      maximum = 16 + 1;
      break;
    case 6:
      maximum = 20 + 1;
      break;
    default:
      maximum = 4 + 1;
      break;
  }

  int faceValue = 0;
  int currentVal = 0;
  for (count >= 0; count--;) {
    currentVal = random(1, maximum);
    Serial.println(currentVal);
    if (which == 3) {
      // Multiply by 10 to get d10
      currentVal = currentVal * 10;
      faceValue = currentVal;
    }
    else {
      faceValue = faceValue + currentVal;
    }
    Serial.println(faceValue);
  }
  // Return the result
  return faceValue;
}

char rollDRG(int which = 0) {
  /*
     The dice program will automatically roll it multiple times if needed
     hence the count parameter is unneeded.
     Most likely < Side list > Least likely
     1: Explosion dice (red), has Scare Away, 1 or 2 side (2), [1(3), 2(2), S(1)]
     2: Bullet (green), has 0 or 1 side (3), [1(4), 0(2)]
     3: Armor-piercing (blue), has 0, 1 or 2 side , [1(3), 2(2), 0(1)]
     4: Flame (yellow), has 0, 1 or 2 side (3), [1(3), 0(2), 2(1)]
     5: Damage (taken) dice (black), has 0, 1 or ! side (2), [1(3), !(2), 0(1)]
     6: Gold or Nitra (orange), G or N or 0 (1), [N(2), G(2), 0(2)]
     7: Pickaxe (white), has 0, 1 or 2 [1(3), 2(2), 0(1)]
  */
  // For a fairer dice roll, the dice faces are indexed
  char rArray[7] = {"022111"};
  char gArray[7] = {"111100"};
  char bArray[7] = {"022111"};
  char yArray[7] = {"200111"};
  char kArray[7] = {" !!111"};
  char mArray[7] = {"  GGNN"};
  char pArray[7] = {"022111"};
  // Just in case we don't hit any cases or ifs, return 0

  int index = random(0, 6);

  switch (which) {
    case 0:
      return rArray[index];
      break;
    case 1:
      return gArray[index];
      break;
    case 2:
      return bArray[index];
      break;
    case 3:
      return yArray[index];
      break;
    case 4:
      return kArray[index];
      break;
    case 5:
      return mArray[index];
      break;
    case 6:
      return pArray[index];
      break;
  }
  return 0;
}

void showMenu(int which = 0) {
  //Serial.print("Got "); Serial.println(which);
  int posx = 12;
  int posy1 = 27;
  int posy2 = posy1 + 50;
  int posy3 = posy2 + 50;
  int posy4 = posy3 + 50;
  int sizeX = 320 - 25;
  int sizeY = 45;

  int textX = 40;
  int textY = 42;
  int offset = 50;

  // Syntax: Start X (left), Start Y (top), Size X, Size Y
  int quickBtn1[4] = {3, 27, 313, 50}; // Top row button of quick menus
  int quickBtn2[4] = {3, 80, 313, 150}; // Bottom button of quick menus
  int quickTxt1[2] = {115, 38};
  int quickTxt2[2] = {64, 120};
  int topRowBtn[4] = {3, 27, 43, 43};
  // Syntax: Spacing between buttons, spacing between row
  int topRowSpacing[2] = {45, 68};
  // Syntax: 1st button start X, Y, 1st button size X, Y, 2nd button start X, Y, 2nd button size X, Y and so on until 4th button
  int midRowPos[16] = {3, 98, 43, 43,   48, 98, 43, 43,   93, 98, 43, 43,   138, 98, 86, 43};
  int midRowSpacing = 8;
  int bottomRowBtn[16] = {3, 153, 85, 85,   90, 153, 85, 85,   177, 153, 85, 85,   264, 153, 50, 50};
  switch (which) {
    case 0:
      // Main menu
      //Serial.println("Drawing main menu");
      tft.fillScreen(BLACK);
      //bmpDraw("MAIN.BMP", 0, 0);
      menuIndex = 0;
      tft.fillRect(posx, posy1, sizeX, sizeY, WHITE);
      tft.drawRect(posx, posy1, sizeX, sizeY, BLACK);
      tft.fillRect(posx, posy2, sizeX, sizeY, WHITE);
      tft.drawRect(posx, posy2, sizeX, sizeY, BLACK);
      tft.fillRect(posx, posy3, sizeX, sizeY, WHITE);
      tft.drawRect(posx, posy3, sizeX, sizeY, BLACK);
      tft.fillRect(posx, posy4, sizeX, sizeY, WHITE);
      tft.drawRect(posx, posy4, sizeX, sizeY, BLACK);
      tft.setTextSize(2);
      tft.setCursor(37, 8);
      tft.setTextColor(WHITE);
      tft.println("Please select a mode!");
      tft.setCursor(textX, textY);
      tft.setTextColor(BLACK);
      tft.println("  Quick Coin Toss!  ");
      textY = textY + offset;
      tft.setCursor(textX, textY);
      tft.println(" Quick 6-sided Dice ");
      textY = textY + offset;
      tft.setCursor(textX, textY);
      tft.println("Dungeons and Dragons");
      textY = textY + offset;
      tft.setCursor(textX, textY);
      tft.println(" Deep Rock Galactic ");
      break;

    case 1:
      // Coin toss
      //Serial.println("Drawing coin toss");
      menuIndex = 1;
      tft.fillScreen(BLACK);
      tft.fillRect(quickBtn1[0], quickBtn1[1], quickBtn1[2], quickBtn1[3], WHITE);
      tft.drawRect(quickBtn1[0], quickBtn1[1], quickBtn1[2], quickBtn1[3], BLACK);
      tft.fillRect(quickBtn2[0], quickBtn2[1], quickBtn2[2], quickBtn2[3], WHITE);
      tft.drawRect(quickBtn2[0], quickBtn2[1], quickBtn2[2], quickBtn2[3], BLACK);
      tft.setTextSize(2);
      tft.setTextColor(WHITE);
      tft.setCursor(64, 8);
      tft.println(F("Quick Coin Toss"));
      tft.setTextColor(BLACK);
      tft.setTextSize(4);
      tft.setCursor(quickTxt1[0], quickTxt1[1]);
      tft.println("Flip");
      tft.setTextSize(8);
      tft.setCursor(quickTxt2[0], quickTxt2[1]);
      if (throwCoin() == true) {
        tft.println("Head");
      }
      else {
        tft.println("Tail");
      }
      break;

    case 2:
      // d6
      //Serial.println("Drawing dice");
      menuIndex = 2;
      tft.fillScreen(BLACK);
      tft.fillRect(quickBtn1[0], quickBtn1[1], quickBtn1[2], quickBtn1[3], WHITE);
      tft.drawRect(quickBtn1[0], quickBtn1[1], quickBtn1[2], quickBtn1[3], BLACK);
      tft.fillRect(quickBtn2[0], quickBtn2[1], quickBtn2[2], quickBtn2[3], WHITE);
      tft.drawRect(quickBtn2[0], quickBtn2[1], quickBtn2[2], quickBtn2[3], BLACK);
      tft.setTextSize(2);
      tft.setTextColor(WHITE);
      tft.setCursor(54, 8);
      tft.println(F("Quick 6-sided Dice"));
      tft.setTextColor(BLACK);
      tft.setTextSize(4);
      tft.setCursor(quickTxt1[0], quickTxt1[1]);
      tft.println("Roll");
      tft.setTextSize(8);
      tft.setCursor(quickTxt2[0] + 74, quickTxt2[1]);
      tft.println(throwDice());
      break;

    case 3:
      // DnD
      //Serial.println("Drawing DnD");
      menuIndex = 3;
      tft.fillScreen(BLACK);
      //bmpDraw("dnd.bmp", 0, 0);
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], WHITE);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], WHITE);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], WHITE);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], WHITE);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], WHITE);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], WHITE);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], WHITE);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);

      tft.fillRect(midRowPos[0], midRowPos[1], midRowPos[2], midRowPos[3], WHITE);
      tft.drawRect(midRowPos[0], midRowPos[1], midRowPos[2], midRowPos[3], BLACK);
      tft.fillRect(midRowPos[4], midRowPos[5], midRowPos[6], midRowPos[7], WHITE);
      tft.drawRect(midRowPos[4], midRowPos[5], midRowPos[6], midRowPos[7], BLACK);
      tft.fillRect(midRowPos[8], midRowPos[9], midRowPos[10], midRowPos[11], WHITE);
      tft.drawRect(midRowPos[8], midRowPos[9], midRowPos[10], midRowPos[11], BLACK);

      tft.fillRect(midRowPos[12], midRowPos[13], midRowPos[14], midRowPos[15], WHITE);
      tft.drawRect(midRowPos[12], midRowPos[13], midRowPos[14], midRowPos[15], BLACK);

      tft.fillRect(bottomRowBtn[0], bottomRowBtn[1], 313, 85, WHITE);
      tft.drawRect(bottomRowBtn[0], bottomRowBtn[1], 313, 85, BLACK);

      tft.setCursor(5, 8);
      tft.setTextSize(2);
      tft.setTextColor(WHITE);
      tft.println(F("Which dice?"));
      tft.setCursor(5, 77);
      tft.println(F("How many?"));

      tft.setTextColor(BLACK);
      tft.setTextSize(4);
      tft.setCursor(15, 35);
      tft.println("4");
      tft.setCursor(59, 35);
      tft.println("6");
      tft.setCursor(105, 35);
      tft.println("8");
      tft.setTextSize(3);
      tft.setCursor(142, 38);
      tft.println("10");
      tft.setCursor(186, 38);
      tft.println("12");
      tft.setCursor(232, 38);
      tft.println("16");
      tft.setCursor(278, 38);
      tft.println("20");

      tft.setTextSize(4);
      tft.setCursor(15, 105);
      tft.println("1");
      tft.setCursor(59, 105);
      tft.println("+");
      tft.setCursor(105, 105);
      tft.println("-");

      tft.setCursor(149, 109);
      tft.setTextSize(3);
      tft.println("Roll");

      tft.setTextSize(8);
      tft.setCursor(25, 170);
      tft.println("0");

      break;

    case 4:
      // The main show, the DRG
      //Serial.println("Drawing DRG");
      menuIndex = 4;
      tft.fillScreen(BLACK);
      //bmpDraw("drg.bmp", 0, 0);

      /*
        1: Explosion dice (red), has Scare Away, 1 or 2 side (2), [1(3), 2(2), S(1)]
        2: Bullet (green), has 0 or 1 side (3), [1(4), 0(2)]
        3: Armor-piercing (blue), has 0, 1 or 2 side , [1(3), 2(2), 0(1)]
        4: Flame (yellow), has 0, 1 or 2 side (3), [1(3), 0(2), 2(1)]
        5: Damage (taken) dice (black), has 0, 1 or ! side (2), [1(3), !(2), 0(1)]
        6: Gold or Nitra (orange), G or N or 0 (1), [N(2), G(2), 0(2)]
        7: Pickaxe (white), has 0, 1 or 2 [1(3), 2(2), 0(1)]
      */
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], RED);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], GREEN);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLUE);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], YELLOW);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], WHITE);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], ORANGE);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);
      topRowBtn[0] = topRowBtn[0] + topRowSpacing[0];
      tft.fillRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], SILVER);
      tft.drawRect(topRowBtn[0], topRowBtn[1], topRowBtn[2], topRowBtn[3], BLACK);

      tft.fillRect(midRowPos[0], midRowPos[1], midRowPos[2], midRowPos[3], WHITE);
      tft.drawRect(midRowPos[0], midRowPos[1], midRowPos[2], midRowPos[3], BLACK);
      tft.fillRect(midRowPos[4], midRowPos[5], midRowPos[6], midRowPos[7], WHITE);
      tft.drawRect(midRowPos[4], midRowPos[5], midRowPos[6], midRowPos[7], BLACK);
      tft.fillRect(midRowPos[8], midRowPos[9], midRowPos[10], midRowPos[11], WHITE);
      tft.drawRect(midRowPos[8], midRowPos[9], midRowPos[10], midRowPos[11], BLACK);

      tft.fillRect(midRowPos[12], midRowPos[13], midRowPos[14], midRowPos[15], WHITE);
      tft.drawRect(midRowPos[12], midRowPos[13], midRowPos[14], midRowPos[15], BLACK);

      tft.fillRect(bottomRowBtn[0], bottomRowBtn[1], bottomRowBtn[2], bottomRowBtn[3], WHITE);
      tft.drawRect(bottomRowBtn[0], bottomRowBtn[1], bottomRowBtn[2], bottomRowBtn[3], BLACK);
      tft.fillRect(bottomRowBtn[4], bottomRowBtn[5], bottomRowBtn[6], bottomRowBtn[7], WHITE);
      tft.drawRect(bottomRowBtn[4], bottomRowBtn[5], bottomRowBtn[6], bottomRowBtn[7], BLACK);
      tft.fillRect(bottomRowBtn[8], bottomRowBtn[9], bottomRowBtn[10], bottomRowBtn[11], WHITE);
      tft.drawRect(bottomRowBtn[8], bottomRowBtn[9], bottomRowBtn[10], bottomRowBtn[11], BLACK);

      tft.fillRect(bottomRowBtn[12], bottomRowBtn[13], bottomRowBtn[14], bottomRowBtn[15], WHITE);
      tft.drawRect(bottomRowBtn[12], bottomRowBtn[13], bottomRowBtn[14], bottomRowBtn[15], BLACK);

      tft.setCursor(5, 8);
      tft.setTextSize(2);
      tft.setTextColor(WHITE);
      tft.println(F("Which dice?"));
      tft.setCursor(5, 77);
      tft.println(F("How many?"));
      tft.setCursor(277, 130);
      tft.println(F("+1"));
      tft.setTextSize(1);
      tft.setCursor(272, 208);
      tft.println(F("Tap to"));
      tft.setCursor(279, 218);
      tft.println(F("Roll"));

      tft.setTextSize(4);
      tft.setCursor(15, 35);
      tft.println(F("R"));
      tft.setCursor(59, 35);
      tft.setTextColor(BLACK);
      tft.println(F("G"));
      tft.setTextColor(WHITE);
      tft.setCursor(105, 35);
      tft.println(F("B"));
      tft.setTextColor(BLACK);
      tft.setCursor(151, 35);
      tft.println(F("Y"));
      tft.setTextColor(WHITE);
      tft.setCursor(195, 35);
      tft.println(F("K"));
      tft.setTextColor(BLACK);
      tft.setCursor(239, 35);
      tft.println(F("O"));
      tft.setCursor(284, 35);
      tft.println(F("W"));

      tft.setCursor(15, 105);
      tft.println(F("1"));
      tft.setCursor(59, 105);
      tft.println(F("2"));
      tft.setCursor(105, 105);
      tft.println(F("3"));

      tft.setCursor(149, 109);
      tft.setTextSize(3);
      tft.println(F("Roll"));
      /*
        tft.setTextSize(8);
        tft.setCursor(25, 168);
        tft.println(F("X"));
        tft.setCursor(112, 168);
        tft.println(F("X"));
        tft.setCursor(199, 168);
        tft.println(F("X"));

        tft.setTextSize(4);
        tft.setCursor(280, 163);
        tft.println(F("X"));
      */
      break;

    case 5:
      // Info screen
      Serial.println("2022 Blackbeard Softworks. All rights reserved.");
      menuIndex = 5;
      tft.fillScreen(BLACK);
      tft.setCursor(5, 5);
      tft.setTextColor(WHITE);
      tft.setTextSize(1);
      tft.println(F("(C) 2022 Blackbeard Softworks"));
      tft.setCursor(5, 15);
      tft.println(versionNr);
      tft.setCursor(5, 30);
      tft.println(F("Logo by Tran"));
      tft.setCursor(5, 45);
      tft.println(F("Made with love, blood, sweat and tears"));
      tft.setCursor(5, 55);
      tft.println(F("For Karl, and for the future"));
      tft.setCursor(5, 65);
      tft.println(F("Made for Deep Rock Galactic community"));
      if (debug == true) {
        tft.setCursor(5, 85);
        tft.println(F("DEBUG flag is enabled."));
      }
      if (noimg == true) {
        tft.setCursor(5, 95);
        tft.println(F("No SD card!"));
      }
      break;

    default:
      // Scary color to show that the program was NOT AMUSED
      tft.fillScreen(RED);
      break;
  }
}

int diceIndex = 0;
int drgSel = 0;
int drgDiceCount = 1;

char res1 = "0";
char res2 = "0";
char res3 = "0";
char res4 = "0";

void redrawDRG(int which = 0, int colorIndex = 0, bool roll = false, int howmany = 0) {
  uint16_t color = WHITE;
  uint16_t textC = BLACK;
  uint16_t border = BLACK;
  // Wipe the bottom half
  tft.fillRect(0, 120, 320, 240, BLACK);
  int allowedDice = 1;
  tft.fillRect(3, 98, 43, 43, WHITE);
  tft.drawRect(3, 98, 43, 43, BLACK);
  tft.fillRect(48, 98, 43, 43, WHITE);
  tft.drawRect(48, 98, 43, 43, BLACK);
  tft.fillRect(93, 98, 43, 43, WHITE);
  tft.drawRect(93, 98, 43, 43, BLACK);
  tft.fillRect(138, 98, 86, 43, WHITE);
  tft.drawRect(138, 98, 86, 43, BLACK);
  tft.setCursor(149, 109);
  tft.setTextSize(3);
  tft.setTextColor(BLACK);
  tft.println(F("Roll"));
  switch (drgDiceCount) {
    case 1:
      tft.fillRect(3, 98, 43, 43, GREEN);
      tft.drawRect(3, 98, 43, 43, BLACK);
      break;
    case 2:
      tft.fillRect(48, 98, 43, 43, GREEN);
      tft.drawRect(48, 98, 43, 43, BLACK);
      break;
    case 3:
      tft.fillRect(93, 98, 43, 43, GREEN);
      tft.drawRect(93, 98, 43, 43, BLACK);
      break;
    default:
      break;
  }
  switch (colorIndex) {
    case 0:
      color = RED;
      textC = WHITE;
      border = BLACK;
      allowedDice = 3;
      break;
    case 1:
      color = GREEN;
      textC = BLACK;
      border = BLACK;
      allowedDice = 4;
      break;
    case 2:
      color = BLUE;
      textC = WHITE;
      border = BLACK;
      allowedDice = 3;
      break;
    case 3:
      color = YELLOW;
      textC = BLACK;
      border = BLACK;
      allowedDice = 4;
      break;
    case 4:
      color = BLACK;
      textC = WHITE;
      border = WHITE;
      allowedDice = 2;
      break;
    case 5:
      color = ORANGE;
      textC = BLACK;
      border = BLACK;
      allowedDice = 1;
      break;
    case 6:
      color = SILVER;
      textC = BLACK;
      border = BLACK;
      allowedDice = 1;
      break;
    default:
      color = WHITE;
      textC = BLACK;
      border = BLACK;
      break;
  }


  tft.setTextColor(textC);
  tft.setTextSize(8);
  if (roll == true) {
    switch (which) {
      case 0:
        res1 = rollDRG(colorIndex);
        res2 = rollDRG(colorIndex);
        res3 = rollDRG(colorIndex);
        if (allowedDice == 4) {
          tft.fillRect(3, 153, 85, 85, color);
          tft.drawRect(3, 153, 85, 85, border);
          tft.fillRect(90, 153, 85, 85, color);
          tft.drawRect(90, 153, 85, 85, border);
          tft.fillRect(177, 153, 85, 85, color);
          tft.drawRect(177, 153, 85, 85, border);
          tft.fillRect(264, 153, 50, 50, color);
          tft.drawRect(264, 153, 50, 50, border);
          if (howmany == 3 || howmany == 0) {
            tft.setCursor(25, 168);
            tft.println(res1);
            tft.setCursor(112, 168);
            tft.println(res2);
            tft.setCursor(199, 168);
            tft.println(res3);
          } else if (howmany == 2) {
            tft.setCursor(25, 168);
            tft.println(res1);
            tft.setCursor(112, 168);
            tft.println(res2);
          } else if (howmany == 1) {
            tft.setCursor(25, 168);
            tft.println(res1);
          }
          tft.setTextSize(2);
          tft.setTextColor(WHITE);
          tft.setCursor(277, 130);
          tft.println(F("+1"));
          tft.setTextSize(1);
          tft.setCursor(272, 208);
          tft.println(F("Tap to"));
          tft.setCursor(279, 218);
          tft.println(F("Roll"));
          break;
        }
        else if (allowedDice == 3) {
          tft.fillRect(3, 153, 85, 85, color);
          tft.drawRect(3, 153, 85, 85, border);
          tft.fillRect(90, 153, 85, 85, color);
          tft.drawRect(90, 153, 85, 85, border);
          tft.fillRect(264, 153, 50, 50, color);
          tft.drawRect(264, 153, 50, 50, border);
          if (howmany == 2 || howmany == 0 || howmany == 3) {
            tft.setCursor(25, 168);
            tft.println(res1);
            tft.setCursor(112, 168);
            tft.println(res2);
          } else if (howmany == 1) {
            tft.setCursor(25, 168);
            tft.println(res1);
          }
          tft.setTextSize(2);
          tft.setTextColor(WHITE);
          tft.setCursor(277, 130);
          tft.println(F("+1"));
          tft.setTextSize(1);
          tft.setCursor(272, 208);
          tft.println(F("Tap to"));
          tft.setCursor(279, 218);
          tft.println(F("Roll"));
          break;
        }
        else if (allowedDice == 2)  {
          tft.fillRect(3, 153, 85, 85, color);
          tft.drawRect(3, 153, 85, 85, border);
          tft.fillRect(90, 153, 85, 85, color);
          tft.drawRect(90, 153, 85, 85, border);
          tft.setTextSize(8);
          if (howmany == 2 || howmany == 0 || howmany == 3) {
            tft.setCursor(25, 168);
            tft.println(res1);
            tft.setCursor(112, 168);
            tft.println(res2);
          } else if (howmany == 1) {
            tft.setCursor(25, 168);
            tft.println(res1);
          }
        }
        else if (allowedDice == 1) {
          tft.fillRect(3, 153, 85, 85, color);
          tft.drawRect(3, 153, 85, 85, border);
          tft.setCursor(25, 168);
          tft.println(res1);
          break;
        }
        break;

      case 1:
        res4 = rollDRG(colorIndex);
        if (allowedDice == 4) {
          tft.fillRect(3, 153, 85, 85, color);
          tft.drawRect(3, 153, 85, 85, border);
          tft.fillRect(90, 153, 85, 85, color);
          tft.drawRect(90, 153, 85, 85, border);
          tft.fillRect(177, 153, 85, 85, color);
          tft.drawRect(177, 153, 85, 85, border);
          tft.fillRect(264, 153, 50, 50, color);
          tft.drawRect(264, 153, 50, 50, border);
          tft.setTextSize(8);
          if (howmany == 3 || howmany == 0) {
            tft.setCursor(25, 168);
            tft.println(res1);
            tft.setCursor(112, 168);
            tft.println(res2);
            tft.setCursor(199, 168);
            tft.println(res3);
          } else if (howmany == 2) {
            tft.setCursor(25, 168);
            tft.println(res1);
            tft.setCursor(112, 168);
            tft.println(res2);
          } else if (howmany == 1) {
            tft.setCursor(25, 168);
            tft.println(res1);
          }
          tft.setTextSize(4);
          tft.setCursor(280, 163);
          tft.println(res4);
          tft.setTextSize(2);
          tft.setTextColor(WHITE);
          tft.setCursor(277, 130);
          tft.println(F("+1"));
          tft.setTextSize(1);
          tft.setCursor(272, 208);
          tft.println(F("Tap to"));
          tft.setCursor(279, 218);
          tft.println(F("Roll"));
          break;
        } else if (allowedDice == 3)  {
          tft.fillRect(3, 153, 85, 85, color);
          tft.drawRect(3, 153, 85, 85, border);
          tft.fillRect(90, 153, 85, 85, color);
          tft.drawRect(90, 153, 85, 85, border);
          tft.fillRect(264, 153, 50, 50, color);
          tft.drawRect(264, 153, 50, 50, border);
          tft.setTextSize(8);
          if (howmany == 2 || howmany == 0 || howmany == 3) {
            tft.setCursor(25, 168);
            tft.println(res1);
            tft.setCursor(112, 168);
            tft.println(res2);
          } else if (howmany == 1) {
            tft.setCursor(25, 168);
            tft.println(res1);
          }
          tft.setTextSize(4);
          tft.setCursor(280, 163);
          tft.println(res4);
          tft.setTextSize(2);
          tft.setTextColor(WHITE);
          tft.setCursor(277, 130);
          tft.println(F("+1"));
          tft.setTextSize(1);
          tft.setCursor(272, 208);
          tft.println(F("Tap to"));
          tft.setCursor(279, 218);
          tft.println(F("Roll"));
          break;
        }
        break;
    
  default:
    Serial.println(F("Error!"));
    break;
    }
  }
else {
  switch (which) {
    case 0:
      if (allowedDice == 4) {
        tft.fillRect(3, 153, 85, 85, color);
        tft.drawRect(3, 153, 85, 85, border);
        tft.fillRect(90, 153, 85, 85, color);
        tft.drawRect(90, 153, 85, 85, border);
        tft.fillRect(177, 153, 85, 85, color);
        tft.drawRect(177, 153, 85, 85, border);
        tft.fillRect(264, 153, 50, 50, color);
        tft.drawRect(264, 153, 50, 50, border);
        tft.setTextSize(2);
        tft.setTextColor(WHITE);
        tft.setCursor(277, 130);
        tft.println(F("+1"));
        tft.setTextSize(1);
        tft.setCursor(272, 208);
        tft.println(F("Tap to"));
        tft.setCursor(279, 218);
        tft.println(F("Roll"));
        break;
      }
      else if (allowedDice == 3) {
        tft.fillRect(3, 153, 85, 85, color);
        tft.drawRect(3, 153, 85, 85, border);
        tft.fillRect(90, 153, 85, 85, color);
        tft.drawRect(90, 153, 85, 85, border);
        tft.fillRect(264, 153, 50, 50, color);
        tft.drawRect(264, 153, 50, 50, border);
        tft.setTextSize(2);
        tft.setTextColor(WHITE);
        tft.setCursor(277, 130);
        tft.println(F("+1"));
        tft.setTextSize(1);
        tft.setCursor(272, 208);
        tft.println(F("Tap to"));
        tft.setCursor(279, 218);
        tft.println(F("Roll"));
        break;
      }
      else if (allowedDice == 2) {
        tft.fillRect(3, 153, 85, 85, color);
        tft.drawRect(3, 153, 85, 85, border);
        tft.fillRect(90, 153, 85, 85, color);
        tft.drawRect(90, 153, 85, 85, border);
        break;
      }
      else if (allowedDice == 1) {
        tft.fillRect(3, 153, 85, 85, color);
        tft.drawRect(3, 153, 85, 85, border);
        break;
      }
      break;

    case 1:
      res4 = rollDRG(colorIndex);
      if (allowedDice == 4) {
        tft.fillRect(3, 153, 85, 85, color);
        tft.drawRect(3, 153, 85, 85, border);
        tft.fillRect(90, 153, 85, 85, color);
        tft.drawRect(90, 153, 85, 85, border);
        tft.fillRect(177, 153, 85, 85, color);
        tft.drawRect(177, 153, 85, 85, border);
        tft.fillRect(264, 153, 50, 50, color);
        tft.drawRect(264, 153, 50, 50, border);
        break;
      } else if (allowedDice == 3)  {
        tft.fillRect(3, 153, 85, 85, color);
        tft.drawRect(3, 153, 85, 85, border);
        tft.fillRect(90, 153, 85, 85, color);
        tft.drawRect(90, 153, 85, 85, border);
        tft.fillRect(264, 153, 50, 50, color);
        tft.drawRect(264, 153, 50, 50, border);
        break;
      }
      break;
    default:
      Serial.println(F("Error!"));
      break;
  }
}

tft.setTextColor(BLACK);
tft.setTextSize(4);
tft.setCursor(15, 105);
tft.println(F("1"));
tft.setCursor(59, 105);
tft.println(F("2"));
tft.setCursor(105, 105);
tft.println(F("3"));

}





void redrawDnD(int which = 0, int count = 1, bool roll = false) {
  uint16_t color = WHITE;
  uint16_t border = BLACK;
  char buf[12];
  tft.fillRect(3, 153, 313, 85, color);
  tft.drawRect(3, 153, 313, 85, border);
  tft.fillRect(3, 98, 43, 43, WHITE);
  tft.fillRect(48, 98, 43, 43, WHITE);
  tft.fillRect(93, 98, 43, 43, WHITE);
  tft.setTextColor(BLACK);
  tft.setTextSize(4);
  tft.setCursor(15, 105);
  tft.println(count);
  tft.setCursor(59, 105);
  tft.println("+");
  tft.setCursor(105, 105);
  tft.println("-");

  tft.setCursor(149, 109);
  tft.setTextSize(3);
  tft.println("Roll");
  if (roll == true) {
    res1 = rollDnD(which, count);
    tft.setTextSize(8);
    tft.setCursor(25, 170);

    tft.println(itoa(res1, buf, 10));
  }
  tft.setTextColor(BLACK);
  tft.setTextSize(4);
  tft.setCursor(15, 105);
  tft.println(count);
  tft.setCursor(59, 105);
  tft.println(F("+"));
  tft.setCursor(105, 105);
  tft.println(F("-"));
}

void menuHandler(int x = 0, int y = 0) {
  // Syntax: Start X (left), Start Y (top), Size X, Size Y
  int quickBtn1[4] = {3, 27, 313, 50}; // Top row button of quick menus
  int quickBtn2[4] = {3, 80, 313, 150}; // Bottom button of quick menus
  int quickTxt1[2] = {115, 38};
  int quickTxt2[2] = {64, 120};
  // Red, Green, Blue, Yellow, Black, Orange, Gray
  int validCount[7] = {3, 4, 3, 4, 2, 1, 1};
  if (debug == true) {
    tft.setTextSize(1);
    tft.setCursor(0, 0);
    tft.setTextColor(RED);
    tft.fillRect(0, 0, 320, 8, BLACK);
    String test = String(String(x) + ":" + String(y));
    tft.println(test);
  }
  switch (menuIndex) {
    case 0:
      // Main menu
      if ((x >= 10 && x <= 235) && (y >= 30  && y <= 90)) {
        showMenu(1);
        break;
      } else if ((x >= 10 && x <= 235) && (y >= 110  && y <= 160)) {
        showMenu(2);
        break;
      } else if ((x >= 10 && x <= 235) && (y >= 170  && y <= 230)) {
        showMenu(3);
        break;
      } else if ((x >= 10 && x <= 235) && (y >= 240  && y <= 290)) {
        showMenu(4);
        break;
      }
      break;
    case 1:
      // Coin toss

      if ((x >= 3 && x <= 240) && (y >= 30  && y <= 100)) {
        tft.fillRect(quickBtn2[0], quickBtn2[1], quickBtn2[2], quickBtn2[3], WHITE);
        tft.drawRect(quickBtn2[0], quickBtn2[1], quickBtn2[2], quickBtn2[3], BLACK);
        tft.setTextColor(BLACK);
        tft.setTextSize(8);
        tft.setCursor(quickTxt2[0], quickTxt2[1]);
        if (throwCoin() == true) {
          tft.println("Head");
        }
        else {
          tft.println("Tail");
        }
      }
      break;
    case 2:
      // D6
      if ((x >= 3 && x <= 240) && (y >= 30  && y <= 100)) {
        tft.fillRect(quickBtn2[0], quickBtn2[1], quickBtn2[2], quickBtn2[3], WHITE);
        tft.drawRect(quickBtn2[0], quickBtn2[1], quickBtn2[2], quickBtn2[3], BLACK);
        tft.setTextColor(BLACK);
        tft.setTextSize(8);
        tft.setCursor(quickTxt2[0] + 74, quickTxt2[1]);
        tft.println(throwDice());
      }
      break;
    case 3:
      // DnD
      if ((x >= 210 && x <= 240) && (y >= 35  && y <= 90)) {
        // d4
        diceIndex = 0;
        redrawDnD(diceIndex, diceCount, false);
        break;
      } else if ((x >= 175 && x <= 205) && (y >= 35  && y <= 100)) {
        // d6
        diceIndex = 1;
        redrawDnD(diceIndex, diceCount, false);
        break;
      }
      else if ((x >= 140 && x <= 172) && (y >= 35  && y <= 100)) {
        // d8
        diceIndex = 2;
        redrawDnD(diceIndex, diceCount, false);
        break;
      }
      else if ((x >= 105 && x <= 137) && (y >= 35  && y <= 100)) {
        // d10
        diceIndex = 3;
        redrawDnD(diceIndex, diceCount, false);
        break;
      }
      else if ((x >= 70 && x <= 102) && (y >= 35  && y <= 100)) {
        // d12
        diceIndex = 4;
        res1 = "0";
        redrawDnD(diceIndex, diceCount, false);
        break;
      }
      else if ((x >= 35 && x <= 67) && (y >= 35  && y <= 100)) {
        // d16
        diceIndex = 5;
        res1 = "0";
        redrawDnD(diceIndex, diceCount, false);
        break;
      }
      else if ((x >= 3 && x <= 32) && (y >= 35  && y <= 100)) {
        // d20
        diceIndex = 6;
        res1 = "0";
        redrawDnD(diceIndex, diceCount, false);
        break;
      }

      else if ((x >= 210 && x <= 240) && (y >= 130  && y <= 190)) {
        // 1 die
        diceCount = 1;
        redrawDnD(diceIndex, diceCount, false);
        break;
      }
      else if ((x >= 175 && x <= 205) && (y >= 130  && y <= 190)) {
        // +
        diceCount++;
        if (diceCount > 9) {
          diceCount = 9;
        }
        redrawDnD(diceIndex, diceCount, false);
        break;
      }
      else if ((x >= 140 && x <= 172) && (y >= 130  && y <= 190)) {
        // -
        diceCount--;
        if (diceCount < 1) {
          diceCount = 1;
        }
        redrawDnD(diceIndex, diceCount, false);
        break;
      }
      else if ((x >= 70 && x <= 137) && (y >= 130  && y <= 190)) {
        // Roll
        redrawDnD(diceIndex, diceCount, true);
        break;
      }
      break;



    case 4:
      // DRG
      //int drgSel = 0;
      //int drgDiceCount = 1;
      if ((x >= 210 && x <= 240) && (y >= 35  && y <= 90)) {
        // RED
        drgSel = 0;
        redrawDRG(0, drgSel, false, drgDiceCount);
        break;
      } else if ((x >= 175 && x <= 205) && (y >= 35  && y <= 100)) {
        // GREEN
        drgSel = 1;
        redrawDRG(0, drgSel, false, drgDiceCount);
        break;
      }
      else if ((x >= 140 && x <= 172) && (y >= 35  && y <= 100)) {
        // BLUE
        drgSel = 2;
        redrawDRG(0, drgSel, false, drgDiceCount);
        break;
      }
      else if ((x >= 105 && x <= 137) && (y >= 35  && y <= 100)) {
        // YELLOW
        drgSel = 3;
        redrawDRG(0, drgSel, false, drgDiceCount);
        break;
      }
      else if ((x >= 70 && x <= 102) && (y >= 35  && y <= 100)) {
        // BLACK
        drgSel = 4;
        res1 = "0";
        redrawDRG(0, drgSel, false, drgDiceCount);
        break;
      }
      else if ((x >= 35 && x <= 67) && (y >= 35  && y <= 100)) {
        // ORANGE
        drgSel = 5;
        res1 = "0";
        redrawDRG(0, drgSel, false, drgDiceCount);
        break;
      }
      else if ((x >= 3 && x <= 32) && (y >= 35  && y <= 100)) {
        // SILVER
        drgSel = 6;
        res1 = "0";
        redrawDRG(0, drgSel, false, drgDiceCount);
        break;
      }
      // validCount[7] = {4, 4, 4, 4, 2, 1, 1};
      else if ((x >= 210 && x <= 240) && (y >= 130  && y <= 190)) {
        // 1 die
        drgDiceCount = 1;
        redrawDRG(0, drgSel, false, drgDiceCount);
        break;
      }
      else if ((x >= 175 && x <= 205) && (y >= 130  && y <= 190)) {
        // 2 dies
        drgDiceCount = 2;
        redrawDRG(0, drgSel, false, drgDiceCount);
        break;
      }
      else if ((x >= 140 && x <= 172) && (y >= 130  && y <= 190)) {
        // 3 dies
        drgDiceCount = 3;
        redrawDRG(0, drgSel, false, drgDiceCount);
        break;
      }
      else if ((x >= 70 && x <= 137) && (y >= 130  && y <= 190)) {
        // Roll
        redrawDRG(0, drgSel, true, drgDiceCount);
        break;
      }


      else if ((x >= 3 && x <= 40) && (y >= 205  && y <= 280)) {
        // 1 extra dice
        // If we are allowed to roll an extra dice
        if (validCount[drgSel] >= 3) {
          redrawDRG(1, drgSel, true, drgDiceCount);
        }
        break;
      }

      break;
    case 5:
      // Info
      delay(3000);
      showMenu(0);
      break;
    default:
      Serial.println(F("OOPS!"));
      break;
  }
}


void setup() {

  Serial.begin(9600);
  randomSeed(analogRead(0));

  if (analogRead(0) == 891) {
    seed = random(13, 2910343);
    randomSeed(seed);
  }
  else if (analogRead(0) == 892) {
    seed = random(5, 91498515);
    randomSeed(seed);
  }
  else if (analogRead(0) == 893) {
    seed = random(54, 55450865);
    randomSeed(seed);
  }
  else {
    seed = random(1, 99999999);
    randomSeed(seed);
  }
  //Serial.print("Random seed is ");
  //Serial.println(seed);
  Serial.println(F("Initializing"));
  tft.reset();
  tft.begin(0x9341);
  tft.setRotation(3); // Landscape mode
  // Init screen with black to get rid of any previous data
  tft.fillScreen(BLACK);
  uint16_t identifier = tft.readID();
  /*
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    progmemPrint(PSTR("Initializing SD card..."));
    if (!SD.begin(SD_CS)) {
    progmemPrintln(PSTR("failed!"));
    noimg = true;
    tft.setCursor(0, 0);
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.println("Oops, SD card init failed!");
    tft.setCursor(0, 30);
    tft.println("Please tap bottom right");
    tft.setCursor(0, 60);
    tft.println("corner of the panel!");
    return;
    }
    progmemPrintln(PSTR("OK!"));
    if (debug == false) {
    bmpDraw("logo.bmp", 0, 0);
    delay(3000);
    }
  */
  tft.fillScreen(BLACK);
  showMenu(0);
}

int buttonRead(int x = 0, int y = 0) {
  //Serial.print("Received "); Serial.print(x); Serial.print(","); Serial.println(y);
  if (y < (TS_MINY - 5)) {
    if ( (HOME - 50) <= x && x <= (HOME + 50)) {
      //Serial.println("HOME button pressed");
      return 1;
    }
    /*
      else if ( (BOOK - 50) <= x && x <= (BOOK + 50) ) {
      //Serial.println("BOOK button pressed");
      return 2;
      }

      else if ( (MSG - 50) <= x && x <= (MSG + 50) ) {
      Serial.println("MSG button pressed");
      return 3;
      }
      else if ( (CAM - 50) <= x && x <= (CAM + 50) ) {
      Serial.println("CAM button pressed");
      return 4;
      }*/
    else if ( (SCR - 50) <= x && x <= (SCR + 50) ) {
      //Serial.println("SCR button pressed");
      return 5;
    }
    else {
      //Serial.println("No buttons pressed");
      return 0;
    }
  }
  return 0;
}

int tmpX, tmpY, tmpZ;
void loop() {
  // put your main code here, to run repeatedly:
  unsigned long timer = millis();
  TSPoint p = ts.getPoint();

  // Change the direction of the pin if you are sharing the pins with other things
  //pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  //pinMode(YM, OUTPUT);
  if (timer - prevTimer >= interval) {
    prevTimer = timer;
    seed = ( ((p.x + p.y * (p.x * p.y) ^ (p.z)) * p.z) ^ random(0, 99999999)); randomSeed(seed);
    Serial.println(seed);
  }
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
    tmpX = p.x;
    tmpY = p.y;
    tmpZ = p.z;
    //Serial.print(p.x);
    //Serial.print(",");
    //Serial.println(p.y);
    int test = buttonRead(p.x, p.y);
    switch (test) {
      case 1:
        showMenu(0);
        break;
      /*
        case 2:
        showMenu(2);
        break;
        case 3:
        showMenu(3);
        break;
        case 4:
        showMenu(4);
        break;
      */
      case 5:
        showMenu(5);
        break;
    }
    p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
    p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());
    menuHandler(p.y, p.x);
  }


  // Touch handler goes here
}

/*
  // This function opens a Windows Bitmap (BMP) file and
  // displays it at the given coordinates.  It's sped up
  // by reading many pixels worth of data at a time
  // (rather than pixel by pixel).  Increasing the buffer
  // size takes more of the Arduino's precious RAM but
  // makes loading a little faster.  20 pixels seems a
  // good balance.

  #define BUFFPIXEL 4

  void bmpDraw(char *filename, int x, int y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3 * BUFFPIXEL]; // pixel in buffer (R+G+B per pixel)
  uint16_t lcdbuffer[BUFFPIXEL];  // pixel out buffer (16-bit per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();
  uint8_t  lcdidx = 0;
  boolean  first = true;

  if ((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  progmemPrint(PSTR("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    progmemPrintln(PSTR("File not found"));
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) == 0x4D42) { // BMP signature
    progmemPrint(PSTR("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    progmemPrint(PSTR("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    progmemPrint(PSTR("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if (read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      progmemPrint(PSTR("Bit Depth: ")); Serial.println(bmpDepth);
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        progmemPrint(PSTR("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width())  w = tft.width()  - x;
        if ((y + h - 1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x + w - 1, y + h - 1);

        for (row = 0; row < h; row++) { // For each scanline...
          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if (bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col = 0; col < w; col++) { // For each column...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              // Push LCD buffer to the display first
              if (lcdidx > 0) {
                tft.pushColors(lcdbuffer, lcdidx, first);
                lcdidx = 0;
                first  = false;
              }
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = tft.color565(r, g, b);
          } // end pixel
        } // end scanline
        // Write any remaining data to LCD
        if (lcdidx > 0) {
          tft.pushColors(lcdbuffer, lcdidx, first);
        }
        progmemPrint(PSTR("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if (!goodBmp) progmemPrintln(PSTR("BMP format not recognized."));
  }

  // These read 16- and 32-bit types from the SD card file.
  // BMP data is stored little-endian, Arduino is little-endian too.
  // May need to reverse subscript order if porting elsewhere.

  uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
  }

  uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
  }
*/
// Copy string from flash to serial port
// Source string MUST be inside a PSTR() declaration!
void progmemPrint(const char *str) {
  char c;
  while (c = pgm_read_byte(str++)) Serial.print(c);
}

// Same as above, with trailing newline
void progmemPrintln(const char *str) {
  progmemPrint(str);
  Serial.println();
}
