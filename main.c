#include <avr/io.h>
#include <string.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define BAUD 9600

#ifndef F_CPU
#error "No F_CPU definition"
#endif
#include <util/setbaud.h>

void uart_init() {
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;

#if USE_2X
	UCSR0A |= _BV(U2X0);
#else
	UCSR0A &= ~(_BV(U2X0));
#endif

	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
    UCSR0B = _BV(RXEN0) | _BV(TXEN0);
}

void uart_putchar(char c) {
   loop_until_bit_is_set(UCSR0A, UDRE0);
   UDR0 = c;
}

inline uint16_t min(int16_t a, int16_t b) {
	return (a < b) ? a : b;
}

inline uint16_t max(int16_t a, int16_t b) {
	return (a > b) ? a : b;
}

#define START_CNT 12

typedef struct {
	uint16_t data[START_CNT];
	uint16_t timers[64];
	uint8_t written;
	uint8_t index;
	uint8_t success;
	uint16_t accepted_minval;
	uint16_t accepted_maxval;
	uint8_t acceptedCnt;
	uint8_t lastBit;
	uint8_t finalBytes[8];
	uint8_t bitsLeft;
	uint8_t bitsInCurrent;
	uint8_t bytesWritten;
} state_t;

volatile state_t state;

ISR(INT5_vect) {
	if(state.success == 2) return;
	if(state.success == 1) {
		uint16_t timer = TCNT1;
		TCNT1 = 0;
		if(timer > 11000) {
			state.lastBit ^= 1;
		}
		uint8_t newb = (state.finalBytes[state.bytesWritten] << 1) + state.lastBit;
		state.finalBytes[state.bytesWritten] = newb;
		state.timers[64 - 9 - state.bitsLeft--] = timer;
		if(--state.bitsInCurrent == 0) {
			state.bytesWritten++;
			state.bitsInCurrent = 8;
		}
		if(state.bitsLeft == 0) { state.success = 2; }
		return;
	} else {
		state.data[state.index] = TCNT1;
		TCNT1 = 0;
		state.index = (state.index + 1) % START_CNT;
		if(state.written < START_CNT) state.written++;
		if(state.written == START_CNT) {
			uint8_t prevIndex = state.index;
			uint16_t minVal = state.data[(prevIndex ? 0 : 1)];
			uint16_t maxVal = minVal;
			for(uint8_t i = 0; i < START_CNT; i++) {
				if(i == prevIndex)
					continue;
				uint16_t curr = state.data[i];
				minVal = min(minVal, curr);	
				maxVal = max(maxVal, curr);
			}
			if((maxVal - minVal) < 50 && state.data[prevIndex] > 10000) {
				state.success = 1;
			}
		}
	}
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

int main() {
	TCCR0A = 0b00010010;
	TCCR0B = 0b00000001;

	TCCR1A = 0;
	TCCR1B = 1;
	OCR0A = 64;
	OCR0B = 64;
	DDRG |= (1 << PB5);
	DDRE &= ~(1 << PE5);
	EICRB |= (1 << ISC51) | (1 << ISC50);
	EIMSK |= (1 << INT5);

	uart_init();
	while(1) {
		state.written = 0;
		state.index = 0;
		state.success = 0;
		state.acceptedCnt = 0;
		state.lastBit = 1;
		state.bitsLeft = 64 - 9;
		state.bytesWritten = 1;
		state.bitsInCurrent = 7;
		
		state.finalBytes[0] = 0xff;
		state.finalBytes[1] = 0x1;
		for(int i = 2; i < 8; i++) {
			state.finalBytes[i] = 0;
		}
		sei();
		while(state.success != 2)
			;

		cli();
		uart_putstring("Start timings:\r\n");
		uart_putint(state.index);
		uart_putstring("\r\n");
		for(uint8_t i = 0; i < START_CNT; i++) {
			uart_putint(state.data[i]);
			if(i < START_CNT - 1)
				uart_putstring(", ");
		}
		uart_putstring("\r\n");
		uart_putstring("Final data:\r\n");
		for(uint8_t i = 0; i < 8; i++) {
			uart_putstring("0x");
			uart_puthexbyte(state.finalBytes[i]);
			if(i < 7)
				uart_putstring(", ");
		}
		uart_putstring("\r\n");
		for(uint8_t i = 0; i < 64 - 9; i++) {
			uart_putint(state.timers[i]);
			uart_putstring(", ");
		}
		uart_putstring("\r\n");
		_delay_ms(100);
	}
}
