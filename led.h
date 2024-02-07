//
// LED related functions
//
// ESP32 devkit LEDS IO2(blue)
// Custom board LEDS IO26(red), IO27(green), IO14(blue)
//
// this code assumes we have the 3 leds, but for dev-kit it will merge all leds into one
//

#define LED_LOG_ENABLE 1

//#define CUSTOM_LED 1 //enable 3leds

#define LED_RED 0
#define LED_GREEN 1
#define LED_BLUE 2
#define LED_BLUE_RED 3
#define LED_RED_BLUE 3
#define LED_BLUE_GREEN 4
#define LED_GREEN_BLUE 4
#define LED_GREEN_RED 5
#define LED_RED_GREEN 5
#define LED_WHITE 6
#define LED_EXISTING 254
#define LED_OFF 255

#define LED_BLINK_FAST_MILLIS 150
#define LED_BLINK_SLOW_MILLIS 1500
#define LED_BLUE_PIN_NUM 2 // for esp-dev-kit

#define LED_MOKO_BLUE_PIN_NUM 14  // for MK110 plug
#define LED_MOKO_RED_PIN_NUM 26   // for MK110 plug
#define LED_MOKO_GREEN_PIN_NUM 27 // for MK110 plug

int led_colour = LED_BLUE;     //default blue led
int led_prev_colour = LED_BLUE;
boolean led_status = true;     //default on
boolean led_pause = false;     //default run
unsigned long led_millis = 0;
unsigned long led_interval_millis = 0; //LED_BLINK_SLOW_MILLIS;
unsigned long led_prev_interval_millis = 0; //LED_BLINK_SLOW_MILLIS;

void led_all_toggle (boolean status);

//////////////// internal functions /////////////////////

void led_blue_toggle (bool status) {
  //Serial.println(F("Toggle BLUE"));
  #ifdef CUSTOM_LED
  if (status) digitalWrite(LED_MOKO_BLUE_PIN_NUM, LOW);
  else digitalWrite(LED_MOKO_BLUE_PIN_NUM, HIGH);
  #else
  if (status) digitalWrite(LED_BLUE_PIN_NUM, LOW);
  else digitalWrite(LED_BLUE_PIN_NUM, HIGH);
  #endif
}

void led_green_toggle (boolean status) {
  //Serial.println(F("Toggle GREEN"));
  #ifdef CUSTOM_LED
  if (status) digitalWrite(LED_MOKO_GREEN_PIN_NUM, LOW);
  else digitalWrite(LED_MOKO_GREEN_PIN_NUM, HIGH);
  #else
  if (status) digitalWrite(LED_BLUE_PIN_NUM, LOW);
  else digitalWrite(LED_BLUE_PIN_NUM, HIGH);
  #endif
}

void led_red_toggle (boolean status) {
  //Serial.println(F("Toggle RED"));
  #ifdef CUSTOM_LED
  if (status) digitalWrite(LED_MOKO_RED_PIN_NUM, LOW);
  else digitalWrite(LED_MOKO_RED_PIN_NUM, HIGH);
  #else
  if (status) digitalWrite(LED_BLUE_PIN_NUM, LOW);
  else digitalWrite(LED_BLUE_PIN_NUM, HIGH);
  #endif
}

//////////////// external functions /////////////////////

void led_setup () {
  #ifdef CUSTOM_LED
  pinMode(LED_MOKO_RED_PIN_NUM, OUTPUT);
  digitalWrite(LED_MOKO_RED_PIN_NUM, HIGH);
  pinMode(LED_MOKO_BLUE_PIN_NUM, OUTPUT);
  digitalWrite(LED_MOKO_BLUE_PIN_NUM, LOW); //enables BLUE
  pinMode(LED_MOKO_GREEN_PIN_NUM, OUTPUT);
  digitalWrite(LED_MOKO_GREEN_PIN_NUM, HIGH);
  #else
  pinMode(LED_BLUE_PIN_NUM, OUTPUT);
  digitalWrite(LED_BLUE_PIN_NUM, HIGH);
  #endif
  led_interval_millis = 0; //solid
  led_colour = LED_BLUE;
  Serial.println("Power ON - BLUE LED");
  //delay(1000);
}

void led_pause_set(bool pause_status) {
  led_pause = pause_status;
}

void led_solid (int colour) {
  #ifdef LED_LOG_ENABLE
  if (led_colour != colour) {
    Serial.print(F("Solid RGB "));
    Serial.println(colour);
  }
  #endif
  //
  // store existing
  led_prev_colour = led_colour;
  led_prev_interval_millis = led_interval_millis;
  //
  if (led_colour != colour) {
    // when changing colour, turn off all leds first
    led_all_toggle(false);
  }
  led_colour = colour;
  led_interval_millis = 0;
  led_status = false; //force to always toggle on
}

