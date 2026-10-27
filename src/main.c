/*******************************************************************************
 * Greenhouse Climate Controller FSM - PIC18F45K22
 *
 * Materia: Electrónica Integrada
 * Microcontrolador: PIC18F45K22
 * Compilador: MPLAB XC8
 * Simulación: PROTEUS
 * Autor: Luis Alexis Calvillo Deveze
 * Fecha: 2026
 *
 * Descripción:
 * Sistema de control de clima para invernadero usando FSM de 5 estados.
 * Regula temperatura (LM35 + ADC) y humedad (SHT21 opcional por I²C).
 * Controla ventilador y calefactor por PWM (CCP1/CCP2).
 * Interfaz: LCD 2x16, teclado matricial 4x4, UART para reporte.
 *******************************************************************************/

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// CONFIGURATION BITS
#pragma config FOSC = INTIO67   // Internal oscillator block
#pragma config PLLCFG = OFF     // 4x PLL disabled
#pragma config PRICLKEN = ON    // Primary clock enabled
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor disabled
#pragma config IESO = OFF       // Internal/External Switchover disabled
#pragma config PWRTEN = ON      // Power-up Timer enabled
#pragma config BOREN = ON       // Brown-out Reset enabled
#pragma config BORV = 190       // VBOR set to 1.90 V
#pragma config WDTEN = OFF      // Watchdog Timer disabled
#pragma config WDTPS = 32768    // Watchdog Timer Postscale
#pragma config CCP2MX = PORTC1  // CCP2 input/output on RC1
#pragma config PBADEN = OFF     // PORTB<5:0> configured as digital I/O on Reset
#pragma config CCP3MX = PORTB5  // CCP3 on RB5
#pragma config HFOFST = ON      // HFINTOSC output and ready status not delayed
#pragma config T3CMX = PORTC0   // T3CKI on RC0
#pragma config P2BMX = PORTB5   // P2B on RB5
#pragma config MCLRE = EXTMCLR  // MCLR pin enabled, RE3 input disabled
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset enabled
#pragma config LVP = OFF        // Single-Supply ICSP disabled
#pragma config XINST = OFF      // Instruction set extension disabled
#pragma config DEBUG = OFF      // Background debugger disabled
#pragma config CP0 = OFF        // Code protection disabled
#pragma config CP1 = OFF
#pragma config CP2 = OFF
#pragma config CP3 = OFF
#pragma config CPB = OFF
#pragma config CPD = OFF
#pragma config WRT0 = OFF       // Write protection disabled
#pragma config WRT1 = OFF
#pragma config WRT2 = OFF
#pragma config WRT3 = OFF
#pragma config WRTC = OFF
#pragma config WRTB = OFF
#pragma config WRTD = OFF
#pragma config EBTR0 = OFF      // Table read protection disabled
#pragma config EBTR1 = OFF
#pragma config EBTR2 = OFF
#pragma config EBTR3 = OFF
#pragma config EBTRB = OFF

#define _XTAL_FREQ 8000000  // 8 MHz internal oscillator

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define LM35_ADC_CHANNEL     0   // AN0 - RA0
#define FAN_PWM_PIN          RC1 // CCP2
#define HEATER_PWM_PIN       RC2 // CCP1
#define SHT21_SDA            RC3 // I2C SDA
#define SHT21_SCL            RC4 // I2C SCL
#define LCD_RS               RB0
#define LCD_EN               RB1
#define LCD_D4               RB2
#define LCD_D5               RB3
#define LCD_D6               RB4
#define LCD_D7               RB5
#define KEYPAD_ROW_PORT      PORTD
#define KEYPAD_COL_PORT      PORTD

// Keypad pins (4x4 matrix on PORTD)
// Rows: RD0-RD3 (outputs), Cols: RD4-RD7 (inputs with pull-up)

