//
// File:        ql_error.cc
// Description: QL_PrintError function
//

#include <cerrno>
#include <cstdio>
#include <iostream>
#include "ql_internal.h"

using namespace std;

//
// Error table
//
static char *QL_WarnMsg[] = {
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

static char *QL_ErrorMsg[] = {
    (char*)"unknown type",
    (char*)"iterator not opened",
    (char*)"unexpected null child"
    //  (char*)"invalid file name"
};

//
// QL_PrintError
//

void QL_PrintError(RC rc)
{
    /// Check the return code is within proper limits
    if (rc >= START_QL_WARN && rc <= QL_LASTWARN)
        // Print warning
        cerr << "QL warning: " << QL_WarnMsg[rc - START_QL_WARN] << "\n";
    // Error codes are negative, so invert everything
    else if (-rc >= -START_QL_ERR && -rc < -QL_LASTERROR)
        // Print error
        cerr << "QL error: " << QL_ErrorMsg[-rc + START_QL_ERR] << "\n";
    else if (rc == 0)
        cerr << "QL_PrintError called with return code of 0\n";
    else
        cerr << "QL error: " << rc << " is out of bounds\n";
}

