#include "ix_internal.h"

#include <iostream>

using namespace std;

void copyGeneric(const int &v1, int &v2) { v2 = v1; }
void copyGeneric(const float &v1, float &v2) { v2 = v1; }
void copyGeneric(const char* v1, char* v2) { strcpy(v2, v1); }

int comparisonGeneric(const int &v1, const int &v2) { return (v1 < v2) ? -1 : ((v1==v2) ? 0 : 1); }
int comparisonGeneric(const float &v1, const float &v2) { return (v1 < v2) ? -1 : ((v1==v2) ? 0 : 1); }
int comparisonGeneric(const char* v1, const char* v2) { return strcmp(v1,v2); }


void printGeneric(const int &v1) { cout << v1 << endl; }
void printGeneric(const float &v1) { cout << v1 << endl; }
void printGeneric(const char* v1) { return; }

void sortGeneric(int *array, PageNum bucket[], const int arrayLength)
{
  for (int i = 1; i < arrayLength; i++)
  {
    int tmp = array[i];
    PageNum tmpBucket = bucket[i];
    int j = i;
    for (; j && tmp < array[j - 1]; --j)
      array[j] = array[j - 1];
    bucket[j] = bucket[j - 1];
    array[j] = tmp;
    bucket[j] = tmpBucket;
  }
}

void sortGeneric(float *array, PageNum bucket[], const int arrayLength)
{
  for (int i = 1; i < arrayLength; i++)
  {
    float tmp = array[i];
    PageNum tmpBucket = bucket[i];
    int j = i;
    for (; j && tmp < array[j - 1]; --j)
      array[j] = array[j - 1];
    bucket[j] = bucket[j - 1];
    array[j] = tmp;
    bucket[j] = tmpBucket;
  }
}

void sortGeneric(char array[][MAXSTRINGLEN], PageNum bucket[], const int arrayLength)
{
  for (int i = 1; i < arrayLength; i++)
  {
    char* tmp;
    PageNum tmpBucket = bucket[i];
    strcpy(tmp, array[i]);
    int j = i;
    for (; j && strcmp(tmp,array[j - 1]); --j)
      strcpy(array[j], array[j - 1]);
    bucket[j] = bucket[j - 1];
    strcpy(array[j],tmp);
    bucket[j] = tmpBucket;
  }
}

void sortNodeGeneric(int *array, PageNum child[IX_MAX_NUMBER_OF_CHILDS], const int arrayLength)
{
  // on crée un tableau temporaire avec uniquement les fils droits
  PageNum temporaryChild[IX_MAX_NUMBER_OF_CHILDS - 1];
  // on le remplit
  for(int k= 1; k< IX_MAX_NUMBER_OF_CHILDS; ++k)
  {
    temporaryChild[k-1] = child[k];
  }
  for (int i = 1; i < arrayLength; i++)
  {
    int tmp = array[i];
    PageNum tmpChild = temporaryChild[i];
    int j = i;
    for (; j && tmp < array[j - 1]; --j)
    {
        array[j] = array[j - 1];
        temporaryChild[j] = temporaryChild[j - 1];
    }
    array[j] = tmp;
    temporaryChild[j] = tmpChild;
  }
  // on remplit les valeurs triées que l'on réinjecte dans child[]
  for(int k=0; k<IX_MAX_NUMBER_OF_CHILDS-1;++k)
  {
    child[k+1] = temporaryChild[k];
  }
}

void sortNodeGeneric(float *array, PageNum child[IX_MAX_NUMBER_OF_CHILDS], const int arrayLength)
{
  // on crée un tableau temporaire avec uniquement les fils droits
  PageNum temporaryChild[IX_MAX_NUMBER_OF_CHILDS - 1];
  // on le remplit
  for(int k= 1; k< IX_MAX_NUMBER_OF_CHILDS; ++k)
  {
    temporaryChild[k-1] = child[k];
  }
  for (int i = 1; i < arrayLength; i++)
  {
    float tmp = array[i];
    PageNum tmpChild = temporaryChild[i];
    int j = i;
    for (; j && tmp < array[j - 1]; --j)
    {
        array[j] = array[j - 1];
        temporaryChild[j] = temporaryChild[j - 1];
    }
    array[j] = tmp;
    temporaryChild[j] = tmpChild;
  }
  // on remplit les valeurs triées que l'on réinjecte dans child[]
  for(int k=0; k<IX_MAX_NUMBER_OF_CHILDS-1;++k)
  {
    child[k+1] = temporaryChild[k];
  }
}

void sortNodeGeneric(char array[][MAXSTRINGLEN], PageNum child[IX_MAX_NUMBER_OF_CHILDS], const int arrayLength)
{
  // on crée un tableau temporaire avec uniquement les fils droits
  PageNum temporaryChild[IX_MAX_NUMBER_OF_CHILDS - 1];
  // on le remplit
  for(int k= 1; k< IX_MAX_NUMBER_OF_CHILDS; ++k)
  {
    temporaryChild[k-1] = child[k];
  }
  for (int i = 1; i < arrayLength; i++)
  {
    char* tmp;
    PageNum tmpChild = temporaryChild[i];
    strcpy(tmp, array[i]);
    int j = i;
    for (; j && strcmp(tmp,array[j - 1]); --j)
    {
        strcpy(array[j], array[j - 1]);
        temporaryChild[j] = temporaryChild[j - 1];
    }
    strcpy(array[j],tmp);
    temporaryChild[j] = tmpChild;
  }
  // on remplit les valeurs triées que l'on réinjecte dans child[]
  for(int k=0; k<IX_MAX_NUMBER_OF_CHILDS-1;++k)
  {
    child[k+1] = temporaryChild[k];
  }
}

