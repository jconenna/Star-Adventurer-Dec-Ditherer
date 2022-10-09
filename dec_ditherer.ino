#include <SPI.h>

// credit to the following for rotary code
// http://domoticx.com/arduino-rotary-encoder-keyes-ky-040/

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <EEPROM.h>
uint8_t previous_reading = 1; // holds last 8 pin reads from pushbutton
uint8_t button_pressed   = 1; // keeps track of debounced button state, 0 false, 1 true

// state machine states
#define SPLASH            0  // intro screen
#define SETTINGS          1  // settings select
#define SETTINGS_FL       10 // set focal length 
#define SETTINGS_PS       11 // set pixel scale
#define SETTINGS_AMT      12 // set dither amount
#define SETTINGS_FREQ     13 // set dither frequency
#define SETTINGS_BACK     14 // go back from settings
#define DITHER_RUN        2  // dither run
#define DEC_CONTROL       3  // manual motor control

// parameters to pass to  update_state_machine()
#define LEFT      2
#define RIGHT     1
#define NONE      0
#define UNPRESSED 0
#define PRESS     1

// state variable
uint16_t state = SPLASH;

// global variables from EEPROM
#define FL_ADDR 0
#define PS_ADDR 2
#define DA_ADDR 4
#define DF_ADDR 5

uint16_t FL  = (EEPROM.read(FL_ADDR+1) << 8) + EEPROM.read(FL_ADDR); // focal length mm
uint16_t PS  = (EEPROM.read(PS_ADDR+1) << 8) + EEPROM.read(PS_ADDR); // pixel size 10*um
int16_t  DA  =  EEPROM.read(DA_ADDR);                                // amount of steps to dither
uint8_t  DF  =  EEPROM.read(DF_ADDR);                                // how many frames between dec dither


// image scale variables
unsigned long pixel_scale = 0; // [min, max] = [10, 206058] in 0.01*[''/px]
float pixels_per_step     = 0; // pixels per step of stepper motor
float dither_px           = 0; // +/- number of pixels the motor will dither in DEC

// set up LCD
#include <LiquidCrystal.h>
//rs = 4, en = 6, d4 = 5, d5 = 7, d6 = A0, d7 = A1
LiquidCrystal lcd(4, 6, 5, 7, A0, A1);

// set up Encoder
#define CLK 2
#define DT  3
#define SW  12
int enc_count     = 0;
int enc_old_count = 0;
int enc_current_CLK;
int enc_last_CLK;
uint8_t enc_current_dir   = 0;
unsigned long last_button = 0; 
uint8_t button = 0;

// shutter release signals
int16_t shu,previous_shu = 0;
#define GND_PIN A3
#define SHU_PIN A4
#define OPEN_CLOSE_THRESHOLD 300

// setup ST-4 (stf) signals
uint8_t stf_posedge = 0; // was there a positive ST-4 DEC edge?
uint8_t stf_previous = 0;
#define DEC_P 13
#define DEC_N A5


// stepper
#include <CheapStepper.h>

//connect pins 8,9,10,11 to IN1,IN2,IN3,IN4 on ULN2003 board
CheapStepper stepper (8,9,10,11); 

#define STEPPER_EN A2
#define BACKLASH   55

// dither loop variables
uint8_t shu_close_ctr = 0;
unsigned long seed = 0;
long dither_steps, previous_dither_steps = 0;
uint8_t last_direction = 0;

// dec control states, values correspond to cursor location on lcd
#define STOP  8
#define L     6
#define LL    4
#define LLL   2
#define R     10
#define RR    12
#define RRR   14

uint8_t dec_ctrl_state = STOP;


//*******************************************************************************************************************************************//

