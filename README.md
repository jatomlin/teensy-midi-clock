# teensy-midi-clock
teensyduino sketch to send USB MIDI clock with MIDI tempo control

This Teensy sketch sends USB midi clock on reception of program change (channel 15) or CC 3 (all channels) with a tempo equal to CC or PC value plus an offset of 40

The built in LED is set to blink on each beat (quarter note)

Synchronization of LED isn't quite right yet after a tempo change, and the SPP sync needs some work

Tested on Teensy 3.2 with Synthstrom Deluge as USB MIDI host and Arduino USB Type: "MIDI"

