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
//  (char*)"no buffer space",
//  (char*)"incomplete read of page from file",
//  (char*)"incomplete write of page to file",
//  (char*)"incomplete read of header from file",
//  (char*)"incomplete write of header from file",
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

}
