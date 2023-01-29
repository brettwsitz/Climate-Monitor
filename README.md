-------------------
About
-------------------
Project Status: Completed

Project Description: This is a climate monitoring system designed for indoor use. It takes advantage of the real-time OS provided by MBED. It gets input from an externally connected keypad & a DHT11 module (temperature & humidity sensor). The current temperature and humidity is displayed on an LCD display. An LED lights up on keypress or when the monitor detects the climate is out of range. A buzzer sounds an alarm and the user is notified on the LCD when a temperature or humidity value out of the set range is detected.

Area of Application: Food Waste Minimization
  - helps reduce damages to crop due to poor growing conditions
  - increases harvest size by allowing user to adjust conditions to be desirable for production

Contribitor List: Brett Sitzman

--------------------
Features
--------------------
- Keypad as input
  - A: Confirm entered value for entry, switches device to MONITOR mode once the last entry is confirmed
  - B: switch the device to IDLE mode
  - C: change the unit of measurement between Celcius and Fahrenheit, in INPUT mode allow user to clear current entry
  - D: switch to input mode, allow user to set the temperature and humidity range
  - 0-9: in INPUT mode, allow user to enter range values

- DHT11 as input
  - Temperature & Humidity Sensor
  - read temperature between 0-50 degrees Celcius
    - error range of +- 2 degrees Celcius
  - read humidity between 20%-95% RH
    - error range of +- 5% RH

- LCD as output
  - display current temperature and humidity information
  - prompt user for input
  - alert user to invalid input
  - notify user when device detects value out of range

- LEDs as output
  -  LED lights up on keypress
  -  LED flashes on interval when monitor detects climate is out of range

--------------------
Required Materials
--------------------
- Nucleo L4R5ZI
- Micro USB Cable
- Solderless Breadboard
- Jumper Wires
- Keypad
- DHT11 - Temperatur & Humidity Sensor
- LEDs
- LCD
- Buzzer Module

--------------------
Resources and References
--------------------
- UBLearns Projects/LCD Materials folder (1802.h, 1802.cpp)
- UBLearns Extra Examples and References/Peripheral Resources (DHT.h, DHT.cpp)
- my project 2 submission
- https://os.mbed.com/docs/mbed-os/v6.15/apis/index.html
- https://www.cplusplus.com/reference/string/string/
- https://www.st.com/resource/en/reference_manual/dm00310109-stm32l4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
- https://www.st.com/resource/en/user_manual/dm00368330-stm32-nucleo-144-boards-mb1312-stmicroelectronics.pdf
- https://components101.com/sites/default/files/component_datasheet/DHT11-Temperature-Sensor.pdf
- http://tinkbox.ph/sites/tinkbox.ph/files/downloads/5V_BUZZER_MODULE.pdf 

--------------------
Getting Started
--------------------
To configure our climate monitor, we need to connect some peripherals to our board. The keypad rows (lines 5-8) are connected to PF_15, PF_14, PF_13 and PF_12 respectively (this is for polling the keypad). The columns (lines 1-4) are connected to the nucleo at pins PC_4, PC_1, PC_3 and PC_0 respectively (these are the interrupts). Our LCD is connected as follows: VCC - 3V3, GND - GND, SDA - PF_0, SCL - PF_1. The buzzer and DHT11 both share the 5V power via the breadboard. The buzzer gets signal from PC_8 and the DHT11 reads signal from PG_0. The LED is connected to PB_8.

A detailed schematic can be found in CSE321_project3_brettsit_report.pdf

--------------------
CSE321_project2_brettsit_main.cpp:
--------------------
This file defines the behavior for our nucleo board in this climate monitoring system. It allows the user to set a temperature and humidity range for it to monitor. When the program reads a value from the sensor that doesn't fall within that range, it will alert the user through the LCD, Buzzer, and LED.