// timer to trigger interrupt to check encoder button state to debounce
static inline void initTimer1(void)
{
  TCCR0A |= (1 << WGM01);                             // clear timer on compare match
  TCCR0B |= (1 << CS01) | (1 << CS00);                // clock prescaler 64
  OCR0A   = 255;                                      // compare match value to trigger interrupt every 1020us ([1 / (16E6 / 4)] * 50 = 1020us)
  TIMSK0 |= (1 << OCIE0A);                            // enable output compare match interrupt
  sei();                                              // enable interrupts
}

//*******************************************************************************************************************************************//

// interrrupt function to check encoder button state and debounce
ISR(TIMER0_COMPA_vect)
{
  if ((PINB & (1 << PB4)) != previous_reading) // if current button pin reading doesn't equal previous_reading 1020us ago,
  {  
    if((PINB & (1 << PB4)) ==0)                // and button wasn't pressed last time 
    {                                                             
      button_pressed = 1;                     // set debounced button press state to 1
    }
    else  
     button_pressed = 0;                      // button has been let go, reset button_pressed 
  }
  previous_reading = (PINB & (1 << PB4));     // set previous reading to current reading for next time
}

//*******************************************************************************************************************************************//
 
// interrupt routine for encoder
// on interrupt, read input pins, compute new state, and adjust count
void pinChangeISR() {
  // Read the current state of CLK
  enc_current_CLK = digitalRead(CLK);
  // If last and current state of CLK are different, then pulse occurred
  // React to only 1 state change to avoid double count
  if (enc_current_CLK != enc_last_CLK  && enc_current_CLK == 1)
  {
    // If the DT state is different than the CLK state then
    // the encoder is rotating CCW so decrement
    if (digitalRead(DT) != enc_current_CLK) {
      enc_old_count = enc_count;
      enc_count--;
      enc_current_dir =0;
      
    } else {
      // Encoder is rotating CW so increment
      enc_old_count = enc_count;
      enc_count++;
      enc_current_dir =1;
    }
  }
 
  // Remember last CLK state
  enc_last_CLK = enc_current_CLK;
}

//*******************************************************************************************************************************************//

void setup() {
  // Init EEPROM to 0
  // Uncomment the following EEPROM write block, upload to device
  // then recomment the lines, upload to device
  //EEPROM.write(FL_ADDR,   0);
  //EEPROM.write(FL_ADDR+1, 0);
  //EEPROM.write(PS_ADDR,   0);
  //EEPROM.write(PS_ADDR+1, 0);
  //EEPROM.write(DA_ADDR,   0);
  //EEPROM.write(DF_ADDR,   0);
  
  // initialize timer and interrupt for button
  initTimer1();        
   
  // set Encoder pinmodes
  pinMode(CLK, INPUT);
  pinMode(DT,  INPUT);
  pinMode(SW,  INPUT_PULLUP);

  // set ST-4 pinmodes
  pinMode(DEC_P, INPUT_PULLUP);
  pinMode(DEC_N, INPUT_PULLUP);
  
  // set Encoder pin-change interrupts
  attachInterrupt(0, pinChangeISR, CHANGE);
  attachInterrupt(1, pinChangeISR, CHANGE);

  // save initial state of CLK
  enc_last_CLK = digitalRead(CLK);

  // write splash screen to lcd
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Star Adventurer ");
  lcd.setCursor(0, 1);
  lcd.print("DEC Ditherer    ");

  // initialize encoder variables to 0
  enc_count     = 0;
  enc_old_count = 0;

  // initial calculation of pixel scale and pixels per step
  calc_pixel_scale();
  calc_pixel_per_step();

  // set stepper speed
  stepper.setRpm(15); 

  // stepper enable transistor base pinmode
  pinMode(STEPPER_EN, OUTPUT);
}

//*******************************************************************************************************************************************//
 
