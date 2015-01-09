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

### Flashing the Firmware

1. Install the Arduino IDE and the libraries (as described above)
2. Connect TeleBall via USB
3. Double-click `TeleBall.ino` or open it manually in the Arduino IDE
4. Select the right Serial Port in the IDE, this can be
   [a bit tricky](http://arduino.cc/en/pmwiki.php?n=Guide/Troubleshooting#toc15)
   also depending on the actual board you are using (original or clone): You need
   to find and install the right USB-to-serial driver.
5. Choose `File/Upload` or press the "=>" icon: The upload should start
6. After a successful upload, TeleBall starts in BreakOut mode.

Hint: USB is not strong enough to power TeleBall. Particularly the analog inputs
are not calibrated correctly when running on USB. That means: USB power is good
enough to check, if flashing the firmware (uploading the firmware) worked, but
not for much more.

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
we have a blocking state. That means, that the main loop (Arduino's `loop()` function) is
always running, i.e. called again and again, like a heartbeat. This leads to:

* Fault tolerance as things can recover in subsequent iterations of the loop.
* Possibility to avoid interrupt programming as the loop() is cycled fast enough
  to easily allow polling of all inputs and even of the radio
* The timeout mechanism can conveniently be implemented at one place at the end of the main loop

The main loop is essentially executing the following steps:

1. Check if a reset is to be performed and execute it, if necessary
2. Check if another player/device is there for Tennis for Two, if yes: initiate the pairing process
3. Read and handle input from the potentiometer and the button (called `Universal Button`)
4. Call a Game Mode handler, which itself is a small main loop on its own
5. Check and handle timeout

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

{% endhighlight%}

### Game Mode State Machine

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
the game. GameMode is modeled as a classical state machine (FSM), whose state progressing is
like described in the following; `b(x)` means `button pressed as long as x`:

    [gmBreakOut | gmTennis] => b(1sec)    => [gmSpeed]
    [gmSpeed]               => b(quickly) => [gmBreakOut | gmTennis]
    [gmSpeed]               => b(4sec)    => [gmBrightness]
    [gmBrightness]          => b(quickly) => [gmPaddleLeft]
    [gmPaddleRight]         => b(quickly) => [gmEEPROM]
    [gmEEPROM]              => b(quickly) => [gmBreakOut | gmTennis]

    [gmBreakOut]            => successful pairing   => [gmTennis]
    [gmBreakOut]            => unsuccessful pairing => [gmBreakOut]
    [gmTennis]              => timeout              => [gmBreakOut]

The button-press related state changes are done in the function `readUniversalButton()`.
All other state changes are scattered all over the code, because changing the Game Mode is
an essential thing to do that can be necessary in a lot of situations.

### Master/Slave Concept in Tennis for Two

Tennis for Two is implemented using a Master/slave concept. That means, that
as soon as the Pairing finished successfully (see below), one device will become
the master and one the slave:

* Master: The master is rendering the whole game mechanics including the ball
  position, "ball physics" and ball impacts like hitting the wall or the paddle.
  Furtheron the master is also counting the score and telling the slave when
  to play which sound.

* Slave: The slave is basically just a display device showing the ball and
  the paddles exactly at the positions that the master defines and playing
  sounds that the master defines. It is additionally transmitting its own
  paddle position and the status of the button.

This approach has the big advantage that no additional device synchronization
is necessary - it is implicitly achieved due to the fact that the slave is doing
nothing without the master. The master/slave approach also fits best to the
radio chip's architecture: The nRF24L01+ can not act as a real bi-directional
transceiver, instead the recipient of a data package (slave) is able to send
payload inside the acknowledge package to the sender (master).

Common Mechanisms
-----------------

### "Ball Physics"

As we only have 8x8 pixels, implementing real ball physics is near to impossible.
This is why TeleBall is doing it according to the following rules:

* Only 45 degree angles.
* If the ball hits the paddle from [left \| right], then the ball is reflected
  to the [right \| left], i.e. the emergent angle equals the angle of incidence.
* The same is true, if the ball hits the wall (top, left, right).
* If the ball hits exactly the corner of the paddle then it is reflected to
  the direction it was coming from.
* If the ball hits a brick, it is randomly deflected either to the left or
  to the right, the incoming angle plays no role here.
* Twisting/spinning the ball: If the user moves the paddle quickly while
  the ball is hitting it, the ball's trajectory can be influenced by
  moving it one pixel to the left or to the right before reflecting it.
  Only with this feature it is possible to win a round of BreakOut at all,
  because otherwise, just like a bishop in chess, the ball would stay on
  well defined diagonal lines forever, never hitting bricks that are on
  "the other" diagonal lines.

The current state of the ball is comprised of the X and Y position stored in
`BallX` and `BallY` as well as the relative ball speed in both directions, stored
in `BalLDX` and `BallDY`. All of the he above mentioned mechanics are implemeted
by the following two functions, that are used within BreakOut as well as within
Tennis for Two.

* `calculateBallMovement(...)`: Move the ball according to `BallDX` and `BallDY`
  speeds and respect playfield boundaries, i.e. reflect at walls.

* `calculatePaddleImpact(...)`: Implement the above-mentioned rules when the ball
  hits the paddle. This is mapped to rather straightforward code. There is one
  exception, though: Due to the fact, that for a smooth gaming experience, the paddle
  movement is completely decoupled from the ball movement, a very special case can
  occur, where the paddle is moved "under" the ball after according to the current
  calculation iteration, the ball was already lost. But loosing the ball now would
  feel very unnatural to the player as he has the feeling, that he just in time
  moved the paddle under the ball, so this special case is handled correctly
  and the code for that is not that straightforward as it undos the last
  movement iteration.

### Waiting

There are multiple situations in BreakOut as well as in Tennis for Two, where the
ball is stopped for a while and "waiting", before the game continues. Some examples
are: at the beginning of the game; when loosing a ball before the new game begins;
after switching to Tennis; after switching back from a configuration menu to the
game; etc.

The waiting is implemented by setting the velocity vector `(BallDX | BallDY)` to zero.
Obviously `calculateBallMovement(...)` is then only adding zeros to the ball's
position and the ball is waiting / standing still.

At each `playBreakOut()` and `playTennis()` game loop iteration, the function
`respawn_management()` is called. It checks, if a certain timeout period set in
the variable `respawn_timer` is over and if yes, it sets the velocity vector
`(BallDX | BallDY) to (BallDX_tbs | BallDY_tbs)`; the suffix `_tbs` stands
for "to-be-set". This "respawn management" mechanism is not only used at actual
respawns (i.e. a ball was lost), but also to pause the game, as shown in `pauseGame(...)`.

### Mirrored X-Axis

Due to the way how we wired the MAX7221 to the KWM-R30881CBB, where the rows are the
cathodes, we need to mirror the x-axis. This is why `putPixel(...)` is doing the
following: `Matrix.setLed(0, 7 - x, y, on)

### Saving SRAM

The Arduino Nano only has about 2kB of SRAM, so it makes sense to use the program
memory for storing constant values. This is done by the `PROGMEM` keyword. Data
retrieval is done by specialized functions having the `_from_PROGMEM` suffix.

Hint: Declaring a global variable `const` is not enough to move it to the program
memory because the ATmega328 CPU needs special machine commands to read data from
there.

### EEPROM Memory Layout

Upon each device start, the TeleBall device fingerprint is checked. If it is there,
then TeleBall assumes valid data for the other variables. Otherwise it assumes a
factory new device and initializes the EEPROM memory with factory default values.

Bytes |Type        |Value
00..07|chars       |TeleBall device fingerprint: {'T', 'e', 'l', 'e', 'B', 'a', 'l', 'l'};
08..09|unsigned int|Ball speed
10    |byte        |Display intensity
11..12|unsigned int|Leftmost poti position (calibrate paddle user experience)
13..14|unsigned int|Rightmost poti position (calibrate paddle user experience)

BreakOut
--------

### Game Loop

BreakOut's game loop is part of the main `loop()` as it is called from the inside the
`switch`statement there. `playBreakOut()` must not disturb the "heartbeat", therefore
also no function there shall be blocking. An excpetion is the case, when a player
wins or looses the game. Here, the game loop comes to a halt and the programm is looping
in a tight while loop.

{% highlight cpp %}
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
{% endhighlight %}

It is worth mentioning, that the movement of the paddle is asynchronously
decoupled from the movement of the ball: `handlePaddle(...)` is called with the
maximum speed that the Arduino can run through the game loop. On the other hand,
`calculateBallMovement(...)` and `calculatePaddleImpact(...)` is only called, when
an amount of milliseconds specified by the variable `Speed` has passed since the
last ball movement. This mechanism ensures, that the paddle movements feel natural
to the user, independent from the game's current speed.

### Adding or Modifing BreakOut Levels

1. Adjust the `const byte Levels` to the actual amount of levels that you have.
2. For each level, add a structure with `1` and `0` as shown below. Each `1` represents
   a brick and each `0` is an empty space.
3. For each level, adjust the `bricks_levelheight` array to reflect the amount of
   lines / rows that the actual level is having.
4. Currently, `4 is the hardcoded maximum height of a level. If your level has a smaller
   amount of rows, then fill it with `0` as shown in the below-mentioned code snippet's
   first level. We do not reccomend to have a larger amount of rows than `4`.

{% highlight cpp%}
const byte Levels = 3;
const byte bricks_levelheight[Levels] = {3, 4, 4};
const byte bricks_reset[Levels][4][8] PROGMEM =
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
{% endhighlight %}

Tennis for Two
--------------

### Device Pairing

In the first two minutes after turning the device on - or to be more precise: until the
milliseconds counter of the Arduino reaches the value specified by the global constant
`RadioPowerSaveTime`, TeleBall is scanning for another device / another player, that
could be a potential partner for Tennis for Two. After that time period, for saving
battery power, the radio device is put into a low-power mode (switched-off).

The scanning itself has two functions: Find another device and determine, who is
master and who is slave. It works like this:

1. Wait a random amount of time as specified by `RadioWait_Min` and `RadioWait_Max`;
   currently this is between 500 and 850 milliseconds.

2. Check if in the meantime - between the last check and the random wait time -  a
   package with the Master Token arrived. If yes, then there is another device: We
   acknowledge (ACK) the Master Token with the Slave Token,  which means that we
   are now the slave and the other device is the master and we continue with step (4). 

3. If no Master Token was recieved, the radio chip is switched into sending mode
   and we are sending a Master Token. If we receive a Slave Token as ACK then
   we are now the master and the other device is the slave and we can continue
   with step (4). Otherwise we switch back the radio chip into receive mode and
   go back to step (1).

4. Switch the global `game_mode` to `gmTennis`, which means that from now on
   the Tennis game loop will run and that the master / slave state machine will
   run.

5. Ask each user, if he wants to play Tennis for Two, by showing a "?" and let
   the user answer the question by turning the paddle. Right means yes and shows
   a "!" and left means no and shows a "?". If no: Stop scanning for other
   TeleBall devices by setting RadioMode to `rmIgnore` and go back to BreakOut.
   This functionality is performed in `tennisHandleMultiplayerQuestion()`. 

6. As soon as a player answers "!", i.e. yes, the state machine takes care,
   that the loop executes `tennisWaitForOtherPartyToJoin()`, which
   does what the name suggests. Additionally, it implements a timeout, so that
   after ~9 seconds of no answer, the device goes back to BreakOut mode and
   also stops scanning via `rmIgnore`.

7. If both players accept, the Tennis for Two game is started.

The above mentioned master and slave tokens are technically 32bit numbers
that are unique within the TeleBall radio protocol.

Note, that this whole Device Pairing process happens kind of asynchronously,
i.e. the whole operation is not blocking but part of the main `loop()` via
the `radioScanAndDetermineMode()` function and part of the `playTennis()`
game loop via "Radio Modes" (see below).

This algorithm, as well as the way the current whole TeleBall firmware talks
with the radio device is not capable of handling more then two TeleBall devices
in radio range; the behaviour would be undefined.

### Radio Specifics

#### Radio Configuration

TeleBall is currently hard coded to "channel 76". The nRF24L01+ has a channel
width of 1MHz on the 2.4GHz band, i.e. we are sending on 2.476GHz. Due to the
fact, that the 2.4 GHz band is pretty congested (WLAN, BlueTooth, microwave
ovens, etc.), we should implement some smarter behaviour and also in general
there is definitly room for improvement, when it comes to the current
shortcomings of TeleBall's radio range as described in [Play](play.html):
We could reduce the transmission speed, increase retries on failed sends,
use a better CRC mode and so on. Future versions of TeleBall will/should
implement this.

For establishing links between two radios, nRF24L01+ works with so called "pipes".
Pipes have addresses and only, if two pipes are having the same address, the sender
and the recipient can communicate with each other. Also this property is hardcoded;
in TeleBall's current firmware, everybody is sending into and listening to a pipe
called `TELEB`, i.e. having the 5-byte address that corresponds to the ASCII code
of `TELEB`.

The payload size as well as the ACK payload size (see below) is set to 4 bytes
length. In the code, we map this to `unsigned long int` values that we send
and receive.

#### FIFO Buffers

The radio hardware is working fully asynchronously from the Arduino by using
a send FIFO buffer (TX FIFO) and a recieve FIFO buffer (RX FIFO). When the
connection is established, the FIFOs will be filled in the background and
TeleBall's firmware code can check, if for example a new package arrived
in the RX FIFO or if there is still something in the RX FIFO which means,
that the recepient did not ackknowledge, yet for a multitude of reasons.

The TeleBall implementation of the receive function `radioReceive(...)`
is currently always emptying the RX FIFO which means, that messages
from the master can get lost not only due to radio problems but also
due to timing problems. Also, there is no TX FIFO for ACK messages
built into `radioReceive(...)`, so also messages from the
slave to the master can get lost. This opens room for improving the
communication / radio performance and robustness of TeleBall.

#### Unidirectional Communication

As described above, our radio chip, the nRF24L01+, is not offering a real bidirectional
communication mode. But it offers an advanced unidirectional communication called
Enhanced Shockburst. The most important feature of it for our purposes is the fact, that
the sender gets not only an acknowledge (ACK) from the recipient, that his package was well
received, but he also can get payload within the ACK packet.

In our master / slave model, the master is always the sender and the slave always the
recipient. In other words: The slave can never directly send something to the master,
he can only wait for receiving something and then use the ACK payload to send to
the master. So the master always needs to poll for the slave's data and the slave
always needs to be listening if a new package from the master arrives.

In normal game operation, we are using two global `32bit (4 byte) unsigned long int`
values to implement this: `RadioGameDataFromMaster` is sent from the master to the
slave and `RadioGameDataFromSlave` is the ACK payload package that is sent from the
slave to the master. Both variables are implemented as *bitfields*, so that we
can conveniently treat these 32bits / 4 bytes like a struct.

In other operation modes, e.g. during Pairing or during configuration, the nature
of the 4 byte packages changes to "Tokens" (as described above) or to simple
`unsigned ints` like in Speed Set mode.

#### Timeout

The two main functions for implementing our pseudo-bidirectional communication
aka sender pools and recipient answers via ACK payload are: `radioSend(...)` and
`radioReceive(...)`.

Both functions are resetting the timeout counter on success, that means that
they need to be called very regularly, otherwise the timeout function of the
main loop will be activated. Therefore, even when "nothing is happening", master
and slave need to communicate with each other. This is why for example in
Tennis for Two even the "won or lost smileys" are not leaving the main game
loop: Communication is kept alive and therefore the timeout will not happen.

### Implementing the Master / Slave Concept

#### Radio Mode State Machine

The master / slave mechanism is implemented using a second state machine on
top of the Game Mode state machine: Radio Mode. The basic Radio Mode states
are handled in the main game loop of tennis called `playTennis()`. Other
Radio Modes are handled at `adjustSpeed()` and at places scattered all
over the code, similarly to the Game Mode.

{% highlight cpp %}
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
{% endhighlight %}

The following state changes are for the sake of simplicity shown by only using the master
as an example. They work the analogical at the slave's side:
 
    [rmNone]         =>  found slave     => [rmMaster_init]
    [rmMaster_init]  =>  answered: yes   => [rmMaster_wait]
    [rmMaster_init]  =>  answered: no    => [rmIgnore] & game_mode = gmBreakOut
    [rmMaster_wait]  =>  other joins     => [rmMaster_run]
    [rmMaster_wait]  =>  timeout: user   => [rmIgnore] & game_mode = gmBreakOut
    [rmMaster_run]   =>  b(quickly)      => [rmMaster_reset]
    [rmMaster_run]   =>  b(1sec)         => [rmMaster_speedset_by_Master]
    [rmMaster_run]   =>  slave: SpeedSet => [rmMaster_speedset_by_Slave]
    [rmMaster_reset] =>  master: reset   => [rmMaster_run]
    [<any state>]    =>  timeout: radio  => [rmNone] & game_mode = gmBreakOut

    [rmMaster_speedset_by_Master] => b(quickly) => [rmMaster_run]

    [rmMaster_speedset_by_Master] => b(4sec)
                                  => [rmMaster_speedset_by_Master] & 
                                     game_mode cycles through:
                    [gmBrightness]          => b(quickly) => [gmPaddleLeft]
                    [gmPaddleRight]         => b(quickly) => [gmEEPROM]
                    [gmEEPROM]              => b(quickly) => [gmTennis]

    [rmMaster_speedset_by_Slave] => slave: Flag_Leave => [rmMaster_run]

Explanation of state changes:

* "`found slave`" indicates a successful step (3) as described above in Device Pairing
* The "`answered`" condition is related to the "?" / "!" choice.
* "`other joins`" means, that while being in the the `tennisWaitForOtherPartyToJoin()` function,
  the master received a `RadioSlaveWaitA` token when querying (polling) for it using
  the `RadioMasterWaitQ` token, i.e. that the slave joined the Tennis for Two match.
* "`timeout: user`"" means, that the master answered "!" and started waiting
  and the slave did not answer at all even after the timeout of ~9 secs, or answered "?".
* "`b(x)`" means the red button on the master device was pressed as long as x.
* "`slave: SpeedSet`" means slave sendet a speed set request via ACK and the
  master acknowledged it [!sic] by setting the flag `RadioGameDataFromMaster.SpeedSet_Ack`.
* "`master: reset`": master has performed a reset and sends a reset command to slave
* "`timeout:radio`" means, that the radio connection between the devices broke
  down; this is why the new state of the state machine is [rmNone], i.e. the devices
  try to connect again, instead of [rmIgnore].

* While cycling through the advanced configuration options (brightness, paddle, EEPROM),
  the Radio Mode stays rmMaster_speedset_by_Master to ensure, that the communication
  between master and slave continues and no `timeout: radio` happens.

#### Different Game Loops for Master and Slave

The master / slave concept implies that both devices are functioning completely differently,
as already described above. In short: The master "does / calculates everything" and the slave
is just a "dumb display device". This is reflected in different game loops for master
and slave.

##### Pseudocode for Master Game Loop `tennisPlayMaster()`:

 1. Transform own coordinates into the coordinate space of the slave
 2. Transmit control structure, including mirrored coordinates, current score of slave, 
    flags (won or lost? reset? speed set?), sound commands (wall, paddle, lost)
 3. Send `RadioGameDataFromMaster` and recieve `RadioGameDataFromSlave`
 4. Handle won or lost state
 5. Draw: ball, local paddle, remote paddle
 6. Handle special requests from the slave: reset, speed set mode
 7. Calculate the ball movement
 8. Calculate the paddle impact: for the master and for the slave paddle
 9. Check for lost ball
10. Issue sound commands during (7), (8) and (9)
11. Handle ball respawn

##### Pseudocode for Slave Game Loop `tennisPlaySlave()`:

 1. Draw own paddle
 2. Transform paddle coordinates into the coordinate space of the master and fill the
    ACK data structure that is used to transmit these coordinates during receiving
 3. Receive `RadioGameDataFromMaster` and send `RadioGameDataFromSlave` via ACK
 4. Copy the ball's coordinates and movement vector plus other data into local variables
 5. Handle won or lost state
 6. Draw the ball and the remote paddle and play sounds
 7. Handle special requests from the master: reset, speed set mode

#### Identifying the Master and the Slave Device

The current version 1.2 of the TeleBall firmware shows during the first two seconds of the
"question mode" (in Device Pairing), while the "?" displayed statically, if a device is
a master or a slave device:

* Master: Leftmost pixel at the bottom line of the 8x8 display is lit.
* Slave: The second-leftmost pixel at the bottom line is lit.

Adding More Games
-----------------

TeleBall's current firmware is only using about half of the program memory of the Arduino Nano.
That means there is plenty of room left for software extensions. As an inspiration for you,
here are simple games, whose gameplay can be kind of adapted to the 8x8 LED display:

* Tennis for Four (four TeleBall devices)
* Tetris
* Space Invaders
* Side scrollers: Jump'n'Run or shooters
* Pac Man
* Snake


If you want to add more games we suggest, that you do it like described here:

1. Use the concept of [Multi File Sketches](http://arduino.cc/en/Hacking/BuildProcess) to stay
   updatable: If you put the main code of your new games to separate files with no file extension,
   e.g. you add the file `MyNewGame` (no file extension) to the folder `src/TeleBall`, then
   you are able to use everything from this file inside the main file `TeleBall.ino`. Even
   though you will need to touch `TeleBall.ino`, this way of working minimizes the code
   merging in future. You might need to declare function headers (one liners) in each of
   the files so that the compiler finds everything.

2. Add a new mode to the `GameMode` enum as shown above and extend the main loop's
   `switch`statement by a new case, something like `gmMyNewGame`. Be mindful, that
   no function should "block", so if something takes longer, implement it asynchronously.
   Furtheron, add a custom reset routine and add it to the `switch` statement in `reset()`.
   Last but not least, write some routines that are able to backup and to restore your
   game state and call them inside `backupGameState()` and `restoreGameState()`.

3. Exending the behaviour of `readUniversalButton()` seems to be a good place to add
   the entry point for your new game. Alternately or additionally, you can change
   `game_mode_default` to let TeleBall start with your new game instead of with BreakOut.

4. There is plenty of room left in the EEPROM, too, so it is safe to create new `loc...`
   constants and to extend `eepromWriteFingerprintAndDefaults()`, `eepromReadSettings()`
   and `eepromWriteSettings()` accordingly. We suggest you keep to the convention and
   use the suffix `_default`for factory default `const` variables.