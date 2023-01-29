/*
 * Author: Brett Sitzman
 *
 * Purpose: Interior climate monitoring and alarm system
 *
 * Modules/Subroutines:
 *  ISR Functions:
 *      - void isr_c0(): ISR for column 0 - allows user to input values '1', '4', and '7' from keypad
 *      - void isr_c1(): ISR for column 1 - allows user to input values '2', '5', '8', and '0' from keypad
 *      - void isr_c2(): ISR for column 2 - allows user to input values '3', '6', and '9' from keypad
 *      - void isr_c3(): ISR for column 3 - allows user to control monitor state from keypad
 *
 *  Functions to Update Monitor State:
 *      - void update_sensor(): read current climate data from DHT11 temperature & humidity sensor
 *      - void beep_and_flash(int millisec): beep the buzzer and flash the LED for millisec milliseconds
 *      - void flash(int millisec): flash the LED for millisec milliseconds
 *      - void monitor_state(): t_monitor callback, switch system to alert mode when climate is out of range
 *      - void alert(): put device into alert mode, blink LED and ring buzzer on interval
 *
 *  Functions for Getting Input:
 *      - void print_prompt(char *prompt): print the input prompt to LCD
 *      - void get_input(char *prompt, int current_stage): print the prompt and current user inputted value as the they enter it
 *      - bool validate_input(): return true if input is valid, false if it is invalid
 *
 *  Functions for Converting Values:
 *      - float toFahrenheit(int celcius): convert celcius to fahrenheit
 *      - int toCelcius(float fahrenheit): convert fahrenheit to celcius
 *
 *
 * Corresponding Assignments: Project 3
 *
 * Inputs:
 *  - DHT11: Temperature & Humidity Sensor
 *  - Matrix Keypad:
 *      - A: Confirm Entered Value for Entry
 *      - B: Switch to IDLE mode
 *      - C: Change unit of measurement
 *           Clear currnet entry in INPUT mode
        - D: Switch to INPUT mode
        - 0-9: Enter Values
        - no programmed functionality for '#' and '*'
 *
 * Ouputs:
 *  - Buzzer: rings when monitor detects climate is out of range
 *  - LED: 
 *      - lights up on keypress
 *      - flashes when monitor detects climate is out of range
 *  - LCD: 
 *      - display current climate data
 *      - prompt user for input
 *      - warn user about invalid input
 *      - notify user when unit changes
 *      - alert user when monitor detects climate is out of range
 *
 * Constraints:
 *  - Needs to help solve a problem: Food Waste Minimization
 *  - Must have at least one previous external output peripheral
 *  - Must have a new input & output perihperal
 *  - Requires a Watchdog failsafe
 *  - Needs to include a synchronization technique for critical section protection
 *  - Requires a task/thread to be intentionally incorporated
 *  - Must include at least 1 ISR
 * 
 * Sources:
 *  - UBLearns Projects/LCD Materials folder (1802.h, 1802.cpp)
 *  - UBLearns Extra Examples and References/Peripheral Resources (DHT.h, DHT.cpp)
 *  - my project 2 submission
 *  - https://os.mbed.com/docs/mbed-os/v6.15/apis/index.html
 *  - https://www.cplusplus.com/reference/string/string/
 *  - https://www.st.com/resource/en/reference_manual/dm00310109-stm32l4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
 *  - https://www.st.com/resource/en/user_manual/dm00368330-stm32-nucleo-144-boards-mb1312-stmicroelectronics.pdf
 *  - https://components101.com/sites/default/files/component_datasheet/DHT11-Temperature-Sensor.pdf
 *  - http://tinkbox.ph/sites/tinkbox.ph/files/downloads/5V_BUZZER_MODULE.pdf 
 */

#include "mbed.h"
#include "mbed_events.h"
#include "stdio.h"
#include "1802.h"
#include "DHT.h"
#include <string>

// NOTE: This program contains code from my project 2 submission. Here are the things reused:
//          - the main() function, for polling the keypad rows
//          - the ISRs, for determining which column corresponds to the user input
//          - LCD initialization

// configure inputs for keypad - each interrupt represents a column
InterruptIn c0(PC_0, PullDown); // connected to: keypad line 4
InterruptIn c1(PC_3, PullDown); // connected to: keypad line 3
InterruptIn c2(PC_1, PullDown); // connected to: keypad line 2
InterruptIn c3(PC_4, PullDown); // connected to: keypad line 1

#define BOUNCE 1000  // the amount of time we wait in order to address bounce
void isr_c0();
void isr_c1();
void isr_c2();
void isr_c3();
int row = -1;

// LCD
CSE321_LCD lcd(16, 2, LCD_5x8DOTS, PF_0, PF_1); // connected to: PF_0 - SDA, PF_1 - SCL
Thread t_lcd;                                   // this thread updates the LCD
void update_lcd();

