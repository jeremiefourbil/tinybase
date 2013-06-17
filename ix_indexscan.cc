#include "ix_internal.h"

#include "ix_btreescan.h"

#include <iostream>

using namespace std;

IX_IndexScan::IX_IndexScan()
{
    pBTreeScan = NULL;
}

IX_IndexScan::~IX_IndexScan()
{
    if(pBTreeScan)
    {
        delete pBTreeScan;
        pBTreeScan = NULL;
    }
}

// Open index scan
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
                          CompOp compOp,
                          void *value,
                          ClientHint  pinHint)
{
    RC rc = OK_RC;

    if(pBTreeScan != NULL)
    {
        delete pBTreeScan;
        pBTreeScan = NULL;
    }

    pBTreeScan = new IX_BTreeScan();

    if((rc = pBTreeScan->OpenScan(indexHandle.pBTree, compOp, value, pinHint)))
        goto err_return;

    return rc;

err_return:
    return rc;
}

// Get the next matching entry
// return IX_EOF if no more matching entries.
RC IX_IndexScan::GetNextEntry(RID &rid)
{
    RC rc = OK_RC;

    if((rc = pBTreeScan->GetNextEntry(rid)))
        goto err_return;

    return rc;

err_return:
    return rc;
}


// Close index scan
RC IX_IndexScan::CloseScan()
{
    RC rc = OK_RC;

    if((rc = pBTreeScan->CloseScan()))
        goto err_return;

    if(pBTreeScan)
    {
        delete pBTreeScan;
        pBTreeScan = NULL;
    }

    return rc;

err_return:
    return rc;
}

