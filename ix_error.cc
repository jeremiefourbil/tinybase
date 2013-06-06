//
// File:        ix_error.cc
// Description: IX_PrintError function
//

#include <cerrno>
#include <cstdio>
#include <iostream>
#include "ix_internal.h"

using namespace std;

//
// Error table
//
static char *IX_WarnMsg[] = {
//  (char*)"page pinned in buffer",
//  (char*)"page is not in the buffer",
//  (char*)"invalid page number",
//  (char*)"file open",
//  (char*)"invalid file descriptor (file closed)",
//  (char*)"page already free",
//  (char*)"page already unpinned",
//  (char*)"end of file",
//  (char*)"attempting to resize the buffer too small",
//  (char*)"invalid filename"
};

static char *IX_ErrorMsg[] = {
  (char*)"impossible to create index",
  (char*)"null pointer",
  (char*)"unknown type",
  (char*)"entry does not exist",
  (char*)"try to access an unauthorized array value",
  (char*)"invalid page number"
//  (char*)"new page to be allocated already in buffer",
//  (char*)"hash table entry not found",
//  (char*)"page already in hash table",
//  (char*)"invalid file name"
};

//
// IX_PrintError
//

void IX_PrintError(RC rc)
{
    cerr << "IX ERROR BIATCH" << "\n";


}
