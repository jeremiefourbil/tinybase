#include "ix_internal.h"

#include <iostream>

using namespace std;

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
    RC rc = OK_RC;

    _indexHandle = indexHandle;
    _compOp = compOp;
    _value = value;
    _pinHint = pinHint;



    // Full scan operator
    if(_compOp == NO_OP && _value == NULL)
    {
        _nextLeafNum = _indexHandle.fileHdr.firstLeafNum;
        _nextLeafSlot = 0;
        _nextBucketSlot = 0;

        if(_nextLeafNum == IX_EMPTY)
        {
            return IX_INVALID_PAGE_NUMBER;
        }
    }
    // Other operators
    else
    {
        if (value == NULL)
            return (IX_NULLPOINTER);

        switch(_indexHandle.fileHdr.attrType)
        {
        case INT:
            rc = OpenScan_t<int,order_INT>();
            break;
        case FLOAT:
            rc = OpenScan_t<float,order_FLOAT>();
            break;
        case STRING:
            rc = OpenScan_t<char[MAXSTRINGLEN],order_STRING>();
            break;
        default:
            rc = IX_BADTYPE;
        }
    }

    return rc;
}

template <typename T, int n>
RC IX_IndexScan::OpenScan_t()
{
    RC rc = OK_RC;

    switch(_compOp)
    {
    case EQ_OP:
        _direction = DONTMOVE;
        break;
    case LE_OP:
    case LT_OP:
        _direction = LEFT;
        break;
    case GT_OP:
    case GE_OP:
        _direction = RIGHT;
        break;
    case NE_OP:
        _direction = RIGHT;
        break;
    default:
        break;
    }

    if((rc = FindInTree_t<T,n>(_value, _nextLeafNum, _nextLeafSlot, _isValueInTree, _nextValue)))
        goto err_return;

    _nextBucketSlot = 0;


    // the value was not found
    if(!_isValueInTree && _compOp == EQ_OP)
    {
        _nextLeafNum = IX_EMPTY;
        _nextLeafSlot = IX_EMPTY;
    }

    if(_nextLeafNum == IX_EMPTY && _compOp == NE_OP && _direction == RIGHT)
    {
        _direction = LEFT;
        if((rc = FindInTree_t<T,n>(_value, _nextLeafNum, _nextLeafSlot, _isValueInTree, _nextValue)))
            goto err_return;
    }
    return rc;


err_return:
    return (rc);
}


template <typename T, int n>
RC IX_IndexScan::FindInTree_t(const void *iValue, PageNum &oLeafNum, int &oSlotIndex, bool &inTree, void *oValue)
{
    RC rc = OK_RC;

    PageNum rootNum = _indexHandle.fileHdr.rootNum;

    // no root
    if(rootNum == IX_EMPTY)
    {
        return IX_EOF;
    }

    // root is a leaf
    if(_indexHandle.fileHdr.height == 0)
    {
        if(rc = ScanLeaf_t<T,n>(rootNum, iValue, oLeafNum, oSlotIndex, inTree, oValue))
            goto err_return;
    }
    // root is a node
    else
    {
        if(rc = ScanNode_t<T,n>(rootNum, iValue, oLeafNum, oSlotIndex, inTree, oValue))
            goto err_return;
    }

    return rc;

err_return:
    return (rc);
}



