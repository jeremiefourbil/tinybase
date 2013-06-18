#ifndef IX_HASH_H
#define IX_HASH_H

#include "pf.h"
#include "rm_rid.h"
#include "ix_common.h"


class IX_Hash
{
    friend class IX_IndexHandle;
    friend class IX_Manager;
    friend class IX_IndexScan;
    friend class IX_HashScan;
public:
    IX_Hash();
    ~IX_Hash();

    struct IX_DirectoryHdr {
        int depth;
    };

//    struct IX_BucketHdr {
//        int depth;
//        int nbFilledSlots;
//        int nbMaxSlots;
//    };

    template <typename T, int n>
    struct IX_Bucket {
        int depth;
        int nbFilledSlots;
        T v[n];
        PageNum child[n];
    };

//    struct IX_BucketValue {
//        int v;
//        PageNum bucketNum;
//    };

    struct IX_RidBucketHdr {
        int nbFilledSlots;
    };

    void setFileHandle(PF_FileHandle & iPfFileHandle) { _pPfFileHandle = &iPfFileHandle; }
    void setFileHdr(IX_FileHdr & iFileHdr) { _pFileHdr = &iFileHdr; }

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Display
    RC DisplayTree();

private:

    // insertion
    template <typename T, int n>
    RC InsertEntry_t(T iValue, const RID &rid);

    template <typename T, int n>
    RC InsertEntryInBucket_t(PageNum iPageNum, T iValue, const RID &rid);

    RC InsertEntryInRidBucket(PageNum iPageNum, const RID &rid);

    template <typename T, int n>
    RC DivideBucketInTwo_t(const PageNum iBucketNum, PageNum &oBucketNum);

    template <typename T, int n>
    RC IsPossibleToInsertInBucket_t(const PageNum iPageNum, bool &needToDivide, int &childDepth);

    // deletion
    template <typename T, int n>
    RC DeleteEntry_t(T iValue, const RID &rid);

    template <typename T, int n>
    RC DeleteEntryInBucket_t(PageNum &ioPageNum, const RID &rid);

    // print
    template <typename T, int n>
    RC DisplayTree_t();

    template <typename T, int n>
    RC DisplayBucket_t(const PageNum iPageNum);

    template <typename T, int n>
    RC AllocateDirectoryPage_t(PageNum &oPageNum);

    template <typename T, int n>
    RC AllocateBucketPage_t(const int depth, PageNum &oPageNum);

    RC AllocateRidBucketPage(PageNum &oPageNum);

    // page management
    RC GetPageBuffer(const PageNum &iPageNum, char * &pBuffer) const;
    RC ReleaseBuffer(const PageNum &iPageNum, bool isDirty) const;

    template <typename T, int n>
    RC GetBucketBuffer(const PageNum &iPageNum, IX_Bucket<T,n> * & pBuffer) const;

    PF_FileHandle *_pPfFileHandle;
    IX_FileHdr * _pFileHdr;
};

#endif // IX_HASH_H
