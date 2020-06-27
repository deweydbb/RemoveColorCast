#include <stdio.h>
#include "ByteOrdering.h"

int isLittleEndian(unsigned char *data) {
    return (data[0] == data[1] && data[0] == 'I');
}

unsigned int getIntLittle(unsigned int start, unsigned int howManyBytes, unsigned char *data) {
    unsigned int result = 0;
    for (unsigned int i = start + howManyBytes - 1; i >= start; i--) {
        result += data[i] << ((i - start) * 8);
    }

    return result;
}

//TODO needs to be tested
unsigned int getIntBig(unsigned int start, unsigned int howManyBytes, unsigned char *data) {
    unsigned int result = 0;

    for (unsigned int i = start; i < start + howManyBytes; i++) {
        result += (data[i] << ((i - start) * 8));
    }

    return result;
}

unsigned int getInt(unsigned int start, unsigned int howManyBytes, unsigned char *data, int isLittleEndian) {
    if (isLittleEndian) {
        return getIntLittle(start, howManyBytes, data);
    }

    return getIntBig(start, howManyBytes, data);
}