// ============================================================================
// FSM STATES
// ============================================================================
typedef enum {
    STATE_IDLE       = 0,  // Waiting for config
    STATE_MONITOR    = 1,  // Normal monitoring
    STATE_HEATING    = 2,  // Heating active
    STATE_COOLING    = 3,  // Cooling (fan) active
    STATE_ALARM      = 4   // Temperature out of bounds
} system_state_t;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
volatile system_state_t current_state = STATE_IDLE;
volatile float temperature_c = 0.0;
volatile float humidity_pct = 0.0;
volatile float temp_min = 18.0;
volatile float temp_max = 28.0;
volatile bool sht21_present = false;
volatile uint16_t adc_result = 0;
volatile bool uart_tx_ready = true;
volatile char uart_buffer[64];
volatile uint8_t uart_idx = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void system_init(void);
void oscillator_init(void);
void adc_init(void);
void pwm_init(void);
void uart_init(void);
void i2c_init(void);
void lcd_init(void);
void keypad_init(void);
void timer0_init(void);
void interrupt_init(void);

float read_temperature_lm35(void);
bool read_humidity_sht21(float *humidity);
void update_outputs(void);
void handle_keypad(void);
void lcd_update(void);
void uart_send_report(void);
void state_machine(void);

// LCD functions
void lcd_cmd(uint8_t cmd);
void lcd_data(uint8_t data);
void lcd_write_string(const char *str);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_clear(void);

// Keypad functions
char keypad_getkey(void);

// UART functions
void uart_write_char(char c);
void uart_write_string(const char *str);

// I2C functions
void i2c_start(void);
void i2c_stop(void);
void i2c_write(uint8_t data);
uint8_t i2c_read(bool ack);

// ============================================================================
// MAIN
// ============================================================================
void main(void) {
    system_init();

    // Initial LCD message
    lcd_clear();
    lcd_write_string("Invernadero FSM");
    lcd_set_cursor(1, 0);
    lcd_write_string("Iniciando...");
    __delay_ms(2000);

    while (1) {
        // Read sensors
        temperature_c = read_temperature_lm35();
        if (sht21_present) {
            read_humidity_sht21(&humidity_pct);
        }

        // Handle user input
        handle_keypad();

        // Run state machine
        state_machine();

        // Update outputs
        update_outputs();

        // Update display
        lcd_update();

        // Send UART report periodically
        static uint16_t uart_counter = 0;
        if (++uart_counter >= 5000) {  // ~every 500ms at 8MHz
            uart_counter = 0;
            uart_send_report();
        }

        __delay_ms(10);  // Small delay for stability
    }
}

// ============================================================================
// INTERRUPT SERVICE ROUTINE
// ============================================================================
void __interrupt() isr(void) {
    // ADC interrupt
    if (PIR1bits.ADIF) {
        adc_result = ((uint16_t)ADRESH << 8) | ADRESL;
        PIR1bits.ADIF = 0;
    }

    // UART TX interrupt
    if (PIR1bits.TXIF && uart_idx < sizeof(uart_buffer)) {
        TXREG = uart_buffer[uart_idx++];
        if (uart_buffer[uart_idx] == '\0') {
            uart_idx = 0;
            PIE1bits.TXIE = 0;
        }
    }

    // Timer0 interrupt for periodic tasks
    if (INTCONbits.TMR0IF) {
        TMR0H = 0x0B;  // Reload for ~10ms @ 8MHz / 256 prescaler
        TMR0L = 0xDC;
        INTCONbits.TMR0IF = 0;
    }
}

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================
void system_init(void) {
    oscillator_init();
    adc_init();
    pwm_init();
    uart_init();
    i2c_init();
    lcd_init();
    keypad_init();
    timer0_init();
    interrupt_init();

    // Check SHT21 presence
    i2c_start();
    i2c_write(0x80);  // SHT21 address + write
    sht21_present = !SSP1CON2bits.ACKSTAT;
    i2c_stop();
}

void oscillator_init(void) {
    OSCCON = 0x70;  // 8 MHz HFINTOSC
    OSCTUNEbits.PLLEN = 0;  // PLL disabled
    while (!OSCCONbits.HFIOFS);  // Wait for stable
}

void adc_init(void) {
    ADCON0 = 0x00;  // Channel 0 (AN0), ADON=0
    ADCON1 = 0x00;  // Vref+ = VDD, Vref- = VSS
    ADCON2 = 0x8A;  // Right justified, 12 TAD, FOSC/32
    TRISAbits.TRISA0 = 1;  // AN0 as input
    ANSELAbits.ANSA0 = 1;  // Analog input
    ADCON0bits.ADON = 1;   // Enable ADC
    PIE1bits.ADIE = 1;     // Enable ADC interrupt
}