template <typename T, int n>
RC IX_IndexScan::ScanNode_t(PageNum iPageNum, const void *iValue, PageNum &oLeafNum, int &oSlotIndex, bool &inTree, void *oValue)
{
    RC rc = OK_RC;

    IX_PageNode<T,n> *pBuffer;
    int pointerIndex;

    // get the current node
    if(rc = _indexHandle.GetNodePageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the branch
    getPointerIndex(pBuffer->v, pBuffer->nbFilledSlots, *((T*) iValue), pointerIndex);


    // the child is a leaf
    if(pBuffer->nodeType == LASTINODE || pBuffer->nodeType == ROOTANDLASTINODE)
    {
        PageNum childPageNum = pBuffer->child[pointerIndex];

        // the leaf is empty
        if(pBuffer->child[pointerIndex] == IX_EMPTY)
        {
            if(rc = _indexHandle.ReleaseBuffer(iPageNum, false))
                goto err_return;

            return IX_EOF;
        }
        else
        {
            if(rc = ScanLeaf_t<T,n>(childPageNum, iValue, oLeafNum, oSlotIndex, inTree, oValue))
                goto err_return;
        }

    }
    // the child is a node
    else
    {
        PageNum childPageNum = pBuffer->child[pointerIndex];

        // the node is empty
        if(childPageNum == IX_EMPTY)
        {
            if(rc = _indexHandle.ReleaseBuffer(iPageNum, false))
                goto err_return;

            return IX_EOF;
        }
        else
        {
            if(rc = ScanNode_t<T,n>(childPageNum, iValue, oLeafNum, oSlotIndex, inTree, oValue))
                goto err_return;
        }
    }

    if(rc = _indexHandle.ReleaseBuffer(iPageNum, false))
        goto err_return;

    return rc;

err_return:
    return (rc);
}


template <typename T, int n>
RC IX_IndexScan::ScanLeaf_t(PageNum iPageNum, const void *iValue, PageNum &oLeafNum, int &oSlotIndex, bool &inTree, void *oValue)
{
    RC rc = OK_RC;

    IX_PageLeaf<T,n> *pBuffer;
    oSlotIndex = -1;

    // get the current node
    if(rc = _indexHandle.GetLeafPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the location
    getSlotIndex(pBuffer->v, pBuffer->nbFilledSlots, *((T*) iValue), oSlotIndex);

    // is it the same value ?
    if(comparisonGeneric(pBuffer->v[oSlotIndex], *((T*) iValue))== 0)
    {
        inTree = true;
        copyGeneric(*((T*) iValue), *((T*) oValue));
        oLeafNum = iPageNum;
    }
    else
    {
        inTree = false;
    }

    if(!inTree)
    {
        if(_direction == LEFT)
        {
            if((rc = ComputePreviousLeafSlot<T,n>(oLeafNum, oSlotIndex, oValue)))
                goto err_return;
        }
        else
        {
            copyGeneric(*((T*) iValue), *((T*) oValue));
            oLeafNum = iPageNum;
        }
    }


    if(rc = _indexHandle.ReleaseBuffer(iPageNum, false))
        goto err_return;


    return rc;

err_return:
    return (rc);

}



// Get the next matching entry
// return IX_EOF if no more matching entries.
RC IX_IndexScan::GetNextEntry(RID &rid)
{
    RC rc = OK_RC;

    switch(_indexHandle.fileHdr.attrType)
    {
    case INT:
        rc = GetNextEntry_t<int,order_INT>(rid);
        break;
    case FLOAT:
        rc = GetNextEntry_t<float,order_FLOAT>(rid);
        break;
    case STRING:
        rc = GetNextEntry_t<char[MAXSTRINGLEN],order_STRING>(rid);
        break;
    default:
        rc = IX_BADTYPE;
    }

    return rc;
}


template <typename T, int n>
RC IX_IndexScan::GetNextEntry_t(RID &rid)
{
    RC rc = OK_RC;

    IX_PageLeaf<T,n> *pBuffer;
    PageNum leafNum = _nextLeafNum;

    if(_nextLeafNum == IX_EMPTY)
    {
        return IX_EOF;
    }

    // get the current node
    if(rc = _indexHandle.GetLeafPageBuffer(leafNum, pBuffer))
        goto err_return;

    // check if the slot exists
    if(_nextLeafSlot >= pBuffer->nbFilledSlots)
    {
        cout << "Scan: débordement mémoire" << endl;
        if((rc = _indexHandle.ReleaseBuffer(leafNum, false)))
            goto err_return;
        return IX_ARRAY_OVERFLOW;
    }

    if(comparisonGeneric(pBuffer->v[_nextLeafSlot], *((T*)_nextValue)) != 0)
    {
        if((rc = FindInTree_t<T,n>(_nextValue, _nextLeafNum, _nextLeafSlot, _isValueInTree, _nextValue)))
            goto err_return;

        _nextBucketSlot = 0;


        // the value was not found
        if(!_isValueInTree && _compOp == EQ_OP)
        {
            _nextLeafNum = IX_EMPTY;
            _nextLeafSlot = IX_EMPTY;
        }

        if(_nextLeafNum == IX_EMPTY && _compOp == NE_OP && _direction == RIGHT)
        {
            _direction = LEFT;
            if((rc = FindInTree_t<T,n>(_value, _nextLeafNum, _nextLeafSlot, _isValueInTree, _nextValue)))
                goto err_return;
        }

        if(_nextLeafNum == IX_EMPTY)
        {
            if((rc = _indexHandle.ReleaseBuffer(leafNum, false)))
                goto err_return;
            return IX_EOF;
        }

        // check if the slot exists
        if(_nextLeafSlot >= pBuffer->nbFilledSlots)
        {
            cout << "Scan: débordement mémoire" << endl;
            if((rc = _indexHandle.ReleaseBuffer(leafNum, false)))
                goto err_return;
            return IX_ARRAY_OVERFLOW;
        }
    }


    // read the rid in the bucket
    rc = ReadBucket(pBuffer->bucket[_nextLeafSlot], rid);

    // if the rid bucket is empty, update the next leaf page num and leaf slot
    if(rc == IX_EOF)
    {
        // switch to find the next place to visit
        switch (_compOp)
        {
        case EQ_OP:
            _nextLeafNum = IX_EMPTY;
            _nextLeafSlot = IX_EMPTY;
            _nextBucketSlot = IX_EMPTY;
            break;
        case LT_OP:
        case LE_OP:
            // get the previous leaf slot
            if((rc = ComputePreviousLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                goto err_return;
            break;
        case GT_OP:
        case GE_OP:
            // get the next leaf slot
            if((rc = ComputeNextLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                goto err_return;
            break;
        case NE_OP:
            // the next is on the right
            if(_direction == RIGHT)
            {
                if((rc = ComputeNextLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                    goto err_return;

                if(_nextLeafNum == IX_EMPTY)
                {
                    _direction = LEFT;
                    if((rc = FindInTree_t<T,n>(_value, _nextLeafNum, _nextLeafSlot, _isValueInTree, _nextValue)))
                        goto err_return;
                }
            }
            else if(_direction == LEFT)
            {
                if((rc = ComputePreviousLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                    goto err_return;
            }
            break;
        case NO_OP:
            // get the next leaf slot
            if((rc = ComputeNextLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                goto err_return;
            break;
        }
    }
    else if(rc)
    {
        goto err_return;
    }

    if(rc = _indexHandle.ReleaseBuffer(leafNum, false))
        goto err_return;

    return rc;

err_return:
    return (rc);
}


RC IX_IndexScan::ReadBucket(PageNum iPageNum, RID &rid)
{
    RC rc = OK_RC;

    char *pBuffer;

    if(iPageNum == IX_EMPTY)
    {
        return IX_INVALID_PAGE_NUMBER;
    }


    // get the current node
    if(rc = _indexHandle.GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // set the rid
    //    cout << "Nb Filled Slot: " << ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots << endl;

    memcpy((void*) &rid,
           pBuffer + sizeof(IX_PageBucketHdr) + _nextBucketSlot * sizeof(RID), sizeof(RID));


    // set the next parameters
    _nextBucketSlot++;

    if(_nextBucketSlot >= ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots)
    {
        _nextBucketSlot = IX_EMPTY;
        if(rc = _indexHandle.ReleaseBuffer(iPageNum, false))
            goto err_return;
        return IX_EOF;
    }



    if(rc = _indexHandle.ReleaseBuffer(iPageNum, false))
        goto err_return;

    return rc;

err_return:
    return (rc);
}


// Close index scan
RC IX_IndexScan::CloseScan()
{
    // delete _value?
    _value = NULL;


    return OK_RC;
}


template <typename T, int n>
RC IX_IndexScan::ComputePreviousLeafSlot(PageNum &ioLeafNum, int &ioSlotIndex, void *oValue)
{
    RC rc = OK_RC;
    IX_PageLeaf<T,n> *pBuffer, *pSecondBuffer;

    PageNum initialPageNum = ioLeafNum;

    if(rc = _indexHandle.GetLeafPageBuffer(initialPageNum, pBuffer))
        goto err_return;

    _nextBucketSlot = 0;

    if(ioSlotIndex > 0)
    {
        ioSlotIndex--;
        copyGeneric(pBuffer->v[ioSlotIndex], *((T*) oValue));
    }
    else
    {
        if(ioLeafNum != IX_EMPTY)
        {
            if(rc = _indexHandle.GetLeafPageBuffer(pBuffer->previous, pSecondBuffer))
                goto err_return;

            if(pSecondBuffer->nbFilledSlots > 0)
            {

                ioSlotIndex = pSecondBuffer->nbFilledSlots - 1;
                copyGeneric(pSecondBuffer->v[ioSlotIndex], *((T*) oValue));
            }
            else
            {
                ioLeafNum = IX_EMPTY;
                ioSlotIndex = IX_EMPTY;
            }

            if(rc = _indexHandle.ReleaseBuffer(pBuffer->previous, false))
                goto err_return;
        }
    }

    if(rc = _indexHandle.ReleaseBuffer(initialPageNum, false))
        goto err_return;

    return rc;

err_return:
    return rc;
}

template <typename T, int n>
RC IX_IndexScan::ComputeNextLeafSlot(PageNum &ioLeafNum, int &ioSlotIndex, void *oValue)
{
    RC rc = OK_RC;
    IX_PageLeaf<T,n> *pBuffer, *pSecondBuffer;

    PageNum initialPageNum = ioLeafNum;

    if(rc = _indexHandle.GetLeafPageBuffer(initialPageNum, pBuffer))
        goto err_return;

    _nextBucketSlot = 0;


    if(ioSlotIndex < pBuffer->nbFilledSlots - 1)
    {
        ioSlotIndex++;
        copyGeneric(pBuffer->v[ioSlotIndex], *((T*) oValue));
    }
    else
    {
        ioLeafNum = pBuffer->next;

        if(ioLeafNum != IX_EMPTY)
        {
            if(rc = _indexHandle.GetLeafPageBuffer(pBuffer->next, pSecondBuffer))
                goto err_return;

            if(pSecondBuffer->nbFilledSlots > 0)
            {
                ioSlotIndex = 0;
                copyGeneric(pSecondBuffer->v[ioSlotIndex], *((T*) oValue));
            }
            else
            {
                ioLeafNum = IX_EMPTY;
                ioSlotIndex = IX_EMPTY;
            }

            if(rc = _indexHandle.ReleaseBuffer(pBuffer->next, false))
                goto err_return;
        }
        else
        {
            if(rc = _indexHandle.ReleaseBuffer(initialPageNum, false))
                goto err_return;

            return IX_EOF;
        }
    }

    if(rc = _indexHandle.ReleaseBuffer(initialPageNum, false))
        goto err_return;

    return rc;

err_return:
    return rc;
}

