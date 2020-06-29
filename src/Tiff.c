#include <stdio.h>
#include <stdlib.h>
#include "Tiff.h"

extern const int NUM_CHANNELS;

// returns true if the file has the tif magic number
int isTiffNum(Tiff *tiff) {
    return getInt(2, 2, tiff->data, tiff->isLittle) == 42;
}

// sets the Directory entries to the given tiff struct
void setEntries(Tiff *tiff) {
    // get pointer to IFD
    unsigned int pointer = getInt(4, 4, tiff->data, tiff->isLittle);
    // get number of directory entries
    tiff->numEntries = getInt(pointer, 2, tiff->data, tiff->isLittle);
    unsigned int startOfEntries = pointer + 2;
    tiff->entries = (DirEntry *) malloc(tiff->numEntries * sizeof(DirEntry));
    // get every directory entry from ifd
    for (int i = 0; i < tiff->numEntries; i++) {
        tiff->entries[i] = getDirEntry(tiff->data, startOfEntries + (i * 12), tiff->isLittle);
    }
}

// returns the bits per sample of the tiff (typically 8 or 16 bit)
// returns 0 if there are more than 3 channels per pixel or if
// bits per sample is not the same
unsigned int getBitsPerSample(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];
        // tag for bits per sample
        if (entry.tag == 258) {
            // return -1 if there are not 3 channels per pixel
            if (entry.count != NUM_CHANNELS) {
                return -1;
            }

            unsigned int bits[entry.count];
            for (int j = 0; j < entry.count; j++) {
                bits[j] = getInt(entry.valueOrOffset + (j * 2), 2, tiff->data, tiff->isLittle);
                // return negative one if the bits per sample are not the same for all channels
                unsigned int test = bits[j];
                if (bits[j] != bits[0]) {
                    return -1;
                }
            }
            // return bits per channel
            return bits[0];
        }
    }

    // bits per channel data not in tif, even though it is mandatory
    // according to TIFF 6.0 specifications
    return -1;
}

// given the tiff and corresponding directory entry
// set the stripOffsets array in the tiff
void setStripOffsets(Tiff *tiff, DirEntry dirEntry) {
    tiff->stripOffsets = malloc(tiff->numStrips * sizeof(unsigned int));

    if (tiff->numStrips == 1) {
        // if num strips is 1, the only stripOffset will be stored in the
        // dirEntry and somewhere else in the file.
        tiff->stripOffsets[0] = dirEntry.valueOrOffset;
    } else {
        // if there is more than one strip, valueOrOffset is a pointer
        // to the first offset in the file
        unsigned int ptr = dirEntry.valueOrOffset;
        // get type of data for offsets. Can either be 2 or 4 byte unsigned integers
        // default is for.
        unsigned int size = 4;
        if (dirEntry.type == 3) {
            // type == 3 signifies 2 byte integer so set size to 2 bytes.
            size = 2;
        }
        // set each strip offset
        for (int stripIndex = 0; stripIndex < tiff->numStrips; stripIndex++) {
            tiff->stripOffsets[stripIndex] = getInt(ptr, size, tiff->data, tiff->isLittle);
            ptr += size;
        }
    }
}

// given the tiff and corresponding directory entry
// set the bytesPerStrip array in the tiff
void setBytesPerStrip(Tiff *tiff, DirEntry dirEntry) {
    tiff->bytesPerStrip = malloc(tiff->numStrips * sizeof(unsigned int));

    if (tiff->numStrips == 1) {
        // if num strips is 1, the only bytesPerStrip will be stored in the
        // dirEntry and somewhere else in the file.
        tiff->bytesPerStrip[0] = dirEntry.valueOrOffset;
    } else {
        // if there is more than one strip, valueOrOffset is a pointer
        // to the first offset in the file
        unsigned int ptr = dirEntry.valueOrOffset;
        // get type of data for bytesPerStrip. Can either be 2 or 4 byte unsigned integers
        // default is for.
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

// set numStrips, stripOffsets, and bytesPerStrip variables in
// the given tiff struct
void setStripValues(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry dirEntry = tiff->entries[i];
        // strip offsets tag, this if statement should always occur first
        // because tags are ordered numerically from least to greatest
        // according to TIFF 6.0 specifications.
        if (dirEntry.tag == 273) {
            tiff->numStrips = dirEntry.count;
            setStripOffsets(tiff, dirEntry);
        }
        // bytes per strip tag
        if (dirEntry.tag == 279) {
            tiff->numStrips = dirEntry.count;
            setBytesPerStrip(tiff, dirEntry);
        }
    }
}

// opens tiff file and sets all variables in
// tiff struct
Tiff *openTiff(char *path, unsigned int fileLen) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("ERROR: could not open file\n");
        return NULL;
    }

    Tiff *tiff = malloc(sizeof(Tiff));

    tiff->dataLen = fileLen;
    tiff->data = (unsigned char *) malloc(fileLen * sizeof(char));
    // read in file data into tiff->data
    fread(tiff->data, fileLen, 1, file);
    fclose(file);
    // determine whether tiff is little or big endian
    tiff->isLittle = isLittleEndian(tiff->data);
    // make sure file has tiff magic number
    if (!isTiffNum(tiff)) {
        printf("ERROR: not a tiff!\n");
        free(tiff);
        return NULL;
    }
    // setup tiff values
    setEntries(tiff);
    tiff->bitsPerSample = getBitsPerSample(tiff);
    setStripValues(tiff);

    return tiff;
}
// returns 1 if the tiff is compressed
int isCompressed(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];
        // tag number specifying compression
        if (entry.tag == 259) {
            // value of 1 represents no compression
            return entry.valueOrOffset != 1;
        }
    }

    return 1;
}