void pwm_init(void) {
    // CCP1 (heater) and CCP2 (fan) in PWM mode
    TRISCbits.TRISC1 = 0;  // CCP2 output
    TRISCbits.TRISC2 = 0;  // CCP1 output

    // Timer2 for PWM period
    PR2 = 0xFF;  // Period = (PR2+1)*4*Tosc*TMR2prescale = 256*4*125ns*16 = ~20.5kHz
    T2CON = 0x06;  // Timer2 ON, prescaler 1:16

    // CCP1 PWM
    CCP1CON = 0x0C;  // PWM mode
    CCPR1L = 0x00;   // 0% duty initially
    CCP1CONbits.DC1B = 0;

    // CCP2 PWM
    CCP2CON = 0x0C;  // PWM mode
    CCPR2L = 0x00;   // 0% duty initially
    CCP2CONbits.DC2B = 0;
}

void uart_init(void) {
    TRISCbits.TRISC6 = 0;  // TX output
    TRISCbits.TRISC7 = 1;  // RX input
    TXSTA1 = 0x24;         // TX enabled, async, high speed
    RCSTA1 = 0x90;         // Serial port enabled, continuous receive
    BAUDCON1 = 0x08;       // BRG16 = 1
    SPBRG1 = 51;           // 9600 baud @ 8MHz (high speed)
    PIE1bits.RCIE = 1;     // Enable RX interrupt
}

void i2c_init(void) {
    TRISCbits.TRISC3 = 1;  // SDA
    TRISCbits.TRISC4 = 1;  // SCL
    SSP1CON1 = 0x28;       // I2C Master mode, clock = FOSC/(4*(SSPADD+1))
    SSP1ADD = 0x13;        // 100kHz @ 8MHz
    SSP1STAT = 0x00;
}

void lcd_init(void) {
    TRISB &= 0xC0;  // RB0-RB5 as outputs
    LATB &= 0xC0;
    __delay_ms(20);

    lcd_cmd(0x33); __delay_ms(5);
    lcd_cmd(0x32); __delay_ms(5);
    lcd_cmd(0x28); __delay_ms(1);  // 4-bit, 2 lines
    lcd_cmd(0x0C); __delay_ms(1);  // Display on, cursor off
    lcd_cmd(0x06); __delay_ms(1);  // Entry mode
    lcd_clear();
}

void keypad_init(void) {
    // RD0-RD3 as outputs (rows), RD4-RD7 as inputs (cols)
    TRISD = 0xF0;  // Upper nibble input, lower output
    PORTD = 0xF0;  // Enable pull-ups on columns
    // Enable weak pull-ups on PORTD
    WPUD = 0xF0;
    INTCON2bits.nRBPU = 0;  // Enable PORTB pull-ups (also affects PORTD on some devices)
}

void timer0_init(void) {
    T0CON = 0x87;  // Timer0 ON, 16-bit, internal clock, prescaler 1:256
    TMR0H = 0x0B;
    TMR0L = 0xDC;  // ~10ms reload
    INTCONbits.TMR0IE = 1;
}

void interrupt_init(void) {
    INTCONbits.GIE = 1;   // Global interrupts
    INTCONbits.PEIE = 1;  // Peripheral interrupts
}

// ============================================================================
// SENSOR FUNCTIONS
// ============================================================================
float read_temperature_lm35(void) {
    ADCON0bits.CHS = LM35_ADC_CHANNEL;
    __delay_us(5);  // Acquisition time
    ADCON0bits.GO = 1;
    while (ADCON0bits.GO);  // Wait for conversion

    // LM35: 10mV/°C, ADC: 10-bit, Vref = 5V
    // Temperature = (ADC * 5.0 / 1024) / 0.01
    float voltage = (adc_result * 5.0) / 1024.0;
    return voltage / 0.01;
}

