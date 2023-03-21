import matplotlib.pyplot as plt
from read_serial import iterate_bytes
import sys
import numpy as np

def check(changes, i):
    a, b = changes[i]
    if a != 1 or b < 30:
        return 0

    i += 1
    for _ in range(8):
        a, b = changes[i]
        i += 1
        if a != 0 or b > 30:
            return 0 
        a, b = changes[i]
        i += 1
        if a != 1 or b > 30:
            return 0

    return i 

def invert_num(a):
    return 1 if a == 0 else 0

def get_data(lst):
    print(len(lst))
    lst2 = lst[9:]
    nibbles = []
    for i in range(10):
        nb = lst2[:5]
        result = 0
        for i in nb:
            result ^= i
        if result:
            return None

        lst2 = lst2[5:]
        nibble = (nb[0] << 3) | (nb[1] << 2) | (nb[2] << 1) | (nb[3] << 0)

        nibbles.append(nibble)

    if lst[-1] != 0:
        return None

    retval = 0
    for nb in nibbles:
        retval = retval << 4 | nb
    return retval

def main():
    if len(sys.argv) != 2:
        print('Need 2 args')
        return
    
    with open(sys.argv[1], 'rb') as f:
        data = f.read()


    bits = [b for b in iterate_bytes(data)]
    if False:
        plt.step(.5 * np.arange(len(bits)), bits, 'r', linewidth=2, where='post')
        plt.show()
    
    changes = []
    last_change = 0
    for i, a, b in zip(range(len(bits)-1), bits[:-1], bits[1:]):
        if a != b:
            diff = i - last_change
            changes.append((b, diff))
            last_change = i

    # 1x zero -> down to up
    # 9x ones -> up to down

    # 2T: down
    # 8x T: up - down - up

    for i in range(len(changes)):
        j = check(changes, i)
        if j != 0:
            print(changes[j:j+10])
            data = [1] * 9
            curr_bit = 1
            while len(data) < 64:
                _, d = changes[j]
                j += 1
                if d > 30:
                    curr_bit ^= 1
                    data += [curr_bit]
                else:
                    data += [curr_bit]
                    if changes[j][1] > 30:
                        data = []
                        break
                    j += 1

            if data:
                val = get_data(data)
                print(val & 0xffffffff, hex(val))

if __name__ == '__main__':
    main()
