/* ******************************************************************
    TeleBall (aka BreakOut on 8x8 Matrix)
    
    made in August .. December 2014
    
    * idea, code and original circurit design by sy2002
    * additional circurit design and board layout by doubleflash
    * body housing/case by Miro
    
    License: You are free to share and adapt for any purpose,
    even commercially as long as you attribute to sy2002 and
    link to http://www.sy2002.de
    
    http://creativecommons.org/licenses/by/4.0/    
   ****************************************************************** */
   
//#define DUINOKIT

/* ******************************************
    Libraries
   ****************************************** */
   
//MAX7221 LED control library
//information: http://playground.arduino.cc/Main/LedControl
//download:    https://github.com/wayoda/LedControl
#include "LedControl.h"

//NRF24L01+ radio driver class
//information & dl: http://tmrh20.github.io/RF24/
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"


/* ******************************************
    Factory default values
   ****************************************** */

enum GameOrientation
{
    goRegular,
    goPaddleRight,
    goUpsideDown,
    goPaddleLeft
};

enum GameMode
{
    //games
    gmBreakOut,
    gmTennis,
    
    //standard configuration menu
    gmSpeed,
    gmBrightness,
    
    //advanced configuration menu
    gmPaddleLeft,
    gmPaddleRight
};

//defaults that are set, when "back to factory default" is selected in the extended menu
const GameMode        game_mode_default = gmBreakOut;    //first game to be started
const int             speed_default = 30;                //speed of the game
const int             intensity_default = 2;             //brightness of the 8x8 matrix
const int             poti_leftmost_default = 200;       //restrict poti range to the left for a more natural feeling
const int             poti_rightmost_default = 823;      //... dito to the right
const GameOrientation orientation_default = goRegular;   //game orientation
const unsigned long   respawn_duration = 1500;           //milliseconds to wait after a ball got lost
const byte            Balls_max = 3;                     //maximum amount of balls in BreakOut
const byte            Tennis_win = 3;                    //amount of points needed to win tennis

//button press duration in milliseconds
const unsigned int    UniversalButtonPressedShort = 100;     //reset game / select menu item
const unsigned int    UniversalButtonPressedLong = 750;      //enter regular menu
const unsigned int    UniversalButtonPressedVeryLong = 4000; //enter advanced menu

//enter multiplayer mode
const unsigned int    MultiplayerQuestionMax = 2000;         //how long is the question mark shown, before the user can choose
boolean               MultiplayerQuestionButton = false;     //universal button pressed during rmMaster_init or rmSlave_init

//the speed setting heavily depends on the used processor type
//on my Arduino Nano, speed_max = 10 and speed_min = 60 seem reasonable values.
//if you're having a faster machine, it makes sense to increase those values
const int speed_max = 10;                      //amount of screen refresh cycles that are wasted...
const int speed_min = 60;                      //...until the ball moves on one pixel

const byte            LED_count = 3;                    //amount of LEDs installed in the device

//needs to be declared here to access the GameMode enum (strange compiler behaviour)
void calculateBallMovement(GameMode nowplaying);
void calculatePaddleImpact(GameMode nowplaying, int& paddle, int& last_paddle_pos);

/* ******************************************
    Global Game Variables
   ****************************************** */

GameOrientation Orientation = orientation_default;

int Speed = speed_default;                     //current speed of the game
int Speed_Old = Speed;

int Paddle = 0;                                //current x-position of paddle
int Paddle_Old = Paddle;
int Paddle_Remote = 0;
int Paddle_Remote_Old = Paddle_Remote;

byte Intensity = intensity_default;            //brightness of the display
byte Intensity_Old = Intensity;                

//poti range restriction for a more natural game feeling
int PotiLeftmost = poti_leftmost_default;
int PotiRightmost = poti_rightmost_default;

//current ball position
char BallX;                                 
char BallY;
char BallX_Old = BallX;
char BallY_Old = BallY;

//current ball speed in x and y direction
//note: the "char" variable can be negative, so -1 in BallDX means,
//that it moves to the left
char BallDX = 0;
char BallDY = 0;
char BallDX_tbs = 0;
char BallDY_tbs = 0;

//remember old values during configuration
byte rBallX, rBallY, rBallX_Old, rBallY_Old;
char rBallDX, rBallDY, rBallDX_tbs, rBallDY_tbs;

//flag, if a full reset of the game is to be performed
boolean perform_reset = true;

//flag, to distinguish between gameplay and the various configuration modes    
GameMode game_mode = game_mode_default;
GameMode game_mode_old = game_mode;

//flag to determin, if the game is currently in a loop state due to won or lost
boolean WonOrLostState = false;

//BreakOut only: amount of balls left
byte Balls;
byte Balls_Old = 0;

//Tennis only: points
char TennisPoints = 0;
char TennisPoints_Old = -1;
char TennisPoints_Remote = 0;

//the timer is used to de-couple the paddle movement from the ball-speed
int timer = 0;

//the respawn_timer is used to wait until the next ball appears after you lose one
unsigned long respawn_timer = 0;

