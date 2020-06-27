
#ifndef COLORCAST_BYTEORDERING_H
#define COLORCAST_BYTEORDERING_H

int isLittleEndian(unsigned char *data);

unsigned int getInt(unsigned int start, unsigned int howManyBytes, unsigned char *data, int isLittleEndian);

#endif //COLORCAST_BYTEORDERING_H
