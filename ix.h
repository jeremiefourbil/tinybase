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

enum Direction {
    LEFT,
    RIGHT
};

struct IX_FileHdr {
    PageNum rootNum;
    AttrType attrType;
    int attrLength;
};

template <typename T>
struct IX_PageNode {
    PageNum parent;
    NodeType nodeType;

    int nbFilledSlots;
    T v[4];

    PageNum child[5];
};

template <typename T>
struct IX_PageLeaf {
    PageNum parent;

    PageNum previous;
    PageNum next;

    int nbFilledSlots;
    T v[4];
    PageNum bucket[4];
};





//
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
    // insertion
    template <typename T>
    RC InsertEntry_t(void *pData, const RID &rid);

    template <typename T>
    RC InsertEntryInNode_t(PageNum iPageNum, void *pData, const RID &rid, PageNum &newChildPageNum,T &medianChildValue);

    template <typename T>
    RC InsertEntryInLeaf_t(PageNum iPageNum, void *pData, const RID &rid, PageNum &newChildPageNum,T &medianValue);

    RC InsertEntryInBucket(PageNum iPageNum, const RID &rid);

    // deletion
    template <typename T>
    RC DeleteEntry_t(T iValue, const RID &rid);

    template <typename T>
    RC DeleteEntryInNode_t(PageNum iPageNum, T iValue, const RID &rid);

    template <typename T>
    RC DeleteEntryInLeaf_t(PageNum iPageNum, T iValue, const RID &rid);

    RC DeleteEntryInBucket(PageNum &ioPageNum, const RID &rid);

    // allocation
    template <typename T>
    RC AllocateNodePage_t(const NodeType nodeType, const PageNum parent, PageNum &oPageNum);

    template <typename T>
    RC AllocateLeafPage_t(const PageNum parent, PageNum &oPageNum);

    RC AllocateBucketPage(const PageNum parent, PageNum &oPageNum);

    // page management
    RC GetPageBuffer(const PageNum &iPageNum, char * &pBuffer) const;

    template <typename T>
    RC GetNodePageBuffer(const PageNum &iPageNum, IX_PageNode<T> * & pBuffer) const;

    template <typename T>
    RC GetLeafPageBuffer(const PageNum &iPageNum, IX_PageLeaf<T> * & pBuffer) const;

    RC ReleaseBuffer(const PageNum &iPageNum, bool isDirty) const;

    // pour les noeuds
    template <typename T>
    RC RedistributeValuesAndChildren(void *pBufferCurrentNode, void *pBufferNewNode,T &medianChildValue, T &medianParentValue,const PageNum &newNodePageNum);

    // pour les feuilles
    template <typename T>
    RC RedistributeValuesAndBuckets(void *pBufferCurrentLeaf, void *pBufferNewLeaf, void *pData, T &medianValue, const PageNum &bucketPageNum);


    // debugging

    template <typename T>
    RC DisplayTree_t();
    template <typename T>
    RC DisplayNode_t(const PageNum pageNum,const int &fatherNodeId, int &currentNodeId,int &currentEdgeId);
    template <typename T>
    RC DisplayLeaf_t(const PageNum page,const int &fatherNodeId, int &currentNodeId,int &currentEdgeId);

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

private:
    template <typename T>
    RC OpenScan_t();

    template <typename T>
    RC ScanNode_t(PageNum iPageNum);

    template <typename T>
    RC ScanLeaf_t(PageNum iPageNum);

    template <typename T>
    RC GetNextEntry_t(RID &rid);

    RC ReadBucket(PageNum iPageNum, RID &rid);

    IX_IndexHandle _indexHandle;
    CompOp _compOp;
    void * _value;
    ClientHint  _pinHint;

    PageNum _nextLeafNum;
    int _nextLeafSlot;
    int _nextBucketSlot;
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

#define IX_INSUFFISANT_PAGE_SIZE        (START_IX_ERR + 0)
#define IX_NULLPOINTER                  (START_IX_ERR + 1)
#define IX_BADTYPE                      (START_IX_ERR + 2)
#define IX_ENTRY_DOES_NOT_EXIST         (START_IX_ERR + 3)
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
