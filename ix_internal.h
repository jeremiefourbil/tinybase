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

void sortGeneric(int *array, const int &arrayLength);
void sortGeneric(float *array, const int &arrayLength);
void sortGeneric(char array[4][MAXSTRINGLEN], const int &arrayLength);


// defined in IX_IndexHandle to fix compilation issue
template <typename T>
void swapLeafEntries(int i, IX_PageLeaf<T> * pBuffer1, int j, IX_PageLeaf<T> *pBuffer2);

template <typename T>
void sortLeaf(IX_PageLeaf<T> * pBuffer);


#endif // IX_INTERNAL_H
