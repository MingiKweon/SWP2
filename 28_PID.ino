#include <Servo.h>

// Arduino pin assignment
#define PIN_LED   9
#define PIN_SERVO 10
#define PIN_IR    A0

// Event interval parameters
#define _INTERVAL_DIST    20    // distance sensor interval (unit: ms)
//////////////// DO NOT modify below section!! //////////////////////////    
#define _INTERVAL_SERVO   20     // servo interval (unit: ms)
#define _INTERVAL_SERIAL  100    // serial interval (unit: ms)
#define _INTERVAL_MOVE    10000  // Target move interval (unit: ms)
/////////////////////////////////////////////////////////////////////////

// EMA filter configuration for the IR distance sensor
#define _EMA_ALPHA 0.2    // EMA weight of new sample (range: 0 to 1)
                          // Setting EMA to 1 effectively disables EMA filter

// Servo adjustment - Set _DUTY_MAX, _NEU, _MIN with your own numbers
#define _DUTY_MAX 2565 // 2000
#define _DUTY_NEU 1780 // 1500
#define _DUTY_MIN 795 // 1000

#define _SERVO_ANGLE_DIFF  150  // Replace with |D - E| degree
#define _SERVO_SPEED       900  // servo speed 

// PID parameters
#define _KP 5.0   // proportional gain
#define _KD 300   // derivative gain
#define _KI 0.5   // integral gain

// global variables

Servo myservo;      // Servo instance
float dist_ema;     // filtered distance

// Event periods
unsigned long last_sampling_time_dist, last_sampling_time_servo, last_sampling_time_serial; // unit: ms
bool event_dist, event_servo, event_serial; // event triggered?

//////////////// DO NOT modify below section!! /////////////////////////        
unsigned long error_sum, error_cnt, toggle_cnt;
unsigned long last_sampling_time_move; // unit: msec
float dist_target = 55;

#define _TIME_TO_STABILIZE  3000  // time to control 
#define _ERROR_ALLOWED      3     // allowable error (unit: mm)
////////////////////////////////////////////////////////////////////////

#define _ITERM_MAX 0.5
// Servo speed control
int duty_chg_per_interval;     // maximum duty difference per interval

// Servo position
int duty_target, duty_current;

// PID variables
int error_current, error_prev;
float pterm, dterm, iterm;

void setup() {
  // initialize GPIO pins
  pinMode(PIN_LED,OUTPUT);
  
  myservo.attach(PIN_SERVO);
  duty_target = duty_current = _DUTY_NEU;
  myservo.writeMicroseconds(duty_current);

  // convert angle speed into duty change per interval
  duty_chg_per_interval = 
    (float)(_DUTY_MAX - _DUTY_MIN) * ((float)_SERVO_SPEED / _SERVO_ANGLE_DIFF) * (_INTERVAL_SERVO / 1000.0); 

  // initialize serial port
  Serial.begin(1000000);
}

void loop() {
  unsigned long time_current = millis();
  
  // wait until next event time
  if (time_current >= (last_sampling_time_dist + _INTERVAL_DIST)) {
    last_sampling_time_dist += _INTERVAL_DIST;
    event_dist = true;
  }
  if (time_current >= (last_sampling_time_servo + _INTERVAL_SERVO)) {
    last_sampling_time_servo += _INTERVAL_SERVO;
    event_servo = true;
  }
  if (time_current >= (last_sampling_time_serial + _INTERVAL_SERIAL)) {
    last_sampling_time_serial += _INTERVAL_SERIAL;
    event_serial = true;
  }

//////////////// DO NOT modify below section!! /////////////////////////        
  if (time_current >= (last_sampling_time_move + _INTERVAL_MOVE)) {
        last_sampling_time_move += _INTERVAL_MOVE;
        dist_target = 310 - dist_target;
        //99번에 0 && 넣으면 안 디버깅할 때 편할듯
        if (++toggle_cnt == 5) {
          Serial.println("--------------------------------------------------");
          Serial.print("ERR_cnt:");
          Serial.print(error_cnt);
          Serial.print(",ERR_sum:");
          Serial.print(error_sum);
          Serial.print(",ERR_ave:");
          Serial.println(error_sum / (float) error_cnt);
          exit(0);
        }
  }
////////////////////////////////////////////////////////////////////////

  if (event_dist) {
    float dist_filtered; // unit: mm
    int control;
    
    event_dist = false;

    // get a distance reading from the distance sensor
    dist_filtered = volt_to_distance(ir_sensor_filtered(10, 0.7, 0));
    dist_ema = _EMA_ALPHA * dist_filtered + (1.0 - _EMA_ALPHA) * dist_ema;

    // PID control logic
    error_current = dist_target - dist_ema;

//////////////// DO NOT modify below section!! /////////////////////////
    if (time_current >= last_sampling_time_move + _TIME_TO_STABILIZE) {
      error_cnt++;
      if (abs(error_current) > _ERROR_ALLOWED) {
        error_sum += abs(error_current);
      }
    }
    if (abs(error_current) <= _ERROR_ALLOWED) {
      digitalWrite(PIN_LED, 0);      
    } else {
      digitalWrite(PIN_LED, 1);
    }
////////////////////////////////////////////////////////////////////////
    
    pterm = _KP * error_current;
    dterm = _KD * (error_current - error_prev);
    iterm += _KI * error_current;

    if (iterm > _ITERM_MAX)
      iterm = _ITERM_MAX;
    if (iterm < - _ITERM_MAX)
      iterm = - _ITERM_MAX;

    //추가로 적어야함

    control = pterm + dterm + iterm;
    error_prev = error_current;

    duty_target = _DUTY_NEU + control;

    // Limit duty_target within the range of [_DUTY_MIN, _DUTY_MAX]
    if (duty_target < _DUTY_MIN)
      duty_target = _DUTY_MIN; // lower limit
    if (duty_target > _DUTY_MAX)
      duty_target = _DUTY_MAX; // upper limit      
  }
  
  if (event_servo) {
    event_servo = false;

    // adjust duty_current toward duty_target by duty_chg_per_interval
    if (duty_target > duty_current) {
      duty_current += duty_chg_per_interval;
      if (duty_current > duty_target)
        duty_current = duty_target;
    } else {
      duty_current -= duty_chg_per_interval;
      if (duty_current < duty_target)
        duty_current = duty_target;
    }
    
    // update servo position
    myservo.writeMicroseconds(duty_current);    
  }
  
  if (event_serial) {
    event_serial = false;
    
    if (0) { // use for debugging with serial plotter
      Serial.print("MIN:0,MAX:310,TARGET:"); Serial.print(dist_target);
      Serial.print(",DIST:");                Serial.print(dist_ema);
      Serial.print(",ERR_ave(10X):");        Serial.println(error_sum / (float) error_cnt * 10);
    }

    if (0) { // use for debugging with serial monitor
      Serial.print("MIN:0,MAX:310,TARGET:"); Serial.print(dist_target);
      Serial.print(",DIST:");    Serial.print(dist_ema);
      Serial.print(",pterm:");   Serial.print(pterm);
      Serial.print(",dterm:");   Serial.print(dterm);
      Serial.print(",iterm:");   Serial.print(iterm);
      Serial.print(",ERR_cnt:"); Serial.print(error_cnt);
      Serial.print(",ERR_sum:"); Serial.print(error_sum);
 
      Serial.print(",ERR_current:"); Serial.print(abs(error_current));
      Serial.print(",ERR_ave:"); Serial.println(error_sum / (float) error_cnt);
    }

    if (1) { // use for evaluation
      Serial.print("ERR_cnt:");
      Serial.print(error_cnt);
      Serial.print(",ERR_current:");
      Serial.print(abs(error_current));
      Serial.print(",ERR_sum:");
      Serial.print(error_sum);
      Serial.print(",ERR_ave:");
      Serial.println(error_sum / (float) error_cnt);
    }
  }
}