void loop() {
 
  if(button_pressed)                          // if button pressed
  {
    button_pressed = 0;                       // reset button flag
    update_state_machine(NONE, PRESS);      
  }
  else if (enc_old_count != enc_count)        // is encoder was turned
  {  
    if(enc_old_count > enc_count)             // if turned right
      update_state_machine(RIGHT, UNPRESSED); 
    else                                      // else turned left
      update_state_machine(LEFT, UNPRESSED);
    delay(400);                               // delay for responsiveness
    enc_old_count = enc_count;                // reset encoder count
  }

  if(state == SPLASH)                         // increment seed until leaving SPLASH
    seed++;

} // end loop

//*******************************************************************************************************************************************//

// dir: 0 none, 1 right, 2 left. button: 0 unpressed, 1 pressed
void update_state_machine(uint8_t dir, uint8_t button)
{

switch(state)
{
  case SPLASH:                              // title screen
  if(button || dir == RIGHT || dir == LEFT) // if encoder moved or pressed go to settings
    state = SETTINGS;
  print_state();                            // update lcd
  break;

  case SETTINGS:                            // settings select
  if(dir == RIGHT)
    {
    state = DITHER_RUN;
    print_state();
    }
  else if(dir == LEFT)
    {
    state = DEC_CONTROL;
    print_state();
    }
  else if(button)
    {
    state = SETTINGS_FL;
    print_state();
    }
  else
    {
    state = SETTINGS;
    print_state();
    }
  break;

  case SETTINGS_FL:                         // set focal length             
  print_state();
  if(dir == RIGHT)
    {
    state = SETTINGS_PS;
    print_state();
    }
  else if(dir == LEFT)
    {
    state = SETTINGS_BACK;
    print_state();
    }
  else if(button)
    {
    print_state();
    set_fl();
    }
  break;

  case SETTINGS_PS:                         // set pixel size
  print_state();
  if(dir == RIGHT)
    {
    state = SETTINGS_AMT;
    print_state();
    }
  else if(dir == LEFT)
    {
    state = SETTINGS_FL;
    print_state();
    }
  else if(button)
    {
    print_state();
    set_ps();
    }
  break;

  case SETTINGS_AMT:                        // set dither amount
  print_state();
  if(dir == RIGHT)
    {
    state = SETTINGS_FREQ;
    print_state();
    }
  else if(dir == LEFT)
    {
    state = SETTINGS_PS;
    print_state();
    }
  else if(button)
    {
    print_state();
    set_da();
    }
  break;

  case SETTINGS_FREQ:                        // set dither frequency
  print_state();
  if(dir == RIGHT)
    {
    state = SETTINGS_BACK;
    print_state();
    }
  else if(dir == LEFT)
    {
    state = SETTINGS_AMT;
    print_state();
    }
  else if(button)
    {
    print_state();
    set_df();
    }
  break;

  case SETTINGS_BACK:                        // leave settings select
  print_state();
  if(dir == RIGHT)
    {
    state = SETTINGS_FL;
    print_state();
    }
  else if(dir == LEFT)
    {
    state = SETTINGS_FREQ;
    print_state();
    }
  else if(button)
    {
    state = SETTINGS;
    print_state();
    }
  break;
    
  case DITHER_RUN:                           // run dither routine
    if(dir == RIGHT)
      {
      state = DEC_CONTROL;
      print_state();
      }
    else if(dir == LEFT)
      {
      state = SETTINGS;
      print_state();
      }   
    else
      {
      randomSeed(seed);
      dither_loop();
      print_state();
    }
    break;

  case DEC_CONTROL:                           // manual dec motor control
    if(dir == RIGHT)
      {
      state = SETTINGS;
      print_state();
      }
    else if(dir == LEFT)
      {
      state = DITHER_RUN;
      print_state();
      }
    else
      {
      state = DEC_CONTROL;
      print_state();
      run_dec_ctrl();
      }
    break;
}// end switch
}// end function

//*******************************************************************************************************************************************//

