#include <EEPROM.h>

#include <Adafruit_SleepyDog.h>

#include <Wire.h>
#include <DS3231.h>
DS3231 clock;
RTCDateTime rdt;

#include <Servo.h>

// Timer to turn off the main gas valve for a stove in Blech mode
// Written for an Arduino Nano (clone)
// Connect a servo to D9.
// Connect a regular "Digital Tube" board (8 LED, 8 7-segment LED units, 8 buttons, TM1638 chip) to Strobe: A2, Clock: A1, Data: A0.
// Connect a DS3231 module to the I2C pins (SDA: A4 and SCL: A5)

// Button 0 and 1 (from left) set the servo to on and off in base mode (display shows On or OFF). Button 3 changes to next state.
// Button 4-7 set the off time (hour + and -, min + and -) for the countdown. Display shows tOFF. Button 3 changes to next state.
// Button 4-7 set the on postition of servo (10s + and -, 1s + and -). Display shows POn. Button 3 changes to next state.
// Button 4-7 set the off postition of servo (10s + and -, 1s + and -). Display shows POFF. Button 3 changes to initial state.

Servo myservo;
int on_pos, off_pos = 0;

// TM1638 code taken from https://github.com/moozzyk/TM1638
const int tm_strobe = A2;
const int tm_clock = A1;
const int tm_data = A0;

#define EE_ON_OFF 0
#define EE_ON_POS 4
#define EE_OFF_POS 8


int d[8];

//State machine
int state = 0;
// blech state
boolean blech_on = false;

unsigned countdown_minutes = 0;

int last_min;


void sendCommand(uint8_t value)
{
  digitalWrite(tm_strobe, LOW);
  shiftOut(tm_data, tm_clock, LSBFIRST, value);
  digitalWrite(tm_strobe, HIGH);
}
 
void tm_reset()
{
  sendCommand(0x40); // set auto increment mode
  digitalWrite(tm_strobe, LOW);
  shiftOut(tm_data, tm_clock, LSBFIRST, 0xc0);   // set starting address to 0
  for(uint8_t i = 0; i < 16; i++)
  {
    shiftOut(tm_data, tm_clock, LSBFIRST, 0x00);
  }
  digitalWrite(tm_strobe, HIGH);
}
 
void setup()
{
//  int countdownMS = Watchdog.enable(20000); // Enable watchdog
//  Watchdog.reset();
  clock.begin();
  rdt = clock.getDateTime();
  last_min = rdt.minute;
  
  // Use this for initial time setting (we don't care if it's off a few minutes - the time is only used for resetting the temperatures)
//  clock.setDateTime(__DATE__, __TIME__);
  myservo.attach(9);  
 
  pinMode(tm_strobe, OUTPUT);
  pinMode(tm_clock, OUTPUT);
  pinMode(tm_data, OUTPUT);
 d[0] = 0x01ff;
 d[1] = 0x01f0;
 d[2] = 0x000f;
 d[3] = 0x000f;
 d[4] = 0x01ff;
 d[5] = 0x01f0;
 d[6] = 0x000f;
 d[7] = 0x000f;

  sendCommand(0x8f);  // activate and set brightness to max
  tm_reset();
  state = 0;
  EEPROM.get(EE_ON_OFF, blech_on);
  EEPROM.get(EE_ON_POS, on_pos);
  EEPROM.get(EE_OFF_POS, off_pos);
}

void loop()
{
  uint8_t btn;
  btn = readButtons();
  // Read time/date
  rdt = clock.getDateTime();
  
  if ((rdt.minute != last_min) && (countdown_minutes > 0)) {
    countdown_minutes -= 1;
    if (countdown_minutes == 0) blech_on = false;
    last_min = rdt.minute;
  }

  state_transition(btn);
  calculate_display_content(btn);
  update_display();

  if (blech_on) {
    myservo.write(on_pos);
  } else {
    myservo.write(off_pos);
  }
  
//  Watchdog.reset();
delay(500);

}

