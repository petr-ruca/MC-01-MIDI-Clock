# MC-01
Arduino based midi clock

A simple Arduino based MIDI clock, with optional rotary encoder and 7-segment display.

Output is wired per the MIDI standard, ie [here](https://learn.sparkfun.com/tutorials/midi-tutorial/hardware--electronic-implementation), with pin 5 of MIDI connector connected to serial TX pin of Arduino / microcontroller.

```digits[]``` in the sketch may need to be adjusted depending on the wiring between shift register and 7-segment display. See comments for details.
