#include <inttypes.h>

/* the camera runs at 200 frames per minute */
#define NUMBER_OF_FRAMES 5300
#define NUMBER_OF_CYCLES 5

static int8_t index = 0;
static int8_t inc = 0;
static const uint8_t inverter[6] = { 052, 043, 061, 025, 034, 016 };

/* automatic funcion state machine */
#define auto_reset 0
#define wait_1     1
#define wait_2     2
#define new_frame  3
#define auto_end   4

static uint16_t frame_counter;
static uint8_t state = auto_reset;
static int8_t direction;
static uint8_t repeat;

/* *almost* 300 times per second, but not quite */
ISR(TIMER0_OVF_vect)
{
  /* update inverter output */
  PORTC = inverter[index];

  /* set next inverter output */
  index += inc;
  if (index == 6)
    index = 0;
  else if (index == -1)
    index = 5;

  if (!digitalRead(9)) /* external button */
  {
    /* automatic function */
    switch (state)
    {
      case auto_reset:
        frame_counter = 0;
        repeat = NUMBER_OF_CYCLES;
        direction = +1;
        inc = +1;
        digitalWrite(4, HIGH);
        state = wait_1;
        break;
      case wait_1:
        if (!digitalRead(5)) /* sensor 1 - shutter open - ON */
          state = wait_2;
        break;
      case wait_2:
        if ( digitalRead(5)) /* sensor 1 - shutter open - OFF */
          state = new_frame;
        break;
      case new_frame:
        frame_counter += direction;
        if (frame_counter == NUMBER_OF_FRAMES)
        {
          /* start rewinding */
          direction = -1;
          inc = -1;
          state = wait_1;
        }
        else if (frame_counter == 0)
        {
          /* rewind done, repeat if necessary */
          repeat--;
          if (repeat)
          {
            /* repeat cycles left, restart going forward */
            direction = +1;
            inc = +1;
            state = wait_1;
          }
          else
          {
            /* no more repeats, end automatic function */
            state = auto_end;
          }
        }
        else
        {
          /* just keep going */
          state = wait_1;
        }
        break;
      case auto_end:
        /* the end. */
        digitalWrite(4, LOW);
        break;
    }
  }
  else
  {
    /* reset automatic function */
    state = auto_reset;

    /* manual function */
    if (!digitalRead(7)) /* button 1 */
    {
      /* forward */
      inc = +1;
      digitalWrite(4, HIGH);
    }
    else if (!digitalRead(8)) /* button 2 */
    {
      /* rewind */
      inc = -1;
      digitalWrite(4, HIGH);
    }
    else
    {
      /* disable inverter */
      inc = 0;
      digitalWrite(4, LOW);
    }
  }
}

int main(void)
{
  pinMode(4, OUTPUT); /* en */
  digitalWrite(4, LOW);

  pinMode(A0, OUTPUT); /* inverter - high 3 */
  pinMode(A1, OUTPUT); /* inverter - high 2 */
  pinMode(A2, OUTPUT); /* inverter - high 1 */
  pinMode(A3, OUTPUT); /* inverter - low 3 */
  pinMode(A4, OUTPUT); /* inverter - low 2 */
  pinMode(A5, OUTPUT); /* inverter - low 1 */

  pinMode(5, INPUT); /* sensor 1 - shutter open */
  pinMode(6, INPUT); /* sensor 2 - shutter close */
  pinMode(7, INPUT); /* button 1 - forward */
  pinMode(8, INPUT); /* button 2 - rewind */
  pinMode(9, INPUT); /* external button */

  /* configure Timer 0 to interrupt every 256 * 208 = 53248 ticks.
   * this leads to the interrupt function being called at 300,480769231 Hz
   * which is *almost* the 300 Hz we need to update the inverter with 6
   * steps leading to the 50 Hz the motor expects. */
  /* reset timers */
  GTCCR = (1 << TSM) | (1 << PSRASY) | (1 << PSRSYNC);

  TCCR0A = (0 << COM0A1) | (0 << COM0A0) /* disconnected */
         | (0 << COM0B1) | (0 << COM0B0) /* disconnected */
         | (0 << WGM01 ) | (1 << WGM00 );
  TCCR0B = (0 << FOC0A ) | (0 << FOC0B )
         | (1 << WGM02 )
         | (1 << CS02  ) | (0 << CS01  ) | (0 << CS00 ); /* 256 prescaler */
  TCNT0  = 0;
  OCR0A  = 104;
  OCR0B  = 0;
  TIMSK0 = (0 << OCIE0B) | (0 << OCIE0A) | (1 << TOIE0); /* interrupt on overflow */
  TIFR0  = (1 << OCF0B ) | (1 << OCF0A ) | (1 << TOV0 );

  GTCCR = 0;

  sei();

  while (1)
  {
    /* loop forever. we could sleep, but I'm too lazy to write it... */
  }
}

/*
  --------------------------
 |                         _|
 |                        |_|-- power plug --------| battery
 |                         _|
 | ARDUINO                |_|-- brown ---- blue ---| camera - b8
 |                        |_|-- red ------ yellow -| camera - b1
 |                        |_|-- black ---- white --| camera - a1
 |            FWD  REW   ___|
 |                      | | |
  --------------------------
                         | |
                         | \--- black -------------| external button
                         \----- red ---------------| external button

  --------------------------
 |                         _|
 |                        |_|-- red ---------------| battery
 |                        |_|-- black -------------| battery
 |                         _|
 |                        |_|-- black -------------| camera - a4
 | INVERTER               |_|-- red ---------------| camera - a3
 |                        |_|-- brown -------------| camera - a2
 |                         _|
 |                        |_|-- red ---------------| shocked face
 |                        |_|-- black -------------| shocked face
 |                          |
  --------------------------

  --------------------------
 |                         _|
 |                        |_|-- red ---------------| inverter
 |                        |_|-- black -------------| inverter
 | SHOCKED FACE            _|
 |                        |_|-- black -------------| battery <-- WATCH OUT FOR THE BLACK/RED ORDER
 |                        |_|-- red ---------------| battery <-- WATCH OUT FOR THE BLACK/RED ORDER
 |                          |
  --------------------------
*/