//used for measuring a quick paddle movement right before the ball hits the paddle
//to give the ball an extra spin; this is needed in situations, where you otherwise
//would not be able to clear all "bricks".
int LastPaddlePos = 0;
int LastPaddlePos_Remote = 0;
const byte PaddleSpeedThreshold = 1;

//universal button incl. debouncing
unsigned long UniversalButtonPressedStartTime = 0;
boolean UniversalButton_firstcontact = true;

unsigned long MultiplayerQuestionTime = 0;
unsigned long MultiplayerWaitStart = 0;

//breakout: screen memory for storing the current level
byte bricks[8][8];

/* ******************************************
    MAX7221 connections to 8x8 LED matrix

    DP => ROW1        DG1 => COL1
    A  => ROW2        DG2 => COL2
    B  => ROW3        DG3 => COL3
      ...                ...    
    G  => ROW8        DG8 => COL8
    
    Additionally:
    
    1. Connect DIN, CLK, CS as shown below to digital inputs of the Arduino Nano
    2. Connect VCC to 5V and GND to GND of the Arduino Nano
   ****************************************** */
   
const byte DataIn = 2;    //D2 => DIN
const byte CLK    = 3;    //D3 => CLK
const byte LOAD   = 4;    //D4 => CS

//create a new object to control the 8x8 LED matrix
LedControl Matrix = LedControl(DataIn, CLK, LOAD, 1);


/* *************************************************
    10k potentiometer to A7: paddle
    unused analog pin for random seed
    PWM enabled digital pin for audio
    3 digital pins for remaining ball LEDs
    Universal Control Button
   ************************************************* */
   
const byte potPaddle     = A7;
const byte UnusedAnalog  = A6;

const byte PWM_Audio     = 9;      //D9: Speaker+ ("LS+")

const byte BallDisplay[LED_count] = {5, 6, 7}; //LED #1 at D5, LED #2 at D6, LED #3 at D7

const byte UniversalButton   = 8;  //D8: "Button+"


/* ******************************************
    Levels / Graphics / Patterns
   ****************************************** */

//this is the level pattern of the "bricks"
//modify to create tougher or easier levels
const byte bricks_levelheight = 3;
const byte bricks_reset[bricks_levelheight][8] =
{
    {1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1}
};

// :-) smiley shown, if you win the game
const byte smiley_won[8] = 
{
    0b00111100,
    0b01000010,
    0b10100101,
    0b10000001,
    0b10100101,
    0b10011001,
    0b01000010,
    0b00111100    
};

// :-| smiley shown, if you loose the game
const byte smiley_lost[8] = 
{
    0b00111100,
    0b01000010,
    0b10100101,
    0b10000001,
    0b10111101,
    0b10000001,
    0b01000010,
    0b00111100    
};

// checkerboard pattern for selecting brightness
const byte select_brightness[8] = 
{
    0b10101010,
    0b01010101,
    0b10101010,
    0b01010101,
    0b10101010,
    0b01010101,
    0b10101010,
    0b01010101
};

//left arrow: select leftmost paddle position
const byte arrow_left[8] = 
{
    0b00000000,
    0b00010000,
    0b00100000,
    0b01111110,
    0b00100000,
    0b00010000,
    0b00000000,
    0b00000000
};

//right arrow: select rightmost paddle position
const byte arrow_right[8] = 
{
    0b00000000,
    0b00001000,
    0b00000100,
    0b01111110,
    0b00000100,
    0b00001000,
    0b00000000,
    0b00000000
};

//question mark: enter multiplayer mode
const byte question_multiplayer[8] = 
{
    0b00011000,
    0b00000100,
    0b00011000,
    0b00100000,
    0b00011000,
    0b00000000,
    0b00001000,
    0b00000000
};

const byte yes_multiplayer[8] =
{
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00000000,
    0b00011000,
    0b00011000
};


/* *************************************************
    NRF24L01+ radio
   ************************************************* */

const byte RadioCE              = A0;
const byte RadioCS              = A1;
const byte RadioAddresses[][6]  = {"1TlBl", "2TlBl"};
const int RadioMasterToken      = 2309;    //initial negotiation: signal that this device is master
const int RadioSlaveToken       = 4711;    //initial negotiation: signal that this device is slave
const int RadioMasterWaitQ      = 2310;    //master waiting to start while slave still decides
const int RadioSlaveWaitA       = 4712;    //slave acknowledging that it is ready to play

const byte RadioPayloadSize     = 2;       //size of all send and receive packets, shal be: 2; maximum possible: 32

//milliseconds, until next Radio command
const int RadioCycle          = 300;
unsigned long Last_RadioCycle = 0;

//wait and listen randomly between RadioWaitMin and
//RadioWaitMax, before 
const int RadioWait_Min       = 1000;
const int RadioWait_Max       = 2000;
unsigned long RadioWait       = 0;

//create a new radio object
RF24 Radio(RadioCE, RadioCS);

