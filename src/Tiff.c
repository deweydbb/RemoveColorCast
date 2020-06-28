#include <stdio.h>
#include <stdlib.h>
#include "Tiff.h"


int isTiffNum(Tiff *tiff) {
    return getInt(2, 2, tiff->data, tiff->isLittle) == 42;
}

void setEntries(Tiff *tiff) {
    unsigned int pointer = getInt(4, 4, tiff->data, tiff->isLittle);

    tiff->numEntries = getInt(pointer, 2, tiff->data, tiff->isLittle);

    unsigned int startOfEntries = pointer + 2;
    DirEntry *entries = (DirEntry *) malloc(tiff->numEntries * sizeof(DirEntry));

    for (int i = 0; i < tiff->numEntries; i++) {
        entries[i] = getDirEntry(tiff->data, startOfEntries + (i * 12), tiff->isLittle);
    }

    tiff->entries = entries;
}

unsigned int getBitsPerSample(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];

        if (entry.tag == 258) {
            if (entry.count != 3) {
                return 0;
            }

            unsigned int bits[entry.count];
            for (int j = 0; j < entry.count; j++) {
                bits[j] = getInt(entry.valueOrOffset + (j * 2), 2, tiff->data, tiff->isLittle);
                if (bits[j] != bits[0]) {
                    return 0;
                }
            }

            return bits[0];
        }
    }
    return 0;
}

void setStripValues(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry dirEntry = tiff->entries[i];
        // strip offsets tag
        if (dirEntry.tag == 273) {
            tiff->numStrips = dirEntry.count;

            tiff->stripOffsets = malloc(tiff->numStrips * sizeof(unsigned int));

            if (tiff->numStrips == 1) {
                tiff->stripOffsets[0] = dirEntry.valueOrOffset;
            } else {
                unsigned int ptr = dirEntry.valueOrOffset;

                unsigned int size = 4;
                if (dirEntry.type == 3) {
                    size = 2;
                }

                for (int stripIndex = 0; stripIndex < tiff->numStrips; stripIndex++) {
                    tiff->stripOffsets[stripIndex] = getInt(ptr, size, tiff->data, tiff->isLittle);
                    ptr += size;
                }
            }
        }
        // rows per strip tag
        if (dirEntry.tag == 279) {
            tiff->bytesPerStrip = malloc(tiff->numStrips * sizeof(unsigned int));

            if (tiff->numStrips == 1) {
                tiff->bytesPerStrip[0] = dirEntry.valueOrOffset;
            } else {
                unsigned int ptr = dirEntry.valueOrOffset;

                unsigned int size = 4;
                if (dirEntry.type == 3) {
                    size = 2;
                }

                for (int stripIndex = 0; stripIndex < tiff->numStrips; stripIndex++) {
                    tiff->bytesPerStrip[stripIndex] = getInt(ptr, size, tiff->data, tiff->isLittle);
                    ptr += size;
                }
            }
        }
    }
}

Tiff *openTiff(char *path) {
    Tiff *tiff = malloc(sizeof(Tiff));

    FILE *fileptr;
    long filelen;

    fileptr = fopen(path, "rb");
    if (fileptr == NULL) {
        printf("could not open file\n");
        exit(1);
    }

    fseek(fileptr, 0, SEEK_END);
    filelen = ftell(fileptr);
    rewind(fileptr);

    tiff->dataLen = filelen;
    tiff->data = (unsigned char *) malloc(filelen * sizeof(char));
    fread(tiff->data, filelen, 1, fileptr);
    fclose(fileptr);

    tiff->isLittle = isLittleEndian(tiff->data);

    if (!tiff->isLittle) {
        printf("WARNING: file is big endian which has not been tested. May not work");
    }

    if (!isTiffNum(tiff)) {
        printf("not a tiff!");
        exit(1);
    }

    setEntries(tiff);
    tiff->bitsPerSample = getBitsPerSample(tiff);
    //setStripValues(tiff);

    return tiff;
}

int isCompressed(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];

        if (entry.tag == 259) {
            return entry.valueOrOffset != 1;
        }
    }

    return 1;
}

int isMultiFiled(Tiff *tiff) {
    unsigned int ifdPointer = getInt(4, 4, tiff->data, tiff->isLittle);
    unsigned int endIfd = ifdPointer + 2 + (tiff->numEntries * 12);

    unsigned int nextIfdPointer = getInt(endIfd, 2, tiff->data, tiff->isLittle);

    return nextIfdPointer != 0;
}

int isRGB(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];

        if (entry.tag == 262) {
            return entry.valueOrOffset == 2;
        }
    }

    return 0;
}

int isValidTiff(Tiff *tiff) {
    if (isCompressed(tiff)) {
        printf("tiff is compressed :( \n");
        return 0;
    }

    if (isMultiFiled(tiff)) {
        printf("tiff contains multiple ifds :( \n");
        return 0;
    }

    if (!isRGB(tiff)) {
        printf("tiff is not rgb :( \n");
        return 0;
    }

    if (tiff->bitsPerSample == 0) {
        printf("not 3 samples per bit or samples per bit are not the same :(");
        return 0;
    }

    return 1;
}

unsigned long getWidth(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];

        if (entry.tag == 256) {
            return entry.valueOrOffset;
        }
    }

    return 0;
}

unsigned long getHeight(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];

        if (entry.tag == 257) {
            return entry.valueOrOffset;
        }
    }

    return 0;
}

unsigned long getPixelStartOffset(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];

        if (entry.tag == 273) {
            return entry.valueOrOffset;
        }
    }

    return 0;
}

int *getPixel(Tiff *tiff, unsigned long pixIndex, unsigned long startOffset) {
    int numBytes = tiff->bitsPerSample / 8;
    int *rgb = malloc(3 * sizeof(int));
    unsigned long startIndex = startOffset + (pixIndex * 3 * (numBytes));

    for (int i = 0; i < 3; i++) {
        unsigned int index = startIndex + (i * numBytes);
        // implicit conversion from unsigned to signed okay here because
        // make value of a 2 byte unsigned int is 65,536 while a 4 bye
        // int has a max value of 2,147,483,647
        rgb[i] = getInt(index, numBytes, tiff->data, tiff->isLittle);
    }

    return rgb;
}

void setPixel(Tiff *tiff, int *rgb, unsigned long pixIndex, unsigned long startOffset) {
    int numBytes = tiff->bitsPerSample / 8;
    unsigned long startIndex = startOffset + (pixIndex * 3 * numBytes);

    for (int i = 0; i < 3; i++) {
        unsigned int index = startIndex + (i * numBytes);
        unsigned char *bytes = getByteOrderFromInt(rgb[i], numBytes, tiff->isLittle);

        for (int byteIndex = 0; byteIndex < numBytes; byteIndex++) {
            tiff->data[index + byteIndex] = bytes[byteIndex];
        }

        free(bytes);
    }

    free(rgb);
}

void writeTiff(Tiff *tiff, char *path) {
    FILE *file = fopen(path, "wb+");

    fwrite(tiff->data, sizeof(char), tiff->dataLen, file);

    fclose(file);
}





