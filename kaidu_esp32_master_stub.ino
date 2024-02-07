//
// Kaidu ESP32 firmware for testing and development BLE configuration apps
//

#define KAIDU_SHOW_LOGS 1
#define KAIDU_SHOW_HEAP_LOGS 1
#define KAIDU_SHOW_WARNING_LOGS 1

#define KAIDU_REALTIME_KAIDU_CMDS 1 // polls for kaidu-cmds after uploading realtime-counts
#define KAIDU_REALTIME_MINICHAINS 1 // uploads minichains every 10 mins, comment out to reduce uploads

#define KAIDU_BULK_MINICHAINS_UPLOADS 1 // get and uploads bulk minichains, instead of one-by-one (unused)
#define KAIDU_ENABLE_DEEP_SLEEP 1       // put ESP into deep sleep after midnight
#define KAIDU_ENABLE_LIGHT_SLEEP 1      // put ESP into light sleep

//#define KAIDU_CONFIG_SETUP_TEST 1 // sets realtime every 2 mins, and minichain uploads every 30 mins (for testing only)
//#define KAIDU_SHOW_MINICHAIN_TYPE49 1 // search and logs minew card minichains (for testing only)

#define KAIDU_WDT_TIMEOUT 60
#define KAIDU_GPIO_WAKE_PIN GPIO_NUM_0
#define KAIDU_MAX_SHORT_CLICK 5        // maximum number of times ble-server can start per day
#define KAIDU_NRF_SERIAL_CMD_RETRY_COUNT 10
#define KAIDU_HTTP_CMD_RETRY_COUNT 10

#define SM_START 0
#define SM_FACTORY_TEST 1
#define SM_READ_FLASH_CONFIG 2

// when there is no valid flash configuration
// then get here to wait for button press to allow
// ble-advert and app to configure scanner, then reset
#define SM_WAIT_FOR_APP_CONFIG 3
#define SM_ENABLE_BLE_CONFIG 4
#define SM_WAITFOR_BLE_CONFIG 5
#define SM_DISABLE_BLE_CONFIG 6

// when scanner has waited too long or wifi could not connect many retries
// then get here to look for other scanners in running state to read their wifi credentials
// #define SM_BLE_CLIENT_INIT 7
// #define SM_BLE_CLIENT_LOOP 8

// when there was a long button press, then go into this state
// this deep-sleep has no wakeup time, only button-wake
#define SM_PREPARE_DEEP_SLEEP 7
#define SM_GO_DEEP_SLEEP 8

// when there is flash configuration, then try to connect to config-server
// and update system time, and read scanner running configuration
// if its time to run, then go to running state
// if not time to run, check kaidu-cmds
#define SM_CONFIG_ENABLE_WIFI 9
#define SM_CONFIG_WAIT_WIFI_RETRY 10   // wait here when retrying wifi
#define SM_CONFIG_READ_SETUP 11        // get todays config, we might have to start scanning right away
#define SM_CONFIG_CHECK_FW_UPDATE 12
#define SM_CONFIG_DISABLE_WIFI 13
#define SM_CONFIG_WAIT_TO_START 14

// // did not get any firmware update command, so go into running mode
// // which is to setup the NRF, wait, read status, repeat
// // when done, sleep, upload
// // perform any firmware updates if any
// #define SM_NRF_DETECT 15
// #define SM_NRF_SETUP_AND_START 16
// #define SM_NRF_SETUP_WAIT 17
// #define SM_NRF_WAIT_LOOP 18
// #define SM_NRF_MONITOR 19 // checks if its time to upload realtime counts, or upload minichains when table is getting full

// // connect to wifi and upload realtime-counts, also if minichain-table is getting full, upload some minichains
// // if unable to connect to wifi, buffer the realtime-counts
// #define SM_NRF_REALTIME_ENABLE_WIFI 20
// #define SM_NRF_REALTIME_WAIT_WIFI_RETRY 21   // wait here when retrying wifi
// #define SM_NRF_REALTIME_UPLOAD_COUNTS 22     // upload counts if realtime or existing realtime
// #define SM_NRF_REALTIME_UPLOAD_MINICHAINS 23 // when minichain table is getting full, upload here
// #define SM_NRF_REALTIME_KAIDU_CMD 24
// #define SM_NRF_REALTIME_DISABLE_WIFI 25

// #define SM_NRF_STOP_SCANNING 26
// #define SM_WAIT_TO_UPLOAD 27
// #define SM_NRF_MINICHAIN_ENABLE_WIFI 28
// #define SM_NRF_MINICHAIN_WAIT_WIFI_RETRY 29    // wait here when retrying wifi
// #define SM_NRF_MINICHAIN_READ_CONFIGSETUP 30   // get config for next day, so we know how long to sleep afterwards
// #define SM_NRF_MINICHAIN_UPLOAD_REALTIME_COUNTS 31 // upload all/remaining realtime-counts
// #define SM_NRF_MINICHAIN_UPLOAD_MINICHAINS 32  // upload all/remaining minichains
// #define SM_NRF_MINICHAIN_KAIDU_CMDS 33         // process any kaidu-cmds
// #define SM_NRF_MINICHAIN_DISABLE_WIFI 34       // then jump to SM_CONFIG_WAIT_TO_START

// #define SM_WAIT_TILL_MIDNIGHT 35

// // got a ESP firmware update
// #define SM_OTA_ENABLE 36
// #define SM_OTA_RUNNING 37

// // got a NRF firmware update
// #define SM_ENABLE_NRF_DFU 38
// #define SM_VERIFY_NRF_DFU 39
// #define SM_START_NRF_DFU 40
// #define SM_PROCESS_NRF_DFU 41

// // in case it is MOKO SGWP
// #define SM_ENABLE_MOKO_DFU 42

// // sleep until nearly time to begin scanning
// // when waking, its like a reboot
// #define SM_NRF_SLEEP 43
// #define SM_ESP_SLEEP 44

// // after ble configuration, hard-reset
// #define SM_NRF_RESET 45
// #define SM_ESP_RESET 46

