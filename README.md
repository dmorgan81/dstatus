#dstatus

I decided to fix up my custom dwm status bar that I've hacked on for awhile into something a bit more presentable. Therefore, I give you dstatus!

So what makes this different from the rest? For starters, it's somewhat laptop specific. Things like battery, backlight, and possibly SSID aren't of much use to a desktop. However, these bits can be turned off at compile time if you don't want them. In fact, all the elements can be toggled and rearranged however you like it.

In addition, the main loop polls as little as necessary; just time, cpu usage, and battery are polled. Other elements are updated by threads listening to events.

##Configuration
* config.mk defines which elements will be compiled into the binary. Turn any of them off by commenting out the appropriate line.
* config.h defines the look of everything. Elements can be formatted (in a limited fashion) according to printf. Some elements can display either a percentage or a bar.

An [AUR package](https://aur.archlinux.org/packages/dstatus-git/) for Arch Linux is also available.

The source is [UNLICENSED](http://unlicense.org) so feel free to hack away. :-)
