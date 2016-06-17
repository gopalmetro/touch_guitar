/*

The audio board uses the following pins:
6 - MEMCS
7 - MOSI
9 - BCLK
10 - SDCS
11 - MCLK
12 - MISO
13 - RX
14 - SCLK
15 - VOL
18 - SDA
19 - SCL
22 - TX
23 - LRCLK

The touch panel uses the following pins:
VIN - VCC
GND - GND
21  - CS
+3V - RESET
20  - D/C
7   - SDI (MOSI)
14  - SCK
VIN - LED
12  - SDO (MISO)
14  - T_CLK
8   - T_CS
7   - T_DIN
12  - T_DO
-   - T_IRQ // Not Used


USEFUL FUNCTIONS
AudioProcessorUsage()
AudioProcessorUsageMax()
AudioProcessorUsageMaxReset()
AudioMemoryUsage()
AudioMemoryUsageMax()
AudioMemoryUsageMaxReset()

The CPU usage is an integer from 0 to 100, and the memory is from 0 to however
many blocks you provided with AudioMemory().

*/


/* === LIBRARIES === */
#include <Audio.h> //Audio
#include <Wire.h>
#include <SD.h> //GOPAL - Don't think we need this
#include <SerialFlash.h>
#include <Bounce.h>  //GOPAL - Don't think we need this
#include <ILI9341_t3.h> //GOPAL - Touchscreen
#include <font_Arial.h> // from ILI9341_t3 //GOPAL - Touchscreen
#include <XPT2046_Touchscreen.h> //GOPAL - Touchscreen
#include <SPI.h>

/* === TOUCHSCREEN === */
// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 150
#define TS_MINY 130
#define TS_MAXX 3800
#define TS_MAXY 4000

#define CS_PIN  8
// For optimized ILI9341_t3 library
#define TFT_DC      20
#define TFT_CS      21
#define TFT_RST    255  // 255 = unused, connect to 3.3V
#define TFT_MOSI     7
#define TFT_SCLK    14
#define TFT_MISO    12
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);

XPT2046_Touchscreen ts(CS_PIN);  // Param 2 - NULL - No interrupts
//XPT2046_Touchscreen ts(CS_PIN, 255);  // Param 2 - 255 - No interrupts
#define TIRQ_PIN  2 //Touch IRQ Pin for interrupt enabled polling - NOT NEEDED
//XPT2046_Touchscreen ts(CS_PIN, TIRQ_PIN);  // Param 2 - Touch IRQ Pin - interrupt enabled polling

/* === TOUCHSCREEN GRAPHICS === */
#define ROTATION 3

//BUTTON DEFINITIONS
#define FRAME_W 100
#define FRAME_H 50

void buttonBuilder(char btnName, int startingPointX, int startingPointY, int frameWidth, int frameHeight) {
    tft.drawRect(startingPointX, startingPointY, frameWidth, frameHeight, ILI9341_BLUE);    
}


//CHORUS BUTTON
#define CHORUSFRAME_X 210
#define CHORUSFRAME_Y 180

#define CHORUSOFF_X CHORUSFRAME_X
#define CHORUSOFF_Y CHORUSFRAME_Y
#define CHORUSOFF_W (FRAME_W/2)
#define CHORUSOFF_H FRAME_H

#define CHORUSON_X (CHORUSOFF_X + CHORUSOFF_W)
#define CHORUSON_Y CHORUSFRAME_Y
#define CHORUSON_W (FRAME_W/2)
#define CHORUSON_H FRAME_H


/* === AUDIO EFFECTS === */
// Number of samples in each chorus line
#define CHORUS_DELAY_LENGTH (75*AUDIO_BLOCK_SAMPLES) //GOPAL - MULTIPLIER CHANGES EFFECT

// Allocate the chorus lines for left and right channels
short chorusDelayLine[CHORUS_DELAY_LENGTH];

// number of "voices" in the chorus which INCLUDES the original voice
int n_chorus = 3;

//Select Mic or Line input
//const int myInput = AUDIO_INPUT_MIC;
const int myInput = AUDIO_INPUT_LINEIN;


/* === AUDIO COMPONENTS === */
// http://www.pjrc.com/teensy/gui/

