---
title: TeleBall - Software
layout: default
---

TeleBall - Software
===================

Basics
------

TeleBall is implemented in a C / C++ mixture (more C than C++) as this is
rather typical for Arduino projects. You need the
[Arduino IDE](http://arduino.cc/de/Main/Software) to get started.

### Folder Structure

All sources are located below the `src` folder, which contains the following
file and folder structure:

    archive         Historic TeleBall software releases
    TeleBall        As per Arduino convention, the "Sketch" that contains
                    the source code is located inside an own folder that
                    has the same name as the Sketch itself: TeleBall.
                    Compile TeleBall.ino and upload it to create TeleBall's
                    firmware.
    libraries.zip   This ZIP contains the 3rd party libraries that
                    TeleBall.ino needs to compile in a proven-to-work
                    versions.

### Libraries

TeleBall is using two 3rd party libraries. As mentioned above, the GIT repository
contains the versions of these libraries that are known to work with TeleBall. If
you prefer to use newer versions, you can find them here:

* MAX7221 driver: [wayoda / LedControl](https://github.com/wayoda/LedControl)
* nRF24L01+ driver: [TMRh20 / RF24](http://tmrh20.github.io/RF24/)

Instructions for how-to install these libraries are [here](http://arduino.cc/en/Guide/Libraries).

Main Ideas
----------

The software implementation of TeleBall relies on three main ideas:

* Heartbeat of TeleBall: Arduino's loop() concept
* Game Mode FSM: A state machine modelling the current macro state of the device
* Master/slave concept in Tennis: One device completely renders the whole game

TeleBall works a lot with global variables and constants as we are in a single instance
scenario and therefore encapsulation is not necessary.

### Heartbeat of TeleBall

The whole TeleBall application is written in a way, that in (nearly) no function or situation
we have a blocking state. That means, that the main loop (Arduino's loop() function) is
always running, i.e. called again and again, like a heartbeat. This leads to:

* Fault tolerance as things can recover in subsequent iterations of the loop.
* Possibility to avoid interrupt programming as the loop() is cycled fast enough
  to easily allow polling.
* The timeout mechanism can conveniently be implemented at one place at the end of the main loop

{% highlight cpp %}
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
    handleBrightness();
    
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
    
    //if radio is active (aka if tennis), time out, e.g. because the other
    //device was switched off or it was moved out of transmission range, etc.
    if (RadioMode > rmNone && millis() > RadioTimedOut)
    {
        Radio.flush_tx();
        radioEmptyReadFIFO();
        game_mode = gmTennis;           //make sure, that tennis is reset, when calling reset() ...
        reset();                        //... game_mode could be config modes like gmSpeed, etc.
        tennisSwitchToBreakOut(false);  //switch to BreakOut but continue to try to find another device for tennis
    }    
}
{% endhighlight %}

### FSM #1 - GameMode

The GameMode models the macro state the device is currently in: Are we playing BreakOut
or Tennis for Two? Are we currently in any configuration menu?

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

There is a global variable called `game_mode` which reflects the current state of
the game. There are 

### FSM #2 - GameMode

    enum
    {
        //some checks are done via "> rmNone", i.e. it is important that the modes are in order
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

Radio Details
-------------

### Unidirectional Communication

### Pairing

### Timeout

EEPROM Memory Layout
--------------------

Upon each device start, the TeleBall device fingerprint is checked. If it is there,
then TeleBall assumes valid data for the other variables. Otherwise it assumes a
factory new device and initializes the EEPROM memory with factory default values.

Bytes |Type        |Value
00..07|chars       |TeleBall device fingerprint: {'T', 'e', 'l', 'e', 'B', 'a', 'l', 'l'};
08..09|unsigned int|Ball speed
10    |byte        |Display intensity
11..12|unsigned int|Leftmost poti position (calibrate paddle user experience)
13..14|unsigned int|Rightmost poti position (calibrate paddle user experience)
