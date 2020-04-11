
// Copyright 2020 Justin Tomlin.
// Author: Justin Tomlin (justin.a.tomlin@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// This Teensy sketch sends USB midi clock on reception of program change (channel 15) or CC 3 (all channels)
//  with a tempo equal to CC or PC value plus an offset of 40
// 
// The built in LED is set to blink on each beat (quarter note)
//
// Synchronization of LED isn't quite right yet after a tempo change, and the SPP sync needs some work
//
// Tested on Teensy 3.2 with Synthstrom Deluge as USB MIDI host and Arduino USB Type: "MIDI"

// includes
#include <MIDI.h>

// defines
#define MIDI_CLOCK_PPN 24u
#define MIN_PER_SEC (1.0f/60.0f)
#define SEC_PER_US (1.0f/(1000000.0f))
#define BPM_OFFSET 40.0f // this will support BPMs from 40 to 167 (CC range of 0-127 offset by +40)
#define SPP_BEATS_PER_QUARTER 4u
#define SPP_BEATS_PER_MEASURE 16u
#define SPP_PULSES_PER_BEAT (MIDI_CLOCK_PPN / SPP_BEATS_PER_QUARTER)
#define LED_BEAT_DIV_RATIO 2

//#define SPP_SYNC // not working yet

// consts
typedef enum
{ CC_Tempo = 3 // an undefined CC, purposed for setting tempo of the clock
} CC_Type;

typedef enum
{ PC_Tempo = 15
} PC_Type;

// static variables
IntervalTimer midiClockTimer;
bool pulsePending = false;
bool extClockPending = false;
uint8_t pulseCounter = 0;
#ifdef SPP_SYNC
uint16_t SPP = 0;
#endif
bool timerEn = false;

// interrupt service routines
void TimerISR(void)
{
  pulsePending = true;
}

// utility functions
uint32_t UpdateIntervalFromBPM(float BPM)
{
  return static_cast<uint32_t>(1.0f / (static_cast<float>(MIDI_CLOCK_PPN) * BPM * MIN_PER_SEC * SEC_PER_US));
}

// event handlers
void CC_Handler(byte channel, byte control, byte value)
{
  if (control == CC_Tempo) // omni, since channel is not being checked
  {
    uint32_t clockIntervalUs = UpdateIntervalFromBPM(static_cast<float>(value) + BPM_OFFSET);

    if (timerEn)
    {
      midiClockTimer.update(clockIntervalUs);
    }
    else
    {
      midiClockTimer.begin(TimerISR, clockIntervalUs);
      timerEn = true;
    }
  }
}

void PC_Handler(byte channel, byte value)
{
  if (channel == PC_Tempo)
  {
    uint32_t clockIntervalUs = UpdateIntervalFromBPM(static_cast<float>(value) + BPM_OFFSET);

    if (timerEn)
    {
      midiClockTimer.update(clockIntervalUs);
    }
    else
    {
      midiClockTimer.begin(TimerISR, clockIntervalUs);
      timerEn = true;
    }
  }
}

void RealTimeSystemHandler(byte realtimebyte)
{
  switch (realtimebyte)
  {
    case usbMIDI.Start:
    {
      pulseCounter = 0; // reset pulseCounter to sync with start
      if (timerEn)
      {
        // disable timer to allow host to take control of clock
        midiClockTimer.end();
        timerEn = false;
      }
    }
    break;
    case usbMIDI.Stop:
    {
      if (timerEn)
      {
        // disable timer to allow host to take control of clock
        midiClockTimer.end();
        timerEn = false;
      }
    }
    break;
    case usbMIDI.Clock:
    {
      extClockPending = true;
    }
    break;
  }
}

#ifdef SPP_SYNC
void SongPositionHandler(uint16_t beats)
{
  SPP = beats;
  if (SPP % SPP_BEATS_PER_MEASURE) // resync once per measure
  {
    pulseCounter = 0;
  }
}

void SongPositionIncrement(void)
{
  SPP++;
  SongPositionHandler(SPP);
}
#endif

void ClockPulseHandler(void)
{
  if (pulseCounter == 0)
  {
    digitalWrite(LED_BUILTIN, HIGH); 
  }
  else if (pulseCounter == (MIDI_CLOCK_PPN / LED_BEAT_DIV_RATIO))
  {
    digitalWrite(LED_BUILTIN, LOW); 
  }
  pulseCounter = (pulseCounter + 1) % MIDI_CLOCK_PPN;
#ifdef SPP_SYNC
  if ((pulseCounter % SPP_PULSES_PER_BEAT) == 0u)
  {
    SongPositionIncrement();
  }
#endif
}

// arduino setup and loop functions
void setup() 
{
  // put your setup code here, to run once:
  usbMIDI.setHandleControlChange(CC_Handler);
  usbMIDI.setHandleRealTimeSystem(RealTimeSystemHandler);
#ifdef SPP_SYNC  
  usbMIDI.setHandleSongPosition(SongPositionHandler);
#endif
  usbMIDI.setHandleProgramChange(PC_Handler);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() 
{
  if (pulsePending)
  { // send clock on each timer tick
    usbMIDI.sendRealTime(usbMIDI.Clock);
    usbMIDI.send_now();
    pulsePending = false;
    ClockPulseHandler();
  }
  else if (extClockPending)
  { // handle external clock (blink LED, etc)
    extClockPending = false;
    ClockPulseHandler();
  }
  else
  {
    usbMIDI.read();
  }
}
