#include "ql_internal.h"

#include <iostream>

using namespace std;


void fillString(char* str, int length)
{
    char nullChar = '\0';
    bool fillString = false;

    for(int i=0; i<length; i++)
    {
        if(fillString)
        {
            memcpy(str + i * sizeof(char), &nullChar, sizeof(char));
        }
        else
        {
            char c;
            memcpy(&c, str + i * sizeof(char), sizeof(char));
            if(c == '\0')
            {
                fillString = true;
            }
        }
    }
}

