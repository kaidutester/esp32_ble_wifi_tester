//
// code to detect that button is pressed for 10s
//

#define RESET_BUTTON_TIME_THRESHOLD_MILLIS 10000
#define RESET_BUTTON_COUNT_THRESHOLD 5
#define RESET_BUTTON_PIN 0

#define RESET_LED_WHITE 6
#define RESET_LED_OFF 255

boolean reset_button_state = false;
boolean reset_button_transition_on, reset_button_transition_off;
boolean reset_button_short_click, reset_button_long_click, reset_button_long_click_detected;
int reset_button_counter = 0;
unsigned long reset_button_start_millis, reset_button_length_millis;

void led_solid (int colour);
void led_blink_fast (int colour, bool is_fast);

void reset_button_setup() {
  // setup to monitor GPIO0
  pinMode(RESET_BUTTON_PIN, INPUT_PULLDOWN);
  reset_button_state = false;
  reset_button_transition_on = false;
  reset_button_transition_off = false;
  reset_button_short_click = false;
  reset_button_long_click = false;
  reset_button_long_click_detected = false;
  reset_button_start_millis = 0;
  reset_button_length_millis = 0;
}

void reset_button_loop() {
  
  // simple debounce algo based on counter
  // count up when we detect button-press
  // count down when no button-press detected
  if (digitalRead(RESET_BUTTON_PIN)) {
    reset_button_counter -= 1;
    //Serial.println(F("GPIO0 is 0"));
  } else {
    reset_button_counter += 1;
    //Serial.println(F("GPIO0 is 1"));
  }

  // when counter is above a threshold declare button-press, and set variable true
  // use variables to detect transition from off-to-on and on-to-off
  //
  if (reset_button_counter > RESET_BUTTON_COUNT_THRESHOLD) { //10
    if (reset_button_state == false) {
      // transition to pressed state & start timing
      reset_button_state = true;
      reset_button_start_millis = millis();
      Serial.println(F("Reset button detected"));
      reset_button_transition_on = true;
      led_solid(RESET_LED_WHITE);
    }
    reset_button_counter = RESET_BUTTON_COUNT_THRESHOLD;
    reset_button_length_millis = millis() - reset_button_start_millis;
    
  } else if (reset_button_counter < 0) {
    if (reset_button_state == true) {
      // transition to off state
      reset_button_state = false;
      Serial.println(F("Reset button released"));
      reset_button_transition_off = true;
      reset_button_length_millis = millis() - reset_button_start_millis;
      led_solid(RESET_LED_OFF);
    }
    reset_button_counter = 0;
  }

  if (reset_button_transition_on == true && reset_button_transition_off == true) {
    // at this point, button is pressed and released once
    reset_button_transition_on = false;
    reset_button_transition_off = false;

    // if button press is short, then mark as a short press
    if (reset_button_length_millis > 500 && reset_button_length_millis < 8000) {
      reset_button_short_click = true;
      Serial.println(F("Short button press"));
    }
    else if (reset_button_length_millis >= 8000 && reset_button_long_click_detected == true) {
      reset_button_long_click = true;
      reset_button_long_click_detected = false;
      Serial.println(F("Long button press"));
    }

  } else if (reset_button_transition_on == true && reset_button_transition_off == false) {
    // button is held down, and not released yet
    // if time is above a long-threshold, then mark as a long press, and blink fast
    if (reset_button_length_millis >= 8000 && reset_button_long_click_detected == false) {
      reset_button_long_click_detected = true;
      Serial.println(F("Long button press detected, blink white"));
      led_blink_fast(RESET_LED_WHITE, true);
    }
  }
}

//boolean reset_button_get_state() {
//  //Serial.println(reset_button_state);
//  //Serial.println(reset_button_counter);
//  //Serial.println(reset_button_start_millis);
//  //return reset_button_state;
//  return (reset_button_state == true && millis() - reset_button_start_millis > RESET_BUTTON_TIME_THRESHOLD_MILLIS);
//}

void reset_button_interrupt() {
  reset_button_counter += 1;
}

boolean reset_button_get_pulse() {
  return (reset_button_counter > 0) && (reset_button_counter <RESET_BUTTON_COUNT_THRESHOLD);
}

boolean reset_button_get_short_click() {
  boolean temp = reset_button_short_click;
  reset_button_short_click = false;
  return temp;
}

boolean reset_button_get_long_click() {
  boolean temp = reset_button_long_click;
  reset_button_long_click = false;
  return temp;
}