// update lcd depending on state variab;e
void print_state()
{
  switch(state)
  {
    case SETTINGS:
    lcd.setCursor(0, 0);
    lcd.print("Settings     <->");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    break;
  
    case SETTINGS_FL:
    lcd.setCursor(0, 0);
    lcd.print("Focal Length mm ");
    lcd.setCursor(0, 1);
    lcd.print("<->             ");
    break;

    case SETTINGS_PS:
    lcd.setCursor(0, 0);
    lcd.print("Pixel Size ");
    lcd.print(char(0b11100100));
    lcd.print("m   ");
    lcd.setCursor(0, 1);
    lcd.print("<->             ");
    break;

    case SETTINGS_AMT:
    lcd.setCursor(0, 0);
    lcd.print("Dither Amount   ");
    lcd.setCursor(0, 1);
    lcd.print("<->             ");
    break;

    case SETTINGS_FREQ:
    lcd.setCursor(0, 0);
    lcd.print("Dither Frequency");
    lcd.setCursor(0, 1);
    lcd.print("<->             ");
    break;

    case SETTINGS_BACK:
    lcd.setCursor(0, 0);
    lcd.print("BACK            ");
    lcd.setCursor(0, 1);
    lcd.print("^               ");
    break;

    case DITHER_RUN:
    lcd.setCursor(0, 0);
    lcd.print("Dither Run   <->");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    break;

    case DEC_CONTROL:
    lcd.setCursor(0, 0);
    lcd.print("DEC Control  <->");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    break;
  }
}

//*******************************************************************************************************************************************//

//void splash_seed()
//{
//  while(1)
//  {
//    if((enc_old_count != enc_count)) // if encoder moved or pressed go to settings
//      {
//        button_pressed = 0;        // reset encoder vars
//        enc_old_count = enc_count;
//        break;
//      }
//      
//    else seed++; // increment seed until leave splash
//  }
//  randomSeed(seed);
//}

//*******************************************************************************************************************************************//