// // when wifi-fails or nrf-fails, deep-sleep for 10 mins
// #define SM_PAUSE_PREPARE 47
// #define SM_NRF_ESP_PAUSE 48

// #define SM_MAX_STATE 48

#define SM_NRF_RESET 15
#define SM_ESP_RESET 16
#define SM_MAX_STATE 16

#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
//#include "driver/adc.h"
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "NimBLEDevice.h"

#define BLE_DEVICE_NAME "KaiduScanner"

#define WATCHDOG_MILLIS 100
#define ONE_MIN_SECS 60
#define TWO_MINS_SECS 120
#define FIVE_MINS_SECS 300
#define TEN_MINS_SECS 600
#define TWENTY_MINS_SECS 1200
#define THIRTY_MINS_SECS 1800
#define ONE_HOUR_SECS 3600
#define ONE_DAY_SECS 86400
#define FIVE_SECS_MILLIS 5000
#define TEN_SECS_MILLIS 10000
#define THIRTY_SECS_MILLIS 30000
#define ONE_MIN_MILLIS 60000
#define TWO_MINS_MILLIS 120000
#define FIVE_MINS_MILLIS 300000
#define TEN_MINS_MILLIS 600000
#define ONE_HOUR_MILLIS 3600000 // 1 hour
#define DEFAULT_SLEEP_MILLIS 6000
#define DEFAULT_LED_PULSE_MILLIS 1500
#define WAIT_FOR_RESET_TIMEOUT_MILLIS 3000 // 3s, give state-machine this much time to self-reset
#define WAIT_FOR_AFTER_REBOOT_MILLIS 10000 // 10s, dont reboot within this time after a reboot
#define WAIT_FOR_HEARTBEAT_TIMEOUT_MILLIS 600000 // 10 mins, wait this long for state-machine to change, and don't reboot within this period after a reboot
#define WAIT_FOR_CONFIGURATION_TIMEOUT_MILLIS 180000 // wait this long for user to configure before resetting

#define MAX_NUM_MINICHAINS 6144
#define MAX_NOP_OR_HTTP_ERROR_THRESHOLD 600 // set to ~10% of total minichains
#define MAX_SHORT_BUTTON_PRESS 10
#define REALTIME_BUFFER_ITEM_SIZE 512 // enough for one line of bulk minichains, follow NRF buffer size
#define REALTIME_BUFFER_MAX_COUNT 110 // enough for a bit more than 18 hours

#ifdef KAIDU_CONFIG_SETUP_TEST
#define REALTIME_UPLOAD_INTERVAL TWO_MINS_SECS
#define KAIDU_MAX_WIFI_RETRY_COUNT 6
#define KAIDU_SHORT_WIFI_RETRY_COUNT 1
#define DEFAULT_DELAY_MILLIS 1000
#define DEFAULT_DOT_COUNT 100
#else
#define REALTIME_UPLOAD_INTERVAL TEN_MINS_SECS
#define KAIDU_MAX_WIFI_RETRY_COUNT 6   // number of times to retry wifi before giving up
#define KAIDU_SHORT_WIFI_RETRY_COUNT 1 // number of times to retry wifi for realtime-counts
#define DEFAULT_DELAY_MILLIS 100
#define DEFAULT_DOT_COUNT 1000
#endif

esp_reset_reason_t sm_reset_reason;
esp_sleep_wakeup_cause_t sm_wakeup_reason;
bool ble_init_done = false;

#include "ble_server.h"
//#include "ble_client.h"
#include "configuration.h"
#include "reset_button.h"
#include "wifi_utils.h"
#include "kaidu_cmds.h"
//#include "serial_comm.h"
#include "led.h"
//#include "ota.h"
//#include "fwu.h"

int current_state = 0;
int next_state = 0;
int sm_general_counter = 0;
int sm_uploaderror_counter = 0;
//int sm_wifi_retry_counter = 0;
int short_button_press_count = 0;
int sm_retry_counter = 0;

bool sm_general_flag;
bool sm_error_flag;
bool nrf_detected = false;
bool config_mode_flag = false;
bool hibernate_flag = false;

unsigned long millisnow = 0;
unsigned long ota_over_time = 0;
unsigned long last_heartbeat_millis = 0;
unsigned long sm_timer_end_millis = 0;
unsigned long sm_timer_end_secs = 0;
unsigned long force_reset_time = 0;
unsigned long last_short_click_millis = 0;

char kaidu_time_ref[10];
int serial_command;
char serial_uart_buf[REALTIME_BUFFER_ITEM_SIZE]; //TODO-fixme //110
int scanner_realtime_queue_pos = 0;
char* scanner_realtime_queue_buffer[REALTIME_BUFFER_MAX_COUNT]; //110

bool scanner_realtime_flag;
unsigned long scanner_start_sec;
unsigned long scanner_end_sec;
unsigned long scanner_upload_sec;
unsigned long scanner_midnight_sec;
unsigned long scanner_random_sec;
unsigned long scanner_realtime_sec;
int scanner_rssi_threshold;
int scanner_passerby_threshold;

#define LED_RTOS_TASK_PRIORITY 1 // 2
#define LED_RTOS_TASK_STACK 1536

TaskHandle_t led_rtos_task_handle;

