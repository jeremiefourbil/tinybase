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
#define IX_MAX_NUMBER_OF_VALUES 4 // the maximum number of values by leaf or node
#define IX_MAX_NUMBER_OF_CHILDS 4 // the maximum number of values by leaf or node


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
    PageNum bucket[4];
};

struct IX_PageBucketHdr {
    PageNum parent;
    PageNum next;

    int nbFilledSlots;
};


void copyGeneric(const int &v1, int &v2);
void copyGeneric(const float &v1, float &v2);
void copyGeneric(const char* v1, char* v2);

int comparisonGeneric(const int &v1, const int &v2);
int comparisonGeneric(const float &v1, const float &v2);
int comparisonGeneric(const char* v1, const char* v2);

void printGeneric(const int &v1);
void printGeneric(const float &v1);
void printGeneric(const char* v1);

void sortGeneric(int *array, PageNum buckets[], const int &arrayLength);
void sortGeneric(float *array, PageNum buckets[], const int &arrayLength);
void sortGeneric(char array[][MAXSTRINGLEN], PageNum buckets[], const int &arrayLength);


// void sortNodeGeneric(int *array, PageNum child[IX_MAX_NUMBER_OF_CHILDS], const int &arrayLength);
// void sortNodeGeneric(float *array, PageNum child[IX_MAX_NUMBER_OF_CHILDS], const int &arrayLength);
// void sortNodeGeneric(char array[][MAXSTRINGLEN], PageNum child[IX_MAX_NUMBER_OF_CHILDS], const int &arrayLength);

#endif // IX_INTERNAL_H