// returns true if tiff contains multiple IFDs which most likely
// means its store multiple images
int isMultiFiled(Tiff *tiff) {
    // get pointer to first ifd
    unsigned int ifdPointer = getInt(4, 4, tiff->data, tiff->isLittle);
    // get pointer at end of first IFD that either is 0 or points to the next IFD
    unsigned int endIfd = ifdPointer + 2 + (tiff->numEntries * 12);
    // get the value of the pointer at the end of the first IFD
    unsigned int nextIfdPointer = getInt(endIfd, 2, tiff->data, tiff->isLittle);
    // if nextIfdPointer is 0 then there is only one IFD in the file (excluding exit IFDs)
    return nextIfdPointer != 0;
}

// returns true if tiff stores image in RGB mode
int isRGB(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];
        // tag for PhotometricInterpretation
        if (entry.tag == 262) {
            return entry.valueOrOffset == 2;
        }
    }

    return 0;
}

// is used to determine if the tiff can be read by program
int isValidTiff(Tiff *tiff) {
    if (isCompressed(tiff)) {
        printf("ERROR: tiff is compressed\n");
        return 0;
    }

    if (isMultiFiled(tiff)) {
        printf("ERROR: tiff contains multiple Image File Directories\n"
               "meaning it probably contains multiple images\n");
        return 0;
    }

    if (!isRGB(tiff)) {
        printf("ERROR: tiff is not rgb\n");
        return 0;
    }

    if (tiff->bitsPerSample == -1) {
        printf("ERROR: not 3 channels per bit or samples per bit are not the same\n");
        return 0;
    }

    return 1;
}

// returns the width of the image
unsigned int getWidth(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];
        // tag for image width
        if (entry.tag == 256) {
            return entry.valueOrOffset;
        }
    }

    return 0;
}

// returns height of image
unsigned int getHeight(Tiff *tiff) {
    for (int i = 0; i < tiff->numEntries; i++) {
        DirEntry entry = tiff->entries[i];
        // tag for image length (height)
        if (entry.tag == 257) {
            return entry.valueOrOffset;
        }
    }

    return 0;
}

// returns and int array that store the rgb values of the pixel at the given
// starting offset and pixel index
int *getPixel(Tiff *tiff, unsigned long pixIndex, unsigned long startOffset) {
    // number of bytes per channel (rgb) either 1 for 8 bit or 2 for 16 bit
    int numBytes = tiff->bitsPerSample / 8;
    int *rgb = malloc(NUM_CHANNELS * sizeof(int));
    // get starting pointer of pixel
    unsigned long startIndex = startOffset + (pixIndex * NUM_CHANNELS * (numBytes));

    for (int i = 0; i < NUM_CHANNELS; i++) {
        unsigned int index = startIndex + (i * numBytes);
        // implicit conversion from unsigned to signed okay here because
        // make value of a 2 byte unsigned int is 65,536 while a 4 bye
        // int has a max value of 2,147,483,647
        rgb[i] = getInt(index, numBytes, tiff->data, tiff->isLittle);
    }

    return rgb;
}

// given the location of a pixel and an int array hold rgb values, set the pixel rgb
// values to the given rgb values
void setPixel(Tiff *tiff, int *rgb, unsigned long pixIndex, unsigned long startOffset) {
    // number of bytes per channel (rgb) either 1 for 8 bit or 2 for 16 bit
    int numBytes = tiff->bitsPerSample / 8;
    // get starting pointer of pixel. 3 represents the 3 channels per pixel (rgb)
    unsigned long startIndex = startOffset + (pixIndex * NUM_CHANNELS * numBytes);

    for (int i = 0; i < NUM_CHANNELS; i++) {
        unsigned int index = startIndex + (i * numBytes);
        // get the byte(s) that represent the r,g,b value in the correct order
        // according to whether tiff is little or big endian
        unsigned char *bytes = getByteOrderFromInt(rgb[i], numBytes, tiff->isLittle);
        // save bytes to correct location in tiff->data
        for (int byteIndex = 0; byteIndex < numBytes; byteIndex++) {
            tiff->data[index + byteIndex] = bytes[byteIndex];
        }

        free(bytes);
    }

    free(rgb);
}

// write the data stored in tiff struct to the output file
void writeTiff(Tiff *tiff, char *path) {
    FILE *file = fopen(path, "wb+");
    fwrite(tiff->data, sizeof(char), tiff->dataLen, file);

    fclose(file);
}





