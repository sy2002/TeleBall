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

//#define DUINOKIT              //mirror x specifically for the DUINOKIT hardware
#define SHOW_MASTER_SLAVE     //show master/slave indicator in question mode
#define STAY_IN_TENNIS        //after tennis was entered once, stay in tennis mode

/* ******************************************
    Libraries
   ****************************************** */
   
//Arduino default EEPROM library for persistent storage
#include <EEPROM.h>
   
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
    gmPaddleRight,
    
    //EEPROM: store current settings or reset to default
    gmEEPROM
};

enum GameSounds
{
    gsWall,        //sound: ball hits the wall
    gsPaddle,      //sound: ball hits the paddle
    gsPlayfield,   //sound: ball hits the BreakOut bricks (playfield)
    gsLost         //sound: ball gets lost
};

//defaults that are set, when "back to factory default" is selected in the extended menu
const GameMode        game_mode_default = gmBreakOut;    //first game to be started
const int             speed_default = 300;               //speed of the game in "milliseconds between moving the ball one pixel"
const byte            intensity_default = 0;             //brightness of the 8x8 matrix
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
const unsigned long   MultiplayerQuestionMax = 2000;         //how long is the question mark shown, before the user can choose
boolean               MultiplayerQuestionButton = false;     //universal button pressed during rmMaster_init or rmSlave_init

//the speed defines how many milliseconds are between two ball movements
//the movement of the paddle is decoupled from the movement speed of the ball
//the maximum speed heavily depends on the processor type and speed
const int             speed_max = 75;                       //amount of screen refresh cycles in milliseconds that are wasted...
const int             speed_min = 600;                      //...until the ball moves on one pixel

//amount of LEDs installed in the device, should be 3
//changing this leads to multiple code parts that need adjustments
const byte            LED_count = 3;

//absolute y-coordinate where a paddle hit shall be counted (normally is 1 line away from the line where the paddle resides)
const byte            PaddleHit_Top    = 1;
const byte            PaddleHit_Bottom = 6;

//flags for the adjust speed mode
const unsigned long   Flag_Leave       = (unsigned long) 1 << 31;
const unsigned long   Flag_Leave_Ack   = (unsigned long) 1 << 30;
const unsigned long   Mask_Speed       = 1023;

//EEPROM fingerprint for detecting, if the EEPROM has ever been initialized by TeleBall
const byte EEPROM_Fingerprint_len = 8;
const byte EEPROM_Fingerprint[8]  = {'T', 'e', 'l', 'e', 'B', 'a', 'l', 'l'};

/* EEPROM layout
   Bytes    Type            Value
   00..07   chars           TeleBall device fingerprint
   08..09   unsigned int    ball speed (variable: Speed)
   10       byte            display intensity (variable: Intensity)
   11..12   unsigned int    leftmost poti position (variable: PotiLeftmost)
   13..14   unsigned int    rightmost poti position (variable: PotiRightmost)
*/
const byte locFingerprint   =  0;
const byte locSpeed         =  8;
const byte locIntensity     = 10;
const byte locPotiLeftmost  = 11;
const byte locPotiRightmost = 13;

//needs to be declared here to access the enums (strange compiler behaviour)
void calculateBallMovement(GameMode nowplaying);
void playSound(GameSounds whichsound);

/* ******************************************
    Global Game Variables
   ****************************************** */

GameOrientation Orientation = orientation_default;

//current speed of the game
//unsigned long (i.e. 4 bytes) due to RadioPayloadSize == 4
unsigned long Speed = speed_default;
unsigned long Speed_Old = Speed;

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

//BreakOut only: amount of balls left and current level
byte Balls;
byte Balls_Old = 0;
byte Level;
const byte Levels = 3;

//Tennis only: points
char TennisPoints = 0;
char TennisPoints_Old = -1;
char TennisPoints_Remote = 0;

//the timer is used to de-couple the paddle movement from the ball-speed
//i.e. it is the central instance for the overall game speed
unsigned long Timer = 0;

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
    Levels / Graphics / Patterns / Melodies
   ****************************************** */

//this is the level pattern of the "bricks"
//modify to create tougher or easier levels
const byte bricks_levelheight[Levels] = {3, 4, 4};
const byte bricks_reset[Levels][4][8] =
{
    {
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 0, 0, 0, 0}
    },
    
    {
        {1, 0, 1, 1, 1, 1, 0, 1},
        {1, 1, 0, 1, 1, 0, 1, 1},
        {0, 1, 1, 0, 0, 1, 1, 0},
        {0, 0, 1, 1, 1, 1, 0, 0}
    },
    
    {
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1}
    }
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

//EEPROM: store current configuration
const byte eeprom_store[8] =
{
    0b11111111,
    0b10000001,
    0b10111101,
    0b10100101,
    0b10100101,
    0b10111101,
    0b10000001,
    0b11111111
};