enum
{
    rmNone         = -1,        //no other TeleBall device found
    rmMaster_init  = 0,         //about to enter Master mode
    rmSlave_init   = 1,         //about to enter Slave mode
    rmMaster_wait  = 2,         //waiting for Slave to start the game
    rmSlave_wait   = 3,         //waiting for Master to start the game
    rmMaster_run   = 4,         //running in Master mode    
    rmSlave_run    = 5          //running in Slave mode
} RadioMode = rmNone;

//sends "sendbuffer" to the receiver: returns true, if sending was successful and the ACK payload
//could be received, else it returns false
//needs preconfigured connections via Radio.openWritingPipe and a receiver, that acknowledges
//the successful receiving via sending a response payload which will be buffered in "ackpayloadbuffer"
//all buffers need to be at least "RadioPayloadSize" in size
boolean radioSend(void* sendbuffer, void* ackpayloadbuffer)
{
    //send payload
    if (Radio.write(sendbuffer, RadioPayloadSize))
    {       
        //if successfully sent, check if an ACK payload is available
        //(since Radio.write is a blocking function that waits for ACK, this should always work)
        if (Radio.available())
        {
            boolean success = false; //be conservative
            
            //use a loop to empty the read FIFO
            //just in case there is more than one ACK payload pending: take the newest
            while (Radio.available())
            {
                success = true;
                
                //retrieve the ACK payload
                Radio.read(ackpayloadbuffer, RadioPayloadSize);
            }
            
            //success only if the sending worked AND the ACK payload could be retrieved
            return success;
        }
    }
    
    //something failed, i.e. the sending itself or the reading of the payload
    return false;
}

/*
boolean radioReceive(void* receivebuffer, void* ackpayloadbuffer)
{
    byte pipeNo;
    if (Radio.available(&pipeNo))
    {
        while (Radio.available(&pipeNo))
            Radio.read(&payload, 2);
        Radio.writeAckPayload(pipeNo, &RadioSlaveWaitA, 2);
    }
    
    return false;
}
*/
//kind of battery and performance saving scanning and negotiation protocol
//that scans for another player/device and then negotiates: who is master and who is slave
//concept: the first who receives a master token is the slave and sends an ACK with a slave token
//due to the random sending intervals and the fact, that the NRF24L01+ buffers received data in
//a FIFO, this method is working very stable
void radioScanAndDetermineMode()
{
    //do the radio related operations only each RadioCycle milliseconds
    unsigned long Millis = millis();
    if (!Last_RadioCycle || Millis - Last_RadioCycle > RadioCycle)
    {
        Last_RadioCycle = Millis;
        
        //scan only, if no other device is detected
        if (RadioMode == rmNone)
        {
            //scan only each RadioWait milliseconds which is in a random interval
            if (!RadioWait)
                RadioWait = Millis + random(RadioWait_Min, RadioWait_Max);
            
            if (Millis >= RadioWait)
            {
                RadioWait = 0;
           
                byte pipeNo;
                unsigned int payload;

                //data received? if yes and if it is the master token, then this device is the slave
                //we use ACK payload mechanism to confirm by sending the slave token
                if (Radio.available(&pipeNo))
                {
                    while (Radio.available(&pipeNo))
                        Radio.read(&payload, 2);
                    Radio.writeAckPayload(pipeNo, &RadioSlaveToken, 2);
                    if (payload == RadioMasterToken)
                        RadioMode = rmSlave_init;
                }
            
                //no data received and random interval is over: send a master token
                //if the other side confirms by a slave token, then this device is the master
                else
                {                       
                    Radio.stopListening();
                    Radio.openWritingPipe(RadioAddresses[0]);
                    Radio.openReadingPipe(1, RadioAddresses[1]);                
                
                    if (Radio.write(&RadioMasterToken, 2))
                    {       
                        if (Radio.available())
                        {
                            while (Radio.available())
                                Radio.read(&payload, 2);
                            if (payload == RadioSlaveToken)
                                RadioMode = rmMaster_init;
                        }
                    }
                    
                    //if no ACK or wrong ACK: consider it as no device and start over
                    if (RadioMode == rmNone)
                    {
                        Radio.openWritingPipe(RadioAddresses[1]);
                        Radio.openReadingPipe(1, RadioAddresses[0]);                                    
                        Radio.startListening();
                    }
                }
            }      
        }
    }
}