// DHT 11
DHT11 sensor(PG_0);
int celcius;
float fahrenheit;
int humidity;

void update_sensor();

// Buzzer
DigitalOut buzzer(PC_8);

// LED
DigitalOut led(PB_8);
void flash(int millisec);

// getting input
#define MAX_INPUT 9             // the maximum number of digits a user can enter on the display
int input_stage = -1;           // flag for determining which input to get (min temp, max temp, min humidity, max humidity)
int input_len = -1;
string input_str;               // used to store the user input as it is entered
bool input_modified = false;    // flag for determining when to update the lcd to display the user input
char prompts[4][17] = {
    "Min Temperature?",
    "Max Temperature?",
    "Min Humidity?",
    "Max Humidity?"
};
void print_prompt(char *prompt);
void get_input(char *prompt, int current_stage);
bool validate_input();

// update monitor state
Thread t_monitor;               // thread for monitoring climate
void monitor_state();
void beep_and_flash(int millisec);
void alert();

// internal state variables
EventQueue queue(32 * EVENTS_EVENT_SIZE);   // allows ISR to tell polling loop to sleep in order to address bounce
#define TEMP_MIN_C 0
#define TEMP_MIN_F 32
int temp_min_c = TEMP_MIN_C;
float temp_min_f = TEMP_MIN_F; 
#define TEMP_MAX_C 50                            
#define TEMP_MAX_F 122
int temp_max_c = TEMP_MAX_C;
float temp_max_f = TEMP_MAX_F;
#define HUMIDITY_MIN 20
int humidity_min = HUMIDITY_MIN;
#define HUMIDITY_MAX 95
int humidity_max = HUMIDITY_MAX;

#define FAHRENHEIT false
#define CELCIUS true
bool unit = CELCIUS;

#define IDLE 0
#define INPUT 1
#define MONITOR 2
#define ALERT 3
int mode = IDLE;

// tools for conversion
float toFahrenheit(int celcius);
int toCelcius(float fahrenheit);

// watchdog
Watchdog &watchdog = Watchdog::get_instance();
#define TIMEOUT_MS 5000

// THREAD 1: main() - initialize program, then poll keypad indefinetly
int main() {
    // printf("------------------ Program Start --------------------\n");
    
    // configure polling for keypad
    RCC->AHB2ENR |= 0x20; // enable clock Port F
    /* 
     *  Turn on output mode for keypad using pins PF_12, PF_13, PF_14,
     *  and PF_15 by setting their corresponding MODER bits to 01
     */
    GPIOF->MODER &= ~(0xaa000000);
    GPIOF->MODER |= 0x55000000;

    // configure isr on rising signal for each keypad column
    c0.rise(&isr_c0);
    c1.rise(&isr_c1);
    c2.rise(&isr_c2);
    c3.rise(&isr_c3);

    // enable the interrupts
    c0.enable_irq();
    c1.enable_irq();
    c2.enable_irq();
    c3.enable_irq();
    
    // start the LCD
    lcd.begin();
    t_lcd.start(callback(update_lcd));

    // start the watchdog failsafe
    watchdog.start(TIMEOUT_MS);

    // start the thread that monitors the climate
    t_monitor.start(callback(monitor_state));

    while (true) {
        /*
         *  This is where we poll the keypad for inputs. We do this by enabling power one
         *  row at a time so that when we complete the circuit and cause an interrupt,
         *  we can determine which key is pressed since we know the row and column.
         */
        
        GPIOF->ODR &= 0x0;          // clear ODR - disable power to all rows
        row++;
        if (row > 3) { row = 0; }

        if (row == 0) {
            GPIOF->ODR |= 0x1000;   // power PF_12 - connected to: keypad line 8
        }
        else if (row == 1) {
            GPIOF->ODR |= 0x2000;   // power PF_13 - connected to: keypad line 7
        }
        else if (row == 2) {
            GPIOF->ODR |= 0x4000;   // power PF_14 - connected to: keypad line 6
        }
        else { //row == 3
            GPIOF->ODR |= 0x8000;   // power PF_15 - connected to: keypad line 5
        }
        
        // feed watchdog to prevent it from resetting system
        watchdog.kick();

        // This event queue was configured so the ISRs can tell the polling loop to sleep in order to address bounce
        queue.dispatch_once();
    }
    return 0;
}