//EEPROM: revert to factory default
const byte eeprom_defaults[8] =
{
    0b10000001,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00011000,
    0b00100100,
    0b01000010,
    0b10000001,
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

//maximum length of any melody
//zero padding of all melodyies needs to be adjusted when changing this
//and hard coded [12] in playMelody function header
const byte melody_maxlen = 12;

//the larger, the quicker the melodies are played
const float melody_speed = 2.5f;

const byte melody_advance_level_len = 7;
const unsigned int melody_advance_level[2][melody_maxlen] =
{
    { 392, 392, 392, 440,  392, 494,  523, 0, 0, 0, 0, 0},    //frequency
    { 500, 250, 250, 500, 1000, 500, 2000, 0, 0, 0, 0, 0}     //duration, 1000 = 1 full beat
};

/* *************************************************
    NRF24L01+ radio
   ************************************************* */

//reflect the wiring on the PCB
const byte RadioCE             = A0;
const byte RadioCS             = A1;

//hardcoded address for sending and listening
//if we'd ever think about un-jam-ing, some other logic needs to be developed including
//a "root" address and then different addresses plus different channel
byte RadioAddress[6]           = {"TELEB"}; 

//need to be 4 bytes long (or, if RadioPayloadSize is other than 4 an appropriate memory buffer)
unsigned long RadioMasterToken  = 2309;        //initial negotiation: signal that this device is master
unsigned long RadioSlaveToken   = 4711;        //initial negotiation: signal that this device is slave
unsigned long RadioMasterWaitQ  = 2310;        //master waiting to start while slave still decides
unsigned long RadioSlaveWaitA   = 4712;        //slave acknowledging that it is ready to play
unsigned long RadioMasterSCP    = 7;           //master's poll code when slave is in speed change mode
                                               //small number not interfere with the bitfields during mode changes
unsigned long RadioMasterSCA    = 2312;        //master's ACK when slave ends speed change mode

//size of all send and receive packets, should be: 4; maximum possible: 32
//changing this from 4 to another size leads to multiple code pieces that need adjustments
const byte RadioPayloadSize    = 4;           

const byte RadioPipe           = 1;           //we only need one pipe, hard code it to 1

//milliseconds, until next Radio command
const int RadioCycle           = 300;
unsigned long Last_RadioCycle  = 0;

//save battery: time in milliseconds after which the device powers down
//the radio and ignores further tennis request
const unsigned long RadioPowerSaveTime = 120000;

//wait and listen randomly between RadioWaitMin and
//RadioWaitMax, before 
const int RadioWait_Min        = 1000;
const int RadioWait_Max        = 2000;
unsigned long RadioWait        = 0;

//avoid jamming the ACK FIFO (TX FIFO) by keeping track of uploaded ACK payloads
boolean RadioACKuploaded      = false;

//create a new radio object
RF24 Radio(RadioCE, RadioCS);

//master/slave state machine
enum
{
    //"<= rmNone" is used in certain cases, so do not change the negative signs
    rmIgnore                    = -2,   //do not scan any more, but ignore other TeleBall devices
    rmNone                      = -1,   //no other TeleBall device found
    
    //initial find devices and ask the question
    rmMaster_init               = 0,    //about to enter Master mode
    rmSlave_init                = 1,    //about to enter Slave mode
    rmMaster_wait               = 2,    //waiting for Slave to start the game
    rmSlave_wait                = 3,    //waiting for Master to start the game
    
    //standard run mode
    rmMaster_run                = 4,    //running in Master mode    
    rmSlave_run                 = 5,    //running in Slave mode
    
    //special modes for reset and speed adjustments
    rmMaster_reset              = 6,    //Master device resetting
    rmSlave_reset               = 7,    //Slave device resetting
    rmMaster_speedset_by_Master = 8,    //Master device in speed set mode
    rmMaster_speedset_by_Slave  = 9,    //Master device listening to Slave's speed set
    rmSlave_speedset_by_Master  = 10,   //Slave device listening to Master's speed set
    rmSlave_speedset_by_Slave   = 11    //Slave device in speed set mode
} RadioMode = rmNone;

//RadioGameDataFromMaster is the payload sent from master to slave
//IMPORTANT: BITFIELD NEEDS TO BE RadioPayloadSize IN SIZE, so padding is used
struct  //use bit fields to squeeze everything in two bytes
{
    unsigned int Paddle           : 3;
    unsigned int BallX            : 3; 
    unsigned int BallY            : 3;
    unsigned int TennisPoints     : 2;
    unsigned int WonOrLostState   : 1;
    unsigned int Reset            : 1;  //master initiates reset
    unsigned int Reset_Ack        : 1;  //master acknowledges a reset request send by slave
    unsigned int SpeedSet         : 1;  //master initiated a change in the game's speed
    unsigned int SpeedSet_Ack     : 1;  //master acknowledges a speedset request by slave
    unsigned int Sound_Wall       : 1;  //make slave play: ball hits a wall
    unsigned int Sound_Paddle     : 1;  //make slave play: ball hits a paddle
    unsigned int Sound_Lost       : 1;  //make slave play: lost ball
    
    //the padding cannot be arbitrarly done as "padding : <amount>" due to compiler specifics
    unsigned int unused_padding1  : 1;
    unsigned int unused_padding2  : 1;
    unsigned int unused_padding3  : 1;
    unsigned int unused_padding4  : 1;
    unsigned int unused_padding5  : 1;
    unsigned int unused_paddingx  : 8;  
} RadioGameDataFromMaster;

//RadioGameDataFromSlave is the payload sent from slave to master during ACK
//IMPORTANT: BITFIELD NEEDS TO BE RadioPayloadSize IN SIZE, so padding is used
struct  //also uses bit fields
{
    unsigned int Paddle           : 3;
    unsigned int Reset            : 1;   //slave initiates reset
    unsigned int SpeedSet         : 1;   //slave initiates a change in the game's speed
    
    //the padding cannot be arbitrarly done as "padding : <amount>" due to compiler specifics    
    unsigned int unused_padding1  : 1;
    unsigned int unused_padding2  : 1;
    unsigned int unused_padding3  : 1;
    unsigned int unused_paddingx  : 24;    
} RadioGameDataFromSlave;

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
            byte pipeNo;
            while (Radio.available(&pipeNo))
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

boolean radioReceive(void* receivebuffer, void* ackpayloadbuffer)
{
    //RadioAckuploaded mechanism:
    //prevent ACK/TX FIFO jam on the receiving device
    //(leads to "forgetting" all ACK payload updates after the first one until
    //new data is received; presumably the roundtrips are so fast that this
    //effect can be neglected
    if (!RadioACKuploaded)
    {
        RadioACKuploaded = true;
        
        //upload ACK payload to TX FIFO, i.e. next read AUTO ACK will use this one
        Radio.writeAckPayload(RadioPipe, ackpayloadbuffer, RadioPayloadSize);
    }
        
    if (Radio.available())
    {
        byte pipeNo;
        while (Radio.available(&pipeNo))
            Radio.read(receivebuffer, RadioPayloadSize);
        
        RadioACKuploaded = false;
        
        return true;
    }
    
    return false;
}

//kind of battery and performance saving scanning and negotiation protocol
//that scans for another player/device and then negotiates: who is master and who is slave
//concept: the first who receives a master token is the slave and sends an ACK with a slave token
//due to the random sending intervals and the fact, that the NRF24L01+ buffers received data in
//a FIFO, this method is working very stable
void radioScanAndDetermineMode()
{    
    //rmIgnore needs a hard reset of the device to be able to find another device again
    if (RadioMode == rmIgnore)
        return;
        
    //power save mode for longer battery life
    //after RadioPowerSaveTime milliseconds, the radio is powered down
    if (RadioMode == rmNone && millis() > RadioPowerSaveTime)
    {
        RadioMode = rmIgnore;
        Radio.powerDown();
    }        
    
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
           
                unsigned long payload;

                //data received? if yes and if it is the master token, then this device is the slave
                //we use ACK payload mechanism to confirm by sending the slave token
                if (radioReceive(&payload, &RadioSlaveToken) && payload == RadioMasterToken)
                {
                    RadioMode = rmSlave_init;    //enter question mode ("want to play tennis?")                                 
                    game_mode = gmTennis;        //switch to tennis game loop
                }
            
                //no data received and random interval is over: send a master token
                //if the other side confirms by a slave token, then this device is the master
                else
                {       
                    //switch to sending mode                
                    Radio.stopListening();
                    Radio.openWritingPipe(RadioAddress);
                
                    if (radioSend(&RadioMasterToken, &payload) && payload == RadioSlaveToken)
                    {
                        RadioMode = rmMaster_init;  //enter question mode ("want to play tennis?")
                        game_mode = gmTennis;       //switch to tennis game loop
                    }
                        
                    //if no ACK or wrong ACK: consider it as no device found and start over by continuing to listen
                    if (RadioMode == rmNone)
                    {
                        //switch back to receiving mode
                        Radio.openReadingPipe(RadioPipe, RadioAddress);
                        Radio.startListening();
                    }
                }
            }      
        }
    }
}

