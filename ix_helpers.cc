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

void sortGeneric(int *array, const int &arrayLength)
{
  for (int i = 1; i < arrayLength; i++)
  {
    int tmp = array[i];
    int j = i;
    for (; j && tmp < array[j - 1]; --j)
      array[j] = array[j - 1];
    array[j] = tmp;
  }
}

void sortGeneric(float *array, const int &arrayLength)
{
  for (int i = 1; i < arrayLength; i++)
  {
    float tmp = array[i];
    int j = i;
    for (; j && tmp < array[j - 1]; --j)
      array[j] = array[j - 1];
    array[j] = tmp;
  }
}

void sortGeneric(char array[4][MAXSTRINGLEN], const int &arrayLength)
{
  for (int i = 1; i < arrayLength; i++)
  {
    char* tmp;
    strcpy(tmp, array[i]);
    int j = i;
    for (; j && strcmp(tmp,array[j - 1]); --j)
        strcpy(array[j], array[j - 1]);
    strcpy(array[j],tmp);
  }
}

