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

void uart_putlargeint(uint32_t x) {
	char buffer[11];
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

enum state_t {
	WAITING_FOR_FIRST,
	READING_PRIMARY,
	READING_DATA,
	ASSERT_T,
	FINISHED
};

#define STORE_COUNTERS 100
volatile struct {
	uint16_t tau; // 1.5T
	uint8_t bit_cnt;
	enum state_t state;
	uint8_t full_data[7]; // 64 - 9
	uint8_t current_bit;
	uint16_t counters[STORE_COUNTERS];
	uint8_t countersFilled;
	char result[64-9];
} reading_status;

void clear_status() {
#if 0
	uart_putstring("Clearing status\r\n");
	uart_putint(reading_status.tau);
	uart_putstring("\r\n");
	uart_putint(reading_status.bit_cnt);
	uart_putstring("\r\n");
	uart_putint(reading_status.current_bit);
	uart_putstring("\r\n");
	switch (reading_status.state) {
		case WAITING_FOR_FIRST:
			uart_putstring("WAITING_FOR_FIRST\r\n");
			break;
		case READING_PRIMARY:
			uart_putstring("READING_PRIMARY\r\n");
			break;
		case READING_DATA:
			uart_putstring("READING_DATA\r\n");
			break;
		case ASSERT_T:
			uart_putstring("ASSERT_T\r\n");
			break;
		case FINISHED:
			uart_putstring("FINISHED\r\n");
			break;
	}
#endif
	reading_status.tau = 0;
	reading_status.bit_cnt = 8*2; // 8 pairs of high->low->high
	reading_status.state = WAITING_FOR_FIRST;
	reading_status.current_bit = 1;
	reading_status.countersFilled = 0;
}

void append_bit(uint8_t bit) {
	uint8_t index = reading_status.bit_cnt++ / 8;
	uint8_t oldByte = reading_status.full_data[index];
	reading_status.full_data[index] = (oldByte << 1) | bit;
}

uint8_t get_bit(uint8_t bit) {
	uint8_t byte_index = bit / 8;
	uint8_t bit_index = bit % 8;
	return (reading_status.full_data[byte_index] >> (7 - (bit_index % 8))) & 1;	
}

uint8_t parse_data(uint8_t *version, uint32_t *tag) {
	uint8_t data_nibbles[5];
	uint8_t col_parity = (get_bit(50) << 3) | (get_bit(51) << 2) | (get_bit(52) << 1) | (get_bit(53));
	if(get_bit(54)) // stop bit
		return 1;

	uint8_t filled;
	for(filled = 0; filled < 10; filled++) {
		uint8_t data = (get_bit(filled*5) << 3) | (get_bit(filled*5 + 1) << 2) | (get_bit(filled*5 + 2) << 1) | (get_bit(filled*5 + 3));
		uint8_t parity = get_bit(filled*5 + 4);
		col_parity ^= data;
		for(int i = 0; i < 4; i++) {
			parity ^= (data >> i) & 1;
		}
		if(parity)
			return 2;

		data_nibbles[filled / 2] = (data_nibbles[filled / 2] << 4) | data;
	} 
	if(col_parity)
		return 3;

	*version = data_nibbles[0];
	for(int i = 0; i < 4; i++) {
		*tag = (*tag << 8) | data_nibbles[i+1];
	}
	return 0;
}

/*
 * start by finding one long transition low->high, followed by 8x pairs of short high->low->high
 * find length of T and 2T
 * read every transition:
 * if length = T, duplicate bit and read one more transition (assert length = T)
 * if length = 2T, flip bit 
 */
ISR(INT5_vect) {
	uint16_t counter_val = TCNT1;
	TCNT1 = 0;
	if(reading_status.countersFilled < STORE_COUNTERS) {
		reading_status.counters[reading_status.countersFilled++] = counter_val;
	}
	uint8_t new_state = (PINE >> PE5) & 1;
	switch(reading_status.state) {
		case WAITING_FOR_FIRST:
			if(new_state) {
				uint16_t T = counter_val / 2;
				reading_status.tau = T + (T / 2);
				reading_status.state = READING_PRIMARY;
			}
			break;
		case READING_PRIMARY:
			reading_status.bit_cnt--;
			if(counter_val > reading_status.tau) { // error if long
				clear_status();	
				return;
			}
			if(reading_status.bit_cnt == 0) {
				reading_status.state = READING_DATA;
			}
			break;
		case READING_DATA:
			if(counter_val < reading_status.tau) {
				reading_status.state = ASSERT_T;
			} else {
				reading_status.current_bit ^= 1;
			}
			append_bit(reading_status.current_bit);
			if(reading_status.bit_cnt >= 64 - 9) {
				reading_status.state = FINISHED;
				reading_status.full_data[6] <<= 1;
			}
			break;
		case ASSERT_T:
			if(counter_val > reading_status.tau) {
				clear_status();
			} else {
				reading_status.state = READING_DATA;
			}
			break;
		case FINISHED:
			break;
	}
}

int main() {
	clear_status();

	TCCR0A = 0b00010010;
	TCCR0B = 0b00000001;
	OCR0A = 64;
	OCR0B = 64;

	TCCR1A = 0;
	TCCR1B = 1;
	DDRG |= (1 << PB5);
	DDRE &= ~(1 << PE5);

	EICRB |= (1 << ISC50); // interrupt on any change
	EIMSK |= (1 << INT5);

	uart_init();
	uart_putstring("Starting...\r\n");
	_delay_ms(1000);
	sei();
	while(1) {
		if(reading_status.state == FINISHED) {
			uart_putstring("Finished\r\nRet: ");
			uint8_t version_num;
			uint32_t tag_id;
			uint8_t retcode = parse_data(&version_num, &tag_id);
			uart_puthexbyte(retcode);
			uart_putstring("\r\n");
			if(retcode) {
				uart_putstring("Failed!\r\n");
			} else {
				uart_puthexbyte(version_num);
				uart_putstring("\r\n");
				uart_putlargeint(tag_id);
				uart_putstring("\r\n");
			}
			_delay_ms(500);
			clear_status();
		}
	}
}
