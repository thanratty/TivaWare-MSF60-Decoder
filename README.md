
# TivaWare-MSF60-Decoder

## Description

A library and implementation example for decoding the UK MSF atomic clock signal broadcast on 60 kHz.

The decoder itself is hardware agnostic and requires only edge triggered interrupts and a free-running millisecond timer counter. Although this particular implementation is targetted at a Texas Instruments TM4C microcontroller it should be easy to port to other platforms.

### Build Environment

- Code Composer Studio 10.4.0
- TivaWare SDK version 2.2.0.295
- T.I. Driver Library 2.2.0.295
- EK-TM4C1294XL evaluation board



The repository contains two project folders:
<table>
<tr><td>MSF60decode</td><td>The decoder library</td></tr>
<tr><td>decoder-test</td><td>Example project using the library</td></tr>
</table>


Import the projects into you CCS workspace and build. You may need to tweak include paths or search paths to match your TivaWare installation.


### Configuration

Hardware & software configuration options are in config.h

To help with porting, when doing a debug build support for an optional debug UART and blinky LED can be included in the library, along with various logging options. See config.h for details.


### Prior Work

Much of the BCD decoding is adapted from an Arduino project on [The Oddbloke Geek Blog](http://danceswithferrets.org/geekblog/?p=44)


### External Links

Wikipedia article [here](https://en.wikipedia.org/wiki/Time_from_NPL_(MSF))<br>
National Physics Laboratory MSF signal specification [here](https://www.npl.co.uk/products-services/time-frequency/msf-radio-time-signal/msf_time_date_code)


### Licence

This software is Public Domain. Do whatever the hell you want with it.