boolean radioHandleMultiplayerQuestion()
{
    if (RadioMode == rmMaster_init || RadioMode == rmSlave_init)
    {   
        //show the question mark independent from the paddle position     
        if (MultiplayerQuestionTime == 0)
        {
            drawPatternBits(question_multiplayer, 8);        
            putPixel(RadioMode, 7, true);  //debug-info: show if this device is master (x-pos = 0) or slave (x-pos = 1)            
            MultiplayerQuestionTime = millis();
        }
        
        //use the paddle to select no (<= 3) or yes (> 3) and display "!" for yes and "?" for no
        if (millis() - MultiplayerQuestionTime > MultiplayerQuestionMax)
        {
            //no
            if (Paddle <= 3)
                drawPatternBits(question_multiplayer, 8);
            //yes
            else
                drawPatternBits(yes_multiplayer, 8);                
        }
        
        //user anwers the question: switch to new game mode
        if (MultiplayerQuestionButton)
        {
            MultiplayerQuestionButton = false;
            MultiplayerQuestionTime = 0;
            
            //just in case: empty write and read FIFOs
            Radio.flush_tx();
            byte pipeNo, NullDevice;        
            while (Radio.available())
                Radio.read(&NullDevice, 1);
            
            //deny multiplayer mode
            if (Paddle <= 3)
            {
                RadioMode = rmNone;
                Matrix.clearDisplay(0);                
                return false;
            }
            
            //enter multiplayer mode and play tennis
            else
            {
                if (RadioMode == rmMaster_init)
                    RadioMode = rmMaster_wait;
                else
                    RadioMode = rmSlave_wait;
                
                //switch to tennis
                game_mode = gmTennis;
                respawn_timer = millis() + (2 * respawn_duration);
                Speed = Speed_Old = 10 * speed_default;
                tennisRespawn();
            }
        }
        
        return true;               
    }    
    
    return false;
}


/* ******************************************
    SETUP
   ****************************************** */   

void setup()
{   
    Serial.begin(57600);
        
    //initialize seed with floating analog number
    randomSeed(analogRead(UnusedAnalog));
        
    //The MAX72XX is in power-saving mode on startup, we have to do a wakeup call
    Matrix.shutdown(0, false);
  
    //Set the brightness
    Matrix.setIntensity(0, Intensity);
    
    //set the digital ports of the Arduino
    //that we use for audio output and for
    //driving the LEDs to OUTPUT
    pinMode(PWM_Audio, OUTPUT);
    for (int i = 0; i < LED_count; i++)
        pinMode(BallDisplay[i], OUTPUT);
        
    //digital input for the Universal Button
    //activating the pullup means and wiring as described
    //above means, that the input will read HIGH when
    //the switch is open and LOW when the switch is pressed
    pinMode(UniversalButton, INPUT_PULLUP);
    
    //setup the nRF23L01+
    Radio.begin();
    Radio.setAutoAck(1);                         //Ensure autoACK is enabled
    Radio.enableAckPayload();                    //Allow optional ack payloads
    Radio.setRetries(0, 10);                     //Smallest time between retries (shall be 0 == 250ms), max no. of retries (shall be 10)
    Radio.setPayloadSize(RadioPayloadSize);      //standard: 2-byte payload
    Radio.setDataRate(RF24_2MBPS);               //higher data rate means lower power consumption (but less robustness)
    Radio.setPALevel(RF24_PA_MAX);               //high power consumption, high distance
    Radio.openWritingPipe(RadioAddresses[1]);    //Both radios listen on the same pipes by default, and switch when writing
    Radio.openReadingPipe(1, RadioAddresses[0]); //Open a reading pipe on address 0, pipe 1
    Radio.startListening();                      //Start listening
    Radio.powerUp();
}


/* ******************************************
    RESET
   ****************************************** */

void reset()
{
    perform_reset = false;
  
    //reset RadioMode
    RadioMode = rmNone;
  
    //clear display
    Matrix.clearDisplay(0);
  
    //copy first level pattern to playfield
    for (byte y = 0; y < bricks_levelheight; y++)
        for (byte x = 0; x < 8; x++)
            bricks[y][x] = bricks_reset[y][x];
            
    //reset Paddle
    handleInput();
    LastPaddlePos = Paddle;
                        
    //random ball start position and direction
    Balls_Old = 0;
    Balls = Balls_max;
    BallDX = BallDY = 0;
    randomBall();
    respawn_timer = millis() + (2 * respawn_duration);    
}


/* ******************************************
    ORIENTATION
   ****************************************** */

void handleOrientation()
{  
    //support "upside-down" orientations by inverting the Paddle
    if (Orientation >= 2)
        Paddle = 6 - Paddle;
}

//draws a pixel while respecting the orientation
void putPixel(byte x, byte y, boolean on)
{
    switch (Orientation)
    {
#ifdef DUINOKIT        
        case 0: Matrix.setLed(0, x, y, on); break;
#else
        case 0: Matrix.setLed(0, 7 - x, y, on); break;
#endif
        case 1: Matrix.setLed(0, y, 7 - x, on); break;
        case 2: Matrix.setLed(0, 7 - x, 7 - y, on); break;
        case 3: Matrix.setLed(0, 7 - y, x, on); break;
    }
}


/* ******************************************
    CONFIGURATION
   ****************************************** */

void changeBrightness()
{
    //change the brightness of the LEDs
    if (Intensity != Intensity_Old)
    {
        Matrix.setIntensity(0, Intensity);
        Intensity_Old = Intensity;
    }
}

void adjustSpeed()
{
    Speed_Old = Speed;
    Speed = map(analogRead(potPaddle), 0, 1023, speed_min, speed_max);
    Speed = (Speed + Speed_Old) / 2;    
    constrain(Speed, speed_min, speed_max);
    
    int ledamount = map(Speed, speed_min, speed_max, -1, 64);
    int the_rest = 64 - ledamount;
    byte x = 0;
    byte y = 0;
    while (ledamount--)
    {
        putPixel(x, y, true);
        y++;
        if (y == 8)
        {
            x++;
            y = 0;
        }
    }
    
    while (the_rest--)
    {
        y++;
        if (y == 8)
        {
            x++;
            y = 0;
        }
        putPixel(x, y, false);
    }    
}