AudioInputI2S            inFromPickups;   //xy=102.00003051757812,86.22222328186035  // audio shield: mic or line-in
AudioSynthWaveform       waveform;        //xy=129.00003051757812,265.22222328186035
AudioMixer4              bridgeNeckMixer; //xy=131.00003051757812,177.22222328186035
AudioEffectFlange        flange;          //xy=361.0000305175781,149.22222328186035
AudioEffectChorus        chorus;          //xy=363.0000305175781,93.22222328186035
AudioEffectMultiply      multiply;        //xy=366.0000305175781,258.22222328186035
AudioEffectBitcrusher    bitcrusher;      //xy=372.0000305175781,204.22222328186035
AudioMixer4              fxMixer;         //xy=564.0000305175781,177.22222328186035
AudioMixer4              masterVol; //xy=772.7778015136719,195.55556297302246
AudioFilterBiquad        biquad1;
AudioOutputI2S           outToAmp;        //xy=939.7778015136719,194.55556297302246  // audio shield: headphones & line-out

/* === AUDIO ROUTING === */
// Create Audio connections between the components

// Both channels of the audio input go to the chorus effect
AudioConnection          patchCord1(inFromPickups, 0, bridgeNeckMixer, 0);
AudioConnection          patchCord2(inFromPickups, 1, bridgeNeckMixer, 1);
AudioConnection          patchCord3(waveform, 0, multiply, 1);
AudioConnection          patchCord4(bridgeNeckMixer, chorus);
AudioConnection          patchCord5(bridgeNeckMixer, flange);
AudioConnection          patchCord6(bridgeNeckMixer, bitcrusher);
AudioConnection          patchCord7(bridgeNeckMixer, 0, multiply, 0);
AudioConnection          patchCord8(flange, 0, fxMixer, 1);
AudioConnection          patchCord9(chorus, 0, fxMixer, 0);
AudioConnection          patchCord10(multiply, 0, fxMixer, 3);
AudioConnection          patchCord11(bitcrusher, 0, fxMixer, 2);
AudioConnection          patchCord12(fxMixer, 0, masterVol, 0);
AudioConnection          patchCord13(masterVol, 0, biquad1, 0);
AudioConnection          patchCord14(biquad1, 0, outToAmp, 0);
AudioConnection          patchCord15(biquad1, 0, outToAmp, 1);


/* === ACTIVATE AUDIO SHIELD === */
AudioControlSGTL5000 audioShield;

// VOLUME CONTROL
float masterVolume = 0;

/* === TOUCH SCREEN GLOBAL VARIABLES === */
boolean wastouched = true; //For touchscreenTest()
boolean ChorusOn = false; //For touchscreen

/* === UTILITY GLOBAL VARIABLES === */
// audio volume
int volume = 0;

// timer for mem-test
unsigned long last_time = millis();


/* === CUSTOM FUNCTIONS === */

void touchscreenSetup() {
  tft.begin();
  if (!ts.begin()) {
      Serial.println("Unable to start touchscreen."); //SANITY CHECK
  }
  else {
      Serial.println("Touchscreen started."); //SANITY CHECK
  }

  tft.fillScreen(ILI9341_BLACK);
  // origin = left,top landscape (USB left upper)
  tft.setRotation(ROTATION);
  turnChorusOffBtn();
}

void touchscreen() {
    //Serial.println("Inside touchscreen();");
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    while ( ! ts.bufferEmpty() ) {
        p = ts.getPoint();
    }

    // REMAP TOUCHSCREEN FOR ROTATION(3)
    p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
    p.y = map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);

     if (ChorusOn) {
      if((p.x > CHORUSOFF_X) && (p.x < (CHORUSOFF_X + CHORUSOFF_W))) {
          if ((p.y > CHORUSOFF_Y) && (p.y <= (CHORUSOFF_Y + CHORUSOFF_H))) {
            Serial.println("turnChorusOffBtn()");
            turnChorusOffBtn();
          }
      }
    } else { //Record is off (ChorusOn == false)
        if((p.x > CHORUSON_X) && (p.x < (CHORUSON_X + CHORUSON_W))) {
            if ((p.y > CHORUSON_Y) && (p.y <= (CHORUSON_Y + CHORUSON_H))) {
              Serial.println("turnChorusOnBtn()");
              turnChorusOnBtn();
            }
        }
    }
  }
  //Serial.println(ChorusOn);
}

