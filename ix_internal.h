#ifndef IX_INTERNAL_H
#define IX_INTERNAL_H


#include "ix.h"

#include <stdlib.h>
#include <string.h>
#include <cassert>


//
// Constants and defines
//
const int IX_HEADER_PAGE_NUM = 0;

#define IX_EMPTY  -1       // the next page does not exist



//
// IX_PageHdr: Header structure for pages
//

template <typename T>
struct IX_PageNode {
    PageNum parent;
    PageNum child[5];
    T v[4];
    NodeType nodeType;
};

template <typename T>
struct IX_PageLeaf {
    PageNum parent;
    PageNum previous;
    PageNum next;
    T v[4];
};


#endif // IX_INTERNAL_H
