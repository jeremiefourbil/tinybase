//
// File:        ix_testshell.cc
// Description: Test IX component
// Authors:     Jan Jannink
//              Dallan Quass (quass@cs.stanford.edu)
//
// This test shell contains a number of functions that will be useful in
// testing your IX component code.  In addition, a couple of sample
// tests are provided.  The tests are by no means comprehensive, you are
// expected to devise your own tests to test your code.
//
// 1997:  Tester has been modified to reflect the change in the 1997
// interface.
// 2000:  Tester has been modified to reflect the change in the 2000
// interface.

#include <cstdio>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>

#include "redbase.h"
#include "pf.h"
#include "rm.h"
#include "ix.h"

using namespace std;

//
// Defines
//
#define FILENAME     "testrel"        // test file name
#define BADFILE      "/abc/def/xyz"   // bad file name
#define STRLEN       39               // length of strings to index
#define FEW_ENTRIES  3000
#define MANY_ENTRIES 30000
#define NENTRIES     30000        // Size of values array
#define PROG_UNIT    200              // how frequently to give progress
// reports when adding lots of entries

//
// Values array we will be using for our tests
//
int values[NENTRIES];

//
// Global component manager variables
//
PF_Manager pfm;
RM_Manager rmm(pfm);
IX_Manager ixm(pfm);

//
// Function declarations
//
RC Test1(void);
RC Test2(void);
RC Test3(void);
RC Test4(void);
RC Test5(void);
RC Test6(void);
RC Test7(void);
RC Test8(void);
RC Test9(void);
RC Test10(void);
RC Test11(void);

void PrintError(RC rc);
void LsFiles(char *fileName);
void ran(int n);
RC InsertIntEntries(IX_IndexHandle &ih, int nEntries);
RC InsertFloatEntries(IX_IndexHandle &ih, int nEntries);
RC InsertStringEntries(IX_IndexHandle &ih, int nEntries);
RC AddRecs(RM_FileHandle &fh, int nRecs);
RC DeleteIntEntries(IX_IndexHandle &ih, int nEntries);
RC DeleteFloatEntries(IX_IndexHandle &ih, int nEntries);
RC DeleteStringEntries(IX_IndexHandle &ih, int nEntries);
RC VerifyIntIndex(IX_IndexHandle &ih, int nStart, int nEntries, int bExists);
RC PrintIndex(IX_IndexHandle &ih);

//
// Array of pointers to the test functions
//
#define NUM_TESTS       11               // number of tests
int (*tests[])() =                      // RC doesn't work on some compilers
{
        Test1,
Test2,
Test3,
Test4,
Test5,
Test6,
Test7,
Test8,
Test9,
Test10,
Test11
};

//
// main
//
int main(int argc, char *argv[])
{
    RC   rc;
    char *progName = argv[0];   // since we will be changing argv
    int  testNum;

    // Write out initial starting message
    printf("Starting IX component test.\n\n");

    // Init randomize function
    srand( (unsigned)time(NULL));

    // Delete files from last time (if found)
    // Don't check the return codes, since we expect to get an error
    // if the files are not found.
    rmm.DestroyFile(FILENAME);
    ixm.DestroyIndex(FILENAME, 0);
    ixm.DestroyIndex(FILENAME, 1);
    ixm.DestroyIndex(FILENAME, 2);
    ixm.DestroyIndex(FILENAME, 3);

    // If no argument given, do all tests
    if (argc == 1) {
        for (testNum = 0; testNum < NUM_TESTS; testNum++)
            if ((rc = (tests[testNum])())) {
                // Print the error and exit
                PrintError(rc);
                return (1);
            }
    }
    else {
        // Otherwise, perform specific tests
        while (*++argv != NULL) {

            // Make sure it's a number
            if (sscanf(*argv, "%d", &testNum) != 1) {
                cerr << progName << ": " << *argv << " is not a number\n";
                continue;
            }

            // Make sure it's in range
            if (testNum < 1 || testNum > NUM_TESTS) {
                cerr << "Valid test numbers are between 1 and " << NUM_TESTS << "\n";
                continue;
            }

            // Perform the test
            if ((rc = (tests[testNum - 1])())) {
                // Print the error and exit
                PrintError(rc);
                return (1);
            }
        }
    }

    // Write ending message and exit
    printf("Ending IX component test.\n\n");

    return (0);
}

