/*************************************************************************
 * main.c
 *
 *  Created on: sep 11, 2024
 *      Author: yara osama mohtady
 *      description:digital stop_watch
 **************************************************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#define F_CPU 16000000UL // 16 MHz
#define DEBOUNCE_DELAY 30 // Delay for de_bouncing in milliseconds

//global variables
unsigned char HOURS, MINUTES, SECONDS = 0;
unsigned char count_flag = 0; // 0 for count up, 1 for count down
unsigned char reset_flag = 0; // Set to 1 to reset
unsigned char adjust_flag = 0; // set to 1 to adjust stop watch
unsigned char debounce_counter[7] = {0, 0, 0, 0, 0, 0, 0}; // Array to track de_bounce states for each button


// Initialization functions
void init_ports(void) {
    DDRA |= 0x3F; // Set PA0-PA5 as output for 7-segment displays
    DDRC |= 0x0F; // Set PC0-PC3 as output for 7-segment display data
    PORTC &= 0xF0; // Clear PC0-PC3

    DDRB &= ~(0xFF); // Set PORTB as input for buttons
    PORTB |= 0xFF; // Enable pull-up resistors on PORTB

    DDRD &= ~(1 << 2 | 1 << 3); // Set PD2 and PD3 as input for external interrupts
    DDRD |= (1 << 0 | 1 << 4 | 1 << 5); // Set PD0, PD4, PD5 as output for buzzer and status
    PORTD &= ~(1 << 4 | 1 << 5); // Clear PD4 and PD5
    PORTD |= (1 << 2); // Enable pull-up resistor on PD2
}

// Timer1 initialization
void TIMER1_INIT(void) {
    TCCR1B = (1 << CS10) | (1 << CS12) | (1 << WGM12); // CTC mode with pre_scaler 1024
    OCR1A = 15624; // Compare match value for 1s
    TCNT1 = 0;  //initialization value of timer register
    TIMSK = (1 << OCIE1A); // Enable Timer1 Compare Match A interrupt
}

// Reset interrupt initialization
void RESET_INIT(void) {
    GICR |= (1 << INT0); // Enable external interrupt INT0
    MCUCR |= (1 << ISC01); // Falling edge trigger
}

// Pause interrupt initialization
void PAUSE_INIT(void) {
    GICR |= (1 << INT1); // Enable external interrupt INT1
    MCUCR |= (1 << ISC10) | (1 << ISC11); // Rising edge trigger
}

// Resume interrupt initialization
void RESUME_INIT(void) {
    GICR |= (1 << INT2); // Enable external interrupt INT2
    MCUCSR &= ~(1 << ISC2); // Falling edge trigger
}

// Decoder control function
void display(unsigned char num) {
    PORTC = (PORTC & 0xF0) | (num & 0x0F); // Send BCD value to decoder
}

// Function to display time on 6 multiplexed 7-segment displays
void display_time(void) {
    unsigned char h10 = HOURS / 10; // Tens digit of hours
    unsigned char h1 = HOURS % 10; // Units digit of hours
    unsigned char m10 = MINUTES / 10; // Tens digit of minutes
    unsigned char m1 = MINUTES % 10; // Units digit of minutes
    unsigned char s10 = SECONDS / 10; // Tens digit of seconds
    unsigned char s1 = SECONDS % 10; // Units digit of seconds

    // Display seconds
    PORTA = 0b00100000; // Enable display 1
    display(s1); // Units digit of seconds
    _delay_ms(2);

    PORTA = 0b00010000; // Enable display 2
    display(s10); // Tens digit of seconds
    _delay_ms(2);

    // Display minutes
    PORTA = 0b00001000; // Enable display 3
    display(m1); // Units digit of minutes
    _delay_ms(2);

    PORTA = 0b00000100; // Enable display 4
    display(m10); // Tens digit of minutes
    _delay_ms(2);

    // Display hours
    PORTA = 0b00000010; // Enable display 5
    display(h1); // Units digit of hours
    _delay_ms(2);

    PORTA = 0b00000001; // Enable display 6
    display(h10); // Tens digit of hours
    _delay_ms(2);
}

// Function to reset the stop watch
void reset_stopwatch(void) {
	PORTD &= ~(1 << 0);
	count_flag=0;
    HOURS = 0;
    MINUTES = 0;
    SECONDS = 0;
    display_time();
}

// Increment time
void increment_time(void) {
    SECONDS++;
    if (SECONDS == 60) {
        SECONDS = 0;
        MINUTES++;
    }
    if (MINUTES == 60) {
        MINUTES = 0;
        HOURS++;
    }
    if (HOURS == 24) {
        reset_stopwatch();
    }
}

// Decrement time
void decrement_time(void) {
    if (SECONDS > 0) {
        SECONDS--;
    } else {
        if (MINUTES > 0) {
            SECONDS = 59;
            MINUTES--;
        } else {
            if (HOURS > 0) {
                SECONDS = 59;
                MINUTES = 59;
                HOURS--;
            } else {
                // Count down reached zero, trigger buzzer
                SECONDS = 0;
                MINUTES = 0;
                HOURS = 0;
                PORTD |= (1 << 0); // Activate buzzer on PD0
            }
        }
    }
}

// Function to stop Timer1
void stop_timer1() {
    TCCR1B &= ~(1 << CS12 | 1 << CS11 | 1 << CS10); // Clear prescaler bits
}

// Function to start Timer1
void start_timer1() {
    TCCR1B |= (1 << CS12) | (1 << CS10); // Set prescaler to 1024
}

// Functions to adjust hours, minutes, and seconds
void increment_hours() {
    if (HOURS < 23) {
        HOURS++;
    }
}

void decrement_hours() {
    if (HOURS > 0) {
        HOURS--;
    }
}

void increment_minutes() {
    if (MINUTES < 59) {
        MINUTES++;
    } else {
        MINUTES = 0;
    }
}

void decrement_minutes() {
    if (MINUTES > 0) {
        MINUTES--;
    } else {
        MINUTES = 59;
    }
}

void increment_seconds() {
    if (SECONDS < 59) {
        SECONDS++;
    } else {
        SECONDS = 0;
    }
}

void decrement_seconds() {
    if (SECONDS > 0) {
        SECONDS--;
    } else {
        SECONDS = 59;
    }
}


// Adjustment function
void adjust_counter(void) {
    // Check button 0 (decrement hours)
    if (!(PINB & (1 << 0))) {
        if (debounce_counter[0] == 0) {
            decrement_hours();
            adjust_flag = 1;
            debounce_counter[0] = DEBOUNCE_DELAY; // Set de_bounce timer
        }
    }

    // Check button 1 (increment hours)
    if (!(PINB & (1 << 1))) {
        if (debounce_counter[1] == 0) {
            increment_hours();
            adjust_flag = 1;
            debounce_counter[1] = DEBOUNCE_DELAY;
        }
    }

    // Check button 3 (decrement minutes)
    if (!(PINB & (1 << 3))) {
        if (debounce_counter[2] == 0) {
            decrement_minutes();
            adjust_flag = 1;
            debounce_counter[2] = DEBOUNCE_DELAY;
        }
    }

    // Check button 4 (increment minutes)
    if (!(PINB & (1 << 4))) {
        if (debounce_counter[3] == 0) {
            increment_minutes();
            adjust_flag = 1;
            debounce_counter[3] = DEBOUNCE_DELAY;
        }
    }

    // Check button 5 (decrement seconds)
    if (!(PINB & (1 << 5))) {
        if (debounce_counter[4] == 0) {
            decrement_seconds();
            adjust_flag = 1;
            debounce_counter[4] = DEBOUNCE_DELAY;
        }
    }

    // Check button 6 (increment seconds)
    if (!(PINB & (1 << 6))) {
        if (debounce_counter[5] == 0) {
            increment_seconds();
            adjust_flag = 1;
            debounce_counter[5] = DEBOUNCE_DELAY;
        }
    }
}

// Interrupt Service Routines
ISR(INT0_vect) {
    reset_stopwatch();
}

ISR(INT1_vect) {
    stop_timer1(); // Pause the counter
}

ISR(INT2_vect) {
    start_timer1(); // Resume the counter
}

ISR(TIMER1_COMPA_vect) {
    if (count_flag == 0) {
        increment_time(); // Count up mode
        PORTD |= (1 << 4); // Status indicator
        PORTD &= ~(1 << 5);
        display_time();
    } else {
        decrement_time(); // Count down mode
        display_time();
        PORTD &= ~(1 << 4); // Clear status indicator
        PORTD |= (1 << 5); // Status indicator
    }
}

// Main function
int main() {
    init_ports();
    reset_stopwatch();
    SREG |= (1 << 7); // Enable global interrupts
    TIMER1_INIT();
    RESET_INIT();
    PAUSE_INIT();
    RESUME_INIT();
    start_timer1();

    while (1) {
        if (!(PINB & (1 << 7))) {
            count_flag = 1; // Set flag to logic 1 to activate count down mode
        }

        adjust_counter(); // Check and adjust stop watch timer

        // Decrease de_bounce counters for each button, if non-zero
        for (int i = 0; i < 6; i++) {
            if (debounce_counter[i] > 0) {
                debounce_counter[i]--;
            }
        }

        // Display the time continuously
        display_time();
    }
}