// watchdog task for test state-machine
void led_rtos_task (void *pvParameters)
{
  unsigned int reset_counter;
  
  // setup stuff
  esp_task_wdt_init(KAIDU_WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  reset_button_setup();
  
  // blinking led and watchdog task runs forever
  while (1) {
    //
    // update leds, button, and wdt
    //
    led_loop();
    reset_button_loop();
    esp_task_wdt_reset();
    millisnow = millis();
  
    //
    // detect button press here
    //

    //XXXDC TODO- should set a wifi-offline and set a flag to tell state-machine to go into deep-sleep/hibernate mode
    if (reset_button_get_long_click()) {
      #ifdef KAIDU_SHOW_WARNING_LOGS
      Serial.println(F("Watchdog: long button pressed, hibernate!"));
      Serial.flush();
      #endif
      hibernate_flag = true;
      wifi_force_offline_mode(true);
    }

    if (reset_button_get_short_click() && short_button_press_count < KAIDU_MAX_SHORT_CLICK) {
      //XXXDC TODO- disconnect wifi, and set flag to tell state-machine to go into ble-config mode
      #ifdef KAIDU_SHOW_WARNING_LOGS
      Serial.println(F("Watchdog: short button pressed!"));
      #endif
      ++short_button_press_count;
      last_short_click_millis = millisnow;
      //ble_server_start();
      config_mode_flag = true;
      wifi_force_offline_mode(true);
      
    } else if (!ble_server_get_connection_status() && ble_server_get_updated_status()) {
      #ifdef KAIDU_SHOW_WARNING_LOGS
      Serial.println(F("Detect that ble-server changed something, resetting!"));
      Serial.flush();
      #endif
      #ifdef KAIDU_CONFIG_SETUP_TEST
      led_solid(LED_BLUE);
      next_state = SM_NRF_RESET;
      if (force_reset_time < millisnow) { // set it once
        force_reset_time = millis() + WAIT_FOR_RESET_TIMEOUT_MILLIS;
      }
      #else
      ESP.restart();
      #endif
      
    } else if (last_short_click_millis > 0 && millisnow > (last_short_click_millis + TEN_MINS_MILLIS) &&
      !ble_server_get_connection_status()) {
      last_short_click_millis = 0;
      ble_server_stop();
    }
    
    if (millisnow < last_heartbeat_millis) last_heartbeat_millis = millisnow; //handle millis() overflow
    if (millisnow > last_heartbeat_millis && force_reset_time < 1000 &&
        millisnow - last_heartbeat_millis > WAIT_FOR_HEARTBEAT_TIMEOUT_MILLIS) { //10mins
      // detects if any calls in the state-machine does not return after a while (ie blocks for too long)
      #ifdef KAIDU_SHOW_WARNING_LOGS
      Serial.println(F("Watchdog: lost heartbeat, resetting!"));
      #endif
      #ifdef KAIDU_CONFIG_SETUP_TEST
      led_solid(LED_RED);
      reset_counter = 0;
      next_state = SM_NRF_RESET;
      if (force_reset_time < millisnow) {
        force_reset_time = millisnow + WAIT_FOR_RESET_TIMEOUT_MILLIS;
      }
      #else
      ESP.restart();
      #endif
    }

    //*******************************
    // Toggle Reset
    //*******************************
    if (next_state == SM_ESP_RESET || current_state == SM_ESP_RESET) {
      ++reset_counter; //take over this counter, usually its a low value      
    }
    if ((next_state == SM_NRF_RESET || next_state == SM_ESP_RESET) && (force_reset_time >= 1000) && (millisnow > WAIT_FOR_AFTER_REBOOT_MILLIS) && 
    (millisnow > force_reset_time || reset_counter > WAIT_FOR_RESET_TIMEOUT_MILLIS/WATCHDOG_MILLIS)) {
      // force reset if it takes too long for the state-machine to reset gracefully
      ESP.restart();
      vTaskDelay(WATCHDOG_MILLIS / portTICK_RATE_MS); // should not get here
    }
    
    // pause to let other things to run
    vTaskDelay(WATCHDOG_MILLIS / portTICK_RATE_MS);
  }
  
  vTaskDelete( NULL ); //err... should not get here!
}

void calculate_working_schedule(unsigned long current_time) {
  //
  // generate new working schedule
  // if new values are not valid, shift old values by 24hrs
  // TODO- handle when scanning-hours go beyond 18hrs
  //
  if (scanner_random_sec == 0) {
    // get a random offset, we use this to distribute calls to the config-server
    scanner_random_sec = current_time % REALTIME_UPLOAD_INTERVAL;
    #ifdef KAIDU_SHOW_LOGS
    Serial.printf("scanner_random_sec = %d \n", scanner_random_sec);
    #endif
  }
  if (kaidu_configsetup_datalen() > 0) {
    scanner_realtime_flag = kaidu_configsetup_realtime_flag();
    scanner_start_sec = kaidu_configsetup_start_ts();
    scanner_end_sec = kaidu_configsetup_end_ts(); // a bit past actual end time
    scanner_upload_sec = scanner_end_sec; // upload after the end-of-scanning
    scanner_midnight_sec = kaidu_configsetup_midnight_ts() + REALTIME_UPLOAD_INTERVAL; // a bit past midnight
    scanner_rssi_threshold = kaidu_configsetup_rssi_threshold();
    scanner_passerby_threshold = kaidu_configsetup_passerby_threshold(); 
  } else {
    scanner_start_sec = 0;
    scanner_end_sec = 1;
    scanner_upload_sec = 2;
    scanner_midnight_sec = 3;
  }
  //
  // start at 10 min boundary, and add one scan-interval because we will subtract one interval below
  scanner_realtime_sec = scanner_start_sec - (scanner_start_sec % REALTIME_UPLOAD_INTERVAL) + REALTIME_UPLOAD_INTERVAL;
  
  //
  //XXXDC TODO- get the rest of the config-setup, rssi-threshold and passerby-length
  //
  #ifdef KAIDU_CONFIG_SETUP_TEST
  //
  //XXX for tests only
  // uploads realtime every 2 or 10 mins, uploads minichains in 10 or 60 mins
  //
  Serial.println(F("CONFIG_SETUP_TEST configsetup"));
  scanner_random_sec = 0; // no randomization
  scanner_realtime_flag = true;
  scanner_start_sec = current_time + REALTIME_UPLOAD_INTERVAL;        // now or 2or10 mins from now
  scanner_end_sec = scanner_start_sec + 5*REALTIME_UPLOAD_INTERVAL;     // run for abit
  scanner_upload_sec = scanner_end_sec + 60; //REALTIME_UPLOAD_INTERVAL;      // upload when finished
  scanner_midnight_sec = scanner_upload_sec + 60; //2*FIVE_MINS_SECS; //REALTIME_UPLOAD_INTERVAL; // wait abit till midnight
  scanner_rssi_threshold = -88;
  scanner_passerby_threshold = 120;
  //
  scanner_realtime_sec = scanner_start_sec - (scanner_start_sec % REALTIME_UPLOAD_INTERVAL) + REALTIME_UPLOAD_INTERVAL;
  #else
  scanner_realtime_flag = true; //XXXDC fake this for now
  #endif
  //
  // catchup to current time, if rebooted in the middle of the day
  // note: its possible that when scanner boots after scanner-end-sec time
  //       that we loop past the scanner-end-sec or even scanner-midnight-sec!
  //
  while (scanner_realtime_sec < current_time) {
    scanner_realtime_sec = scanner_realtime_sec + REALTIME_UPLOAD_INTERVAL;
  }
  // step-back one time because calling next_realtime_upload_time() will step forward again
  scanner_realtime_sec = scanner_realtime_sec - REALTIME_UPLOAD_INTERVAL;
  //
  // limit to 18 hours - we adjust here so that when scanner is assigned to a customer with 24hr operation
  // the scanner will go 18hrs from time of bootup.
  // then when it finishes, it can reboot and continue for another 6hrs.
  //
  if (scanner_end_sec > scanner_realtime_sec && scanner_end_sec - scanner_realtime_sec > 64800) {
    #ifdef KAIDU_SHOW_WARNING_LOGS
    Serial.println(F("calculate_working_schedule: auto-limit to 18 hours of operation!"));
    #endif
    scanner_end_sec = scanner_realtime_sec + 64800;
    scanner_upload_sec = scanner_end_sec;
    scanner_midnight_sec = scanner_upload_sec;
  }
  //
  #ifdef KAIDU_SHOW_LOGS
  if (current_time < scanner_start_sec) {
    Serial.printf("Schedule: now=%d << %d << %d << %d << %d\n", current_time, scanner_start_sec, scanner_end_sec, scanner_upload_sec, scanner_midnight_sec);
  } else if (scanner_start_sec <= current_time && current_time < scanner_end_sec) {
    Serial.printf("Schedule: %d << now=%d << %d << %d << %d\n", scanner_start_sec, current_time, scanner_end_sec, scanner_upload_sec, scanner_midnight_sec);
  } else if (scanner_end_sec <= current_time && current_time < scanner_upload_sec) {
    Serial.printf("Schedule: %d << %d << now=%d << %d << %d\n", scanner_start_sec, scanner_end_sec, current_time, scanner_upload_sec, scanner_midnight_sec);
  } else if (scanner_upload_sec <= current_time) {
    Serial.printf("Schedule: %d << %d << %d << now=%d << %d\n", scanner_start_sec, scanner_end_sec, scanner_upload_sec, current_time, scanner_midnight_sec);
  }
  #endif
}


unsigned long next_realtime_upload_time(unsigned long current_time) {
  //unsigned long next_time;
  //#ifdef KAIDU_CONFIG_SETUP_TEST
  //Serial.println(F("next_realtime_upload_time in 5 minutes (or less)"));
  //#endif
  //next_time = current_time + REALTIME_UPLOAD_INTERVAL; //ten minutes (2 mins for test-mode)
  //next_time -= (next_time % REALTIME_UPLOAD_INTERVAL);
  if (current_time > scanner_realtime_sec) {
    scanner_realtime_sec = scanner_realtime_sec + REALTIME_UPLOAD_INTERVAL;
    #ifdef KAIDU_SHOW_LOGS
    Serial.printf("next_realtime_upload_time is %d \n", scanner_realtime_sec);
    #endif
  }
  return scanner_realtime_sec;
}

//void esp_wakeup_reason() {
//  esp_sleep_wakeup_cause_t wakeup_reason;
//
//  wakeup_reason = esp_sleep_get_wakeup_cause();
//
//  switch(wakeup_reason)
//  {
//    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println(F("Wakeup RTC_IO")); ++short_button_press_count; break;
//    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println(F("Wakeup RTC_CNTL")); break;
//    case ESP_SLEEP_WAKEUP_TIMER : Serial.println(F("Wakeup timer")); break;
//    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println(F("Wakeup touchpad")); break;
//    case ESP_SLEEP_WAKEUP_ULP : Serial.println(F("Wakeup ULP")); break;
//    default : Serial.printf("Wakeup: %d\n",wakeup_reason); break;
//  }
//}

void led_pulse_during_sleep() {
  //unsigned long wake_time = millisnow;
  led_toggle(true);
  delay(DEFAULT_LED_PULSE_MILLIS);
  led_toggle(false);
  if (reset_button_get_pulse()) {
    if (sm_uploaderror_counter > 0) {
      led_blue_toggle(true);
      delay(DEFAULT_LED_PULSE_MILLIS);
      led_blue_toggle(false);
    } else if (nrf_detected == false) {
      led_red_toggle(true);
      delay(DEFAULT_LED_PULSE_MILLIS);
      led_red_toggle(false);
    }
  }
}

bool light_sleep (unsigned long sleep_millis) {
  unsigned long wake_time = millisnow + sleep_millis;
  unsigned long return_to_sleep = millisnow;
  #ifdef KAIDU_SHOW_LOGS
  unsigned int dots = 0;
  Serial.printf("light-sleep at %d for %d secs \n", time(nullptr), sleep_millis/1000);
  Serial.flush();
  delay(DEFAULT_DELAY_MILLIS);
  wake_time -= DEFAULT_DELAY_MILLIS;
  #endif
  last_heartbeat_millis = millisnow; // feed the watchdog
  if (sleep_millis < ONE_MIN_MILLIS) {
    // just use delay() for realtime-mode aka high-power or short sleep
    //
    while (millisnow < wake_time && next_state != SM_NRF_RESET && next_state != SM_ESP_RESET) {
      #ifdef KAIDU_SHOW_LOGS
      if (++dots%DEFAULT_DOT_COUNT == 0) {
        Serial.println(F("."));
      } else if (dots%(DEFAULT_DOT_COUNT/10) == 0) {
        Serial.print(F("."));
      }
      delay(DEFAULT_DELAY_MILLIS);
      #endif
      //led_pulse_during_sleep();
      last_heartbeat_millis = millisnow;
    }
    
  } else {
    // light-sleep
    //
    led_loop(); // force update led
    //
    #ifdef KAIDU_ENABLE_LIGHT_SLEEP
    // we are going to call esp-light-sleep, so turn off led processing
    led_pause_set(true); // stop normal led processing
    led_toggle(false);   // turn off led to show that we are sleeping
    //
    esp_wifi_stop();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    while (millisnow < wake_time && !config_mode_flag && !hibernate_flag) {
      esp_sleep_enable_ext0_wakeup(KAIDU_GPIO_WAKE_PIN, 0);
      //esp_sleep_enable_timer_wakeup(DEFAULT_SLEEP_MILLIS * 1000LL); // in microseconds
      esp_sleep_enable_timer_wakeup(sleep_millis * 1000LL); // in microseconds
      esp_light_sleep_start();
      //esp_deep_sleep_start(); //for testing only
      //esp_wifi_start();
      //esp_wakeup_reason();
      millisnow = millis();
      if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        // woke from button press, stay for 10s to show led-status
        // meanwhile check if its a short press (config-mode-flag is true) or long press (hibernate)
        #ifdef KAIDU_SHOW_WARNING_LOGS
        Serial.print(F("button-woke! "));
        #endif
        reset_button_interrupt();
        led_pause_set(false); // let normal led-processing to start
        return_to_sleep = millisnow + TEN_SECS_MILLIS;
        while (millisnow < return_to_sleep && !config_mode_flag && !hibernate_flag) {
          #if defined(KAIDU_SHOW_LOGS) && defined(KAIDU_CONFIG_SERVER_DEV)
          if (++dots%DEFAULT_DOT_COUNT == 0) {
            Serial.println(F("#"));
          } else if (dots%(DEFAULT_DOT_COUNT/10) == 0) {
            Serial.print(F("#"));
          }
          delay(DEFAULT_SLEEP_MILLIS);
          #endif
          last_heartbeat_millis = millisnow;
          if (config_mode_flag || hibernate_flag) {
            // button was held down, so leave this sleep routine
            return false;
          }
          led_pulse_during_sleep();
        }
        led_pause_set(true); // disable normal led processing
        // done processing button toggle
      }
      // going back to esp-light-sleep loop
      last_heartbeat_millis = millisnow;
    }
    // sleep done
    // normally, we should be turning on wifi for uploading
    esp_wifi_start();
    //
    led_pause_set(false); // enable normal led processing
    
    #else  //KAIDU_ENABLE_LIGHT_SLEEP
    // not calling esp-light-sleep, use delays instead
    while (millisnow < wake_time && !config_mode_flag && !hibernate_flag) {
      #ifdef KAIDU_SHOW_LOGS
      if (++dots%DEFAULT_DOT_COUNT == 0) {
        Serial.println(F("."));
      } else if (dots%(DEFAULT_DOT_COUNT/10) == 0) {
        Serial.print(F("."));
      }
      delay(DEFAULT_SLEEP_MILLIS);
      #endif 
      //led_pulse_during_sleep();
      last_heartbeat_millis = millisnow;
    }
    //
    #endif //KAIDU_ENABLE_LIGHT_SLEEP
  }

  //
  //millisnow = millis();    // update here, just in case
  #ifdef KAIDU_SHOW_LOGS
  Serial.printf(" woke from light-sleep at %d \n", time(nullptr));
  #endif
  led_pause_set(false);
  return true;
}