void radioEmptyReadFIFO()
{
    byte pipeNo;
    byte NullDevice[RadioPayloadSize];
    while (Radio.available(&pipeNo))
        Radio.read(&NullDevice, RadioPayloadSize);
}

/* ******************************************
    SETUP
   ****************************************** */   

void setup()
{   
//    Serial.begin(57600); //debug
//    eepromReset(); //debug
    
    //EEPROM
    //if it has never been writen: fill with default
    if (!eepromCheckFingerprint())
        eepromWriteFingerprintAndDefaults();
    else
        eepromReadSettings();

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
    Radio.setAutoAck(1);                             //Ensure autoACK is enabled
    Radio.enableAckPayload();                        //Allow optional ack payloads
    
//    Radio.setRetries(0, 10);                         //Smallest time between retries (shall be 0 == 250ms), max no. of retries (shall be 10)
    Radio.setRetries(0, 4);
    
    Radio.setPayloadSize(RadioPayloadSize);          //standard: 4-byte payload
    
//    Radio.setDataRate(RF24_2MBPS);                   //higher data rate means lower power consumption (but less robustness)
    Radio.setDataRate(RF24_1MBPS);
    
    Radio.setPALevel(RF24_PA_MAX);                   //high power consumption, high distance
    Radio.openReadingPipe(RadioPipe, RadioAddress);  //open read pipe on hard coded pipe no and address
    Radio.startListening();                          //Start listening
    Radio.powerUp();
}


/* ******************************************
    RESET
   ****************************************** */

//reset BreakOut means: new game at level 1
void reset_BreakOut()
{    
    //copy first level pattern to playfield
    Level = 0;
    breakoutFillLevel();
            
    //reset ball counter
    Balls_Old = 0;
    Balls = Balls_max;
    
    //random ball start position and direction
    BallDX = BallDY = 0;
    randomBall();
    
    //first respawn is longer than the other ones                        
    respawn_timer = millis() + (2 * respawn_duration);    
}

