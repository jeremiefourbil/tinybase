#include "ix_internal.h"

#include "ix_btreescan.h"
#include "ix_hashscan.h"

#include <iostream>

using namespace std;

IX_IndexScan::IX_IndexScan()
{
    pBTreeScan = NULL;

#ifdef IX_USE_HASH
    pHashScan = NULL;
#endif
}

IX_IndexScan::~IX_IndexScan()
{
    if(pBTreeScan)
    {
        delete pBTreeScan;
        pBTreeScan = NULL;
    }

#ifdef IX_USE_HASH
    if(pHashScan)
    {
        delete pHashScan;
        pHashScan = NULL;
    }
#endif
}

// Open index scan
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
                          CompOp compOp,
                          void *value,
                          ClientHint  pinHint)
{
    RC rc = OK_RC;

    _compOp = compOp;

    if(pBTreeScan != NULL)
    {
        delete pBTreeScan;
        pBTreeScan = NULL;
    }

    pBTreeScan = new IX_BTreeScan();

#ifdef IX_USE_HASH
    if(pHashScan)
    {
        delete pHashScan;
        pHashScan = NULL;
    }

    pHashScan = new IX_HashScan();
#endif


    if(_compOp == EQ_OP)
    {
#ifdef IX_USE_HASH
        if((rc = pHashScan->OpenScan(indexHandle.pHash, _compOp, value, pinHint)))
            goto err_return;
#else
        if((rc = pBTreeScan->OpenScan(indexHandle.pBTree, _compOp, value, pinHint)))
            goto err_return;
#endif
    }
    else
    {
        if((rc = pBTreeScan->OpenScan(indexHandle.pBTree, _compOp, value, pinHint)))
            goto err_return;
    }

    return rc;

err_return:
    return rc;
}

// Get the next matching entry
// return IX_EOF if no more matching entries.
RC IX_IndexScan::GetNextEntry(RID &rid)
{
    RC rc = OK_RC;

    if(_compOp == EQ_OP)
    {
#ifdef IX_USE_HASH
        if((rc = pHashScan->GetNextEntry(rid)))
            goto err_return;
#else
        if((rc = pBTreeScan->GetNextEntry(rid)))
            goto err_return;
#endif
    }
    else
    {
        if((rc = pBTreeScan->GetNextEntry(rid)))
            goto err_return;
    }

    return rc;

err_return:
    return rc;
}


// Close index scan
RC IX_IndexScan::CloseScan()
{
    RC rc = OK_RC;

    if(_compOp == EQ_OP)
    {
#ifdef IX_USE_HASH
        if((rc = pHashScan->CloseScan()))
            goto err_return;
#else
        if((rc = pBTreeScan->CloseScan()))
            goto err_return;
#endif
    }
    else
    {
        if((rc = pBTreeScan->CloseScan()))
            goto err_return;
    }


    if(pBTreeScan)
    {
        delete pBTreeScan;
        pBTreeScan = NULL;
    }

#ifdef IX_USE_HASH
    if(pHashScan)
    {
        delete pHashScan;
        pHashScan = NULL;
    }
#endif

    return rc;

err_return:
    return rc;
}