#ifdef KAIDU_ENABLE_DEEP_SLEEP
// on ESP32 devkit board, the cpu does not wakeup!
//
void deep_sleep (unsigned long sleep_millis) {
  //unsigned long wake_time = millisnow + sleep_millis;
  #ifdef KAIDU_SHOW_LOGS
  Serial.printf("deep-sleep at %d for %d secs \n", time(nullptr), sleep_millis/1000);
  Serial.flush();
  delay(1000); //allow serial to flush
  #endif
  // turn off LEDs and UARTs so that it does not wake right away
  // the NRF should also be already be put to sleep, with UART disabled
  led_solid(LED_OFF);  // turn LED off
  led_loop();          // force update led
  Serial.end();
  Serial2.end();
  //
  esp_wifi_stop();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_ext0_wakeup(KAIDU_GPIO_WAKE_PIN, 0); // wake up if button pushed, but does not work
  esp_sleep_enable_timer_wakeup(sleep_millis * 1000LL); // in microseconds
  //esp_sleep_enable_timer_wakeup(60000000LL); // 60 seconds
  esp_deep_sleep_start();
  //
  led_solid(LED_WHITE); // should not get here, instead chip should reset when awoke
  ESP.restart();
}
#endif

// for some state transition, we want to directly go to enable ble-config
// instead of the normal operation
int get_next_state(int next_state_request) {
  if (hibernate_flag) {
    sm_retry_counter = next_state_request;
    return SM_PREPARE_DEEP_SLEEP;
  } else if (config_mode_flag) {
    sm_retry_counter = next_state_request;
    return SM_ENABLE_BLE_CONFIG;
  } else {
    return next_state_request;
  }
}

