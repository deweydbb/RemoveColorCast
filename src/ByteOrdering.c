#include <stdio.h>
#include <stdlib.h>
#include "ByteOrdering.h"

const int BYTE = 8;

int isLittleEndian(unsigned char *data) {
    return (data[0] == data[1] && data[0] == 'I');
}

unsigned int getIntLittle(unsigned int start, unsigned int howManyBytes, unsigned char *data) {
    unsigned int result = 0;
    for (unsigned int i = start + howManyBytes - 1; i >= start; i--) {
        result += data[i] << ((i - start) * BYTE);
    }

    return result;
}

//TODO needs to be tested
unsigned int getIntBig(unsigned int start, unsigned int howManyBytes, unsigned char *data) {
    unsigned int result = 0;

    for (unsigned int i = start; i < start + howManyBytes; i++) {
        result += (data[i] << ((i - start) * BYTE));
    }

    return result;
}

unsigned int getInt(unsigned int start, unsigned int howManyBytes, unsigned char *data, int isLittleEndian) {
    if (isLittleEndian) {
        return getIntLittle(start, howManyBytes, data);
    }

    return getIntBig(start, howManyBytes, data);
}

unsigned char *getByteOrderFromIntLittle(int input, int numBytes) {
    unsigned char *res = malloc(numBytes * sizeof(char));

    for (int i = 0; i < numBytes; i++) {
        res[i] = input >> (i * BYTE);
    }

    return res;
}

// todo needs to be tested
unsigned char *getByteOrderFromIntBig(int input, int numBytes) {
    unsigned char *res = malloc(numBytes * sizeof(char));

    for (int i = 0; i < numBytes; i++) {
        res[numBytes - i - 1] = input >> (i * BYTE);
    }

    return res;
}

unsigned char *getByteOrderFromInt(int input, int numBytes, int isLittleEndian) {
    if (isLittleEndian) {
        return getByteOrderFromIntLittle(input, numBytes);
    }

    return getByteOrderFromIntBig(input, numBytes);
}