//
// PrintError
//
// Desc: Print an error message by calling the proper component-specific
//       print-error function
//
void PrintError(RC rc)
{
    if (abs(rc) <= END_PF_WARN)
        PF_PrintError(rc);
    else if (abs(rc) <= END_RM_WARN)
        RM_PrintError(rc);
    else if (abs(rc) <= END_IX_WARN)
        IX_PrintError(rc);
    else
        cerr << "Error code out of range: " << rc << "\n";
}

////////////////////////////////////////////////////////////////////
// The following functions may be useful in tests that you devise //
////////////////////////////////////////////////////////////////////

//
// LsFiles
//
// Desc: list the filename's directory entry
//
void LsFiles(char *fileName)
{
    char command[80];

    sprintf(command, "ls -l *%s*", fileName);
    printf("Doing \"%s\"\n", command);
    system(command);
}

//
// Create an array of random nos. between 0 and n-1, without duplicates.
// put the nos. in values[]
//
void ran(int n)
{
    int i, r, t, m;

    // Initialize values array
    for (i = 0; i < NENTRIES; i++)
        values[i] = i;

    // Randomize first n entries in values array
    for (i = 0, m = n; i < n-1; i++) {
        r = (int)(rand() % m--);
        t = values[m];
        values[m] = values[r];
        values[r] = t;
    }
}

//
// InsertIntEntries
//
// Desc: Add a number of integer entries to the index
//
RC InsertIntEntries(IX_IndexHandle &ih, int nEntries)
{
    RC  rc;
    int i;
    int value;

    const int length = nEntries;
    int sequence[length];

    printf("             Adding %d int entries\n", nEntries);
    ran(nEntries);
    for(i = 0; i < nEntries; i++) {
        value = values[i] + 1;
        RID rid(value, value*2);

//        printf("Value to insert: %d \n", value);

        if ((rc = ih.InsertEntry((void *)&value, rid)))
        {
            printf("pb at %d\n",i);
            return (rc);
        }

//        if((rc = ih.DisplayTree()))
//            return rc;

//        printf("Value inserted: %d \n", value);

        if((i + 1) % PROG_UNIT == 0){
            // cast to long for PC's
            printf("\r\t%d%%    ", (int)((i+1)*100L/nEntries));
            fflush(stdout);
        }

        sequence[i] = value;
    }
    printf("\r\t%d%%      \n", (int)(i*100L/nEntries));

    // printf("\n Inserted sequence:\n");
    // for(int i=0; i<nEntries; i++)
    // {
    //     printf("%d,", sequence[i]);
    // }
    // printf("\n\n");

    // Return ok
    return (0);
}



RC InsertSequelOfInt(IX_IndexHandle &ih, int entries[],const int entriesLength)
{
    RC  rc;
    int i;
    int value;

    printf("             Adding %d int entries\n", entriesLength);
    for(i = 0; i < entriesLength; i++) {
        value = entries[i];
        RID rid(value, value*2);
        if ((rc = ih.InsertEntry((void *)&value, rid)))
            return (rc);

        if((i + 1) % PROG_UNIT == 0){
            // cast to long for PC's
            printf("\r\t%d%%    ", (int)((i+1)*100L/entriesLength));
            fflush(stdout);
        }
    }
    printf("\r\t%d%%      \n", (int)(i*100L/entriesLength));

    // Return ok
    return (0);
}


//
// Desc: Add a number of float entries to the index
//
RC InsertFloatEntries(IX_IndexHandle &ih, int nEntries)
{
    RC    rc;
    int   i;
    float value;

    printf("             Adding %d float entries\n", nEntries);
    ran(nEntries);
    for (i = 0; i < nEntries; i++) {
        value = values[i] + 1;
        RID rid((PageNum)value, (SlotNum)value*2);
        if ((rc = ih.InsertEntry((void *)&value, rid)))
            return (rc);

        if((i + 1) % PROG_UNIT == 0){
            printf("\r\t%d%%    ", (int)((i+1)*100L/nEntries));
            fflush(stdout);
        }
    }
    printf("\r\t%d%%      \n", (int)(i*100L/nEntries));

    // Return ok
    return (0);
}