void adjustBrightness()
{
    drawPatternBits(select_brightness, 8);
    Intensity = map(analogRead(potPaddle), 0, 1023, 0, 15);
}

void adjustPaddle()
{
    int poti = analogRead(potPaddle);
    
    if (game_mode == gmPaddleLeft)
    {
        drawPatternBits(arrow_left, 8);
        PotiLeftmost = poti;
    }
    else
    {
        drawPatternBits(arrow_right, 8);
        PotiRightmost = poti;
    }
}


/* ******************************************
    GENERIC GAME ROUTINES
   ****************************************** */

void backupGameState()
{
    game_mode_old = game_mode;
    rBallX        = BallX;
    rBallY        = BallY;
    rBallX_Old    = BallX_Old;
    rBallY_Old    = BallY_Old;
    rBallDX       = BallDX;
    rBallDY       = BallDY;
    rBallDX_tbs   = BallDX_tbs;
    rBallDY_tbs   = BallDY_tbs;
}

void restoreGameState()
{
    game_mode = game_mode_old;
    BallX        = rBallX;
    BallY        = rBallY;
    BallX_Old    = rBallX_Old;
    BallY_Old    = rBallY_Old;
    BallDX       = rBallDX;
    BallDY       = rBallDY;
    BallDX_tbs   = rBallDX_tbs;
    BallDY_tbs   = rBallDY_tbs;    
}

//read universal button, distinguish between short and long
void readUniversalButton()
{
    //the current Universal Button is using an inverse hardware
    //so HIGH is pressed and LOW is open
    if (digitalRead(UniversalButton) == HIGH)
    {
        //trivial state machine to debounce
        if (UniversalButton_firstcontact)
            UniversalButtonPressedStartTime = millis();            
        UniversalButton_firstcontact = false;
    }
    else
    {
        if (UniversalButtonPressedStartTime != 0)
        {
            unsigned long interval = millis() - UniversalButtonPressedStartTime;
            UniversalButtonPressedStartTime = 0;
            UniversalButton_firstcontact = true;
                        
            //configuration modes OR back to game
            if (interval >= UniversalButtonPressedLong)
            {
                //a long button-press also means reset, when game is in Won or Lost state
                if (WonOrLostState)
                {
                    WonOrLostState = false;
                    perform_reset = true;
                    return;
                }
                
                //during multiplayer question: long button means the same as short button: answer the question
                if (RadioMode == rmMaster_init || RadioMode == rmSlave_init)
                {
                    MultiplayerQuestionButton = true;
                    return;
                }
                
                Matrix.clearDisplay(0);
                switch (game_mode)
                {
                    case gmBreakOut:
                    case gmTennis:
                        backupGameState();                        
                        game_mode = gmSpeed;
                        break;
                        
                    case gmSpeed:
                    case gmBrightness:
                        //enter the advanced configuration mode by pressing the button
                        //very long while being in the standard configuration mode
                        if (UniversalButtonPressedVeryLong)
                            game_mode = gmPaddleLeft;
                        else
                            restoreGameState();
                        break;
                }
            }
            
            //reset game OR advance to next config setting
            else if (interval >= UniversalButtonPressedShort)
            {
                //during multiplayer question: long button means the same as short button: answer the question
                if (RadioMode == rmMaster_init || RadioMode == rmSlave_init)
                {
                    MultiplayerQuestionButton = true;
                    return;
                }
                                
                switch (game_mode)
                {
                    case gmBreakOut:
                    case gmTennis:
                        perform_reset = true;
                        break;
                        
                    case gmSpeed:
                        Matrix.clearDisplay(0);
                        game_mode = gmBrightness;
                        break;
                        
                    case gmBrightness:
                        Matrix.clearDisplay(0);               
                        restoreGameState();
                        break;
                        
                    case gmPaddleLeft:
                        Matrix.clearDisplay(0);
                        game_mode = gmPaddleRight;
                        break;
                        
                    case gmPaddleRight:
                        Matrix.clearDisplay(0);
                        restoreGameState();
                        break;
                }
            }            
        }
    }
}

//read various input ports (also depending on TARGET)
void handleInput()
{
    //sample analog data and average it to avoid fibrillation
    Paddle = constrain(map(analogRead(potPaddle), PotiLeftmost, PotiRightmost, 0, 7), 0, 7);
    Paddle = (Paddle + Paddle_Old) / 2;
    
    if (Paddle < 0)
        Paddle = 0;
    if (Paddle > 6)
        Paddle = 6;

    //sample digital data, i.e. handle the Universal Button                   
    readUniversalButton();    
}

