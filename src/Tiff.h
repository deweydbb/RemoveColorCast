#include "ByteOrdering.h"
#include "DirEntry.h"

#ifndef COLORCAST_TIFF_H
#define COLORCAST_TIFF_H

typedef struct {
    int isLittle;
    unsigned int bitsPerSample;
    unsigned int numEntries;
    DirEntry *entries;
    unsigned long dataLen;
    unsigned char *data;
    unsigned int numStrips;
    unsigned int *bytesPerStrip;
    unsigned int *stripOffsets;
} Tiff;

Tiff *openTiff(char *path);

int isValidTiff(Tiff *tiff);

unsigned long getWidth(Tiff *tiff);

unsigned long getHeight(Tiff *tiff);

unsigned long getPixelStartOffset(Tiff *tiff);

int *getPixel(Tiff *tiff, unsigned long pixIndex, unsigned long startOffset);

void setPixel(Tiff *tiff, int *rgb, unsigned long pixIndex, unsigned long startOffset);

void writeTiff(Tiff *tiff, char *path);

#endif //COLORCAST_TIFF_H
