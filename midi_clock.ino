/*
 * MC-01 MIDI Clock
 * 
 * Arduino-based MIDI clock, with optional rotary encoder and 7-segment display.
 * 
 * Minimal configuration is an Arduino with two buttons and two LEDs.
 * 7-segment display driven by 74HC164 shift register.
 * 
 * copyright 2018 Rob Hailman
 * licensed under MIT license
 */
 
#define ENCODER // comment out this line if not using a rotary encoder
#define DISPLAY // comment out this line if not using a 7-seg display

#ifdef ENCODER
#define ENCODER_PIN_1 2 // IC pin 4
#define ENCODER_PIN_2 3 // IC pin 5
#endif

#ifdef DISPLAY
#define DATA_PIN 5    // IC pin 11
#define ENABLE_PIN 6  // IC pin 12
#define CLOCK_PIN 7   // IC pin 13
#define DELAY_TIME 1  // time between digits for display multiplexing
#endif

#define TAP_TEMPO_PIN A2    // IC pin 25
#define START_STOP_PIN A3   // IC pin 26
#define TEMPO_LED_PIN A4    // IC pin 27
#define RUNNING_LED_PIN A5  // IC pin 28

#define TEMPO_MIN 40
#define TEMPO_MAX 300

#define PPQ 24              // ticks per quarter note - 24 for MIDI
#define DEBOUNCE_TIME 30    // debounce time for button presses in millis. Increase as needed for chosen buttons.
#define TAP_EXPIRE_FACTOR 2
#define TAP_EXPIRE_DEFAULT 1715000

#include <TimerOne.h>
#include <MIDI.h>

#ifdef ENCODER
#include <Encoder.h>

Encoder tempoEncoder(ENCODER_PIN_1, ENCODER_PIN_2);
int position = 0;
#endif

#ifdef DISPLAY
int output_pins[3] = {9, 10, 8};  // in reverse order (least significant digit first). 
                                  // IC pins 15 (3), 16 (2), 14 (1)

char digits[10] = {               // these may need to be adjusted for wiring between shift register outputs and 
  0xEB,                           // 7-seg inputs
  0x28,
  0xB3,                           // this is based on following order: E, D, DP, C, G, B, F, A
  0xBA,
  0x78,
  0xDA,
  0xDB,
  0xA8,
  0xFB,
  0xFA
};
#endif

bool started = false;

unsigned long lastTapDebounceTime = 0; // millis
unsigned long lastStartDebounceTime = 0; // millis
unsigned long lastTapTime = 0; // micros
unsigned long tapExpireTime = TAP_EXPIRE_DEFAULT; // micros

byte lastTapState;
byte lastStartState;
int tempo = 120;
byte currentTick = 0;

MIDI_CREATE_DEFAULT_INSTANCE();

void setup() {
  pinMode(TEMPO_LED_PIN, OUTPUT);
  pinMode(RUNNING_LED_PIN, OUTPUT);
  pinMode(TAP_TEMPO_PIN, INPUT_PULLUP);
  pinMode(START_STOP_PIN, INPUT_PULLUP);
  lastTapState = digitalRead(TAP_TEMPO_PIN);
  lastStartState = digitalRead(START_STOP_PIN);
  Timer1.initialize(calcTempoMicros());
  Timer1.attachInterrupt(clockTick);
  MIDI.begin(MIDI_CHANNEL_OMNI);
  
#ifdef DISPLAY
  for (int i = 0; i < 3; i++)
  {
    pinMode(output_pins[i], OUTPUT);
    digitalWrite(output_pins[i], HIGH);
  }
  pinMode(DATA_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
#endif
  
}

void loop() {
#ifdef ENCODER
  long newPos = tempoEncoder.read();
  if (newPos != position)
  {
    position = newPos;
    if (position > 3)
    {
      changeTempo(1);
    } else if (position < -3)
    {
      changeTempo(-1);
    }
  }
#endif
  doTapInput();
  doStartStopInput();
#ifdef DISPLAY
  doDisplay();
#endif
}

void doStartStopInput()
{
  int newStartState = digitalRead(START_STOP_PIN);
  if (newStartState != lastStartState)
  {
    if (lastStartDebounceTime == 0)
    {
      lastStartDebounceTime = millis();
      lastStartState = newStartState;
      if (lastStartState == LOW)
      {
        started = !started;
        if (started)
        {
          MIDI.sendRealTime(midi::Start);
          digitalWrite(RUNNING_LED_PIN, HIGH);
          currentTick = 0;
        } else {
          MIDI.sendRealTime(midi::Stop);
          for (int i=1; i<=16; i++)
          {
            MIDI.sendControlChange(123, 0, i); // all notes off
          }
          digitalWrite(RUNNING_LED_PIN, LOW);
        }
      }
    }
  }
  if (millis() - DEBOUNCE_TIME >= lastStartDebounceTime)
  {
    lastStartDebounceTime = 0;
  }
}

void doTapInput()
{
  int newTapState = digitalRead(TAP_TEMPO_PIN);
  if (newTapState != lastTapState)
  {
    if (lastTapDebounceTime == 0)
    {
      lastTapDebounceTime = millis();
      lastTapState = newTapState;
      if (lastTapState == LOW)
      {
        tapTempo();
      }
    }
  } else {
    if (lastTapTime > 0 && micros() - tapExpireTime >= lastTapTime)
    {
      lastTapTime = 0;
    }
  }
  if (millis() - DEBOUNCE_TIME >= lastTapDebounceTime)
  {
    lastTapDebounceTime = 0;
  }
}

void tapTempo()
{
  if (lastTapTime == 0)
  {
    lastTapTime = micros();
    tapExpireTime = TAP_EXPIRE_DEFAULT;
  } else {
    unsigned long newTapTime = micros();
    unsigned long timeDifference = newTapTime - lastTapTime;
    tapExpireTime = timeDifference * TAP_EXPIRE_FACTOR;
    int bpm = 60000000 / timeDifference;
    tempo = bpm;
    Timer1.setPeriod(calcTempoMicros());
    lastTapTime = newTapTime;
  }
}

void changeTempo(int offset) {
  tempo += offset;
  if (tempo < TEMPO_MIN)
  {
    tempo = TEMPO_MIN;
  } else if (tempo > TEMPO_MAX)
  {
    tempo = TEMPO_MAX;
  }
  Timer1.setPeriod(calcTempoMicros());
#ifdef ENCODER
  tempoEncoder.write(0);
#endif
}

long calcTempoMicros() {
  long tempoMicros = (60 * 1000000) / (tempo * PPQ);
  return tempoMicros;
}

void clockTick()
{
  if (started)
  {
    MIDI.sendRealTime(midi::Clock);
  }
  if (currentTick == 0)
  {
    digitalWrite(TEMPO_LED_PIN, HIGH);
  }
  if (currentTick == 4)
  {
    digitalWrite(TEMPO_LED_PIN, LOW);
  }
  currentTick++;
  if (currentTick == 24)
  {
    currentTick = 0;
  }
}

#ifdef DISPLAY
void doDisplay()
{
  int digit = tempo;
  int i = 0;
  while (digit)
  {
    digitalWrite(ENABLE_PIN, HIGH);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, digits[digit % 10]);
    digitalWrite(ENABLE_PIN, LOW);
    digitalWrite(output_pins[i], LOW);
    delay(DELAY_TIME);
    digitalWrite(output_pins[i], HIGH);
    digit /= 10;
    i++;
  }
}
#endif
