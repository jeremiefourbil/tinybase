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
    (char*)"insuffisant page size",
    (char*)"null pointer",
    (char*)"unknown type",
    (char*)"entry does not exist",
    (char*)"try to access an unauthorized array value",
    (char*)"invalid page number",
    (char*)"delete case invalid",
    (char*)"the bucket cannot store anymore RID",
    (char*)"the bucket already contains this RID",
    (char*)"the index number should be a positive number",
    (char*)"incorrect operator",
    (char*)"index already opened"
    //  (char*)"invalid file name"
};

//
// IX_PrintError
//

void IX_PrintError(RC rc)
{
    /// Check the return code is within proper limits
    if (rc >= START_IX_WARN && rc <= IX_LASTWARN)
        // Print warning
        cerr << "IX warning: " << IX_WarnMsg[rc - START_IX_WARN] << "\n";
    // Error codes are negative, so invert everything
    else if (-rc >= -START_IX_ERR && -rc < -IX_LASTERROR)
        // Print error
        cerr << "IX error: " << IX_ErrorMsg[-rc + START_IX_ERR] << "\n";
    else if (rc == 0)
        cerr << "IX_PrintError called with return code of 0\n";
    else
        cerr << "IX error: " << rc << " is out of bounds\n";
}
