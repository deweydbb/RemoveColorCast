
#ifndef COLORCAST_BYTEORDERING_H
#define COLORCAST_BYTEORDERING_H

// returns true if the first two bytes of a file
// match the format of a little endian tiff file
int isLittleEndian(unsigned char* data);

// returns an unsigned integer given its start location in the data, how many bytes long the int is
// and the ordering of the bytes
unsigned int getInt(unsigned int start, unsigned int howManyBytes, unsigned char* data, int isLittleEndian);

// given an int, returns an array of unsigned chars in the correct order based on the
// given byte ordering convention
unsigned char* getByteOrderFromInt(int input, int numBytes, int isLittleEndian);

#endif //COLORCAST_BYTEORDERING_H
