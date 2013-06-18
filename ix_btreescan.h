#ifndef IX_BTREESCAN_H
#define IX_BTREESCAN_H

#include "pf.h"
#include "rm_rid.h"
#include "ix_common.h"
#include "ix_btree.h"

class IX_BTreeScan
{
public:
    IX_BTreeScan();
    ~IX_BTreeScan();

    // Open index scan
    RC OpenScan(IX_BTree * ipBTree,
                CompOp compOp,
                void *value,
                ClientHint  pinHint = NO_HINT);

    // Get the next matching entry return IX_EOF if no more matching
    // entries.
    RC GetNextEntry(RID &rid);

    // Close index scan
    RC CloseScan();

private:
    template <typename T, int n>
    RC OpenScan_t();

    template <typename T, int n>
    RC FindInTree_t(const void* iValue, PageNum &oLeafNum, int &oSlotIndex, bool &inTree, void *oValue);

    template <typename T, int n>
    RC ScanNode_t(PageNum iPageNum, const void *iValue, PageNum &oLeafNum, int &oSlotIndex, bool &inTree, void *oValue);

    template <typename T, int n>
    RC ScanLeaf_t(PageNum iPageNum, const void *iValue, PageNum &oLeafNum, int &oSlotIndex, bool &inTree, void *oValue);

    template <typename T, int n>
    RC GetNextEntry_t(RID &rid);

    RC ReadBucket(PageNum iPageNum, RID &rid);

    template <typename T, int n>
    RC ComputePreviousLeafSlot(PageNum &ioLeafNum, int &ioSlotIndex, void *oValue);

    template <typename T, int n>
    RC ComputeNextLeafSlot(PageNum &ioLeafNum, int &ioSlotIndex, void *oValue);

    template <typename T, int n>
    RC ReadFirstValue(PageNum &iLeafNum, int &iSlotIndex, void * oValue);

    template <typename T, int n>
    RC GreaterOnTheRight(const PageNum iLeafNum, const int iSlotIndex, const void * oValue);

    CompOp _compOp;
    void * _value;
    ClientHint  _pinHint;

    PageNum _nextLeafNum;
    int _nextLeafSlot;
    int _nextBucketSlot;
    void * _nextValue;

    bool _isValueInTree;

    IX_BTree::Direction _direction;

    IX_BTree * _pBTree;
};

#endif // IX_BTREESCAN_H