//
// initialize/setup all variables
//
void setup() {

  Serial.begin(115200);
  //Serial.print(F("\nCPU Freq: "));
  //Serial.println(getCpuFrequencyMhz());

  // 1= power-up
  // 8= deep-sleep button wake
  sm_reset_reason = esp_reset_reason();
  sm_wakeup_reason = esp_sleep_get_wakeup_cause();
  if (sm_reset_reason == 1 || (sm_reset_reason == 8 && sm_wakeup_reason == 2)) {
    Serial.println(F("\nUser-wake!"));
  } else {
    Serial.print(F("\nReset reason:"));
    Serial.print(sm_reset_reason);
    Serial.print(F(", wakeup reason:"));
    Serial.println(sm_wakeup_reason);
  }

  //Serial.println(F("\nSetup..."));
  
  led_setup(); //default BLUE
  configuration_setup();
  //serial_uart_init();
  reset_button_setup();
  //serialcomm_setup();
  wifi_setup();

  ble_server_setup(BLE_DEVICE_NAME);
  //
  NimBLEDevice::init(BLE_DEVICE_NAME);
  const uint8_t* rawMacAddr = NimBLEDevice::getAddress().getNative();
  kaidu_set_mac_address((uint8_t *)rawMacAddr);
  //
  //serial_uart_buf = (char *) malloc(REALTIME_BUFFER_ITEM_SIZE);
  for (int i=0; i<REALTIME_BUFFER_MAX_COUNT; i++) {
    scanner_realtime_queue_buffer[i] = (char *) malloc(REALTIME_BUFFER_ITEM_SIZE);
    memset(scanner_realtime_queue_buffer[i], 0, REALTIME_BUFFER_ITEM_SIZE);
  }
  //
  // initial values
  scanner_rssi_threshold = -72;
  scanner_passerby_threshold = 120;
  //
  //Serial.println(F("...setup done!"));
}

