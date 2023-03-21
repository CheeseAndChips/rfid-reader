#include <avr/io.h>
#include <string.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define BAUD 1000000 

#ifndef F_CPU
#error "No F_CPU definition"
#endif
#include <util/setbaud.h>

void uart_init() {
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;

#if USE_2X
#warning "Using 2x"
	UCSR0A |= _BV(U2X0);
#else
	UCSR0A &= ~(_BV(U2X0));
#endif

    UCSR0B = _BV(TXEN0);
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

void uart_putchar(char c) {
   loop_until_bit_is_set(UCSR0A, UDRE0);
   UDR0 = c;
}

void uart_putint(uint16_t x) {
	char buffer[5];
	uint8_t filled = 0;
	while(x) {
		buffer[filled++] = x % 10;
		x /= 10;
	}
	for(uint8_t i = 0; i < filled; i++) {
		uart_putchar('0' + buffer[filled - i - 1]);
	}
	if(!filled) uart_putchar('0');
}

void uart_putstring(const char* s) {
	while(*s != '\0') {
		uart_putchar(*(s++));
	}
}

char convbyte(uint8_t x) {
	if(x < 10) return x + '0';
	else return x + 'a' - 10;
}

void uart_puthexbyte(uint8_t x) {
	uart_putchar(convbyte((x >> 4) & 0xf));
	uart_putchar(convbyte(x & 0xf));
}

volatile uint8_t outByte;
volatile uint8_t bitsWritten;
volatile uint8_t error;
volatile uint8_t inint;
ISR(TIMER1_COMPA_vect, ISR_NOBLOCK) { // timer interrupt
	if(inint)
		error = 2;
	
	if(error)
		return;

	inint = 1;
	uint8_t newb, bw = bitsWritten;
	newb = (outByte << 1) | ((PINE >> PE5) & 1);
	if(++bw == 8) {
		bw = 0;
		if(!(UCSR0A & (1 << UDRE0))) {
			error = 1;
			inint = 0;
			return;
		}
		UDR0 = newb;
	}
	outByte = newb;
	bitsWritten = bw;
	inint = 0;
}

int main() {
	outByte = 0;
	bitsWritten = 0;
	error = 0;
	inint = 0;

	TCCR0A = 0b00010010;
	TCCR0B = 0b00000001;
	OCR0A = 64;
	OCR0B = 64;

	TCCR1A = 0;
	TCCR1B = 0b1001;
	OCR1A = 200;
	TIMSK1 = 0b10;
	DDRG |= (1 << PB5);
	DDRE &= ~(1 << PE5);

	uart_init();
	uart_putstring("Starting\r\n");
	_delay_ms(4000);
	sei();
	while(1) {
		if(error) {
			uart_putstring("Failed! ");
			uart_puthexbyte(error);
			uart_putstring("\r\n");
		}
	}
}
