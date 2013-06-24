//
// ix.h
//
//   Index Manager Component Interface
//

#ifndef IX_H
#define IX_H




//#define IX_USE_HASH




// Please do not include any other files than the ones below in this file.

#include "redbase.h"  // Please don't change these lines
#include "rm_rid.h"  // Please don't change these lines
#include "pf.h"

#include "ix_btree.h"
#include "ix_hash.h"

#include "ix_btreescan.h"
#include "ix_hashscan.h"


// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
    friend class IX_Manager;
    friend class IX_IndexScan;
public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Force index files to disk
    RC ForcePages();

    // Display
    RC DisplayTree();

private:
    IX_BTree *pBTree;
#ifdef IX_USE_HASH
    IX_Hash *pHash;
#endif

    IX_FileHdr fileHdr;
    PF_FileHandle pfFileHandle;
};

//
// IX_IndexScan: condition-based scan of index entries
//
class IX_IndexScan {
public:
    IX_IndexScan();
    ~IX_IndexScan();

    // Open index scan
    RC OpenScan(const IX_IndexHandle &indexHandle,
                CompOp compOp,
                void *value,
                ClientHint  pinHint = NO_HINT);

    // Get the next matching entry return IX_EOF if no more matching
    // entries.
    RC GetNextEntry(RID &rid);

    // Close index scan
    RC CloseScan();

private:
    CompOp _compOp;

    IX_BTreeScan * pBTreeScan;
#ifdef IX_USE_HASH
    IX_HashScan *pHashScan;
#endif
};

//
// IX_Manager: provides IX index file management
//
class IX_Manager {
public:
    IX_Manager(PF_Manager &pfm);
    ~IX_Manager();

    // Create a new Index
    RC CreateIndex(const char *fileName, int indexNo,
                   AttrType attrType, int attrLength);

    // Destroy and Index
    RC DestroyIndex(const char *fileName, int indexNo);

    // Open an Index
    RC OpenIndex(const char *fileName, int indexNo,
                 IX_IndexHandle &indexHandle);

    // Close an Index
    RC CloseIndex(IX_IndexHandle &indexHandle);

private:
    const char* GenerateFileName(const char *fileName, int indexNo);

    PF_Manager *pPfm;
};

//
// Print-error function
//
void IX_PrintError(RC rc);

#define IX_INSUFFISANT_PAGE_SIZE        (START_IX_ERR - 0)
#define IX_NULLPOINTER                  (START_IX_ERR - 1)
#define IX_BADTYPE                      (START_IX_ERR - 2)
#define IX_ENTRY_DOES_NOT_EXIST         (START_IX_ERR - 3)
#define IX_ARRAY_OVERFLOW               (START_IX_ERR - 4)
#define IX_INVALID_PAGE_NUMBER          (START_IX_ERR - 5)
#define IX_DELETE_INVALID_CASE          (START_IX_ERR - 6)
#define IX_BUCKET_OVERFLOW              (START_IX_ERR - 7)
#define IX_RID_ALREADY_IN_BUCKET        (START_IX_ERR - 8)
#define IX_NON_POSITIVE_INDEX_NUMBER    (START_IX_ERR - 9)
#define IX_BADOPERATOR                  (START_IX_ERR - 10)
#define IX_ALREADY_OPEN                 (START_IX_ERR - 11)
#define IX_LASTERROR                    IX_ALREADY_OPEN
//#define RM_INVALIDRECSIZE  (START_RM_WARN + 2) // invalid record size
//#define RM_INVALIDSLOTNUM  (START_RM_WARN + 3) // invalid slot number
//#define RM_RECORDNOTFOUND  (START_RM_WARN + 4) // record not found
//#define RM_INVALIDCOMPOP   (START_RM_WARN + 5) // invalid comparison operator
//#define RM_INVALIDATTR     (START_RM_WARN + 6) // invalid attribute parameters
//#define RM_NULLPOINTER     (START_RM_WARN + 7) // pointer is null
//#define RM_SCANOPEN        (START_RM_WARN + 8) // scan is open
//#define RM_CLOSEDSCAN      (START_RM_WARN + 9) // scan is closed
//#define RM_CLOSEDFILE      (START_RM_WARN + 10)// file handle is closed
#define IX_LASTWARN        START_IX_WARN

#define IX_EOF             PF_EOF

#endif