//show the remaining balls in a row of LEDs
void manageLEDs(byte how_many_on)
{
    for (int i = 0; i < LED_count; i++)
        digitalWrite(BallDisplay[i], i < how_many_on ? HIGH : LOW);
}

   
//draw paddle and handle paddle position
void handlePaddle(int& paddle, int& paddle_old, byte paddle_pos = 7)
{
    if (paddle != paddle_old)
    {
        putPixel(paddle_old, paddle_pos, false);
        putPixel(paddle_old + 1, paddle_pos, false);        
    }
    putPixel(paddle, paddle_pos, true);
    putPixel(paddle + 1, paddle_pos, true);
       
    paddle_old = paddle;
}

//draw the new ball and remove it at its old position
void drawBall()
{
    if ((BallX != BallX_Old) || (BallY != BallY_Old))
        putPixel(BallX_Old, BallY_Old, false);
    putPixel(BallX, BallY, true);
}

//draw a pattern from a byte array (e.g. the playfield)
void drawPattern(byte pattern[8][8], byte y_count)
{
    for (byte y = 0; y < y_count; y++)
        for (byte x = 0; x < 8; x++)
            putPixel(x, y, pattern[y][x] == 1);
}

//draw a pattern from a bit array (smileys, icons, etc.)
void drawPatternBits(const byte pattern[], byte y_count)
{
    for (byte y = 0; y < y_count; y++)
    {
        byte pixel = pattern[y];
        byte mask = 0b10000000;
        for (byte x = 0; x < 8; x++)
        {
            putPixel(x, y, (pixel & mask) > 0);
            mask = mask >> 1;
        }
    }    
}

//play various noisy sounds
void noise(int freq1, int freq2, int duration)
{
    while (duration--)
    {
        digitalWrite(PWM_Audio, HIGH);
        delayMicroseconds(random(freq1) / 2);
        digitalWrite(PWM_Audio, LOW);
        delayMicroseconds(random(freq2) / 2);
    }
}

//if a "to-be-set" (_tbs) variable is set (i.e. not 0), then wait until the respawn duration has
//passed and then set the motion variables BallDX and BallDY to the new to-be-set (_tbs) values
void respawnManagement()
{
    //respawn management
    if (BallDX_tbs != 0)
    {
        if (millis() >= respawn_timer)
        {
            BallDX = BallDX_tbs;
            BallDY = BallDY_tbs;
            BallDX_tbs = BallDY_tbs = respawn_timer = 0;
        }
    }
}

void calculateBallMovement(GameMode nowplaying)
{            
    //calculate ball movement
    BallX_Old = BallX;
    BallY_Old = BallY;
    BallX += BallDX;
    BallY += BallDY;
        
    //reflection at the LEFT playfield side
    if (BallX == 0)
    {
        BallDX = 1;
        noise(100, 100, 2);
    }

    //reflection at the RIGHT playfield side            
    if (BallX == 7)
    {
        BallDX = -1;
        noise(100, 100, 2);
    }
        
    //BreakOut only: reflection at the TOP of the playfield
    if (nowplaying == gmBreakOut && BallY == 0)
    {
        BallDY = 1;
        noise(100, 100, 2);
    }
}    
 
void calculatePaddleImpact(GameMode nowplaying, int& paddle, int& last_paddle_pos)
{
    //RelativePaddleSpeed is important, if you need to give the ball a special
    //"spin" either to left or to right in cases it moves to uniformly and you
    //cannot reach certain bricks
    int relative_paddle_speed = paddle - last_paddle_pos;
    last_paddle_pos = paddle;
                
    //ball hits the paddle
    if (BallY == 6 || (nowplaying == gmTennis && BallY == 1))
    {
        //if the ball is coming from the left (and moving to the right)...
        if (BallDX > 0)
        {
            //... and if the ball hits the paddle
            if (BallX + 1 == paddle || BallX  == paddle || BallX - 1 == paddle)
            {
                //... then reflect it (upwards or downwards depends on which paddle was hit)
                BallDY = (BallY == 6) ? -1 : 1; // -1 == upwards as 0|0 is the top/left corner
                
                //and if it hit the paddle at the very left, then also reflect it left
                if (BallX + 1 == paddle && BallX != 0)
                    BallDX = -BallDX;
                    
                //make a short clicking noise when the paddle was hit
                noise(200, 200, 6);
            }
        }
        
        //the same game as shown above - but now, the ball is coming from the right
        else
        {
            if (BallX - 2 == paddle || BallX - 1 == paddle || BallX == paddle)
            {
                BallDY = (BallY == 6) ? -1 : 1;
                if (BallX - 2 == paddle && BallX != 7)
                    BallDX = -BallDX;
                noise(200, 200, 6);
            }
        }
        
        //possibility to influence the ball with a speedy paddle movement
        //you can give it a "spin" either to the left or to the right
        if (abs(relative_paddle_speed) >= PaddleSpeedThreshold)
        {
            if (BallX < 6 && relative_paddle_speed > 0)
                BallX++;
            
            if (BallX > 1 && relative_paddle_speed < 0)
                BallX--;
        }
    }        
}

/* ******************************************
    BREAKOUT
   ****************************************** */

