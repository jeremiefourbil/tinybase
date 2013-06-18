#ifndef IX_HASHSCAN_H
#define IX_HASHSCAN_H

#include "pf.h"
#include "rm_rid.h"
#include "ix_common.h"
#include "ix_hash.h"

class IX_HashScan
{

public:
    IX_HashScan();
    ~IX_HashScan();

    // Open index scan
    RC OpenScan(IX_Hash * ipHash,
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

    RC ScanBucket(const PageNum iPageNum);

    template <typename T>
    RC GetNextEntry_t(RID &rid);

    RC ReadBucket(PageNum iPageNum, RID &rid);

    CompOp _compOp;
    void * _value;
    ClientHint  _pinHint;

    PageNum _nextBucketNum;
    PageNum _nextRidBucketNum;
    int _nextRidBucketSlot;
    int _nextHash;

    IX_Hash * _pHash;
};

#endif // IX_HASHSCAN_H
