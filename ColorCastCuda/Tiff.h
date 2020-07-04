#include "ByteOrdering.h"
#include "DirEntry.h"

#ifndef COLORCAST_TIFF_H
#define COLORCAST_TIFF_H

typedef struct {
    int isLittle;                   // whether tif is Little Endian or Big Endian
    unsigned int bitsPerSample;     // typically 8 or 16 bit
    unsigned int numEntries;        // number of 'tags' in IFD
    DirEntry* entries;              // each tag and its info in IFD
    unsigned long dataLen;          // size of file in bytes
    unsigned char* data;            // each byte in file
    unsigned int numStrips;         // number of strips in image
    unsigned int* bytesPerStrip;    // number of bytes in each strip
    unsigned int* stripOffsets;     // offset (pointer) of each strip in file
} Tiff;

// returns a tiff struct, returns null if cannot
// open file or file does not have tif magic number
Tiff* openTiff(char* path, unsigned int fileLen);

// determines in this program can read tif
// prints why tif is invalid
int isValidTiff(Tiff* tiff);

// returns width of image
unsigned int getWidth(Tiff* tiff);

// returns height of image
unsigned int getHeight(Tiff* tiff);

// returns an int array of length 3, representing the rgb values of a
// pixel at the given starting offset and pixel number
int* getPixel(Tiff* tiff, unsigned long pixIndex, unsigned long startOffset);

// sets the pixel at the given position to the given rgb values
void setPixel(Tiff* tiff, int* rgb, unsigned long pixIndex, unsigned long startOffset);

// writes the tiff data to the output file
void writeTiff(Tiff* tiff, char* path);

#endif //COLORCAST_TIFF_H
