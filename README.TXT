==============================================================================

    T E L E B A L L 

    
    Retro style Arduino based BreakOut & Tennis game
    made in August .. December 2014
    
    * idea, code and original circurit design by sy2002
    * additional circurit design and board layout by doubleflash
    * body housing/case by Miro
    
    License: You are free to share and adapt for any purpose,
    even commercially as long as you attribute to
    sy2002, doubleflash and Miro and link to http://www.sy2002.de
    
    http://creativecommons.org/licenses/by/4.0/    

==============================================================================

* TeleBall is fun to play! It offers two play modes: A 3-level BreakOut clone
  as a single player game and a two player Tennis game using radio to connect
  two TeleBall devices. It is delighting to see, how much fun playing on a
  8x8 dot matrix display using a poti-controller can be.

* Open index.html with a browser to get a much better project documentation,
  how-to play and how-to build guide and a bill of materials (BOM).

* Folder structure:

    cad:    CAD data for 3D printing the TeleBall case
            Use ".stl" to modify the design or to create a custom output file.
            Most online 3D print shops can use the ".cmb.gz" for production.
            Made with FreeCad: http://www.freecadweb.org

    doc:    HTML documentation; use "index.html" in root folder to browse

    eagle:  Electronic schematic diagram including PCB layout.
            The libraries.zip files are only needed, if you want to modify
            anything; otherwise you can just send the ".brd" file to any
            online PCB manufacturing company to receive a TeleBall PCB.
            Made with the freeware version of Eagle: http://www.cadsoftusa.com

    src:    Arduino source code aka Sketch in the subfolder "TeleBall".
            Compile and upload it to your device using Arduino's IDE. You need
            to install the libraries from libraries.zip first, before being
            able to compile. The "archive" folder contains historic versions.
            Made with Arduino: http://arduino.cc/de/Main/Software


Last modified by sy2002 on 26th of December 2014