//
// Desc: Add a number of string entries to the index
//
RC InsertStringEntries(IX_IndexHandle &ih, int nEntries)
{
    RC   rc;
    int  i;
    char value[STRLEN];

    printf("             Adding %d string entries\n", nEntries);
    ran(nEntries);
    for (i = 0; i < nEntries; i++) {
        memset(value, ' ', STRLEN);
        sprintf(value, "number %d", values[i] + 1);
        RID rid(values[i] + 1, (values[i] + 1)*2);
        if ((rc = ih.InsertEntry(value, rid)))
            return (rc);

        if((i + 1) % PROG_UNIT == 0){
            printf("\r\t%d%%    ", (int)((i+1)*100L/nEntries));
            fflush(stdout);
        }
    }
    printf("\r\t%d%%      \n", (int)(i*100L/nEntries));

    // Return ok
    return (0);
}

//
// AddRecs
//
// Desc: Add a number of integer records to an RM file
//
RC AddRecs(RM_FileHandle &fh, int nRecs)
{
    RC      rc;
    int     i;
    int     value;
    RID     rid;
    PageNum pageNum;
    SlotNum slotNum;

    printf("           Adding %d int records to RM file\n", nRecs);
    ran(nRecs);
    for(i = 0; i < nRecs; i++) {
        value = values[i] + 1;
        if ((rc = fh.InsertRec((char *)&value, rid)) ||
                (rc = rid.GetPageNum(pageNum)) ||
                (rc = rid.GetSlotNum(slotNum)))
            return (rc);

        if((i + 1) % PROG_UNIT == 0){
            printf("\r\t%d%%      ", (int)((i+1)*100L/nRecs));
            fflush(stdout);
        }
    }
    printf("\r\t%d%%      \n", (int)(i*100L/nRecs));

    // Return ok
    return (0);
}

//
// DeleteIntEntries: delete a number of integer entries from an index
//
RC DeleteIntEntries(IX_IndexHandle &ih, int nEntries)
{
    RC  rc;
    int i;
    int value;

    const int length = nEntries;
    int sequence[length];

    printf("        Deleting %d int entries\n", nEntries);
    ran(nEntries);
    for (i = 0; i < nEntries; i++) {

        //          if((rc == ih.DisplayTree()))
        //              return rc;
        value = values[i] + 1;
        RID rid(value, value*2);
        sequence[i] = value;
        // printf("before %d\n", i);
        if ((rc = ih.DeleteEntry((void *)&value, rid)))
        {
            printf("\n Deletion sequence (%d):\n", i+1);
            for(int k=0; k<=i; k++)
            {
                printf("%d,", sequence[k]);
            }
            printf("\n\n");
            return (rc);
        }

        if((i + 1) % PROG_UNIT == 0){
            printf("\r\t%d%%      ", (int)((i+1)*100L/nEntries));
            fflush(stdout);
        }


    }
    printf("\r\t%d%%      \n", (int)(i*100L/nEntries));

    return (0);
}

//
// DeleteFloatEntries: delete a number of float entries from an index
//
RC DeleteFloatEntries(IX_IndexHandle &ih, int nEntries)
{
    RC  rc;
    int i;
    float value;

    printf("        Deleting %d float entries\n", nEntries);
    ran(nEntries);
    for (i = 0; i < nEntries; i++) {
        value = values[i] + 1;
        RID rid((PageNum)value, (SlotNum)value*2);
        if ((rc = ih.DeleteEntry((void *)&value, rid)))
            return (rc);

        if((i + 1) % PROG_UNIT == 0){
            printf("\r\t%d%%      ", (int)((i+1)*100L/nEntries));
            fflush(stdout);
        }
    }
    printf("\r\t%d%%      \n", (int)(i*100L/nEntries));

    return (0);
}

