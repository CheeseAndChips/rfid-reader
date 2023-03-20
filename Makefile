SRC = main.c
OBJ = main.o
TARGET = main
HEXFILE = main.hex
CC = avr-gcc
MMCU = atmega2560 
FREQ = 16000000UL
FLAGS = -O3 -Wall

all: $(HEXFILE)

$(OBJ): $(SRC)
	$(CC) $(FLAGS) -DF_CPU=$(FREQ) -mmcu=$(MMCU) -c -o $(OBJ) $(SRC)

$(TARGET): $(OBJ)
	$(CC) -mmcu=$(MMCU) $(OBJ) -o $(TARGET)

$(HEXFILE): $(TARGET)
	avr-objcopy -O ihex -R .eeprom $(TARGET) $(HEXFILE)

.PHONY: program
program: $(HEXFILE)
	avrdude -cwiring -v -patmega2560 -P/dev/ttyACM0 -b 115200 -D -Uflash:w:$(HEXFILE)

.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET) $(HEXFILE)
