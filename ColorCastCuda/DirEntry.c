#include "DirEntry.h"
#include "ByteOrdering.h"

// constructor for DirEntry, fills in all fields
DirEntry getDirEntry(unsigned char* data, unsigned int pointer, int isLittle) {
    DirEntry res;

    res.tag = getInt(pointer, 2, data, isLittle);
    res.type = getInt(pointer + 2, 2, data, isLittle);
    res.count = getInt(pointer + 4, 4, data, isLittle);

    if (isLittle || res.type == 4 || res.count > 1) {
        res.valueOrOffset = getInt(pointer + 8, 4, data, isLittle);
    }
    else {
        res.valueOrOffset = getInt(pointer + 8, 2, data, isLittle);
    }

    return res;
}