//
// Desc: Delete a number of string entries from an index
//
RC DeleteStringEntries(IX_IndexHandle &ih, int nEntries)
{
    RC      rc;
    int     i;
    char    value[STRLEN+1];

    printf("             Deleting %d float entries\n", nEntries);
    ran(nEntries);
    for (i = 0; i < nEntries; i++) {
        sprintf(value, "number %d", values[i] + 1);
        RID rid(values[i] + 1, (values[i] + 1)*2);
        if ((rc = ih.DeleteEntry(value, rid)))
            return (rc);
        if((i + 1) % PROG_UNIT == 0){
            printf("\r\t%d%%    ", (int)((i+1)*100L/nEntries));
            fflush(stdout);
        }
    }
    printf("\r\t%d%%      \n", (int)(i*100L/nEntries));

    // Return ok
    return (0);
}

//
// VerifyIntIndex
//   - nStart is the starting point in the values array to check
//   - nEntries is the number of entries in the values array to check
//   - If bExists == 1, verify that an index has entries as added
//     by InsertIntEntries.
//     If bExists == 0, verify that entries do NOT exist (you can
//     use this to test deleting entries).
//
RC VerifyIntIndex(IX_IndexHandle &ih, int nStart, int nEntries, int bExists)
{
    RC      rc;
    int     i;
    RID     rid;
    IX_IndexScan scan;
    PageNum pageNum;
    SlotNum slotNum;

    // Assume values still contains the array of values inserted/deleted

    printf("Verifying index contents\n");

    for (i = nStart; i < nStart + nEntries; i++) {
        int value = values[i] + 1;

        if ((rc = scan.OpenScan(ih, EQ_OP, &value))) {
            printf("Verify error: opening scan\n");
            return (rc);
        }

        rc = scan.GetNextEntry(rid);
        if (!bExists && rc == 0) {
            printf("Verify error: found non-existent entry %d\n", value);
            return (IX_EOF);  // What should be returned here?
        }
        else if (bExists && rc == IX_EOF) {
            printf("Verify error: entry %d not found\n", value);
            return (IX_EOF);  // What should be returned here?
        }
        else if (rc != 0 && rc != IX_EOF)
            return (rc);

        if (bExists && rc == 0) {
            // Did we get the right entry?
            if ((rc = rid.GetPageNum(pageNum)) ||
                    (rc = rid.GetSlotNum(slotNum)))
                return (rc);

            if (pageNum != value || slotNum != (value*2)) {
                printf("Verify error: incorrect rid (%d,%d) found for entry %d\n",
                       pageNum, slotNum, value);
                return (IX_EOF);  // What should be returned here?
            }

            // Is there another entry?
            rc = scan.GetNextEntry(rid);
            if (rc == 0) {
                printf("Verify error: found two entries with same value %d\n", value);
                return (IX_EOF);  // What should be returned here?
            }
            else if (rc != IX_EOF)
                return (rc);
        }

        if ((rc = scan.CloseScan())) {
            printf("Verify error: closing scan\n");
            return (rc);
        }

        printf("Entry %d correctly found: %d\n", value, bExists);
    }

    return (0);
}

/////////////////////////////////////////////////////////////////////
// Sample test functions follow.                                   //
/////////////////////////////////////////////////////////////////////

//
// Test1 tests simple creation, opening, closing, and deletion of indices
//
RC Test1(void)
{
    RC rc;
    int index=0;
    IX_IndexHandle ih;

    printf("Test 1: create, open, close, delete an index... \n");

    if ((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = ixm.CloseIndex(ih)))
        return (rc);

    LsFiles(FILENAME);

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 1\n\n");
    return (0);
}

//
// Test2 tests inserting a few integer entries into the index.
//
RC Test2(void)
{
    RC rc;
    IX_IndexHandle ih;
    int index=0;

    printf("Test2: Insert a few integer entries into an index... \n");

    if ((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = InsertIntEntries(ih, FEW_ENTRIES)) ||
//            (rc = ih.DisplayTree()) ||
            (rc = ixm.CloseIndex(ih)) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||

            // ensure inserted entries are all there
//            (rc = VerifyIntIndex(ih, 0, FEW_ENTRIES, TRUE)) ||

            // ensure an entry not inserted is not there
//            (rc = VerifyIntIndex(ih, FEW_ENTRIES, 1, FALSE)) ||
            (rc = ixm.CloseIndex(ih)))
        return (rc);

    LsFiles(FILENAME);

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 2\n\n");
    return (0);
}

