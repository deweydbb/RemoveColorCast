#include <stdio.h>
#include <stdlib.h>
#include "DirEntry.h"
#include "ByteOrdering.h"

DirEntry getDirEntry(unsigned char *data, unsigned int pointer, int isLittle) {
    DirEntry res;

    res.tag = getInt(pointer, 2, data, isLittle);
    res.type = getInt(pointer + 2, 2, data, isLittle);
    res.count = getInt(pointer + 4, 4, data, isLittle);
    res.valueOrOffset = getInt(pointer + 8, 4, data, isLittle);

    return res;
}