bool read_humidity_sht21(float *humidity) {
    if (!sht21_present) return false;

    // Trigger humidity measurement (no hold master mode)
    i2c_start();
    i2c_write(0x80);  // SHT21 address + write
    if (SSP1CON2bits.ACKSTAT) { i2c_stop(); return false; }
    i2c_write(0xF5);  // Measure humidity command
    i2c_stop();

    __delay_ms(30);  // Measurement time

    // Read result
    i2c_start();
    i2c_write(0x81);  // SHT21 address + read
    if (SSP1CON2bits.ACKSTAT) { i2c_stop(); return false; }
    uint16_t raw = (i2c_read(true) << 8) | i2c_read(false);
    i2c_stop();

    // Convert: RH = -6 + 125 * (raw / 65536)
    *humidity = -6.0 + 125.0 * (raw / 65536.0);
    return true;
}

// ============================================================================
// STATE MACHINE
// ============================================================================
void state_machine(void) {
    switch (current_state) {
        case STATE_IDLE:
            // Wait for valid temperature range config
            if (temp_min < temp_max && temp_max > temp_min) {
                current_state = STATE_MONITOR;
            }
            break;

        case STATE_MONITOR:
            if (temperature_c < temp_min) {
                current_state = STATE_HEATING;
            } else if (temperature_c > temp_max) {
                current_state = STATE_COOLING;
            }
            break;

        case STATE_HEATING:
            if (temperature_c >= temp_min) {
                current_state = STATE_MONITOR;
            } else if (temperature_c < (temp_min - 5.0)) {
                current_state = STATE_ALARM;
            }
            break;

        case STATE_COOLING:
            if (temperature_c <= temp_max) {
                current_state = STATE_MONITOR;
            } else if (temperature_c > (temp_max + 5.0)) {
                current_state = STATE_ALARM;
            }
            break;

        case STATE_ALARM:
            if (temperature_c >= temp_min && temperature_c <= temp_max) {
                current_state = STATE_MONITOR;
            }
            break;
    }
}

// ============================================================================
// OUTPUT CONTROL
// ============================================================================
void update_outputs(void) {
    uint16_t heater_duty = 0;
    uint16_t fan_duty = 0;

    switch (current_state) {
        case STATE_HEATING:
            // Proportional heating
            heater_duty = (uint16_t)((temp_min - temperature_c) / temp_min * 1023);
            if (heater_duty > 1023) heater_duty = 1023;
            break;

        case STATE_COOLING:
            // Proportional cooling
            fan_duty = (uint16_t)((temperature_c - temp_max) / temp_max * 1023);
            if (fan_duty > 1023) fan_duty = 1023;
            break;

        case STATE_ALARM:
            // Max cooling + alarm
            fan_duty = 1023;
            break;
    }

    // CCP1 (heater) - 10-bit PWM
    CCPR1L = (heater_duty >> 2) & 0xFF;
    CCP1CONbits.DC1B = heater_duty & 0x03;

    // CCP2 (fan) - 10-bit PWM
    CCPR2L = (fan_duty >> 2) & 0xFF;
    CCP2CONbits.DC2B = fan_duty & 0x03;
}

// ============================================================================
// KEYPAD HANDLING
// ============================================================================
void handle_keypad(void) {
    char key = keypad_getkey();
    if (key == 0) return;

    static char input_buffer[10];
    static uint8_t input_len = 0;
    static bool editing_min = false;
    static bool editing_max = false;

    switch (key) {
        case 'A':  // Edit min temp
            editing_min = true;
            editing_max = false;
            input_len = 0;
            lcd_clear();
            lcd_write_string("Temp MIN: ");
            break;

        case 'B':  // Edit max temp
            editing_max = true;
            editing_min = false;
            input_len = 0;
            lcd_clear();
            lcd_write_string("Temp MAX: ");
            break;

        case 'C':  // Confirm
            if (editing_min || editing_max) {
                input_buffer[input_len] = '\0';
                float value = atof(input_buffer);
                if (editing_min) temp_min = value;
                if (editing_max) temp_max = value;
                editing_min = editing_max = false;
                input_len = 0;
                lcd_clear();
            }
            break;

        case 'D':  // Cancel
            editing_min = editing_max = false;
            input_len = 0;
            lcd_clear();
            break;

        default:  // Numeric input
            if ((editing_min || editing_max) && input_len < 9) {
                if ((key >= '0' && key <= '9') || key == '.') {
                    input_buffer[input_len++] = key;
                    lcd_data(key);
                }
            }
            break;
    }
}

