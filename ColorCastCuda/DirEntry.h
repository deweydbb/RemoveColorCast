#ifndef COLORCAST_DIRENTRY_H
#define COLORCAST_DIRENTRY_H

typedef struct {
    unsigned int tag;
    unsigned int type;
    unsigned int count;
    unsigned int valueOrOffset;
} DirEntry;

// constructor for DirEntry, fills in all fields
DirEntry getDirEntry(unsigned char* data, unsigned int pointer, int isLittle);

#endif //COLORCAST_DIRENTRY_H