//reset Tennis means: both devices back to the question mode
void reset_Tennis()
{       
    //reset score
    TennisPoints = 0;
    TennisPoints_Old = -1;
    TennisPoints_Remote = 0;       
    tennisRespawn(2 * respawn_duration);    
}

void reset()
{
    Matrix.clearDisplay(0);
            
    //reset Paddle
    handleInput();
    LastPaddlePos = Paddle;
            
    //reset housekeepking variables
    Timer = 0;
    perform_reset  = false;
    WonOrLostState = false;    
    MultiplayerWaitStart = 0;
    MultiplayerQuestionTime = 0;
    MultiplayerQuestionButton = false;
    
           
    switch (game_mode)
    {
        //reset game
        case gmBreakOut:    reset_BreakOut();     break;    

        //back to question mode: one more round of Tennis or back to BreakOut        
        case gmTennis:
            //reset initiated on the master device
            if (RadioMode == rmMaster_run)
                RadioMode = rmMaster_reset;    
            //reset initiated on the slave device
            else
                RadioMode = rmSlave_reset;
            //reset local stats        
            reset_Tennis();
            break ;
    }    
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
    unsigned long payload;
    unsigned long NullDevice = 0; //needs to be zero, otherwise the slave might send a reset signal during ACK
    
    if (RadioMode == rmMaster_run)
    {
        RadioGameDataFromMaster.SpeedSet = 1;
        Radio.flush_tx();
        if (radioSend(&RadioGameDataFromMaster, &NullDevice))
        {
            RadioMode = rmMaster_speedset_by_Master;
            delay(300); //allow slave to digest this
        }
    }
    
    if (RadioMode == rmSlave_run)
    {
        RadioGameDataFromSlave.SpeedSet = 1;
        RadioGameDataFromMaster.SpeedSet_Ack = 0;
        while (!RadioGameDataFromMaster.SpeedSet_Ack)
        {
            if (radioReceive(&RadioGameDataFromMaster, &RadioGameDataFromSlave))
            {
                RadioMode = rmSlave_speedset_by_Slave;
            }
        }
    }
                    
    if (RadioMode <= rmNone || RadioMode == rmMaster_speedset_by_Master || RadioMode == rmSlave_speedset_by_Slave)
    {
        Speed_Old = Speed;
        Speed = map(analogRead(potPaddle), 0, 1023, speed_min, speed_max);
        Speed = (Speed + Speed_Old) / 2;    
        constrain(Speed, speed_min, speed_max);
        
        if (RadioMode == rmMaster_speedset_by_Master)
            radioSend(&Speed, &NullDevice);

        if (RadioMode == rmSlave_speedset_by_Slave)
            radioReceive(&NullDevice, &Speed);
    }
    
    if (RadioMode == rmMaster_speedset_by_Slave)
    {
        if (radioSend(&RadioMasterSCP, &payload))
        {
            //remove flags for getting the real speed
            Speed = payload & Mask_Speed;            
            
            //check if slave asks for leaving the SpeedSet mode
            if (payload & Flag_Leave)
            {
                delay(300); //let slave move to restoreGameState()                
                radioSend(&RadioMasterSCA, &NullDevice);
                Matrix.clearDisplay(0);
                restoreGameState();
                return;
            }                        
        }
    }
    
    if (RadioMode == rmSlave_speedset_by_Master)
    {
        if (radioReceive(&payload, &NullDevice))
        {            
            //remove flags for getting the real speed
            Speed = payload & Mask_Speed;            
            
            //check if master commands to leave the SpeedSet mode
            if (payload & Flag_Leave)
            {
                Matrix.clearDisplay(0);
                restoreGameState();
                return;
            }            
        }        
    }
         
    int ledamount = map(Speed, speed_min, speed_max, 0, 64);
    constrain(ledamount, 0, 64);
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
    EEPROM ROUTINES
   ****************************************** */

void eepromWriteInt(int address, int value)
{
    EEPROM.write(address,     (byte) value);         //write low order byte
    EEPROM.write(address + 1, (byte) (value >> 8));  //write high order byte    
}

int eepromReadInt(int address)
{
    return (EEPROM.read(address + 1) << 8) + EEPROM.read(address);
}
   
void eepromReset()
{
    for (int i = 0; i < 255; i++)
        EEPROM.write(i, 255);
}

boolean eepromCheckFingerprint()
{    
    boolean FingerprintMatch = true;
    for (int i = 0; i < EEPROM_Fingerprint_len; i++)
    {
//        Serial.print("cfp: i = "); Serial.print(i); Serial.print(" value = "); Serial.println(EEPROM.read(locFingerprint + i));
        if (EEPROM.read(locFingerprint + i) != EEPROM_Fingerprint[i])
            FingerprintMatch = false;
    }            
    return FingerprintMatch;    
}

void eepromWriteFingerprintAndDefaults()
{    
//    Serial.println("EEPROM: Writing fingerprint and defaults.");
        
    //write fingerprint
    for (int i = 0; i < EEPROM_Fingerprint_len; i++)
    {
        Serial.print("wfp: i = "); Serial.print(i); Serial.print(" value = "); Serial.println(EEPROM_Fingerprint[i]);
//        EEPROM.write(locFingerprint + i, EEPROM_Fingerprint[i]);
    }
    
    //set variables to factory default and write them to EEPROM
    Speed         = speed_default;
    Intensity     = intensity_default;
    PotiLeftmost  = poti_leftmost_default;
    PotiRightmost = poti_rightmost_default;        
    eepromWriteSettings();
}

void eepromReadSettings()
{
    Speed            = eepromReadInt(locSpeed);
    Intensity        = EEPROM.read(locIntensity);
    PotiLeftmost     = eepromReadInt(locPotiLeftmost);
    PotiRightmost    = eepromReadInt(locPotiRightmost);
    
//    eepromDumpSettings("EEPROM: Read settings:");
}

void eepromWriteSettings()
{    
    eepromWriteInt(locSpeed, Speed);
    EEPROM.write(locIntensity, Intensity);
    eepromWriteInt(locPotiLeftmost, PotiLeftmost);
    eepromWriteInt(locPotiRightmost, PotiRightmost);
    
//    eepromDumpSettings("EEPROM: Wrote settings:");
}

void manageEEPROM()
{
    //reset to factory default shall be a concious decision, so we
    //check for < 2 instead of < 4: the paddle needs to be very far left
    if (Paddle < 2)
        drawPatternBits(eeprom_defaults, 8);
    else
        drawPatternBits(eeprom_store, 8);
}

/*
//debug only function
void eepromDumpSettings(const char* printmsg)
{
    Serial.println(printmsg);
    Serial.print("Speed = "); Serial.println(Speed);
    Serial.print("Intensity = "); Serial.println(Intensity);
    Serial.print("PotiLeftmost = "); Serial.println(PotiLeftmost);
    Serial.print("PotiRightmost = "); Serial.println(PotiRightmost);
}
*/

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
    game_mode    = game_mode_old;
    BallX        = rBallX;
    BallY        = rBallY;
    BallX_Old    = rBallX_Old;
    BallY_Old    = rBallY_Old;
    BallDX       = rBallDX;
    BallDY       = rBallDY;
    BallDX_tbs   = rBallDX_tbs;
    BallDY_tbs   = rBallDY_tbs;
    
    unsigned long NullDevice;
    unsigned long sendspeed;
    unsigned long payload = 0;

    switch (RadioMode)
    {
        //send the final speed to the slave and tell the it leave the SpeedSet mode
        //then return to tennis in master mode
        case rmMaster_speedset_by_Master:
            Radio.flush_tx();
            sendspeed = Speed | Flag_Leave;
            if (radioSend(&sendspeed, &NullDevice))
            {
                RadioMode = rmMaster_run; //back to play mode
                delay(300); //give the slave the chance to digest the message                              
                pauseGame(2 * respawn_duration); //give the players the chance to get into the game again
            }                
            break;
            
        //return to tennis in master mode
        case rmMaster_speedset_by_Slave:
            game_mode = gmTennis;
            RadioMode = rmMaster_run;
            break;            
            
        //return to tennis in slave mode
        case rmSlave_speedset_by_Master:
            game_mode = gmTennis;
            RadioMode = rmSlave_run;
            break;
            
        case rmSlave_speedset_by_Slave:
            sendspeed = Speed | Flag_Leave;
            radioEmptyReadFIFO();
            while (payload != RadioMasterSCA)
                radioReceive(&payload, &sendspeed);
            RadioMode = rmSlave_run;
            break;            
    }
    
    //as also in Tennis (see rmMaster_speedset_by_Master above):
    //do the same in BreakOut and give the player the chance to get into the game again
    if (game_mode == gmBreakOut)
    {
        //use the pauseGame function in situations, where no "_tbs-process" is already running
        if (!BallDX_tbs)
            pauseGame(2 * respawn_duration);
        
        //else prolong the already running "_tbs-process"
        else
            respawn_timer = millis() + 2 * respawn_duration;
    }        
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
            
            //ignore button in case the other device is in SpeedSet mode
            if (RadioMode == rmMaster_speedset_by_Slave || RadioMode == rmSlave_speedset_by_Master)
                return;            
                        
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
                        game_mode = gmEEPROM;
                        break;
                        
                    case gmEEPROM:
                        //depending on the paddle: revert to factory default or store config settings to EEPROM
                        if (Paddle < 2)
                            eepromWriteFingerprintAndDefaults();
                        else
                            eepromWriteSettings();
                        
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

//play specific game sounds and in Tennis Master mode: send them to the Slave
void playSound(GameSounds whichsound)
{
    //need to send first to avoid latency (particularly for long sounds)
    if (game_mode == gmTennis && RadioMode == rmMaster_run)
    {
        switch (whichsound)
        {
            case gsWall:    RadioGameDataFromMaster.Sound_Wall = 1;    break;
            case gsPaddle:  RadioGameDataFromMaster.Sound_Paddle = 1;  break;
            case gsLost:    RadioGameDataFromMaster.Sound_Lost = 1;    break;
        }
        
        byte NullDevice[RadioPayloadSize];
        radioSend(&RadioGameDataFromMaster, &NullDevice);
    }
    
    //play sounds
    switch (whichsound)
    {
        case gsWall:        noise(100, 100, 2);        break;
        case gsPaddle:      noise(200, 200, 6);        break;
        case gsPlayfield:   noise(500, 500, 3);        break;
        case gsLost:        noise(7000, 10000, 100);   break;
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

void playMelody(const unsigned int melody[2][12], byte melody_len)
{
    for (byte i = 0; i < melody_len; i++)
    {
        tone(PWM_Audio, melody[0][i]);
        delay((float) melody[1][i] / melody_speed);
        noTone(PWM_Audio);
        delay(50); //otherwise the tones sound too legato
    }
}

void pauseGame(unsigned int duration)
{
    //remember old movement vector
    BallDX_tbs = BallDX;
    BallDY_tbs = BallDY;

    //stop ball movement
    BallDX = 0;
    BallDY = 0;
    
    //trigger the specified delay
    respawn_timer = millis() + duration;
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
            Timer = 0;
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
        playSound(gsWall);
    }

    //reflection at the RIGHT playfield side            
    if (BallX == 7)
    {
        BallDX = -1;
        playSound(gsWall);
    }
        
    //BreakOut only: reflection at the TOP of the playfield
    if (nowplaying == gmBreakOut && BallY == 0)
    {
        BallDY = 1;
        playSound(gsWall);
    }
}    
 
void calculatePaddleImpact(int paddle, int& last_paddle_pos, byte scanline_of_impact)
{
    //RelativePaddleSpeed is important, if you need to give the ball a special
    //"spin" either to left or to right in cases it moves to uniformly and you
    //cannot reach certain bricks
    int relative_paddle_speed = paddle - last_paddle_pos;
    last_paddle_pos = paddle;
                
    //ball hits the paddle
    if (BallY == scanline_of_impact)
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
                playSound(gsPaddle);
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
                playSound(gsPaddle);
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

void breakoutFillLevel()
{
    for (byte y = 0; y < bricks_levelheight[Level]; y++)
        for (byte x = 0; x < 8; x++)
            bricks[y][x] = bricks_reset[Level][y][x];    
}

//random ball position and random x-movement
void randomBall()
{
    BallX = random(1, 7);
    BallY = random(bricks_levelheight[Level], 5);
    BallDX_tbs = random(10) < 5 ? 1 : -1;
    BallDY_tbs = 1;
}

void checkCollisionAndWon()
{
    //collision detection with playfield
    if (BallY < bricks_levelheight[Level]) //performance opt., unclear if lazy eval works, so two statements
        if (bricks[BallY][BallX] == 1)
        {
            playSound(gsPlayfield);

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
            while (won && wy < bricks_levelheight[Level])
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
                //advance to next level                
                Level++;                                
                if (Level < Levels)
                {
                    playMelody(melody_advance_level, melody_advance_level_len);
                    Matrix.clearDisplay(0);
                    breakoutFillLevel();
                    
                    //pause game for the player to be able to see the new level before it starts
                    BallDX = BallDY = 0;
                    randomBall();
                    respawn_timer = millis() + (2 * respawn_duration);
                }
                
                //last level won: go to applause mode
                else
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
}

void checkLost()
{       
    //lost ball
    if (BallY == 8)
    {
        playSound(gsLost);
        Balls--;
        
        //if there are balls left: respawn
        if (Balls)
        {
            BallDX = BallDY = 0;                             //halt ball movement            
            randomBall();                                    //set a new random movement...
            respawn_timer = millis() + respawn_duration;     //...which will be activated after the respawn_duration
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
    drawPattern(bricks, bricks_levelheight[Level]);
    drawBall();
    handlePaddle(Paddle, Paddle_Old);

    checkCollisionAndWon();    

    //calculate ball movement
    //the ball movement is de-coupled from the paddle movement, i.e. a lower ball speed
    //does NOT lower the paddle movement speed    
    if (!Timer)
        Timer = millis();
    else if (millis() - Timer >= Speed)
    {
        Timer = 0;
        calculateBallMovement(gmBreakOut);
        calculatePaddleImpact(Paddle, LastPaddlePos, PaddleHit_Bottom);    
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

void tennisHandleMultiplayerQuestion()
{
    //show the question mark independent from the paddle position     
    if (MultiplayerQuestionTime == 0)
    {
        drawPatternBits(question_multiplayer, 8);
        
#ifdef SHOW_MASTER_SLAVE
        //debug-info: show if this device is master (x-pos = 0) or slave (x-pos = 1)
        putPixel(RadioMode, 7, true);              
#endif  

        MultiplayerQuestionTime = millis();
    }
    
    //use the paddle to select no (<= 3) or yes (> 3) and display "!" for yes and "?" for no
    if ((millis() - MultiplayerQuestionTime) > MultiplayerQuestionMax)
    {
        //no
        if (Paddle <= 3)
            drawPatternBits(question_multiplayer, 8);
        //yes
        else
            drawPatternBits(yes_multiplayer, 8);                
    }
    
    //in regular operations, Paddle_Old is set by handlePaddle
    //since it is used to even out fibrilations, it needs to be set
    //otherwise strange effect will occur, i.e. not being able to control the ? vs. !
    Paddle_Old = Paddle;
    
    //user anwers the question: switch to new game mode
    if (MultiplayerQuestionButton)
    {
        MultiplayerQuestionButton = false;
        MultiplayerQuestionTime = 0;
        
        //just in case: empty write and read FIFOs
        Radio.flush_tx();
        radioEmptyReadFIFO();
        
        //deny multiplayer mode: back to BreakOut and ignore further tennis game requests
        if (Paddle <= 3)
        {            
            //ignore further tennis game requests and power down radio to save battery life
            RadioMode = rmIgnore;    
            Radio.powerDown();
            
            //return to BreakOut
            game_mode = gmBreakOut;
            Balls_Old = 0;  //take care that when returning, the right amount of balls is shown via the LEDs            
            Matrix.clearDisplay(0);                
        }
        
        //enter multiplayer mode and play tennis
        else
        {
            if (RadioMode == rmMaster_init)
                RadioMode = rmMaster_wait;
            else
                RadioMode = rmSlave_wait;
            
            //after entering tennis: wait a bit longer until the game starts
            tennisRespawn(2 * respawn_duration);
        }
    }        
}

void tennisWaitForOtherPartyToJoin()
{
    unsigned long payload;
    
    if (MultiplayerWaitStart == 0)
        MultiplayerWaitStart = millis();
    
    unsigned long interval = millis() - MultiplayerWaitStart;
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
        if (radioSend(&RadioMasterWaitQ, &payload) && payload == RadioSlaveWaitA)
        {
            Matrix.clearDisplay(0);
            RadioMode = rmMaster_run;                
        }              
    }
    
    //answer to master's polls
    if (RadioMode == rmSlave_wait)
    {
        if (radioReceive(&payload, &RadioSlaveWaitA) && payload == RadioMasterWaitQ)
        {
            Matrix.clearDisplay(0);                   
            RadioMode = rmSlave_run;
            
            //make sure, the read FIFO is empty
            delay(300); //make sure that the master has also left rmMaster_wait
            radioEmptyReadFIFO();
        }                        
    }
}

void tennisPlayMaster()
{
    //mirror own coordinates into the coordinate space of the slave
    RadioGameDataFromMaster.Paddle = 6 - Paddle;
    RadioGameDataFromMaster.BallX = 7 - BallX;
    RadioGameDataFromMaster.BallY = 7 - BallY;
    
    //transmit control structure
    RadioGameDataFromMaster.TennisPoints = TennisPoints_Remote;
    RadioGameDataFromMaster.WonOrLostState = WonOrLostState;
    RadioGameDataFromMaster.Reset = 0;
    RadioGameDataFromMaster.Reset_Ack = 0;
    RadioGameDataFromMaster.SpeedSet = 0;
    RadioGameDataFromMaster.SpeedSet_Ack = 0;
    RadioGameDataFromMaster.Sound_Wall = 0;
    RadioGameDataFromMaster.Sound_Paddle = 0;
    RadioGameDataFromMaster.Sound_Lost = 0;    
        
    if (radioSend(&RadioGameDataFromMaster, &RadioGameDataFromSlave))
    {
        if (WonOrLostState)
            tennisHandleWonOrLost();            
        else
        {
            drawBall();
            
            //local paddle
            handlePaddle(Paddle, Paddle_Old);
            
            //remote paddle
            Paddle_Remote = RadioGameDataFromSlave.Paddle;            
            handlePaddle(Paddle_Remote, Paddle_Remote_Old, 0);
        }
        
        //game reset on slave's side
        if (RadioGameDataFromSlave.Reset)
        {
            RadioGameDataFromMaster.Reset_Ack = 1;
            if (radioSend(&RadioGameDataFromMaster, &RadioGameDataFromSlave))
            {
                reset();                   //reset local stats
#ifdef STAY_IN_TENNIS
                RadioMode = rmMaster_run;  //next tennis match
#else                
                RadioMode = rmMaster_init; //back to question mode
#endif                
                return;
            }
        }
        
        //speed set on slave's side
        if (RadioGameDataFromSlave.SpeedSet)
        {
            RadioGameDataFromMaster.SpeedSet_Ack = 1;
            if (radioSend(&RadioGameDataFromMaster, &RadioGameDataFromSlave))
            {
                RadioMode = rmMaster_speedset_by_Slave;
                game_mode = gmSpeed;
                delay(300); //give slave the chance to digest the ACK
                return;
            }
        }
    }
    
    if (!Timer)
        Timer = millis();
    else if (millis() - Timer >= Speed && !WonOrLostState)
    {
        Timer = 0;
        
        //the Master device keeps track of the ball's x, y, dx and dy and calculates paddle impacts
        calculateBallMovement(gmTennis);
        calculatePaddleImpact(Paddle, LastPaddlePos, PaddleHit_Bottom);
        calculatePaddleImpact(Paddle_Remote, LastPaddlePos_Remote, PaddleHit_Top);

        //the Master device counts the points: check for lost ball
        if (BallY == -1 || BallY == 8) 
        {
            playSound(gsLost);
            
            if (BallY == -1) 
                TennisPoints++;          //Slave player looses a ball
            else
                TennisPoints_Remote++;  //Master player looses a ball
                  
            //game ends: either this device or remote won the game              
            if (TennisPoints == Tennis_win || TennisPoints_Remote == Tennis_win)
            {
                WonOrLostState = true;
                RadioGameDataFromMaster.TennisPoints = TennisPoints_Remote;
                RadioGameDataFromMaster.WonOrLostState = WonOrLostState;
                
                //transmit WinOrLostState
                radioSend(&RadioGameDataFromMaster, &RadioGameDataFromSlave);
            }
            
            //next ball
            else
            {
                tennisRespawn(respawn_duration);
                Matrix.clearDisplay(0);                    
            }                    
        }            
    } 
}

void tennisPlaySlave()
{
    if (!WonOrLostState)
        handlePaddle(Paddle, Paddle_Old);

    //send the paddle position from master's perspective (aka 6 - Paddle) as master is
    //doing all the game's calculations
    RadioGameDataFromSlave.Paddle = 6 - Paddle;
    RadioGameDataFromSlave.Reset = 0;
    RadioGameDataFromSlave.SpeedSet = 0;
    
    if (radioReceive(&RadioGameDataFromMaster, &RadioGameDataFromSlave))
    {
        BallX_Old = BallX;
        BallY_Old = BallY;
        BallX = RadioGameDataFromMaster.BallX;
        BallY = RadioGameDataFromMaster.BallY;
        Paddle_Remote = RadioGameDataFromMaster.Paddle;
        TennisPoints = RadioGameDataFromMaster.TennisPoints;
        WonOrLostState = RadioGameDataFromMaster.WonOrLostState;
        
        if (WonOrLostState)
            tennisHandleWonOrLost();
        else
        {
            drawBall();
            handlePaddle(Paddle_Remote, Paddle_Remote_Old, 0);
        }
        
        //play sounds
        if (RadioGameDataFromMaster.Sound_Wall)
            playSound(gsWall);            
        if (RadioGameDataFromMaster.Sound_Paddle)
            playSound(gsPaddle);
        if (RadioGameDataFromMaster.Sound_Lost)
            playSound(gsLost);

        //game reset on master's side
        if (RadioGameDataFromMaster.Reset)
        {
            reset();                  //reset local stats
#ifdef STAY_IN_TENNIS
            RadioMode = rmSlave_run;  //next tennis match
#else
            RadioMode = rmSlave_init; //back to question mode
#endif            
            return;       
        }
        
        //speed set on master's side
        if (RadioGameDataFromMaster.SpeedSet)
        {
            game_mode = gmSpeed;
            RadioMode = rmSlave_speedset_by_Master;
            return;
        }
    }    
}

void tennisResetMaster()
{
    RadioGameDataFromMaster.Reset = 1;
    
    //if sending the reset flag works...
    if (radioSend(&RadioGameDataFromMaster, &RadioGameDataFromSlave))
    {
#ifdef STAY_IN_TENNIS
        //ignore ACK and initiate next round of tennis
        RadioMode = rmMaster_run;
#else
        //ignore any ACK payload specifics and switch to question mode
        RadioMode = rmMaster_init;
#endif
    }
}

void tennisResetSlave()
{
    RadioGameDataFromSlave.Reset = 1;
    RadioGameDataFromMaster.Reset_Ack = 0;
    
    //it does not matter what data we receive from the master, it is important to send the ACK payload including the Reset signal
    //and to do that until the master acknowledges it, because otherwise the ACK could have been lost
    while (!RadioGameDataFromMaster.Reset_Ack)
    {
        if (radioReceive(&RadioGameDataFromMaster, &RadioGameDataFromSlave))
        {
#ifdef STAY_IN_TENNIS
            //after a successful ACK send (i.e. radioReceive is true), the next round of tennis is initiated
            RadioMode = rmSlave_run;
#else
            //after a successful ACK send (i.e. radioReceive is true), the device is set to the question mode
            RadioMode = rmSlave_init;
#endif            
        }
    }
}

//set a random x/y position and a random dx/dy movement
void tennisRespawn(int duration)
{
    BallX = BallX_Old = random(1, 7);
    BallY = BallY_Old = random(2, 6);
    
    BallDX_tbs = random(10) < 5 ? 1 : -1;

    //depending where the ball starts in y,
    //decide that it flies to the opposite    
    if (BallY < 4)
        BallDY_tbs = 1;
    else
        BallDY_tbs = -1;
    
    //pause game briefly
    BallDX = BallDY = 0;
    respawn_timer = millis() + duration;
}

void tennisHandleWonOrLost()
{
    manageLEDs(TennisPoints);
    
    //this device won
    if (TennisPoints == Tennis_win)
        drawPatternBits(smiley_won, 8);
    
    //the other device won
    else
        drawPatternBits(smiley_lost, 8);            
}

void playTennis()
{
    switch (RadioMode)
    {
        case rmMaster_init:
        case rmSlave_init:
            tennisHandleMultiplayerQuestion();
            break;
            
        case rmMaster_wait:
        case rmSlave_wait:
            tennisWaitForOtherPartyToJoin();
            break;
            
        case rmMaster_run:     tennisPlayMaster();     break;
        case rmSlave_run:      tennisPlaySlave();      break;
        case rmMaster_reset:   tennisResetMaster();    break;
        case rmSlave_reset:    tennisResetSlave();     break;        
    }
                                     
    respawnManagement();
    
    if (TennisPoints != TennisPoints_Old)
    {
        TennisPoints_Old = TennisPoints;
        manageLEDs(TennisPoints);        
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

        case gmEEPROM:      manageEEPROM();        break;            
    }            
}