// DRAW THE WINDOW
void drawFrame() {
    tft.drawRect(CHORUSFRAME_X, CHORUSFRAME_Y, FRAME_W, FRAME_H, ILI9341_BLUE);
}

// DRAW THE RED BUTTON
void turnChorusOffBtn() {
    tft.fillRect(CHORUSOFF_X, CHORUSOFF_Y, CHORUSOFF_W, CHORUSOFF_H, ILI9341_RED);
    tft.fillRect(CHORUSON_X, CHORUSON_Y, CHORUSON_W, CHORUSON_H, ILI9341_BLACK);
    drawFrame();
    tft.setCursor(CHORUSON_X + 6 , CHORUSON_Y + (CHORUSON_H/2));
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.println("ON");

    //Print "Chorus"
    tft.setCursor(CHORUSOFF_X, CHORUSOFF_Y - (CHORUSOFF_H / 2));
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.println("CHORUS");
    
    // TURNS CHORUS EFFECT OFF
    chorus.voices(0);
    bridgeNeckMixer.gain(0, .5);
    bridgeNeckMixer.gain(1, .5);
 
    // SETS CHORUS FLAG   
    ChorusOn = false;
}

// DRAW THE GREEN BUTTON
void turnChorusOnBtn() {
    tft.fillRect(CHORUSON_X, CHORUSON_Y, CHORUSON_W, CHORUSON_H, ILI9341_GREEN);
    tft.fillRect(CHORUSOFF_X, CHORUSOFF_Y, CHORUSOFF_W, CHORUSOFF_H, ILI9341_BLACK);
    drawFrame();
    tft.setCursor(CHORUSOFF_X + 6 , CHORUSOFF_Y + (CHORUSOFF_H/2));
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.println("OFF");
    
    // TURNS CHORUS EFFECT ON
    chorus.voices(n_chorus);
    bridgeNeckMixer.gain(0, 1);
    bridgeNeckMixer.gain(1, 1);
    
    
    // SETS CHORUS FLAG 
    ChorusOn = true;
}

// VOLUME CONTROL - CURRENTLY CONFIGURED FOR HEADPHONES; NEED A "MIXER" DEVICE FOR FX CONTROL
void volumeControl() {
    // Volume control
    int n = analogRead(15);

    if (n != volume) {
        volume = n;
        masterVolume = (float)n / 1023;
        if (masterVolume >= 1) {
          masterVolume = 1;
        }
        
        if (masterVolume <= 0) {
          masterVolume = 0;
        }
        Serial.print("masterVolume = ");Serial.println(masterVolume);
        masterVol.gain(0, masterVolume );
        
        //audioShield.volume( ( (float)n / 1023 ));
    }
}

// CHECK ON THE STATUS OF THE BOARD
void memReport() {
    if(0) {
        if(millis() - last_time >= 5000) {
            Serial.print("Proc = ");
            Serial.print(AudioProcessorUsage());
            Serial.print(" (");
            Serial.print(AudioProcessorUsageMax());
            Serial.print("),  Mem = ");
            Serial.print(AudioMemoryUsage());
            Serial.print(" (");
            Serial.print(AudioMemoryUsageMax());
            Serial.println(")");
            last_time = millis();
        }
    }
}

/* === BEGIN SETUP FUNCTION === */
void setup() {

    Serial.begin(38400); //Begin serial communiction
    while (!Serial && (millis() <= 1000)); //Wait at least 1 second and until serial is active

    touchscreenSetup();

    // Define the Audio Memory
    AudioMemory(8);

    // Activate the Audio Shield
    audioShield.enable(); // Enable shield
    audioShield.inputSelect(myInput); // Set input
    //audioShield.volume(0.65); // Set headphone output volume (not needed)
    masterVol.gain(0, 0.65); // SET OUTPUT VOLUME TO 65% (this output affect headphone out)
        
    if(!chorus.begin(chorusDelayLine, CHORUS_DELAY_LENGTH, n_chorus)) {
      Serial.println("AudioEffectChorus - left channel begin failed");
      while(1);
    }
    
    chorus.voices(0);
    
    //FILTER OUT NASTY SQUEAL
    biquad1.setLowpass(0, 800, 0.707);

    Serial.println("setup done");
    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
}


/* === BEGIN LOOP === */
void loop() {
    touchscreen();
    volumeControl();
    memReport();
}