//
// Test3 tests deleting a few integer entries from an index
//
RC Test3(void)
{
    RC rc;
    int index=0;
    int nDelete = FEW_ENTRIES * 8/10;
    IX_IndexHandle ih;

    printf("Test3: Delete a few integer entries from an index... \n");

    if ((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = InsertIntEntries(ih, FEW_ENTRIES)) ||
//            (rc = ih.DisplayTree()) ||
            (rc = DeleteIntEntries(ih, nDelete)) ||
            (rc = ixm.CloseIndex(ih)) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            // ensure deleted entries are gone
            (rc = VerifyIntIndex(ih, 0, nDelete, FALSE)) ||
            // ensure non-deleted entries still exist
            (rc = VerifyIntIndex(ih, nDelete, FEW_ENTRIES - nDelete, TRUE)) ||
            (rc = ixm.CloseIndex(ih)))
        return (rc);

    LsFiles(FILENAME);

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 3\n\n");
    return (0);
}

//
// Test 4 tests a few inequality scans on Btree indices
//
RC Test4(void)
{
    RC             rc;
    IX_IndexHandle ih;
    int            index=0;
    int            i;
    int            value=FEW_ENTRIES/3;
    RID            rid;

    printf("Test4: Inequality scans... \n");



    int sequelLength = 20;
    int sequel[20] = {5,20,7,10,11,16,17,3,1,6,4,19,12,18,2,15,9,14,8,13};

    printf("Test4: Scan test. Fist, insert entries. Second, delete some of them by scan-delete. Third, perform some scans.\n");


    if ((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = InsertIntEntries(ih, FEW_ENTRIES)))
        return (rc);


    //    // Scan-deletion with >= OP
    printf("Scan-deletion with >= OP for %d\n", value);
    IX_IndexScan scandel;
    if ((rc = scandel.OpenScan(ih, GE_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }
    i = 0;
    while (!(rc = scandel.GetNextEntry(rid))) {
        int val;
        rid.GetPageNum(val);

        if((rc = ih.DeleteEntry(&val, rid)))
            return rc;

        if ((rc = ih.DisplayTree()))
            return (rc);
        //        printf("Trying to delete %d\n", val);
        i++;
    }
    if (rc != IX_EOF)
        return (rc);
    printf("Deleted %d entries in <-scan.\n", i);
    if((rc=scandel.CloseScan())) {
        printf("Scan error: closing scan\n");
        return (rc);
    }


    if ((rc = ih.DisplayTree()))
        return (rc);


    // Scan =
    printf("Scan = for %d\n", value);
    IX_IndexScan scaneq;
    if ((rc = scaneq.OpenScan(ih, EQ_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scaneq.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);
    printf("Found %d entries in =scan.\n", i);

    // Scan <
    printf("Scan < for %d\n", value);
    IX_IndexScan scanlt;
    if ((rc = scanlt.OpenScan(ih, LT_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scanlt.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);
    printf("Found %d entries in <-scan.\n", i);

    // Scan <=
    printf("Scan <= for %d\n", value);
    IX_IndexScan scanle;
    if ((rc = scanle.OpenScan(ih, LE_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }
    i = 0;
    while (!(rc = scanle.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in <=-scan.\n", i);


    // Scan >
    printf("Scan > for %d\n", value);
    IX_IndexScan scangt;
    if ((rc = scangt.OpenScan(ih, GT_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }
    i = 0;
    while (!(rc = scangt.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in >-scan.\n", i);

    // Scan >=
    printf("Scan >= for %d\n", value);
    IX_IndexScan scange;
    if ((rc = scange.OpenScan(ih, GE_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }
    i = 0;
    while (!(rc = scange.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in >=-scan.\n", i);



    // Scan <>
    printf("Scan <> for %d\n", value);
    IX_IndexScan scanne;
    if ((rc = scanne.OpenScan(ih, NE_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scanne.GetNextEntry(rid))) {
        i++;
    }

    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in <>scan.\n", i);



    // Scan noscan
    printf("Scan ALL for %d\n", value);
    IX_IndexScan scanno;
    if ((rc = scanno.OpenScan(ih, NO_OP, NULL))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }
    i = 0;
    while (!(rc = scanno.GetNextEntry(rid))) {
        i++;
    }

    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in NULL scan.\n", i);




    if ((rc = ih.DisplayTree()))
        return (rc);


    if ((rc = ixm.CloseIndex(ih)))
        return (rc);

    LsFiles(FILENAME);

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 4\n\n");
    return (0);
}

// Test 5 tests for displaying

#define XML_BOTTOM "</graphml>"

RC Test5(void)
{
    RC rc;
    IX_IndexHandle ih;
    int index=0;

    printf("Test5: test for making a graphml file... \n");

    if ((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = InsertIntEntries(ih, MANY_ENTRIES)))
        return (rc);

    // create the xml file
    if((rc = ih.DisplayTree()))
        return (rc);

    if((rc = ixm.CloseIndex(ih)))
        return rc;

    LsFiles(FILENAME);

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 5\n\n");
    return (0);
}

// Test 6 tests for displaying with a specific sequel

#define XML_BOTTOM "</graphml>"

RC Test6(void)
{
    RC rc;
    IX_IndexHandle ih;
    int index=0;
    int m=0;
    //   int sequelLength = 7;
    //   int sequel[7] = {10,1,13,3,4,5,2};
    // int sequelLength = 11;
    // int sequel[11] = {11,6,5,17,8,7,18,1,9,14,15};
    int sequelLength = 20;
    int sequel[20] = {4,8,6,3,9,15,12,13,11,1,5,7,17,20,14,19,2,16,18,10};

    const int deleteSequelLength = 16;
    int deleteSequel[deleteSequelLength] = {10,8,2,13,7,16,6,5,14,9,15,12,4,3,1,11};

    //    const int deleteSequelLength = 7;
    //    int deleteSequel[deleteSequelLength] = {10,8,2,13,7,16,6,5};



    printf("Test7: test de suppression simple d'une valeur \n");

    if ((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = InsertSequelOfInt(ih, sequel, sequelLength)))
        return rc;


    for(int i=0; i<deleteSequelLength; i++)
    {
        RID rid(deleteSequel[i], deleteSequel[i]*2);
        if((rc = ih.DeleteEntry((void *)&deleteSequel[i], rid)))
            return rc;
    }

    // create the xml file
    if((rc = ih.DisplayTree()))
        return (rc);

    for(int k=0; k<deleteSequelLength; k++)
    {
        RID rid;
        // Scan =
        IX_IndexScan scaneq;
        if ((rc = scaneq.OpenScan(ih, EQ_OP, &deleteSequel[k]))) {
            printf("Scan error: opening scan\n");
            return (rc);
        }

        m = 0;
        while (!(rc = scaneq.GetNextEntry(rid))) {
            m++;
        }

        if (rc != IX_EOF)
            return (rc);

        printf("Found %d entries in =scan for %d.\n", m, deleteSequel[k]);
    }



    if((rc = ixm.CloseIndex(ih)))
        return rc;

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 6\n\n");
    return (0);
}

// Test 7 : suppression simple d'une valeur dans une feuille
RC Test7(void)
{
    RC rc;
    IX_IndexHandle ih;
    int index=0;
    //    int sequelLength = 20;
    //    int sequel[20] = {5,20,71,10,11,16,17,3,1,6,4,19,12,18,2,15,9,14,8,13};


    const int sequelLength = 90;
    int sequel[sequelLength] = {57,58,9,14,50,43,34,46,20,2,25,32,73,47,13,87,78,18,11,40,75,72,29,70,19,30,6,12,26,36,41,15,60,90,3,7,76,21,77,39,10,81,8,16,51,45,85,82,48,54,68,5,23,27,56,64,62,49,38,74,59,86,67,63,65,22,37,61,79,28,88,17,52,35,84,66,83,69,89,42,80,44,4,33,24,71,55,1,53,31};

    //     const int deleteSequelLength = 43;
    //     int deleteSequel[deleteSequelLength] = {59,15,33,43,67,28,34,37,23,30,54,26,64,62,46,56,49,53,17,51,48,29,65,16,18,44,3,5,68,52,40,19,20,47,66,61,25,36,9,8,27,35,10};

    const int deleteSequelLength = 38;
    int deleteSequel[deleteSequelLength] = {59,15,33,43,67,28,34,37,23,30,54,26,64,62,46,56,49,53,17,51,48,29,65,16,18,44,3,5,68,52,40,19,20,47,66,61,25,36};
    printf("Test7: test for making a graphml file with a specific sequel... \n");

    if ((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = InsertSequelOfInt(ih, sequel, sequelLength)))
        return rc;


    for(int i=0; i<deleteSequelLength; i++)
    {
        RID rid(deleteSequel[i], deleteSequel[i]*2);
        if((rc = ih.DeleteEntry((void *)&deleteSequel[i], rid)))
            return rc;
    }



    // create the xml file
    if((rc = ih.DisplayTree()))
        return (rc);

    if((rc = ixm.CloseIndex(ih)))
        return rc;

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 7\n\n");
    return (0);
}

//
// Test8 tests inserting a few STRING entries into the index.
//
RC Test8(void)
{
    RC rc;
    IX_IndexHandle ih;
    int index=0;
    int            i;
    char*            value="number 2";
    RID            rid;

    printf("Test8: Insert a few string entries into an index... \n");

    if ((rc = ixm.CreateIndex(FILENAME, index, STRING, sizeof(char[STRLEN]))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = InsertStringEntries(ih, FEW_ENTRIES)) ||
            (rc = ih.DisplayTree())  ||
            (rc = ixm.CloseIndex(ih)) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)))
        return rc;


    // Scan =
    IX_IndexScan scaneq;
    if ((rc = scaneq.OpenScan(ih, EQ_OP, value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scaneq.GetNextEntry(rid))) {
        i++;
    }

    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in =scan.\n", i);

    // Scan <
    IX_IndexScan scanlt;
    if ((rc = scanlt.OpenScan(ih, LT_OP, value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scanlt.GetNextEntry(rid))) {
        i++;
    }

    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in <-scan.\n", i);

    // Scan <=
    IX_IndexScan scanle;
    if ((rc = scanle.OpenScan(ih, LE_OP, value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scanle.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in <=-scan.\n", i);

    // Scan >
    IX_IndexScan scangt;
    if ((rc = scangt.OpenScan(ih, GT_OP, value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scangt.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in >-scan.\n", i);

    // Scan >=
    IX_IndexScan scange;
    if ((rc = scange.OpenScan(ih, GE_OP, value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scange.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in >=-scan.\n", i);



    // Scan <>
    IX_IndexScan scanne;
    if ((rc = scanne.OpenScan(ih, NE_OP, value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scanne.GetNextEntry(rid))) {
        i++;
    }

    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in <>scan.\n", i);



    // Scan noscan
    IX_IndexScan scanno;
    if ((rc = scanno.OpenScan(ih, NO_OP, NULL))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scanno.GetNextEntry(rid))) {
        i++;
    }

    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in NULL scan.\n", i);



    printf("close...\n");

    if(rc = ixm.CloseIndex(ih))
        return (rc);

    printf("...closed\n");

    LsFiles(FILENAME);

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 8\n\n");
    return (0);
}




// Test 9: suppression par la gauche
// merge successifs de feuilles et de noeuds à droite
// changement de racine
RC Test9(void)
{
    RC rc;
    IX_IndexHandle ih;
    int index=0;
    int sequelLength = 20;
    int sequel[20] = {5,20,7,10,11,16,17,3,1,6,4,19,12,18,2,15,9,14,8,13};

    const int deleteSequelLength = 20;
    int deleteSequel[deleteSequelLength] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};

    printf("Test9: Insert entries, delete 1 then 2 then 3... and insert them again \n");

    if ((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = InsertSequelOfInt(ih, sequel, sequelLength)))
        return rc;

    for(int i=0; i<deleteSequelLength; i++)
    {
        RID rid(deleteSequel[i], deleteSequel[i]*2);
        if((rc = ih.DeleteEntry((void *)&deleteSequel[i], rid)))
            return rc;
    }

    if((rc = InsertSequelOfInt(ih, sequel, sequelLength)))
        return rc;

    // create the xml file
    if((rc = ih.DisplayTree()))
        return (rc);

    if((rc = ixm.CloseIndex(ih)))
        return rc;

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 9\n\n");
    return (0);
}

// Test 10: suppression par la gauche
// merge successifs de feuilles et de noeuds à droite
// changement de racine
RC Test10(void)
{
    RC rc;
    IX_IndexHandle ih;
    int index=0;
    int sequelLength = 20;
    int sequel[20] = {5,20,7,10,11,16,17,3,1,6,4,19,12,18,2,15,9,14,8,13};

    const int deleteSequelLength = 20;
    int deleteSequel[deleteSequelLength] = {20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};

    printf("Test10: Insert entries, delete 20 then 19 then 18...\n");

    if ((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = InsertSequelOfInt(ih, sequel, sequelLength)))
        return rc;


    for(int i=0; i<deleteSequelLength; i++)
    {
        RID rid(deleteSequel[i], deleteSequel[i]*2);
        if((rc = ih.DeleteEntry((void *)&deleteSequel[i], rid)))
            return rc;
    }

    // create the xml file
    if((rc = ih.DisplayTree()))
        return (rc);

    if((rc = ixm.CloseIndex(ih)))
        return rc;

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 10\n\n");
    return (0);
}


//
// Test11 tests inserting a few INT entries into the index.
// Then several scans are performed
//
RC Test11(void)
{
    RC rc;
    IX_IndexHandle ih;
    int index=0;
    int            i;
    int            value=FEW_ENTRIES/3;
    RID            rid;

    printf("Test11: Insert a few INT entries into an index then perform scans... \n");

    if ((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)) ||
            (rc = InsertIntEntries(ih, FEW_ENTRIES)) ||
            (rc = ih.DisplayTree()) ||
            (rc = ixm.CloseIndex(ih)) ||
            (rc = ixm.OpenIndex(FILENAME, index, ih)));

    printf("Scan value: %d\n", value);

    // Scan =
    IX_IndexScan scaneq;
    if ((rc = scaneq.OpenScan(ih, EQ_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scaneq.GetNextEntry(rid))) {
        i++;
    }

    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in =scan.\n", i);

    // Scan <
    IX_IndexScan scanlt;
    if ((rc = scanlt.OpenScan(ih, LT_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scanlt.GetNextEntry(rid))) {
        i++;
    }

    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in <-scan.\n", i);

    // Scan <=
    IX_IndexScan scanle;
    if ((rc = scanle.OpenScan(ih, LE_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scanle.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in <=-scan.\n", i);

    // Scan >
    IX_IndexScan scangt;
    if ((rc = scangt.OpenScan(ih, GT_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scangt.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in >-scan.\n", i);

    // Scan >=
    IX_IndexScan scange;
    if ((rc = scange.OpenScan(ih, GE_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scange.GetNextEntry(rid))) {
        i++;
    }
    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in >=-scan.\n", i);



    // Scan <>
    IX_IndexScan scanne;
    if ((rc = scanne.OpenScan(ih, NE_OP, &value))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scanne.GetNextEntry(rid))) {
        i++;
    }

    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in <>scan.\n", i);



    // Scan noscan
    IX_IndexScan scanno;
    if ((rc = scanno.OpenScan(ih, NO_OP, NULL))) {
        printf("Scan error: opening scan\n");
        return (rc);
    }

    i = 0;
    while (!(rc = scanno.GetNextEntry(rid))) {
        i++;
    }

    if (rc != IX_EOF)
        return (rc);

    printf("Found %d entries in NULL scan.\n", i);





    if(rc = ixm.CloseIndex(ih))
        return (rc);

    LsFiles(FILENAME);

    if ((rc = ixm.DestroyIndex(FILENAME, index)))
        return (rc);

    printf("Passed Test 11\n\n");
    return (0);
}