// routine to set focal length
void set_fl()
{
// set up lcd
lcd.setCursor(0, 1);
if(FL < 100)
  lcd.print(0);
if(FL < 10)
  lcd.print(0);
lcd.print(FL);
lcd.print("      [8-999]");
lcd.setCursor(0, 1);        // set cursor under first digit
lcd.cursor();
lcd.blink();

// loop until first digit set
while(1)
{   
    if(button_pressed)                                     // button pressed
    {
      button_pressed = 0;                                  // reset encoder vars
      enc_old_count = enc_count;
      break;                                               // break
    }
    else if((enc_old_count > enc_count) && ((FL/100) < 9)) // encoder moved right, increment if allowed
    {
      FL += 100;                                           // increment
      print_fl();                                          // update lcd
      lcd.setCursor(0, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
    else if((enc_old_count < enc_count) && ((FL/100) > 0)) // encoder moved left, decrement if allowed
    {
      FL -= 100;                                           // decrement
      print_fl();                                          // update lcd
      lcd.setCursor(0, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
}

// set cursor under second digit
lcd.setCursor(1, 1);
lcd.cursor();
lcd.blink();

// loop until second digit set
while(1)
{   
    if(button_pressed)
    {
      button_pressed = 0;
      enc_old_count = enc_count;
      break;
    }
    else if((enc_old_count > enc_count) && (((FL-(FL/100)*100)/10) < 9))
    {
      FL += 10;
      print_fl();
      lcd.setCursor(1, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
    else if((enc_old_count < enc_count) && (((FL-(FL/100)*100)/10) > 0))
    {
      FL -= 10;
      print_fl();
      lcd.setCursor(1, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
}

if(FL < 10)
{
  FL = 9;
  print_fl();
}
lcd.setCursor(2, 1);
lcd.cursor();
lcd.blink();

while(1)
{   
    if(button_pressed)
    {
      button_pressed = 0;
      enc_old_count = enc_count;
      break;
    }
    else if((enc_old_count > enc_count) && ((FL-((FL/10)*10)) < 9))
    {
      FL += 1;
      print_fl();
      lcd.setCursor(2, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
    
    //else if((enc_old_count < enc_count) && ((FL-((FL/10)*10)) > 0))
    else if((enc_old_count < enc_count) && ((((FL/10) == 0) && (FL > 8)) || ((FL > 10) && ((FL-((FL/10)*10)) > 0))))
    {
      FL -= 1;
      print_fl();
      lcd.setCursor(2, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
}
print_state();
calc_pixel_scale();
calc_pixel_per_step();
lcd.noBlink();
lcd.noCursor();

// Write new value to EEPROM
EEPROM.write(FL_ADDR, FL);
EEPROM.write(FL_ADDR+1, FL >> 8);
}

//*******************************************************************************************************************************************//

void print_fl()
{
  lcd.setCursor(0, 1);
  if(FL < 100)
    lcd.print(0);
  if(FL < 10)
    lcd.print(0);
  lcd.print(FL);
}

void set_ps()
{
  
lcd.setCursor(0, 1);
lcd.print(PS/100);            // print ones place
lcd.print(".");               // print decimal pt
if(PS-((PS/100)*100) < 10)    // if decimal value < 10
  lcd.print(0);               // print 0
lcd.print(PS-((PS/100)*100)); // print decimal value
lcd.print(" [0.50-9.99]");    // fill rest of space

lcd.setCursor(0, 1);          // set cusror under ones place
lcd.cursor();
lcd.blink();

while(1) // set ones place
{   
    if(button_pressed)                                    // if button was pressed break to next while loop
    {
      button_pressed = 0;
      enc_old_count = enc_count;
      break;
    }
    else if((enc_old_count > enc_count) && ((PS/100) < 9)) // if encoder turned right and ones place can increment
    {
      PS += 100;                                           // increment PS ones place
      print_ps();                                          // update lcd
      lcd.setCursor(0, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
    else if((enc_old_count < enc_count) && ((PS/100) > 0)) // if encoder turned left and ones place can decrement
    {
      PS -= 100;                                           // decrement PS ones place
      print_ps();                                          // update lcd
      lcd.setCursor(0, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
}

lcd.setCursor(2, 1);
lcd.cursor();
lcd.blink();

while(1) // set tenths place
{   
    if(button_pressed)
    {
      button_pressed = 0;
      enc_old_count = enc_count;
      break;
    }
    else if((enc_old_count > enc_count) && (((PS-(PS/100)*100)/10) <9))  // if encoder turned right and tenths place can increment
    {
      PS += 10 ;                                                         // increment PS tenths place
      print_ps();                                                        // update lcd
      lcd.setCursor(2, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
    else if((enc_old_count < enc_count) && (((PS/100) == 0) && ((((PS-(PS/100)*100)/10) > 5)) || (((PS/100) > 0) && (((PS-(PS/100)*100)/10) > 0))))  // if encoder turned left and tenths place can decrement
    {
      PS -= 10;                                                           // decrement PS tenths place
      print_ps();                                                         // update lcd
      lcd.setCursor(2, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
}

lcd.setCursor(3, 1);
lcd.cursor();
lcd.blink();

while(1) // set hundredths place
{   
    if(button_pressed)
    {
      button_pressed = 0;
      enc_old_count = enc_count;
      break;
    }
    else if((enc_old_count > enc_count) && ((PS-((PS/10)*10)) < 9))  // if encoder turned right and hundredths place can increment
    {
      PS += 1 ;                                                      // increment PS hundredths place
      print_ps();                                                    // update lcd
      lcd.setCursor(3, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
    else if((enc_old_count < enc_count) && ((PS-((PS/10)*10)) > 0))  // if encoder turned left and one's place can decrement
    {
      PS -= 1;                                                       // decrement PS hundredths place
      print_ps();                                                    // update lcd
      lcd.setCursor(3, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
}
print_state();
calc_pixel_scale();
calc_pixel_per_step();
lcd.noBlink();
lcd.noCursor();

// Write new value to EEPROM
EEPROM.write(PS_ADDR, PS);
EEPROM.write(PS_ADDR+1, PS >> 8);
}

//*******************************************************************************************************************************************//

// update lcd with pixel scale
void print_ps()
{
  lcd.setCursor(0, 1);
  lcd.print(PS/100);                                   // print ones place  
  lcd.print(".");                                      // print decimal pt
  if(PS-((PS/100)*100) < 10)                           // if decimal value < 10
    lcd.print(0);                                      // print 0
  lcd.print(PS-((PS/100)*100));                        // print decimal value1);
}

//*******************************************************************************************************************************************//

// calculate pixel scale given focal length
void calc_pixel_scale()
{
  unsigned long temp = PS * 206265;
  temp /= FL;
  temp /= 1000;
  pixel_scale = temp;
}

//*******************************************************************************************************************************************//

// calculate arcminutes
void show_arcmin()
{
  float temp = pixel_scale*pixels_per_step;
  temp *= DA;
  temp /= 6000;
  temp *= 2; // convert +/- range to full range that SA uses
  lcd.setCursor(0, 0);
  lcd.print("Set SA Dither to");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(temp);
  lcd.print(" arcmin");
  lcd.noBlink();
  lcd.noCursor();
}

//*******************************************************************************************************************************************//

void calc_pixel_per_step()
{
// 4096 steps (technically half steps) per rotation of stepper
// 360[deg]/4096[step] = 0.08789[deg/step]
// Dec Bracket has 122 shaft rotations / 1 plate rotation
// 0.08789[deg/step]/122 = 0.0007204[deg/step]
// 0.0007204[deg/step]*3600["/deg] = 2.5934["/step]
// pixels_per_step = 2.59["/step]/X["/pix] = Y[pix/step]

pixels_per_step = float(259.34)/float(pixel_scale);  
}

//*******************************************************************************************************************************************//

// set dither amount
void set_da()
{
//DA = 0;
lcd.setCursor(0, 0);
lcd.print("Motor Resolution");
lcd.setCursor(0, 1);
if(int(pixels_per_step) < 10)
  lcd.print(0);
lcd.print(pixels_per_step,2);
lcd.print(" px/step  ");
lcd.print(char(0b01111110));

while(1)
{ 
  if(button_pressed || (enc_old_count != enc_count))
    {
    button_pressed = 0;
    enc_old_count = enc_count;
    break;
    }
  delay(1);
}

lcd.setCursor(0, 0);
lcd.print("Set A to dither ");
lcd.setCursor(0, 1);
lcd.print("+/- A*");
if(int(pixels_per_step) < 10)
  lcd.print(0);
lcd.print(pixels_per_step,2);
lcd.print(" px ");
lcd.print(char(0b01111110));

while(1)
{ 
  if(button_pressed || (enc_old_count != enc_count))
    {
    button_pressed = 0;
    enc_old_count = enc_count;
    break;
    }
  delay(1);
}

lcd.setCursor(0, 0);
lcd.print("A: [1,999]      ");
lcd.setCursor(15, 1);
lcd.print(" ");
print_dither_px();


while(1)
{  
    if(button_pressed)
    {
      button_pressed = 0;
      enc_old_count = enc_count;
      break;
    }
    else if((enc_old_count > enc_count) && DA < 900)  
    {
      DA += 1 ;                                                 // increment 
      print_dither_px();                                        // update lcd
      lcd.setCursor(2, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
    else if((enc_old_count < enc_count) && DA > 1)              // if encoder turned left and one's place can decrement
    {
      DA -= 1 ;                                                 // decrement 
      print_dither_px();                                        // update lcd
      lcd.setCursor(2, 1);
      lcd.cursor();
      lcd.blink();
      enc_old_count = enc_count;
    }
    delay(1);
}

show_arcmin();

while(1)
{ 
  if(button_pressed || (enc_old_count != enc_count))
    {
    button_pressed = 0;
    enc_old_count = enc_count;
    break;
    }
  delay(1);
}

print_state();  
lcd.noBlink();

// Write new value to EEPROM
EEPROM.write(DA_ADDR, DA);
}

//*******************************************************************************************************************************************//

// update lcd for dither amount
void print_dither_px()
{
  dither_px = float(DA)*pixels_per_step;
  lcd.setCursor(0, 1);
  if(DA < 10)
    lcd.print("00");
  else if(DA < 100)
    lcd.print("0");
  lcd.print(DA);
  lcd.print("=+/-");
  if(dither_px < 10)
    lcd.print("00");
  else if(dither_px < 100)
    lcd.print("0");
  lcd.print(dither_px,2);
  lcd.print(" px");
  lcd.setCursor(2, 1);
  lcd.cursor();
}

//*******************************************************************************************************************************************//

// set dither frequency
void set_df()
{

lcd.setCursor(0, 0);
lcd.print("Dither every    ");
lcd.setCursor(0, 1);
lcd.print(DF);
lcd.print(" frames   [1-9]");
lcd.setCursor(0, 1);
lcd.cursor();
lcd.blink();

while(1)
{  
  
    if(button_pressed)
    {
      button_pressed = 0;
      enc_old_count = enc_count;
      break;
    }
    else if((enc_old_count > enc_count) && (DF < 9))  
    {
      DF += 1 ;                                                 // increment 
      lcd.setCursor(0, 1);
      lcd.print(DF);
      lcd.setCursor(0, 1);
      lcd.cursor();
      lcd.blink(); 
      enc_old_count = enc_count;
    }
    else if((enc_old_count < enc_count) && (DF > 1))            // if encoder turned left and one's place can decrement
    {
      DF -= 1 ;                                                 // decrement 
      lcd.setCursor(0, 1);
      lcd.print(DF);
      lcd.setCursor(0, 1);
      lcd.cursor();
      lcd.blink(); 
      enc_old_count = enc_count;
    }
    delay(1);
}
print_state();  
lcd.noBlink(); 
lcd.noCursor();

// Write new value to EEPROM
EEPROM.write(DF_ADDR, DF);
}

void dither_loop()
{

uint16_t steps = 0;

// set baseline
//gnd = analogRead(GND_PIN);
shu = analogRead(SHU_PIN);
//diff = gnd-shu;
//diff = abs(diff);
//previous_diff = diff;
previous_shu = shu;

lcd.setCursor(0, 0);
lcd.print(DF-shu_close_ctr);
lcd.print(" Frames Until  ");
lcd.setCursor(0, 1);
lcd.print("Dither...       ");


while(1) // loop until DF shutter closes occur
{
  while(shu_close_ctr < DF)
  {
    check_STF();
    shu = analogRead(SHU_PIN);
    if(((shu-previous_shu) > OPEN_CLOSE_THRESHOLD) || stf_posedge)
      {
        shu_close_ctr++;
        stf_posedge = 0;
        
        if((DF-shu_close_ctr) != 0)
        {
          lcd.setCursor(0, 0);
          lcd.print(DF-shu_close_ctr);
          lcd.noCursor();
        }
      }
    previous_shu = shu;
    delay(100);
  }

  dither_steps = random(-DA,DA);
  while(dither_steps == previous_dither_steps) // loop until new random steps is different than previous
     {dither_steps = random(-DA,DA);
     }

  if(dither_steps > previous_dither_steps)
    { 
      if(last_direction == 1)
        steps = abs(dither_steps-previous_dither_steps) + BACKLASH;
      else steps = abs(dither_steps-previous_dither_steps);  
      digitalWrite(A2, HIGH);
      delay(50);
      stepper.move(0, steps);
      digitalWrite(A2, LOW);
      delay(50);
      digitalWrite(8, LOW);
      digitalWrite(9, LOW);
      digitalWrite(10, LOW);
      digitalWrite(11, LOW);
      last_direction = 0;
    }
  else
    {
      if(last_direction == 0)
        steps = abs(dither_steps-previous_dither_steps) + BACKLASH;
      else steps = abs(dither_steps-previous_dither_steps);  
      digitalWrite(A2, HIGH);
      delay(50);
      stepper.move(1, steps);
      digitalWrite(A2, LOW);
      delay(50);
      digitalWrite(8, LOW);
      digitalWrite(9, LOW);
      digitalWrite(10, LOW);
      digitalWrite(11, LOW);
      last_direction = 1;
    }

  lcd.setCursor(0, 0);
  lcd.print("    DITHERING   ");
  lcd.setCursor(0, 1);
  lcd.print("    !!!!!!!!!   ");
  delay(5000);
  
  shu_close_ctr = 0;
  previous_dither_steps = dither_steps;

  lcd.setCursor(0, 0);
  lcd.print(DF-shu_close_ctr);
  lcd.print(" Frames Until  ");
  lcd.setCursor(0, 1);
  lcd.print("Dither...       ");
}
  
}

//*******************************************************************************************************************************************//
void check_STF()
{
  if( (!digitalRead(DEC_P) || !digitalRead(DEC_N)) && !stf_posedge)
      {stf_posedge  = 1;}
      
}
//*******************************************************************************************************************************************//

void run_dec_ctrl()
{
lcd.setCursor(0, 0);
lcd.print("DEC Control     ");
lcd.setCursor(0, 1);
lcd.print("  < < < - > > > ");
lcd.setCursor(8, 1);
lcd.blink();

while(1)
{  
    
    if(button_pressed)                                              // if button pressed, break from dec control
    {
      button_pressed = 0;
      enc_old_count = enc_count;
      break;
    }
    else if((enc_old_count > enc_count) && (dec_ctrl_state < RRR)) // if encoder rot right, increment state
    {
      dec_ctrl_state += 2;
      enc_old_count = enc_count;
    }
    else if((enc_old_count < enc_count) && (dec_ctrl_state > LLL))  // if encoder rot left , decrement state
    {
      dec_ctrl_state -= 2;
      enc_old_count = enc_count;
    }
    
   switch (dec_ctrl_state)
   {
    case STOP:
    lcd.setCursor(STOP, 1);
    lcd.blink();
    digitalWrite(A2, LOW);
    break;
    
    case R:
    lcd.setCursor(R, 1);
    lcd.blink();
    digitalWrite(A2, HIGH);
    stepper.setRpm(1); 
    stepper.move(0, 1);
    break;

    case RR:
    lcd.setCursor(RR, 1);
    lcd.blink();
    digitalWrite(A2, HIGH);
    stepper.setRpm(10);
    stepper.move(0, 5);
    break;

    case RRR:
    lcd.setCursor(RRR, 1);
    lcd.blink();
    digitalWrite(A2, HIGH);
    stepper.setRpm(15);
    stepper.move(0, 20);
    break;

    case L:
    lcd.setCursor(L, 1);
    lcd.blink();
    digitalWrite(A2, HIGH);
    stepper.setRpm(1);
    stepper.move(1, 1);
    break;

    case LL:
    lcd.setCursor(LL, 1);
    lcd.blink();
    digitalWrite(A2, HIGH);
    stepper.setRpm(10);
    stepper.move(1, 5);
    break;

    case LLL:
    lcd.setCursor(LLL, 1);
    lcd.blink();
    digitalWrite(A2, HIGH);
    stepper.setRpm(15);
    stepper.move(1, 20);
    break;
   }
   delay(1);
}

  stepper.setRpm(15);
  digitalWrite(A2, LOW);
  digitalWrite(8, LOW);
  digitalWrite(9, LOW);
  digitalWrite(10, LOW);
  digitalWrite(11, LOW);
  dec_ctrl_state = STOP;
  print_state();
}