char keypad_getkey(void) {
    const char keys[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    for (uint8_t row = 0; row < 4; row++) {
        // Set current row low, others high
        LATD = ~(1 << row) & 0x0F;
        __delay_us(10);

        uint8_t cols = PORTD >> 4;
        for (uint8_t col = 0; col < 4; col++) {
            if (!(cols & (1 << col))) {
                __delay_ms(20);  // Debounce
                while (!(PORTD & (1 << (col + 4))));  // Wait for release
                return keys[row][col];
            }
        }
    }
    return 0;
}

// ============================================================================
// LCD FUNCTIONS
// ============================================================================
void lcd_cmd(uint8_t cmd) {
    LATBbits.LATB0 = 0;  // RS = 0
    LATB = (LATB & 0xC3) | ((cmd & 0xF0) >> 2);  // Upper nibble
    LATBbits.LATB1 = 1;  // EN = 1
    __delay_us(1);
    LATBbits.LATB1 = 0;  // EN = 0
    __delay_us(1);

    LATB = (LATB & 0xC3) | ((cmd & 0x0F) << 2);  // Lower nibble
    LATBbits.LATB1 = 1;
    __delay_us(1);
    LATBbits.LATB1 = 0;
    __delay_ms(2);
}

void lcd_data(uint8_t data) {
    LATBbits.LATB0 = 1;  // RS = 1
    LATB = (LATB & 0xC3) | ((data & 0xF0) >> 2);
    LATBbits.LATB1 = 1;
    __delay_us(1);
    LATBbits.LATB1 = 0;
    __delay_us(1);

    LATB = (LATB & 0xC3) | ((data & 0x0F) << 2);
    LATBbits.LATB1 = 1;
    __delay_us(1);
    LATBbits.LATB1 = 0;
    __delay_ms(2);
}

void lcd_write_string(const char *str) {
    while (*str) lcd_data(*str++);
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
    uint8_t addr = (row == 0) ? 0x80 : 0xC0;
    lcd_cmd(addr + col);
}

void lcd_clear(void) {
    lcd_cmd(0x01);
    __delay_ms(2);
}

void lcd_update(void) {
    static char line1[17], line2[17];

    lcd_set_cursor(0, 0);
    sprintf(line1, "T:%.1fC  H:%.0f%%", temperature_c, humidity_pct);
    lcd_write_string(line1);

    lcd_set_cursor(1, 0);
    const char *state_str[] = {"IDLE", "MONITOR", "HEATING", "COOLING", "ALARM"};
    sprintf(line2, "%s Min:%.0f Max:%.0f", state_str[current_state], temp_min, temp_max);
    lcd_write_string(line2);
}

// ============================================================================
// UART FUNCTIONS
// ============================================================================
void uart_write_char(char c) {
    while (!TXSTA1bits.TRMT);
    TXREG = c;
}

void uart_write_string(const char *str) {
    while (*str) uart_write_char(*str++);
}

void uart_send_report(void) {
    char report[128];
    sprintf(report, "T:%.1f,H:%.1f,S:%d,Min:%.1f,Max:%.1f\n",
            temperature_c, humidity_pct, current_state, temp_min, temp_max);
    uart_write_string(report);
}

// ============================================================================
// I2C FUNCTIONS
// ============================================================================
void i2c_start(void) {
    SSP1CON2bits.SEN = 1;
    while (SSP1CON2bits.SEN);
    PIR1bits.SSP1IF = 0;
}

void i2c_stop(void) {
    SSP1CON2bits.PEN = 1;
    while (SSP1CON2bits.PEN);
    PIR1bits.SSP1IF = 0;
}

void i2c_write(uint8_t data) {
    SSP1BUF = data;
    while (!PIR1bits.SSP1IF);
    PIR1bits.SSP1IF = 0;
}

uint8_t i2c_read(bool ack) {
    SSP1CON2bits.RCEN = 1;
    while (!PIR1bits.SSP1IF);
    PIR1bits.SSP1IF = 0;
    uint8_t data = SSP1BUF;
    SSP1CON2bits.ACKDT = ack ? 0 : 1;
    SSP1CON2bits.ACKEN = 1;
    while (SSP1CON2bits.ACKEN);
    return data;
}