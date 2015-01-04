---
title: TeleBall - Case
layout: default
---

This is Build
=============

Here comes a code example. kqejrklq welkqje lkqwje qwlkjelqkje lkqwj e
qwejklqkj falkdj sdlkfj lwkqrj,sdv jlqewfn welfjweöf.

{% highlight cpp %}
//show the remaining balls in a row of LEDs
void manageLEDs(byte how_many_on)
{
    for (int i = 0; i < LED_count; i++)
        digitalWrite(BallDisplay[i], i < how_many_on ? HIGH : LOW);
}
{% endhighlight %}
