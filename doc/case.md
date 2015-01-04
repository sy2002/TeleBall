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
---

{% include gallery_init.html %}

TeleBall Case - CAD and 3D Printing
===================================

Designing the Case: CAD
---

[FreeCad](http://www.freecadweb.org) is an Open Source parametric 3D CAD modeler.
We used it to design the TeleBall Case. The following gallery is showing you the
case and the lid 3D rendered by FreeCad.

{% include gallery_show.html images=page.cad_images group=1 %}

The mechanism for keeping the case closed stable can be best understood when you
have a look at the rightmost image (click to zoom): The lever shown there is
clicking into place when being moved over the hole shown at the leftmost image
while closing.


Manufacturing the Case: 3D Printing
-----------------------------------

The TeleBall package contains a folder called `cad`. The files stored there
are used to 3D print the TeleBall device consisting of the case and the lid.

    case_full_3dprinter.cmb.gz   
    case_body_v2.stl
    case_lid_v2.stl

Most online 3D printing shops are accepting `.cmb.gz` files, this is why added this
package already for your convenience; it contains both, the case and the lid.
Alternately, you can use FreeCad or other tools to create your own 3D Printing
file package using the `.stl` files as sources.

3D Printed Case and Lid
-----------------------

The following photos show you, what you can expect back from your 3D printing shop.

{% include gallery_show.html images=page.photos width=260 group=2 %}

Ready Assembled Device
----------------------

This is some sample Lorem Ipsum text. Wslk jfwkljeflwkejf welkjf w.
This is some sample Lorem Ipsum text. Wslk jfwkljeflwkejf welkjf w.
This is some sample Lorem Ipsum text. Wslk jfwkljeflwkejf welkjf w.
This is some sample Lorem Ipsum text. Wslk jfwkljeflwkejf welkjf w.