// THREAD 2: callback t_lcd
void update_lcd() {
    while (true) {
        if (mode == MONITOR || mode == IDLE) {          // display climate information in MONITOR and INPUT mode
            lcd.clear();
            update_sensor();

            if (unit == CELCIUS) {     
                lcd.print("Temp (C): ");
                lcd.print(to_string(celcius).c_str());  // access critical resource
            }
            else { //unit == FAHRENHEIT
                lcd.print("Temp (F): ");
                lcd.print(to_string(fahrenheit).c_str());// access critical resource

            }
            lcd.setCursor(0, 1);
            lcd.print("Humidity: ");
            lcd.print(to_string(humidity).c_str());     // access critical resource
            
            thread_sleep_for(1000);
        }
        else if (mode == INPUT) { // prompt user for input 
            input_stage = 0;
            
            for (int current_stage = 0; current_stage < 4; current_stage++) {
                char *prompt = prompts[current_stage];
                // first we get the input using one of our 4 prompts
                get_input(prompt, current_stage);
                // now convert the user inputted string to its proper internal value
                if (current_stage == 0) { // Prompt: "Minimum Temperatrue?"
                    if (unit == CELCIUS) {
                        temp_min_c = atoi(input_str.c_str());
                        temp_min_f = toFahrenheit(temp_min_c);
                    }
                    else {
                        temp_min_f = atoi(input_str.c_str());
                        temp_min_c = toCelcius(temp_min_f);
                    }
                    
                }
                else if (current_stage == 1) { // Prompt: "Maximum Temperature?"
                    if (unit == CELCIUS) {
                        temp_max_c = atoi(input_str.c_str());
                        temp_max_f = toFahrenheit(temp_max_c);
                    }
                    else {
                        temp_max_f = atoi(input_str.c_str());
                        temp_max_c = toCelcius(temp_max_f);
                    }
                }
                else if (current_stage == 2) { // Prompt: "Minimum Humidity?"
                    humidity_min = atoi(input_str.c_str());
                }
                else if (current_stage == 3) { // Prompt: "Maximum Humidity?"
                    humidity_max = atoi(input_str.c_str());
                }
            }
            
            // printf("min temp - c: %d, f: %f\n", temp_min_c, temp_min_f);
            // printf("max temp - c: %d, f: %f\n", temp_max_c, temp_max_f);
            // printf("min humidity: %d\n", humidity_min);
            // printf("max humidity: %d\n", humidity_max);

            input_stage = -1;

            if (validate_input()) {
                mode = MONITOR;
            }
            else {
                lcd.clear();
                lcd.print("Invalid Input");
                lcd.setCursor(0, 1);
                lcd.print("Please Try Again");
                thread_sleep_for(3000);
            } 
        }
    }
}

//////////////////////////////////
//      ISR - Keypad Input      //
//////////////////////////////////

// ISR for column 0 - allows user to input values '1', '4', '7', and '*' from keypad
void isr_c0() {
    /*
     *  We only want to accept input when we are in input mode and the 
     *  input_str value isn't completely filled with user inputted numbers
     */
    if (mode == INPUT && input_len < MAX_INPUT) {
        if (row == 0) {
            input_str.append("1");
        }
        else if (row == 1) {
            input_str.append("4");
        }
        else if (row == 2) {
            input_str.append("7");
        }
        input_len++;
        input_modified = true; // flag to protect critical resource: input_str
    }
    queue.call(flash, BOUNCE);
}

// ISR for column 1 - allows user to input the values '2', '5', '8', and '0' from keypad
void isr_c1() {
    /*
     *  We only want to accept input when we are in input mode and the 
     *  input_str value isn't completely filled with user inputted numbers
     */
     if (mode == INPUT && input_len < MAX_INPUT) {
        if (row == 0) {
            input_str.append("2");
        }
        else if (row == 1) {
            input_str.append("5");
        }
        else if (row == 2) {
            input_str.append("8");
        }
        else if (row == 3) {
            input_str.append("0");
        }
        input_len++;
        input_modified = true; // flag to protect critical resource: input_str
    }
    queue.call(flash, BOUNCE);
}

// ISR for column 2 - allows user to input the values '3', '6', and '9' from keypad
void isr_c2() {
    /*
     *  We only want to accept input when we are in input mode and the 
     *  input_str value isn't completely filled with user inputted numbers
     */
    if (mode == INPUT && input_len < MAX_INPUT) {
        if (row == 0) {
            input_str.append("3");
        }
        else if (row == 1) {
            input_str.append("6");
        }
        else if (row == 2) {
            input_str.append("9");
        }
        input_len++;
        input_modified = true; // flag to protect critical resource: input_str
    }
    queue.call(flash, BOUNCE);
}

// ISR for column 3 - allows user to control timer state from keypad
void isr_c3() {   
    if (row == 0) {             // key: A
        if(input_stage != -1) { // updates flag for enters=ing current input value and progressing to the next entry
            input_stage++;
        }
    }
    else if (row == 1) {        // key: B
        if(mode != INPUT) { mode = IDLE; }
    }
    else if (row == 2) {        // key: C
        if (mode == INPUT) {    // clear current input entry
            input_str = "";
            input_len = 0;
            input_modified = true; // flag to protect critical resource: input_str
        }
        else {                  // flip unit between celcius and fahrenheit
            unit = !unit;
        } 
    }
    else if (row == 3) {        // key: D
        mode = INPUT;
    }
    queue.call(flash, BOUNCE);
}

