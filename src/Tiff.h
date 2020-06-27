#include "ByteOrdering.h"
#include "DirEntry.h"

#ifndef COLORCAST_TIFF_H
#define COLORCAST_TIFF_H

typedef struct {
    int isLittle;
    unsigned int numEntries;
    unsigned int bitsPerSample;
    DirEntry *entries;
    unsigned long dataLen;
    unsigned char *data;
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
