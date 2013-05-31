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
    NodeType nodeType;

    int nbFilledSlots;
    T v[4];

    PageNum child[5];
};

template <typename T>
struct IX_PageLeaf {
    PageNum parent;

    PageNum previous;
    PageNum next;

    int nbFilledSlots;
    T v[4];  

    RID rid[4];
};


void copyGeneric(const int &v1, int &v2);
void copyGeneric(const float &v1, float &v2);
void copyGeneric(const char* v1, char* v2);

int comparisonGeneric(const int &v1, const int &v2);
int comparisonGeneric(const float &v1, const float &v2);
int comparisonGeneric(const char* v1, const char* v2);


#endif // IX_INTERNAL_H
