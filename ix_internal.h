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

  const int order_INT = 2 * ((int)(0.5*(PF_PAGE_SIZE - 4 * sizeof(int) - 10) / (sizeof(int) + sizeof(int))));
//const int order_INT = 8;
//const int order_INT = 4;
const int order_FLOAT = 2 * ((int)(0.5*(PF_PAGE_SIZE - 4 * sizeof(float) - 10) / (sizeof(int) + sizeof(float))));
const int order_STRING = 2 * ((int)(0.5*(PF_PAGE_SIZE - 4 * sizeof(int) - 10) / (sizeof(int) + MAXSTRINGLEN * sizeof(char))));



#define IX_EMPTY  -1       // the next page does not exist

//
// IX_PageHdr: Header structure for pages
//



struct IX_PageBucketHdr {
    PageNum parent;
    PageNum next;

    int nbFilledSlots;
};

// copy
void copyGeneric(const int &v1, int &v2);
void copyGeneric(const float &v1, float &v2);
void copyGeneric(const char* v1, char* v2);

// comparison
int comparisonGeneric(const int &v1, const int &v2);
int comparisonGeneric(const float &v1, const float &v2);
int comparisonGeneric(const char* v1, const char* v2);

// display
void printGeneric(const int &v1);
void printGeneric(const float &v1);
void printGeneric(const char* v1);

// sort
void sortGeneric(int *array, PageNum buckets[], const int arrayLength);
void sortGeneric(float *array, PageNum buckets[], const int arrayLength);
void sortGeneric(char array[][MAXSTRINGLEN], PageNum buckets[], const int arrayLength);

// sort
template <int n>
void sortNodeGeneric(int *array, PageNum child[n+1], const int arrayLength);
template <int n>
void sortNodeGeneric(float *array, PageNum child[n+1], const int arrayLength);
template <int n>
void sortNodeGeneric(char array[][MAXSTRINGLEN], PageNum child[n+1], const int arrayLength);

// search
void getSlotIndex(const int *array, const int arrayLength, const int iValue, int &oIndex);
void getSlotIndex(const float *array, const int arrayLength, const float iValue, int &oIndex);
void getSlotIndex(const char array[][MAXSTRINGLEN], const int arrayLength, const char iValue[MAXSTRINGLEN], int &oIndex);

void getPointerIndex(const int *array, const int arrayLength, const int iValue, int &oIndex);
void getPointerIndex(const float *array, const int arrayLength, const float iValue, int &oIndex);
void getPointerIndex(const char array[][MAXSTRINGLEN], const int arrayLength, const char iValue[MAXSTRINGLEN], int &oIndex);

// defined in IX_IndexHandle to fix compilation issue
template <typename T, int n>
void swapLeafEntries(int i, IX_PageLeaf<T,n> * pBuffer1, int j, IX_PageLeaf<T,n> *pBuffer2);

template <typename T, int n>
void sortLeaf(IX_PageLeaf<T,n> * pBuffer);

//template <typename T, int n>
//void setOffsetInNode(IX_PageNode<T,n> *pBuffer, const int startIndex, const int offset);


#endif // IX_INTERNAL_H