void state_transition(uint8_t btn) {
  switch (state) {
    case 0:
      if ((btn&0x01) != 0) {
        blech_on = 1;
        EEPROM.put(EE_ON_OFF, blech_on);
        }
      if ((btn&0x02) != 0) {
        blech_on = 0;
        EEPROM.put(EE_ON_OFF, blech_on);
      }

      if ((btn&0x08) != 0) 
        {
          state = 1;
         }
      break;
    case 1:
      if ((btn&0x08) != 0) 
        {
          state = 2;
         }
      if ((btn&0x10) != 0) countdown_minutes += 60;
      if (((btn&0x20) != 0)  && (countdown_minutes > 60))countdown_minutes -= 60;
      if ((btn&0x40) != 0) countdown_minutes += 1;
      if (((btn&0x80) != 0) && (countdown_minutes > 0))countdown_minutes -= 1;
      break;
    case 2:
      if ((btn&0x08) != 0) 
        {
          EEPROM.put(EE_ON_POS, on_pos);
          state = 3;
         }
      if ((btn&0x10) != 0) on_pos += 10;
      if (((btn&0x20) != 0)  && (on_pos > 10)) on_pos -= 10;
      if ((btn&0x40) != 0) on_pos += 1;
      if (((btn&0x80) != 0) && (on_pos > 0)) on_pos -= 1;
      break;
    case 3:
      if ((btn&0x08) != 0) 
        {
          EEPROM.put(EE_OFF_POS, off_pos);
          state = 0;
         }
      if ((btn&0x10) != 0) off_pos += 10;
      if (((btn&0x20) != 0)  && (off_pos > 10)) off_pos -= 10;
      if ((btn&0x40) != 0) off_pos += 1;
      if (((btn&0x80) != 0) && (off_pos > 0)) off_pos -= 1;
      break;
    default:
      state = 0;
      break;
  }
}

void calculate_display_content(uint8_t btn) {
  switch (state) {
    case 0:
      d_on_off(blech_on, 0);
      d[3] = 0;
      d_time(rdt.hour, rdt.minute, rdt.second, 0, 4);
      break;
    case 1:
      d[0] = 0x70;
      d_on_off(false, 1);
      
      d_time(rdt.hour, rdt.minute, rdt.second, countdown_minutes, 4);
      break;
    case 2:
      d[0] = 0x73;
      d_on_off(true, 1);
      
      d_num(on_pos, 4);
      d[7] = 0;
      break;
    case 3:
      d[0] = 0x73;
      d_on_off(false, 1);
      
      d_num(off_pos, 4);
      d[7] = 0;
      break;
    default: 
      break;
  }
}

void d_on_off(boolean val, int start_index) {
  if (!val) {
    d[start_index] = 0x3f;
    d[start_index+1] = 0x71;
    d[start_index+2] = 0x71;
    } 
  else {
    d[start_index] = 0x3f;
    d[start_index+1] = 0x54;
    d[start_index+2] = 0x00;
    }
}

void d_num(int num, int start_index) {
                       /*0*/ /*1*/ /*2*/ /*3*/ /*4*/ /*5*/ /*6*/ /*7*/ /*8*/ /*9*/
  uint8_t digits[] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };

 d[start_index] = digits[num/100]; 
 d[start_index+1] = digits[(num/10)%10]; 
 d[start_index+2] = digits[num%10]; 
}

void d_time(int hour, int minute, int second, int offset, int start_index) {
                         /*0*/ /*1*/ /*2*/ /*3*/ /*4*/ /*5*/ /*6*/ /*7*/ /*8*/ /*9*/
  uint8_t digits[] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
  int offset_h = hour;
  int offset_m = minute + offset;
  offset_h += offset_m / 60;
  offset_m = offset_m % 60;
  d[start_index] = digits[offset_h/10];
  d[start_index+1] = digits[offset_h%10] + ((second%2 == 1) ? 0x80 : 0x00);
  d[start_index+2] = digits[offset_m/10];
  d[start_index+3] = digits[offset_m%10];
}

void update_display(void) {
  int i=0;    
  sendCommand(0x40);  // set auto increment address
  digitalWrite(tm_strobe, LOW);
  shiftOut(tm_data, tm_clock, LSBFIRST, 0xc0); 

  for (i=0; i<8; i++) {
    shiftOut(tm_data, tm_clock, LSBFIRST, d[i]&0xff);
    shiftOut(tm_data, tm_clock, LSBFIRST, (d[i] & 0x0100) >> 8);
  }
 
 digitalWrite(tm_strobe, HIGH);
}

uint8_t readButtons(void)
{
  uint8_t buttons = 0;
  digitalWrite(tm_strobe, LOW);
  shiftOut(tm_data, tm_clock, LSBFIRST, 0x42);
 
  pinMode(tm_data, INPUT);
 
  for (uint8_t i = 0; i < 4; i++)
  {
    uint8_t v = shiftIn(tm_data, tm_clock, LSBFIRST) << i;
    buttons |= v;
  }
 
  pinMode(tm_data, OUTPUT);
  digitalWrite(tm_strobe, HIGH);
  return buttons;
}
  