////////////////////////////////////
//      Update Monitor State      //
////////////////////////////////////

// Purpose: update climate information, with critical section protection
void update_sensor() {
    sensor.read();
    celcius = sensor.getCelsius();
    fahrenheit = sensor.getFahrenheit();
    humidity = sensor.getHumidity();
}

// Purpose: enable buzzer and led for given number of milliseconds
void beep_and_flash(int millisec) {
    buzzer.write(1);
    led.write(1);
    thread_sleep_for(millisec);
    buzzer.write(0);
    led.write(0);
}

// Purpose: Flash LED on interval, used to light LED on keypad button press
void flash(int millisec) {
    led.write(1);
    thread_sleep_for(millisec);
    led.write(0);
}

// Thread 3: t_monitor callback
// Purpose: when the device is in monitor mode, trigger alert mode if climate data leaves the user specified range
void monitor_state() {
    while (true) {
        while (mode == MONITOR) {
            update_sensor();
            if ((unit == FAHRENHEIT && fahrenheit < temp_min_f) || (unit == CELCIUS && celcius < temp_min_c)) {
                // printf("unit: %d, f: %f < %f, c: %d < %d\n", unit, fahrenheit, temp_min_f, celcius, temp_min_c);
                lcd.clear();
                lcd.print("Temperature Too");
                lcd.setCursor(6, 1);
                lcd.print("Low");
                alert();
            }
            else if ((unit == FAHRENHEIT && fahrenheit > temp_max_f) || (unit == CELCIUS && celcius > temp_max_c)) {
                // printf("unit: %d, f: %f > %f, c: %d > %d\n", unit, fahrenheit, temp_max_f, celcius, temp_max_c);
                lcd.clear();
                lcd.print("Temperature Too");
                lcd.setCursor(6, 1);
                lcd.print("High");
                alert();
            }
            else if (humidity < humidity_min) {
                lcd.clear();
                lcd.print("Humidity Too Low");
                alert();
            }
            else if (humidity > humidity_max) {
                lcd.clear();
                lcd.print("Humidity Too");
                lcd.setCursor(6, 1);
                lcd.print("High");
                alert();
            }
        }
        thread_sleep_for(1000);
    }
}

// Purpose: ALERT mode - blink LED and buzzer on interval until B or D is pressed (or the climate data gets back in range)
void alert() {
    mode = ALERT;
    int interval = 1000;
    while (mode == ALERT) {
        beep_and_flash(interval);
        thread_sleep_for(interval);
    }

}

//////////////////////////////
//      Getting Input       //
//////////////////////////////

// Purpose: print the given prompt to the display, and set the LCD cursor to the next line
void print_prompt(char *prompt) {
    lcd.clear();
    lcd.print(prompt);
    lcd.setCursor(0, 1);
}

// Purpose: print input prompt along with the currently inputted value
void get_input(char *prompt, int current_stage) {
    print_prompt(prompt);
    input_str = "";
    input_len = 0;
    
    while(input_stage <= current_stage) {
        // Scynhronization Technique: flag based critical section protection
        // the flag input_modified is set when our ISR tells the program it detected a new input
        // it prevents us from printing the input string before it is updated with the user value
        if (input_modified) {
            print_prompt(prompt);
            lcd.print(input_str.c_str());   // critical resource: input_str
            input_modified = false;
        }
    }
}

// Purpose: ensure all entered values are valid ranges that the DHT11 is capable of sensing (0-50 celcius, 20%-95% RH) 
bool validate_input() {
    bool valid_temp_c = false;
    if (TEMP_MIN_C <= temp_min_c && temp_min_c <= temp_max_c && temp_max_c <= TEMP_MAX_C) {
        valid_temp_c = true;
    }
    bool valid_temp_f = false;
    if (TEMP_MIN_F <= temp_min_f && temp_min_f <= temp_max_f && temp_max_f <= TEMP_MAX_F) {
        valid_temp_f = true;
    }
    bool valid_humidity = false;
    if (HUMIDITY_MIN <= humidity_min && humidity_min <= humidity_max && humidity_max <= HUMIDITY_MAX) {
        valid_humidity = true;
    }
    return (valid_humidity & valid_temp_c & valid_temp_f);
}

/////////////////////////////////////
//      Tools for Conversion       //
/////////////////////////////////////

// Purpose: convert celcius to fahrenheit
float toFahrenheit(int celcius) {
    return celcius * 1.8 + 32;
}

// Purpose: convert fahrenheit to celcius
int toCelcius(float fahrenheit) {
    return (fahrenheit - 32) / 1.8;
}