//random ball position and random x-movement
void randomBall()
{
    BallX = random(1, 7);
    BallY = random(bricks_levelheight, 5);
    BallDX_tbs = random(10) < 5 ? 1 : -1;
    BallDY_tbs = 1;
}

void checkCollisionAndWon()
{
    //collision detection with playfield
    if (BallY < bricks_levelheight) //performance opt., unclear if lazy eval works, so two statements
        if (bricks[BallY][BallX] == 1)
        {
            noise(500, 500, 3);

            bricks[BallY][BallX] = 0;    //the hit brick disappears
            BallDY = 1;                  //ball reflects and starts to move down again
            
            //x-reflection of the ball while respecting the playfield boundaries
            if (BallX == 0)
                BallDX = 1;
            else if (BallX == 7)
                BallDX = -1;
            else
                BallDX = random(10) < 5 ? 1 : -1;
                
            //check if the game is won
            boolean won = true; //asume the game is won
            byte wy = 0;
            while (won && wy < bricks_levelheight)
            {
                byte wx = 0;                    
                while (won && wx < 8)
                {
                    if (bricks[wy][wx] == 1)
                        won = false; //a single remaining bricks leads to continued playing
                    wx++;
                }
                wy++;
            }
                        
            //end screen and "applause"
            //endless loop: game can be restarted only by a reset
            if (won)
            {
                WonOrLostState = true;
                drawPatternBits(smiley_won, 8);
                while (!perform_reset)
                {
                    noise(300, 500, 5);
                    readUniversalButton(); 
                }
            }
        }
}

void checkLost()
{       
    //lost ball
    if (BallY == 8)
    {
        noise(7000, 10000, 100);
        Balls--;
        
        //if there are balls left: respawn
        if (Balls)
        {
            randomBall();
            BallDX = 0;
            BallDY = 0;
            respawn_timer = millis() + respawn_duration;
        }
        
        //otherwise: show the sad smiley and go to an
        //endless loop that can be only left by resetting the system
        else
        {
            manageLEDs(Balls);
            WonOrLostState = true;
            drawPatternBits(smiley_lost, 8);            
            while (!perform_reset)
            {
                readUniversalButton();
                delay(5);
            }
        }
    }    
}

void playBreakOut()
{
    //draw all playfield elements
    drawPattern(bricks, bricks_levelheight);
    drawBall();
    handlePaddle(Paddle, Paddle_Old);

    checkCollisionAndWon();    
    
    //calculate ball movement
    //the ball movement is de-coupled from the paddle movement, i.e. a lower ball speed
    //does NOT lower the paddle movement speed
    if (timer++ >= Speed)
    {
        timer = 0;
        calculateBallMovement(gmBreakOut);
        calculatePaddleImpact(gmBreakOut, Paddle, LastPaddlePos);    
    }
        
    checkLost();
    respawnManagement();
   
    if (Balls != Balls_Old)
    {
        Balls_Old = Balls;
        manageLEDs(Balls);
    }        
}


/* ******************************************
    TENNIS
   ****************************************** */

//set a random x/y position and a random dx/dy movement
void tennisRespawn()
{
    BallX = BallX_Old = random(1, 7);
    BallY = BallY_Old = random(2, 6);
    
    BallDX_tbs = random(10) < 5 ? 1 : -1;
    BallDY_tbs = random(10) < 5 ? 1 : -1;
    
    Matrix.clearDisplay(0);
}

void tennisHandleWonOrLost()
{
    //this device won
    if (TennisPoints == Tennis_win)
    {
        manageLEDs(TennisPoints);
        drawPatternBits(smiley_won, 8);            
        while (!perform_reset)
        {
            readUniversalButton();
            delay(5);
        }
    }
    
    //the other device won
    else
    {
        manageLEDs(TennisPoints);
        drawPatternBits(smiley_lost, 8);            
        while (!perform_reset)
        {
            readUniversalButton();
            delay(5);
        }
    }
}