----------
Things Declared
----------
Variables:
  - InterruptIn c0(PC_0, PullDown); // connected to: keypad line 4
  - InterruptIn c1(PC_3, PullDown); // connected to: keypad line 3
  - InterruptIn c2(PC_1, PullDown); // connected to: keypad line 2
  - InterruptIn c3(PC_4, PullDown); // connected to: keypad line 1
  - int row = -1;
  - CSE321_LCD lcd(16, 2, LCD_5x8DOTS, PF_0, PF_1); // connected to: PF_0 - SDA, PF_1 - SCL
  - Thread t_lcd;                                   // this thread updates the LCD
  - void update_lcd();
  - DHT11 sensor(PG_0);
  - int celcius;
  - float fahrenheit;
  - int humidity;
  - DigitalOut buzzer(PC_8);
  - DigitalOut led(PB_8);
  - int input_stage = -1;                           // flag for determining which input to get (min temp, max temp, min humidity, max humidity)
  - int input_len = -1;
  - string input_str;                               // used to store the user input as it is entered
  - bool input_modified = false;                    // flag for determining when to update the lcd to display the user input
  - char prompts[4][17] = {
    "Min Temperature?",
    "Max Temperature?",
    "Min Humidity?",
    "Max Humidity?"
    };
  - Thread t_monitor;                               // thread for monitoring climate
  - EventQueue queue(32 * EVENTS_EVENT_SIZE);       // allows ISR to tell polling loop to sleep in order to address bounce
  - int temp_min_c = TEMP_MIN_C;
  - float temp_min_f = TEMP_MIN_F; 
  - int temp_max_c = TEMP_MAX_C;
  - float temp_max_f = TEMP_MAX_F;
  - int humidity_min = HUMIDITY_MIN;
  - int humidity_max = HUMIDITY_MAX;
  - bool unit = CELCIUS;
  - int mode = IDLE;
  - Watchdog &watchdog = Watchdog::get_instance();

Macros:
  - #define MAX_INPUT 9
  - #define TEMP_MIN_C 0
  - #define TEMP_MIN_F 32
  - #define TEMP_MAX_C 50                            
  - #define TEMP_MAX_F 122
  - #define HUMIDITY_MIN 20
  - #define HUMIDITY_MAX 95
  - #define FAHRENHEIT false
  - #define CELCIUS true
  - #define IDLE 0
  - #define INPUT 1
  - #define MONITOR 2
  - #define ALERT 3
  - #define TIMEOUT_MS 5000

Functions:
  - void isr_c0();
  - void isr_c1();
  - void isr_c2();
  - void isr_c3();
  - void update_sensor();
  - void flash(int millisec);
  - void print_prompt(char *prompt);
  - void get_input(char *prompt, int current_stage);
  - bool validate_input();
  - void monitor_state();
  - void beep_and_flash(int millisec);
  - void alert();
  - float toFahrenheit(int celcius);
  - int toCelcius(float fahrenheit);

Header Files:
  - #include "mbed.h"
  - #include "mbed_events.h"
  - #include "stdio.h"
  - #include "1802.h"
  - #include "DHT.h"
  - #include &lt;string&gt;

----------
API and Built In Elements Used
----------
- MBED API
- LCD Library (1802.h, 1802.cpp)
- c++ string library
- DHT11 Library (DHT.h, DHT.cpp)

----------
Custom Functions
----------
ISR Functions:
  - void isr_c0(): ISR for column 0 - allows user to input values '1', '4', and '7' from keypad
  - void isr_c1(): ISR for column 1 - allows user to input values '2', '5', '8', and '0' from keypad
  - void isr_c2(): ISR for column 2 - allows user to input values '3', '6', and '9' from keypad
  - void isr_c3(): ISR for column 3 - allows user to control monitor state from keypad

Functions to Update Monitor State:
  - void update_sensor(): read current climate data from DHT11 temperature & humidity sensor
  - void beep_and_flash(int millisec): beep the buzzer and flash the LED for millisec milliseconds
  - void flash(int millisec): flash the LED for millisec milliseconds
  - void monitor_state(): t_monitor callback, switch system to alert mode when climate is out of range
  - void alert(): put device into alert mode, blink LED and ring buzzer on interval
 
Functions for Getting Input:
  - void print_prompt(char *prompt): print the input prompt to LCD
  - void get_input(char *prompt, int current_stage): print the prompt and current user inputted value as the they enter it
  - bool validate_input(): return true if input is valid, false if it is invalid
 
Functions for Converting Values:
  - float toFahrenheit(int celcius): convert celcius to fahrenheit
  - int toCelcius(float fahrenheit): convert fahrenheit to celcius