float volt_to_distance(int a_value)
{
  // Replace next line into your own equation
  // return (6762.0 / (a_value - 9) - 4.0) * 10.0; 
  return 590 + -0.159 * a_value + -0.0117 * a_value * a_value + 3.15E-05 * a_value * a_value * a_value + -2.35E-08 * a_value * a_value * a_value * a_value;
}

int compare(const void *a, const void *b) {
  return (*(unsigned int *)a - *(unsigned int *)b);
}

unsigned int ir_sensor_filtered(unsigned int n, float position, int verbose)
{
  // Eliminate spiky noise of an IR distance sensor by repeating measurement and taking a middle value
  // n: number of measurement repetition
  // position: the percentile of the sample to be taken (0.0 <= position <= 1.0)
  // verbose: 0 - normal operation, 1 - observing the internal procedures, and 2 - calculating elapsed time.
  // Example 1: ir_sensor_filtered(n, 0.5, 0) => return the median value among the n measured samples.
  // Example 2: ir_sensor_filtered(n, 0.0, 0) => return the smallest value among the n measured samples.
  // Example 3: ir_sensor_filtered(n, 1.0, 0) => return the largest value among the n measured samples.

  // The output of Sharp infrared sensor includes lots of spiky noise.
  // To eliminate such a spike, ir_sensor_filtered() performs the following two steps:
  // Step 1. Repeat measurement n times and collect n * position smallest samples, where 0 <= postion <= 1.
  // Step 2. Return the position'th sample after sorting the collected samples.

  // returns 0, if any error occurs

  unsigned int *ir_val, ret_val;
  unsigned int start_time;
 
  if (verbose >= 2)
    start_time = millis(); 

  if ((n == 0) || (n > 100) || (position < 0.0) || (position > 1))
    return 0;
    
  if (position == 1.0)
    position = 0.999;

  if (verbose == 1) {
    Serial.print("n: "); Serial.print(n);
    Serial.print(", position: "); Serial.print(position); 
    Serial.print(", ret_idx: ");  Serial.println((unsigned int)(n * position)); 
  }

  ir_val = (unsigned int *)malloc(sizeof(unsigned int) * n);
  if (ir_val == NULL)
    return 0;

  if (verbose == 1)
    Serial.print("IR:");
  
  for (int i = 0; i < n; i++) {
    ir_val[i] = analogRead(PIN_IR);
    if (verbose == 1) {
        Serial.print(" ");
        Serial.print(ir_val[i]);
    }
  }

  if (verbose == 1)
    Serial.print  ("  => ");

  qsort(ir_val, n, sizeof(unsigned int), compare);
  ret_val = ir_val[(unsigned int)(n * position)];

  if (verbose == 1) {
    for (int i = 0; i < n; i++) {
        Serial.print(" ");
        Serial.print(ir_val[i]);
    }
    Serial.print(" :: ");
    Serial.println(ret_val);
  }
  free(ir_val);

  if (verbose >= 2) {
    Serial.print("Elapsed time: "); Serial.print(millis() - start_time); Serial.println("ms");
  }
  
  return ret_val;
}
