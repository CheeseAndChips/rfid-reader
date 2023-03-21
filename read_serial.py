import serial
import time
import sys

max_printed = 0
def print_returning(s):
    global max_printed
    max_printed = max(max_printed, len(s))
    toFill = max_printed - len(s)
    print('\r' + s + ' ' * toFill, end='')

def get_kb(i):
    return f'{round(i / 1024, 2)} kb'

def iterate_bytes(bt):
    for b in bt:
        for bit in range(8):
            yield (b >> (7 - bit)) & 1

if __name__ == '__main__':
    ser = serial.Serial(port='/dev/ttyACM0', baudrate=10**6, timeout=.1)
    print('Serial opened')

    data = b''
    startingString = b'Starting\r\n'
    while True:
        data += ser.read(16 * 1024)
        try:
            ind = data.index(startingString)
            data = data[ind + len(startingString):]
            break
        except ValueError:
            print_returning('Not found yet...')
        time.sleep(.1)

    try:
        while True:
            data += ser.read(1 * 1024 * 1024)
            print_returning(f'Collected: {get_kb(len(data))}') 
            time.sleep(1)
    except KeyboardInterrupt:
        print('\nStopped collecting')

    print(f'Total collected: {get_kb(len(data))}') 
    if b'Failed' in data:
        print('Found failed!!')
        sys.exit(1)

    with open(f'{int(time.time())}.dump', 'wb') as f:
        f.write(data)