void free_allocated_memory() {
  //free(serial_uart_buf);
  for (int i=0; i<REALTIME_BUFFER_MAX_COUNT; i++) {
    free(scanner_realtime_queue_buffer[i]);
  }
}

void loop() {
  unsigned long timenow = time(nullptr);

  current_state = next_state;
  last_heartbeat_millis = millisnow;
  
  if (current_state < 0 || current_state > SM_MAX_STATE) {
    current_state = 0;
    next_state = 0;
    #ifdef KAIDU_SHOW_WARNING_LOGS
    Serial.println(F("Invalid current state, reset to 0"));
    #endif
  }
  
  else if (current_state == SM_START) {
    Serial.println(VERSION_STRING);
    #ifdef KAIDU_SHOW_LOGS
    Serial.println(F("SM_START"));
    Serial.println(F("Start LED and Watchdog task"));
    #endif
    #ifdef KAIDU_SHOW_HEAP_LOGS
    Serial.print(F("Free heap ")); Serial.println(ESP.getFreeHeap());
    #endif

    xTaskCreate(led_rtos_task, "LedTask", LED_RTOS_TASK_STACK, NULL, LED_RTOS_TASK_PRIORITY, &led_rtos_task_handle);
    next_state = SM_FACTORY_TEST;
    led_solid(LED_BLUE); // should have already been set to blue by default
    //delay(5000); //give time to see white led, and for nrf to startup
  }

  else if (current_state == SM_FACTORY_TEST) {
    #ifdef KAIDU_SHOW_LOGS
    Serial.println(F("SM_FACTORY_TEST"));
    #endif
    // perform ESP32 and nRF52 functionality tests
    //
    nrf_detected = true; // fake the test

    // keep going no matter what, at least try to send a boot msg
    // if factory test failed, then boot msg will show no nrf version
    next_state = SM_READ_FLASH_CONFIG;
  }

  else if (current_state == SM_READ_FLASH_CONFIG) {
    #ifdef KAIDU_SHOW_LOGS
    Serial.println(F("SM_READ_FLASH_CONFIG"));
    #endif
    // commented out when flash-configuration is empty
    // since we have wifi-presets, so its ok for empty configuration
    //
//    if (configuration_check_all()) {
//      // wifi credentials exists, so let enable wifi and read config-server
//      sm_general_counter = 0; // wifi connect retry count
//      next_state = SM_CONFIG_ENABLE_WIFI;
//      ble_server_update_manufacture_data_device_status(BLE_SERVER_DEVICE_STATE_CONFIGURED, 0);
//    }
//    else {
//      // no configuration, go wait for a button press to enable ble-advert and setup
//      #ifdef KAIDU_SHOW_WARNING_LOGS
//      Serial.println(F("Configuration check failed, go on and wait for button press"));
//      #endif
//      next_state = SM_WAIT_FOR_APP_CONFIG;
//      led_solid(LED_RED);
//      sm_timer_end_millis = millisnow + WAIT_FOR_CONFIGURATION_TIMEOUT_MILLIS; //3mins
//    }
    configuration_check_all();
    sm_general_counter = 0;    // tracks number of times we retry wifi
    ota_over_time = millisnow; // reuse this var to keep advert start time
    ble_server_update_manufacture_data_device_status(BLE_SERVER_DEVICE_STATE_CONFIGURED, (int)wifi_get_avg_wifi_rssi()); // setup ble-advert bytes
    ble_server_start();        // start ble-advert
    next_state = SM_CONFIG_ENABLE_WIFI;
  }

  else if (current_state == SM_WAIT_FOR_APP_CONFIG) {
//    #ifdef KAIDU_SHOW_LOGS
//    Serial.println(F("SM_WAIT_FOR_APP_CONFIG"));
//    #endif
    //
    // use this state to wait for a user to connect and configure this device
    // meanwhile, blink LEDs to alert user
    //
    if (reset_button_get_short_click()) {
      // got a click, so enable ble-advert and setup
      next_state = get_next_state(SM_ENABLE_BLE_CONFIG);
      
    } else if (millisnow > sm_timer_end_millis && !ble_server_get_connection_status()) {
      // waited for app configuration but nothing happened, go deep-sleep for an hour
      ble_server_stop();
      if (sm_reset_reason == 1 || (sm_reset_reason == 8 && sm_wakeup_reason == 2)) {
        // user powerup device or button-woke it, but something went wrong
        // but user did not configure this device, so lets do a short sleep and restart
        next_state = SM_ESP_RESET; // goto 10min deep sleep
      } else {
        // device woke-up by itself, and something went wrong, and we got here
        // so sleep for a long time to conserve power and restart to retry
        next_state = SM_ESP_RESET; // goto 1hr deep sleep
      }

    } else if (ble_server_get_connection_status() && ble_server_get_connect_secs() > FIVE_MINS_SECS) {
      // app connected for too long, maybe forgotten?
      // in-case something was written, go reset
      ble_server_stop();
      led_solid(LED_RED);
      next_state = SM_NRF_RESET;
      
    } else {
      next_state = get_next_state(SM_WAIT_FOR_APP_CONFIG);
      #ifdef KAIDU_SHOW_LOGS
      delay(DEFAULT_DELAY_MILLIS);
      #endif
    }
  }

  else if (current_state == SM_ENABLE_BLE_CONFIG) {
    #ifdef KAIDU_SHOW_LOGS
    Serial.println(F("SM_ENABLE_BLE_CONFIG"));
    #endif
    ble_server_start();
    led_blink_fast(LED_BLUE, true);
    next_state = SM_WAITFOR_BLE_CONFIG;
    #ifdef KAIDU_CONFIG_SETUP_TEST
    sm_timer_end_millis = millisnow + ONE_MIN_MILLIS; //for testing only
    #else
    sm_timer_end_millis = millisnow + WAIT_FOR_CONFIGURATION_TIMEOUT_MILLIS; //3mins
    #endif
    #ifdef KAIDU_SHOW_LOGS
    Serial.println(F("SM_WAITFOR_BLE_CONFIG"));
    #endif
  }

  else if (current_state == SM_WAITFOR_BLE_CONFIG) {
    next_state = SM_WAITFOR_BLE_CONFIG;
    if (hibernate_flag) {
      led_blink_fast(LED_WHITE, true);
      next_state = SM_PREPARE_DEEP_SLEEP;
    }
    else if (!ble_server_get_connection_status() && millisnow > sm_timer_end_millis) { //5mins
      next_state = SM_DISABLE_BLE_CONFIG;
    }
    else if (!ble_server_get_connection_status() && ble_server_get_updated_status()) {
      #ifdef KAIDU_SHOW_LOGS
      Serial.println(F("Detect that ble-server changed something, resetting!"));
      #endif
      next_state = SM_DISABLE_BLE_CONFIG; //which should also go to SM_NRF_RESET;
    }
    else if (ble_server_get_connection_status() && ble_server_get_connect_secs() > FIVE_MINS_SECS) { //5mins
      next_state = SM_DISABLE_BLE_CONFIG;
    }
    else if (ble_server_get_connection_status()) {
      led_solid(LED_BLUE); // show connection
    }
    else {
      #ifdef KAIDU_SHOW_LOGS
      Serial.print(F("."));
      delay(DEFAULT_DELAY_MILLIS);
      #endif
    }
  }

  else if (current_state == SM_DISABLE_BLE_CONFIG) {
    #ifdef KAIDU_SHOW_LOGS
    Serial.println(F("SM_DISABLE_BLE_CONFIG"));
    #endif
    ble_server_stop();
    config_mode_flag = false;
    wifi_force_offline_mode(true);
    next_state = SM_NRF_RESET;
  }

  else if (current_state == SM_CONFIG_ENABLE_WIFI) {
    //
    // turn on wifi to get scanner setup, ie. start/end/midnight times
    //
    #ifdef KAIDU_SHOW_LOGS
    Serial.println(F("SM_CONFIG_ENABLE_WIFI"));
    #endif
    #ifdef KAIDU_SHOW_HEAP_LOGS
    Serial.print(F("Free heap ")); Serial.println(ESP.getFreeHeap());
    #endif
    //
    next_state = SM_CONFIG_DISABLE_WIFI; //default action
    sm_error_flag = false;
    sm_uploaderror_counter = 0;
    led_blink_fast(LED_BLUE, true);
    //
    wifi_connect(true);
    //
    sm_general_flag = false;
    if (wifi_get_connected_status()) {
      sm_general_flag = true; //kaidu_cmd_find_config_server_url();
    }
    if (sm_general_flag) {
      // got wifi and internet
      led_blink_fast(LED_BLUE, false);
      next_state = SM_CONFIG_READ_SETUP; //SM_CONFIG_CHECK_FW_UPDATE;
      ble_server_update_manufacture_data_device_status(BLE_SERVER_DEVICE_STATE_OPERATIONAL, (int)wifi_get_avg_wifi_rssi());
      
    } else if (reset_button_get_short_click()) {
      // wifi not connected (no ssid or no internet)
      // got a click, so enable ble-advert and setup
      next_state = SM_ENABLE_BLE_CONFIG;
      
    } else {
      // wifi not connected (no ssid or no internet)
      // change ble-advert to show device status
      if (wifi_get_ssid_status()) {
        // wifi ssid exists but failed to connect
        ble_server_update_manufacture_data_device_status(BLE_SERVER_DEVICE_STATE_WIFI_ERROR, 0);
      } else {
        // no wifi found, assume ssid-configuration error
        ble_server_update_manufacture_data_device_status(BLE_SERVER_DEVICE_STATE_SSID_ERROR, 0);
      }
      // 
      sm_general_counter += 1; // wifi connect retry count
      sm_timer_end_millis = millisnow + THIRTY_SECS_MILLIS;
      next_state = SM_CONFIG_WAIT_WIFI_RETRY; // wait to retry or will deep-sleep for 10 mins
      #ifdef KAIDU_SHOW_LOGS
      Serial.print(F("SM_CONFIG_WAIT_WIFI_RETRY"));
      Serial.println(sm_general_counter);
      #endif
      //
    }
  }

  else if (current_state == SM_CONFIG_WAIT_WIFI_RETRY) {
    //
    // wifi failed to connect, pause here then retry a few times
    // otherwise go to wait state
    // note: allow hibernate or goto wait for configuration state
    //
    if (sm_general_counter < KAIDU_MAX_WIFI_RETRY_COUNT && 
        millisnow < ota_over_time + KAIDU_MAX_WIFI_RETRY_COUNT * THIRTY_SECS_MILLIS) { //6 or 3 mins

      if (reset_button_get_short_click() || ble_server_get_connection_status()) {
        next_state = SM_ENABLE_BLE_CONFIG;
        
      } else {
        // decide how long to stay here
        if (millisnow > ota_over_time + sm_general_counter * THIRTY_SECS_MILLIS) {
          next_state = get_next_state(SM_CONFIG_ENABLE_WIFI);
          #ifdef KAIDU_SHOW_LOGS
          Serial.println(F("."));
          #endif
          
        } else {
          // keep waiting in this state
          next_state = get_next_state(SM_CONFIG_WAIT_WIFI_RETRY);
          #ifdef KAIDU_SHOW_LOGS
          Serial.print(F("."));
          delay(DEFAULT_DELAY_MILLIS);
          #endif
        }
      }
      
    } else {
      // give up trying wifi, optional to wait-&-show-leds, and then deep-sleep for 10 mins
      led_blink_fast(LED_BLUE_RED, true);
      sm_timer_end_millis = millisnow + WAIT_FOR_CONFIGURATION_TIMEOUT_MILLIS; //3 mins
      next_state = get_next_state(SM_ESP_RESET); //was SM_WAIT_FOR_APP_CONFIG
    }
  }

  else if (current_state == SM_CONFIG_READ_SETUP) {
    //
    // read config-server for today's setup settings start/end/midnight
    //
    #ifdef KAIDU_SHOW_LOGS
    Serial.println(F("SM_CONFIG_READ_SETUP"));
    #endif
    next_state = SM_CONFIG_CHECK_FW_UPDATE; //SM_CONFIG_DISABLE_WIFI; //default next state
    //
    // should get realtime-flag, start/stop-time, rssi-thresholds
    // if we could not get configuration, then shift previous schedule by 24 hours
    //
    kaidu_scanner_get_config(); // get mqtt-device-id
    if (nrf_detected) {
      kaidu_send_http_telemetry("BOOT");
    } else {
      kaidu_send_http_telemetry("BOOT-NRF-ERROR");
    }
    sm_general_flag = kaidu_scanner_get_setup();  // get realtime-flag, start/stop timestamp, far/near rssi
    calculate_working_schedule(timenow);
    //
    if (!kaidu_rx_last_http_status()) {
      ble_server_update_manufacture_data_device_status(BLE_SERVER_DEVICE_STATE_HTTP_ERROR, (int)wifi_get_avg_wifi_rssi());
      next_state = SM_CONFIG_DISABLE_WIFI;
      
    } else {
      //
      sm_general_flag = kaidu_process_command();    // get kaidu-commands
      //
      while (sm_general_flag) {
        if (kaidu_rx_reset_cmd()) {
          next_state = SM_NRF_RESET;
          sm_general_flag = false;
          
        } else {
          sm_general_flag = kaidu_process_command();
          next_state = SM_CONFIG_CHECK_FW_UPDATE; //SM_CONFIG_DISABLE_WIFI; // go here if we break from loop
        }
      }
    }
  }

  else if (current_state == SM_CONFIG_CHECK_FW_UPDATE) {
    //
    // first check for firmware updates, and reboot after updates
    //
    // check both ESP and NRF
    #ifdef KAIDU_SHOW_LOGS
    Serial.println(F("SM_CONFIG_CHECK_FW_UPDATE"));
    #endif
    // note: use flag to store firmware check api call status
    //       both calls must succeed for state-machine to go on
    sm_general_flag = kaidu_check_firmware_update();
    
    #ifdef KAIDU_SHOW_WARNING_LOGS
    if (sm_general_flag == false) Serial.println(F("SM_CONFIG_CHECK_FW_UPDATE api fail!"));
    #endif

    next_state = SM_CONFIG_DISABLE_WIFI;
  }

  else if (current_state == SM_CONFIG_DISABLE_WIFI) {
    #ifdef KAIDU_SHOW_LOGS
    Serial.println(F("SM_CONFIG_DISABLE_WIFI"));
    #endif
    //
    // turn off wifi to save power, reduces heap, and potential memory leaks
    //
    if (sm_general_flag) {
      // if all is well, send a telemetry msg before disconnecting wifi
      kaidu_send_http_telemetry("TELE");
    } else {
      kaidu_send_http_telemetry("TELE-ERROR");
    }
    wifi_disconnect();
    //
    
    if (ble_server_get_connection_status()) {
      next_state = SM_ENABLE_BLE_CONFIG;
      
    } else {
      //
      #ifdef KAIDU_SHOW_HEAP_LOGS
      Serial.print(F("Free heap ")); Serial.println(ESP.getFreeHeap());
      #endif
      //
      if (timenow > WIFI_TIME_MIN_START && scanner_start_sec > WIFI_TIME_MIN_START) { // Oct 1, 2023
        // got valid setup
        if (timenow < scanner_upload_sec) {
          // go and wait to start ble scanning
          next_state = SM_CONFIG_WAIT_TO_START;
          #ifdef KAIDU_SHOW_LOGS
          Serial.println(F("SM_CONFIG_WAIT_TO_START"));
          #endif
          
        } else if (timenow < scanner_midnight_sec) {
          // wait till midnight
          next_state = SM_CONFIG_WAIT_TO_START;
          
        } else {
          // should not get here if config is proper
          // just go pause for 10 mins and re-read config
          next_state = SM_ESP_RESET;
        }
        
      } else {
        led_blink_fast(LED_BLUE_GREEN, true);
        next_state = get_next_state(SM_WAIT_FOR_APP_CONFIG);
        sm_timer_end_millis = millisnow + WAIT_FOR_CONFIGURATION_TIMEOUT_MILLIS; //3mins
      }
        
      // stop ble-advert
      //ble_server_stop();
    }
  }

  else if (current_state == SM_CONFIG_WAIT_TO_START) {
    //
    // wait here until its time to start ble scanning
    // TODO- if short sleep, do it here, otherwise go to NRF+ESP sleep states, which leads to reset on wake
    //
    short_button_press_count = 0; // allow more button presses
    
    // loop forever
    next_state = get_next_state(SM_CONFIG_WAIT_TO_START);
  }

}