void led_blink_fast (int colour, bool is_fast) {
  #ifdef LED_LOG_ENABLE
  if (led_colour != colour) {
    if (is_fast) {
      Serial.print(F("Fast RGB "));
      Serial.println(colour);
    }
    else {
      Serial.print(F("Slow RGB "));
      Serial.println(colour);
    }
  }
  #endif
  //
  // store existing
  led_prev_colour = led_colour;
  led_prev_interval_millis = led_interval_millis;
  //
  if (led_colour != colour) {
    // when changing colour, turn off all leds first
    led_all_toggle(false);
  }
  led_colour = colour;
  if (is_fast) {
    led_interval_millis = LED_BLINK_FAST_MILLIS;
  }
  else {
    led_interval_millis = LED_BLINK_SLOW_MILLIS;
  }
}

void led_previous () {
  // return to previous configuration
  if (led_prev_interval_millis < 1) {
    led_solid(led_prev_colour);
  }
  else if(led_prev_interval_millis > LED_BLINK_FAST_MILLIS) {
    led_blink_fast(led_prev_colour, false);
  }
  else {
    led_blink_fast(led_prev_colour, true);
  }
}

void led_toggle (boolean status) {
  led_status = status;
  if (led_colour == LED_RED) {
    led_red_toggle(led_status);
  } else if (led_colour == LED_GREEN) {
    led_green_toggle(led_status);
  } else if (led_colour == LED_BLUE) {
    led_blue_toggle(led_status);
  } else if (led_colour == LED_WHITE) {
    led_all_toggle(led_status);
  }
}

void led_all_toggle (boolean status) {
  #ifdef CUSTOM_LED
  if (status) {
    digitalWrite(LED_MOKO_RED_PIN_NUM, LOW);
    digitalWrite(LED_MOKO_GREEN_PIN_NUM, LOW);
    digitalWrite(LED_MOKO_BLUE_PIN_NUM, LOW);
  }
  else {
    digitalWrite(LED_MOKO_RED_PIN_NUM, HIGH);
    digitalWrite(LED_MOKO_GREEN_PIN_NUM, HIGH);
    digitalWrite(LED_MOKO_BLUE_PIN_NUM, HIGH);
  }
  #else
  if (status) digitalWrite(LED_BLUE_PIN_NUM, LOW);
  else digitalWrite(LED_BLUE_PIN_NUM, HIGH);
  #endif
}

void led_loop () {
  unsigned long millisnow = millis();
  if (millisnow > led_millis && led_pause == false) {
    // toggle
    led_status = !led_status;
    
    if (led_colour == LED_RED) {
      led_red_toggle(led_status);
      
    } else if (led_colour == LED_GREEN) {
      led_green_toggle(led_status);
      
    } else if (led_colour == LED_BLUE) {
      led_blue_toggle(led_status);
      
    } else if (led_colour == LED_WHITE) {
      led_all_toggle(led_status);
      
    } else if (led_colour == LED_BLUE_RED) {
      if (led_interval_millis > 1) {
        // alternate red-blue
        led_red_toggle(led_status);
        led_blue_toggle(!led_status);
      } else {
        // both on/off together for zero interval
        led_red_toggle(led_status);
        led_blue_toggle(led_status);
      }
      
    } else if (led_colour == LED_BLUE_GREEN) {
      if (led_interval_millis > 1) {
        // alternate red-blue
        led_green_toggle(led_status);
        led_blue_toggle(!led_status);
      } else {
        // both on/off together for zero interval
        led_green_toggle(led_status);
        led_blue_toggle(led_status);
      }
      
    } else if (led_colour == LED_GREEN_RED) {
      if (led_interval_millis > 1) {
        // alternate green-red for non-zero interval
        led_red_toggle(led_status);
        led_green_toggle(!led_status);
      } else {
        // both on/off together for zero interval
        led_red_toggle(led_status);
        led_green_toggle(led_status);
      }
      
    } else if (led_colour == LED_OFF) {
      led_all_toggle(false);
      led_status = true;
      
    } else {
      // else no changes, when led-colour is incorrect/undefined
      return;
    }
    //
    if (led_interval_millis > 1) {
      led_millis = millisnow + led_interval_millis;
      
    } else {
      // for solid leds
      led_millis = millisnow + LED_BLINK_SLOW_MILLIS;
      led_status = false; //force to always toggle on
    }
  }
}
