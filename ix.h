//
// ix.h
//
//   Index Manager Component Interface
//

#ifndef IX_H
#define IX_H

// Please do not include any other files than the ones below in this file.

#include "redbase.h"  // Please don't change these lines
#include "rm_rid.h"  // Please don't change these lines
#include "pf.h"



enum NodeType {
    ROOT,
    INODE,
    LASTINODE,
    ROOTANDLASTINODE
};

struct IX_FileHdr {
    PageNum rootNum;
    AttrType attrType;
    int attrLength;
};





//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
    friend class IX_Manager;
public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Force index files to disk
    RC ForcePages();

private:
    template <typename T>
    RC InsertEntry_t(void *pData, const RID &rid);

    template <typename T>
    RC InsertEntryInNode_t(PageNum iPageNum, void *pData, const RID &rid);

    template <typename T>
    RC AllocateNodePage_t(const NodeType nodeType, const PageNum parent, PageNum &oPageNum);

    template <typename T>
    RC AllocateLeafPage_t(const PageNum parent, PageNum &oPageNum);

    RC GetPageBuffer(const PageNum &iPageNum, char *pBuffer);
    RC ReleaseBuffer(const PageNum &iPageNum, bool isDirty);


    PF_FileHandle pfFileHandle;
    IX_FileHdr fileHdr;
    int bHdrChanged;
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
    RC CreateRoot();
    const char* GenerateFileName(const char *fileName, int indexNo);

    PF_Manager *pPfm;
};

//
// Print-error function
//
void IX_PrintError(RC rc);

#define IX_INSUFFISANT_PAGE_SIZE        (START_IX_ERR + 0)
#define IX_NULLPOINTER                  (START_IX_ERR + 1)
//#define RM_INVALIDRECSIZE  (START_RM_WARN + 2) // invalid record size
//#define RM_INVALIDSLOTNUM  (START_RM_WARN + 3) // invalid slot number
//#define RM_RECORDNOTFOUND  (START_RM_WARN + 4) // record not found
//#define RM_INVALIDCOMPOP   (START_RM_WARN + 5) // invalid comparison operator
//#define RM_INVALIDATTR     (START_RM_WARN + 6) // invalid attribute parameters
//#define RM_NULLPOINTER     (START_RM_WARN + 7) // pointer is null
//#define RM_SCANOPEN        (START_RM_WARN + 8) // scan is open
//#define RM_CLOSEDSCAN      (START_RM_WARN + 9) // scan is closed
//#define RM_CLOSEDFILE      (START_RM_WARN + 10)// file handle is closed
//#define RM_LASTWARN        RM_CLOSEDFILE

#define IX_EOF             PF_EOF

#endif
