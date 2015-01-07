---
title: TeleBall - Case
layout: default
imgfolder: cad
cad_images:
  - name: case1.png
    thumb: case1.png
    text: TeleBall CAD Case Front
  - name: case2.png
    thumb: case2.png
    text: TeleBall CAD Case Back
  - name: lid1.png
    thumb: lid1.png
    text: TeleBall CAD Lid Front
  - name: lid2.png
    thumb: lid2.png
    text: TeleBall CAD Lid Back
photos:
  - name: photo_case1.jpg
    thumb: thumb_case1.jpg
    text: TeleBall 3D Printed Case Front
  - name: photo_case2.jpg
    thumb: thumb_case2.jpg
    text: TeleBall 3D Printed Case and Lid - Back
readydevice:
  - name: photo1.jpg
    thumb: thumb1.jpg
    text: TeleBall Front View
  - name: photo2.jpg
    thumb: thumb2.jpg
    text: TeleBall Side View
  - name: photo3.jpg
    thumb: thumb3.jpg
    text: TeleBall Top View
  - name: photo4.jpg
    thumb: thumb4.jpg
    text: TeleBall Rear View
  - name: photo5.jpg
    thumb: thumb5.jpg
    text: TeleBall Bottom View        
  - name: photo6.jpg
    thumb: thumb6.jpg
    text: TeleBall Disassembly 1
  - name: photo7.jpg
    thumb: thumb7.jpg
    text: TeleBall Disassembly 2
  - name: photo8.jpg
    thumb: thumb8.jpg
    text: TeleBall Disassembly 3
  - name: photo9.jpg
    thumb: thumb9.jpg
    text: TeleBall Disassembly 4
  - name: photo10.jpg
    thumb: thumb10.jpg
    text: TeleBall Disassembly 5
  - name: 16teleballs.jpg
    thumb: 16teleballs_thumb.jpg
    text: 16 TeleBalls
---

{% include gallery_init.html %}

TeleBall Case - CAD and 3D Printing
===================================

Designing the Case: CAD
---

[FreeCad](http://www.freecadweb.org) is an Open Source parametric 3D CAD modeler.
We used it to design the TeleBall Case. The following gallery is showing you the
case and the lid 3D rendered by FreeCad.

{% include gallery_show.html images=page.cad_images width=130 group=1 %}

The mechanism for keeping the case closed stable can be best understood when you
have a look at the rightmost image (click to zoom): The lever shown there is
clicking into place when being moved over the hole shown at the leftmost image
while closing.

Toggle *Thingiview* on [Thingiverse](http://www.thingiverse.com/thing:621294)
to get a realtime interactive 3D view of the case and the lid: Click the blue
"photos", then click on *Thingiview* and then use your mouse to zoom and rotate.

Manufacturing the Case: 3D Printing
-----------------------------------

The TeleBall package contains a folder called `cad`. The files stored there
are used to 3D print the TeleBall device consisting of the case and the lid.

    case_full_3dprinter.cmb.gz   
    case_body_v2.stl
    case_lid_v2.stl

Some online 3D printing shops are accepting convenient packaged `.cmb.gz` files,
which are already containing the whole device. Others are accepting `.stl` files,
where you are uploading and ordering the body (case) and the lid separately.
Alternately, you can use FreeCad or other tools to create your own 3D Printing
file package using the `.stl` files as sources.

You can also download the files needed for 3D printing from
[Thingiverse](http://www.thingiverse.com/thing:621294).

3D Printed Case and Lid
-----------------------

The following photos show you, what you can expect back from your 3D printing shop.

{% include gallery_show.html images=page.photos width=280 group=2 %}

Ready Assembled Device
----------------------

{% include gallery_show.html images=page.readydevice height=150 group=3 %}
