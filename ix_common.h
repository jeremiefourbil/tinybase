#ifndef IX_COMMON_H
#define IX_COMMON_H

#include "redbase.h"
#include "rm_rid.h"
#include "pf.h"


struct IX_FileHdr {
    // General
    AttrType attrType;
    int attrLength;

    // BTree
    PageNum rootNum;
    int height;
    PageNum firstLeafNum;

    // Hash
    PageNum directoryNum;
};


#endif // IX_COMMON_H
