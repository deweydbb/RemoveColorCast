#include <stdlib.h>
#include "ByteOrdering.h"

const int BYTE = 8;

// returns true if the first two bytes of a file
// match the format of a little endian tiff file
int isLittleEndian(unsigned char *data) {
    return (data[0] == data[1] && data[0] == 'I');
}

// returns an unsigned integer given its start location in the data, how many bytes long the int
// assumes that bytes are in little endian ordering
unsigned int getIntLittle(unsigned int start, unsigned int howManyBytes, unsigned char *data) {
    unsigned int result = 0;
    for (unsigned int i = start + howManyBytes - 1; i >= start; i--) {
        result += data[i] << ((i - start) * BYTE);
    }

    return result;
}

// returns an unsigned integer given its start location in the data, how many bytes long the int
// assumes that bytes are in big endian ordering
unsigned int getIntBig(unsigned int start, unsigned int howManyBytes, unsigned char *data) {
    unsigned int result = 0;

    for (unsigned int i = start; i < start + howManyBytes; i++) {
        unsigned int bitsToShift = (start + howManyBytes - 1 - i) * BYTE;
        result += data[i] << bitsToShift;
    }

    return result;
}

// returns an unsigned integer given its start location in the data, how many bytes long the int is
// and the ordering of the bytes. Max number of howManyBytes should be 4.
unsigned int getInt(unsigned int start, unsigned int howManyBytes, unsigned char *data, int isLittleEndian) {
    if (isLittleEndian) {
        return getIntLittle(start, howManyBytes, data);
    }

    return getIntBig(start, howManyBytes, data);
}

// given an int, returns an array of unsigned chars in the correct order
// based on little endian byte ordering
unsigned char *getByteOrderFromIntLittle(int input, int numBytes) {
    unsigned char *res = malloc(numBytes * sizeof(char));

    for (int i = 0; i < numBytes; i++) {
        res[i] = input >> (i * BYTE);
    }

    return res;
}

// given an int, returns an array of unsigned chars in the correct order
// based on bid endian byte ordering
unsigned char *getByteOrderFromIntBig(int input, int numBytes) {
    unsigned char *res = malloc(numBytes * sizeof(char));

    for (int i = 0; i < numBytes; i++) {
        res[numBytes - i - 1] = input >> (i * BYTE);
    }

    return res;
}

// given an int, returns an array of unsigned chars in the correct order based on the
// given byte ordering convention.
unsigned char *getByteOrderFromInt(int input, int numBytes, int isLittleEndian) {
    if (isLittleEndian) {
        return getByteOrderFromIntLittle(input, numBytes);
    }

    return getByteOrderFromIntBig(input, numBytes);
}


