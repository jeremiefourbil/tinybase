#include "ix_internal.h"

IX_IndexScan::IX_IndexScan()
{

}

IX_IndexScan::~IX_IndexScan()
{

}



// Open index scan
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
            CompOp compOp,
            void *value,
            ClientHint  pinHint)
{
    RC rc;

    _indexHandle = indexHandle;
    _compOp = compOp;
    _value = value;
    _pinHint = pinHint;


    if (value == NULL)
        return (IX_NULLPOINTER);


    switch(_indexHandle.fileHdr.attrType)
    {
    case INT:
        rc = OpenScan_t<int>();
        break;
    case FLOAT:
        rc = OpenScan_t<float>();
        break;
    case STRING:
        rc = OpenScan_t<char[MAXSTRINGLEN]>();
        break;
    default:
        rc = IX_BADTYPE;
    }

    return rc;
}

template <typename T>
RC IX_IndexScan::OpenScan_t()
{
    RC rc = OK_RC;

    PageNum pageNum;
    pageNum = _indexHandle.fileHdr.rootNum;

    // no root
    if(pageNum == IX_EMPTY)
    {
        _nextLeafNum = IX_EMPTY;
        return 0;
    }

    // let's call the recursion method, start with root
    if(rc = ScanNode_t<T>(pageNum))
        goto err_return;

    return rc;

err_unpin:
    _indexHandle.pfFileHandle.UnpinPage(pageNum);
err_return:
    return (rc);
}



// Insert a new data in the index
template <typename T>
RC IX_IndexScan::ScanNode_t(PageNum iPageNum)
{
    RC rc = OK_RC;

    char *pBuffer;
    int slotIndex;

    // get the current node
    if(rc = _indexHandle.GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the next branch to follow
    slotIndex = 0;
    while( slotIndex < ((IX_PageNode<T> *)pBuffer)->nbFilledSlots
            && comparisonGeneric(*((T*) _value), ((IX_PageNode<T> *)pBuffer)->v[slotIndex]) >= 0 )
    {
        ++slotIndex;
    }

    // the last pointer of the node is reached
    if(slotIndex == ((IX_PageNode<T> *)pBuffer)->nbFilledSlots)
    {
        slotIndex++;
    }

    // the child is a leaf
    if(((IX_PageNode<T> *)pBuffer)->nodeType == LASTINODE || ((IX_PageNode<T> *)pBuffer)->nodeType == ROOTANDLASTINODE)
    {
        PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[slotIndex];

        // the leaf is empty
        if(((IX_PageNode<T> *)pBuffer)->child[slotIndex] == IX_EMPTY)
        {
            // nothing?
        }
        else
        {
            if(rc = ScanLeaf_t<T>(childPageNum))
                goto err_return;
        }

    }
    // the child is a node
    else
    {
        PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[slotIndex];

        // the node is empty
        if(childPageNum == IX_EMPTY)
        {

        }
        else
        {
            if(rc = ScanNode_t<T>(childPageNum))
                goto err_return;
        }
    }


    if(rc = _indexHandle.ReleaseBuffer(iPageNum, false))
        goto err_return;

    return rc;

err_return:
    return (rc);
}

// Insert a new data in the index
template <typename T>
RC IX_IndexScan::ScanLeaf_t(PageNum iPageNum)
{

    RC rc = OK_RC;

    char *pBuffer;
    int slotIndex = -1;

    // get the current node
    if(rc = _indexHandle.GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the right place in the tree
    for(int i=0; i<((IX_PageLeaf<T> *)pBuffer)->nbFilledSlots; i++)
    {
        if(comparisonGeneric(((IX_PageLeaf<T> *)pBuffer)->v[i], *((T*) _value)) == 0)
        {
            slotIndex = i;
        }
    }

    // the value was not found
    if(slotIndex == -1)
    {
        _nextLeafNum = IX_EMPTY;
        _nextLeafSlot = IX_EMPTY;
        _nextRidSlot = IX_EMPTY;
    }
    // the value was found
    if(slotIndex >= 0)
    {
        _nextLeafNum = iPageNum;
        _nextLeafSlot = slotIndex;
        _nextRidSlot = 0;
    }


    if(rc = _indexHandle.ReleaseBuffer(iPageNum, false))
        goto err_return;


    return rc;

err_return:
    return (rc);

}







// Get the next matching entry return IX_EOF if no more matching
// entries.
RC IX_IndexScan::GetNextEntry(RID &rid)
{
    RC rc = IX_EOF;

    switch(_indexHandle.fileHdr.attrType)
    {
    case INT:
        rc = GetNextEntry_t<int>(rid);
    case FLOAT:
        rc = GetNextEntry_t<float>(rid);
    case STRING:
        rc = GetNextEntry_t<char[MAXSTRINGLEN]>(rid);
    }

    return rc;
}


template <typename T>
RC IX_IndexScan::GetNextEntry_t(RID &rid)
{
    RC rc = OK_RC;

    char *pBuffer;

    if(_nextLeafNum == IX_EMPTY)
    {
        return IX_EOF;
    }


    // get the current node
    if(rc = _indexHandle.GetPageBuffer(_nextLeafNum, pBuffer))
        goto err_return;

    // set the right rid
    if(_nextLeafSlot>=0)
    {
        ((IX_PageLeaf<T> *)pBuffer)->rid[_nextLeafSlot];
    }

    // set the next parameters
    _nextLeafNum = IX_EMPTY;
    _nextLeafSlot = IX_EMPTY;
    _nextRidSlot = IX_EMPTY;

    if(rc = _indexHandle.ReleaseBuffer(_nextLeafNum, false))
        goto err_return;

err_return:
    return (rc);
}


// Close index scan
RC IX_IndexScan::CloseScan()
{
    return OK_RC;
}