void playTennis()
{
    byte pipeNo;
    unsigned int payload;
    bool quit = true;
        
    //wait for the other party to join
    if (RadioMode == rmMaster_wait || RadioMode == rmSlave_wait)
    {
        if (MultiplayerWaitStart == 0)
            MultiplayerWaitStart = millis();
        
        unsigned int interval = millis() - MultiplayerWaitStart;
        if (interval < 8500)
            Matrix.setColumn(0, 7 - (interval / 1000), 0); //shrink the exclamation mark while timing out
        //timeout
        else
        {
            RadioMode = rmNone;
            game_mode = gmBreakOut;
            Matrix.clearDisplay(0);
            return;
        }
                
        //poll slave status
        if (RadioMode == rmMaster_wait)
        {
            if (Radio.write(&RadioMasterWaitQ, 2))
            {       
                if (Radio.available())
                {
                    while (Radio.available())
                        Radio.read(&payload, 2);
                    if (payload == RadioSlaveWaitA)
                    {
                        Matrix.clearDisplay(0);
                        RadioMode = rmMaster_run;
                    }
                }
            }
      
            quit = false; //todo: error states?      
        }
        
        //answer to master's polls
        if (RadioMode == rmSlave_wait)
        {
            if (Radio.available(&pipeNo))
            {
                while (Radio.available(&pipeNo))
                    Radio.read(&payload, 2);
                Radio.writeAckPayload(pipeNo, &RadioSlaveWaitA, 2);
                if (payload == RadioMasterWaitQ)
                {
                    Matrix.clearDisplay(0);                   
                    RadioMode = rmSlave_run;
                    
                    //make sure, the read FIFO is empty
                    delay(300);
                    while (Radio.available(&pipeNo))
                        Radio.read(&payload, 2);
                }
            }
            
            quit = false; //todo: error states?            
        }
    }
    
    //RadioGameData is the payload used for sending and receiving the game data
    struct  //use bit fields to squeeze everything in two bytes
    {
        unsigned int Paddle         : 3;
        unsigned int BallX          : 3;
        unsigned int BallY          : 3;
        unsigned int TennisPoints   : 2;
        unsigned int WonOrLostState : 1;
    } RadioGameData;
           
    if (RadioMode == rmMaster_run)
    {        
        RadioGameData.Paddle = 6 - Paddle;
        RadioGameData.BallX = 7 - BallX;
        RadioGameData.BallY = 7 - BallY;
        RadioGameData.TennisPoints = TennisPoints_Remote;
        RadioGameData.WonOrLostState = WonOrLostState;
        
        if (Radio.write(&RadioGameData, 2))
        {
            if (Radio.available())
            {
                while (Radio.available())
                    Radio.read(&payload, 2);
                    
                drawBall();
                Paddle_Remote = payload;
                handlePaddle(Paddle_Remote, Paddle_Remote_Old, 0);
            }
        }       
        quit = false; //TODO error handling
     
        handlePaddle(Paddle, Paddle_Old);                
        
        if (timer++ >= Speed && !WonOrLostState)
        {
            timer = 0;
            
            //the Master device keeps track of the ball's x, y, dx and dy
            calculateBallMovement(gmTennis);
            calculatePaddleImpact(gmTennis, Paddle, LastPaddlePos);
            calculatePaddleImpact(gmTennis, Paddle_Remote, LastPaddlePos_Remote);

            //the Master device counts the points
            if (BallY == -1 || BallY == 8) 
            {
                noise(7000, 10000, 100);
                
                if (BallY == -1) 
                    TennisPoints++;          //Slave player looses a ball
                else
                    TennisPoints_Remote++;  //Master player looses a ball
                                    
                if (TennisPoints == Tennis_win || TennisPoints_Remote == Tennis_win)
                {
                    WonOrLostState = true;
                    RadioGameData.TennisPoints = TennisPoints_Remote;
                    RadioGameData.WonOrLostState = WonOrLostState;
                    
                    //transmit WinOrLostState
                    if (Radio.write(&RadioGameData, 2))
                    {
                        if (Radio.available())
                        {
                            while (Radio.available())
                                Radio.read(&payload, 2);
                        }
                    }                                            
                }
                else
                    tennisRespawn();
            }            
        } 
    }
            
    if (RadioMode == rmSlave_run)
    {
        handlePaddle(Paddle, Paddle_Old);
        
        if (Radio.available(&pipeNo))
        {
            while (Radio.available(&pipeNo))
                Radio.read(&RadioGameData, 2);
                
            int paddletobesent = 6 - Paddle;
            Radio.writeAckPayload(pipeNo, &paddletobesent, 2);
            
            BallX_Old = BallX;
            BallY_Old = BallY;
            BallX = RadioGameData.BallX;
            BallY = RadioGameData.BallY;
            Paddle_Remote = RadioGameData.Paddle;
            TennisPoints = RadioGameData.TennisPoints;
            WonOrLostState = RadioGameData.WonOrLostState;
            
            drawBall();
            handlePaddle(Paddle_Remote, Paddle_Remote_Old, 0);            
        }
        quit = false;        
    }
    
    respawnManagement();
    
    if (TennisPoints != TennisPoints_Old)
    {
        TennisPoints_Old = TennisPoints;
        manageLEDs(TennisPoints);        
    }
    
    if (WonOrLostState)
        tennisHandleWonOrLost();
        
    if (quit)
    {
        Matrix.clearDisplay(0),
        RadioMode = rmNone;
        game_mode = gmBreakOut;
    }
}


/* ******************************************
    MAIN LOOP
   ****************************************** */

void loop()
{   
    if (perform_reset)
        reset();
        
    //check for other player/device and set the variable RadioMode,
    //i.e. no radio connection, act as master or act as slave
    radioScanAndDetermineMode();
    
    //handle various input ports and the orientation
    handleInput();
    handleOrientation();
    changeBrightness();
    
    if (radioHandleMultiplayerQuestion())
        return;

    switch (game_mode)
    {
        case gmBreakOut:    playBreakOut();        break;
        case gmTennis:      playTennis();          break;
        
        case gmSpeed:       adjustSpeed();         break;
        case gmBrightness:  adjustBrightness();    break;   
        
        case gmPaddleLeft:     
        case gmPaddleRight:
            adjustPaddle();
            break;
    }            
}
