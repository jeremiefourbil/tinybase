#include "ix_btreescan.h"
#include "ix_internal.h"

#include "ix_btree.h"

#include <iostream>

using namespace std;

IX_BTreeScan::IX_BTreeScan()
{
    _pBTree = NULL;
}

IX_BTreeScan::~IX_BTreeScan()
{
    _pBTree = NULL;
}

// Open index scan
RC IX_BTreeScan::OpenScan(IX_BTree * ipBTree,
                          CompOp compOp,
                          void *value,
                          ClientHint  pinHint)
{
    RC rc = OK_RC;

    _pBTree = ipBTree;
    _compOp = compOp;
    _value = value;
    _pinHint = pinHint;

    _nextValue = NULL;


    // Full scan operator
    if(_compOp == NO_OP && _value == NULL)
    {
        _nextLeafNum = _pBTree->_pFileHdr->firstLeafNum;
        _nextLeafSlot = 0;
        _nextBucketSlot = 0;
        _direction = IX_BTree::RIGHT;

        if(_nextLeafNum == IX_EMPTY)
        {
            return IX_INVALID_PAGE_NUMBER;
        }

        switch(_pBTree->_pFileHdr->attrType)
        {
        case INT:
            _nextValue = new int();
            if(_nextValue == NULL) return IX_NULLPOINTER;
            rc = ReadFirstValue<int,order_INT>(_nextLeafNum, _nextLeafSlot, _nextValue);
            break;
        case FLOAT:
            _nextValue = new float();
            if(_nextValue == NULL) return IX_NULLPOINTER;
            rc = ReadFirstValue<float,order_FLOAT>(_nextLeafNum, _nextLeafSlot, _nextValue);
            break;
        case STRING:
            _nextValue = new char[MAXSTRINGLEN];
            if(_nextValue == NULL) return IX_NULLPOINTER;
            rc = ReadFirstValue<char[MAXSTRINGLEN],order_STRING>(_nextLeafNum, _nextLeafSlot, _nextValue);
            break;
        default:
            rc = IX_BADTYPE;
        }
    }
    // Other operators
    else
    {
        if (value == NULL)
            return (IX_NULLPOINTER);

        switch(_pBTree->_pFileHdr->attrType)
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
RC IX_BTreeScan::OpenScan_t()
{
    RC rc = OK_RC;

    switch(_pBTree->_pFileHdr->attrType)
    {
    case INT:
        _nextValue = new int();
        break;
    case FLOAT:
        _nextValue = new float();
        break;
    case STRING:
        _nextValue = new char[MAXSTRINGLEN];
        break;
    default:
        rc = IX_BADTYPE;
    }

    switch(_compOp)
    {
    case EQ_OP:
        _direction = IX_BTree::DONTMOVE;
        break;
    case LE_OP:
    case LT_OP:
        _direction = IX_BTree::LEFT;
        break;
    case GT_OP:
    case GE_OP:
        _direction = IX_BTree::RIGHT;
        break;
    case NE_OP:
        _direction = IX_BTree::RIGHT;
        break;
    default:
        break;
    }

    if((rc = FindInTree_t<T,n>(_value, _nextLeafNum, _nextLeafSlot, _isValueInTree, _nextValue)))
        goto err_return;

    _nextBucketSlot = 0;


    switch(_compOp)
    {
    case EQ_OP:
        if(!_isValueInTree)
        {
            _nextLeafNum = IX_EMPTY;
            _nextLeafSlot = IX_EMPTY;
        }
        break;
    case LT_OP:
        if(_isValueInTree)
        {
            if((rc = ComputePreviousLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                goto err_return;
        }
        break;
    case GE_OP:
        // to fix >= for a value > than all the values in the tree
        if(!_isValueInTree)
        {
            if((rc = GreaterOnTheRight<T,n>(_nextLeafNum, _nextLeafSlot, _value)))
            {
                if(rc == IX_EOF)
                {
                    _nextLeafNum = IX_EMPTY;
                    _nextLeafSlot = IX_EMPTY;
                }
                else
                    goto err_return;
            }
        }
        break;
    case GT_OP:
        // to fix > for a value > than all the values in the tree
        if(!_isValueInTree)
        {
            if((rc = GreaterOnTheRight<T,n>(_nextLeafNum, _nextLeafSlot, _value)))
            {
                if(rc == IX_EOF)
                {
                    _nextLeafNum = IX_EMPTY;
                    _nextLeafSlot = IX_EMPTY;
                }
                else
                    goto err_return;
            }
        }
        else if(_isValueInTree)
        {
            if((rc = ComputeNextLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                goto err_return;
        }
        break;
    case NE_OP:
        if(_isValueInTree)
        {
            if((rc = ComputeNextLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                goto err_return;
        }
        else if(_nextLeafNum == IX_EMPTY)
        {
            _direction = IX_BTree::LEFT;
            if((rc = FindInTree_t<T,n>(_value, _nextLeafNum, _nextLeafSlot, _isValueInTree, _nextValue)))
                goto err_return;

            if(_isValueInTree)
            {
                if((rc = ComputePreviousLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                    goto err_return;
            }
        }
        break;
    default:
        break;
    }

    return OK_RC;


err_return:
    return (rc);
}


template <typename T, int n>
RC IX_BTreeScan::FindInTree_t(const void *iValue, PageNum &oLeafNum, int &oSlotIndex, bool &inTree, void *oValue)
{
    RC rc = OK_RC;

    PageNum rootNum = _pBTree->_pFileHdr->rootNum;

    // no root
    if(rootNum == IX_EMPTY)
    {
        return IX_EOF;
    }

    // root is a leaf
    if(_pBTree->_pFileHdr->height == 0)
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
RC IX_BTreeScan::ScanNode_t(PageNum iPageNum, const void *iValue, PageNum &oLeafNum, int &oSlotIndex, bool &inTree, void *oValue)
{
    RC rc = OK_RC;

    IX_BTree::IX_PageNode<T,n> *pBuffer;
    int pointerIndex;

    // get the current node
    if(rc = _pBTree->GetNodePageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the branch
    getPointerIndex(pBuffer->v, pBuffer->nbFilledSlots, *((T*) iValue), pointerIndex);


    // the child is a leaf
    if(pBuffer->nodeType == IX_BTree::LASTINODE || pBuffer->nodeType == IX_BTree::ROOTANDLASTINODE)
    {
        PageNum childPageNum = pBuffer->child[pointerIndex];

        // the leaf is empty
        if(pBuffer->child[pointerIndex] == IX_EMPTY)
        {
            if(rc = _pBTree->ReleaseBuffer(iPageNum, false))
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
            if(rc = _pBTree->ReleaseBuffer(iPageNum, false))
                goto err_return;

            return IX_EOF;
        }
        else
        {
            if(rc = ScanNode_t<T,n>(childPageNum, iValue, oLeafNum, oSlotIndex, inTree, oValue))
                goto err_return;
        }
    }

    if(rc = _pBTree->ReleaseBuffer(iPageNum, false))
        goto err_return;

    return rc;

err_return:
    return (rc);
}


template <typename T, int n>
RC IX_BTreeScan::ScanLeaf_t(PageNum iPageNum, const void *iValue, PageNum &oLeafNum, int &oSlotIndex, bool &inTree, void *oValue)
{
    RC rc = OK_RC;

    IX_BTree::IX_PageLeaf<T,n> *pBuffer;
    oSlotIndex = -1;
    int comparison;

    // get the current node
    if(rc = _pBTree->GetLeafPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the location
    getSlotIndex(pBuffer->v, pBuffer->nbFilledSlots, *((T*) iValue), oSlotIndex);

    // is it the same value ?
    comparison = comparisonGeneric(pBuffer->v[oSlotIndex], *((T*) iValue));
    if(comparison == 0)
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
        oLeafNum = iPageNum;

        if(_direction == IX_BTree::LEFT)
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


    if(rc = _pBTree->ReleaseBuffer(iPageNum, false))
        goto err_return;


    return rc;

err_return:
    return (rc);

}



// Get the next matching entry
// return IX_EOF if no more matching entries.
RC IX_BTreeScan::GetNextEntry(RID &rid)
{
    RC rc = OK_RC;

    switch(_pBTree->_pFileHdr->attrType)
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
RC IX_BTreeScan::GetNextEntry_t(RID &rid)
{
    RC rc = OK_RC;
    RC rcBucket;

    IX_BTree::IX_PageLeaf<T,n> *pBuffer;
    PageNum leafNum = _nextLeafNum;

    bool needToResetSearch = false;

    if(_nextLeafNum == IX_EMPTY)
    {
        return IX_EOF;
    }

    // get the current node
    if((rc = _pBTree->GetLeafPageBuffer(leafNum, pBuffer)))
    {
        if(rc != PF_INVALIDPAGE)
            goto err_return;

        else
            needToResetSearch = true;
    }

    if(!rc)
    {
        // check if the slot exists
        if(_nextLeafSlot >= pBuffer->nbFilledSlots)
        {
            needToResetSearch = true;
        }

        if(comparisonGeneric(pBuffer->v[_nextLeafSlot], *((T*)_nextValue)) != 0)
        {
            needToResetSearch = true;
        }

        if(needToResetSearch)
        {
            if(rc = _pBTree->ReleaseBuffer(leafNum, false))
                goto err_return;
        }
    }

    if(needToResetSearch)
    {
        if(_compOp == EQ_OP)
        {
            _nextLeafNum = IX_EMPTY;
            _nextLeafSlot = IX_EMPTY;

            return IX_EOF;
        }
        else
        {
            if((rc = FindInTree_t<T,n>(_nextValue, _nextLeafNum, _nextLeafSlot, _isValueInTree, _nextValue)))
                goto err_return;

            _nextBucketSlot = 0;

            if(_compOp == NE_OP && _direction == IX_BTree::RIGHT && _nextLeafNum == IX_EMPTY)
            {

                _direction = IX_BTree::LEFT;

                if((rc = FindInTree_t<T,n>(_value, _nextLeafNum, _nextLeafSlot, _isValueInTree, _nextValue)))
                    goto err_return;

                if(_isValueInTree)
                {
                    if((rc = ComputePreviousLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)));

                }
            }

            if(_nextLeafNum == IX_EMPTY)
            {
                return IX_EOF;
            }

            leafNum = _nextLeafNum;

            if((rc = _pBTree->GetLeafPageBuffer(leafNum, pBuffer)))
                goto err_return;

            // check if the slot exists, otherwise the tree does not contain any more entry
            if(_nextLeafSlot >= pBuffer->nbFilledSlots)
            {
                if((rc = _pBTree->ReleaseBuffer(leafNum, false)))
                    goto err_return;
                return IX_EOF;
            }
        }
    }

    // read the rid in the bucket
    rcBucket = ReadBucket(pBuffer->bucket[_nextLeafSlot], rid);

    if(rc = _pBTree->ReleaseBuffer(leafNum, false))
        goto err_return;

    rc = rcBucket;
    if(rc && rc != IX_EOF)
        goto err_return;

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
            // the next is on the IX_BTree::RIGHT
            if(_direction == IX_BTree::RIGHT)
            {
                if((rc = ComputeNextLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                    goto err_return;

                if(_nextLeafNum == IX_EMPTY)
                {
                    _direction = IX_BTree::LEFT;
                    if((rc = FindInTree_t<T,n>(_value, _nextLeafNum, _nextLeafSlot, _isValueInTree, _nextValue)))
                        goto err_return;

                    if(_isValueInTree)
                    {
                        if((rc = ComputePreviousLeafSlot<T,n>(_nextLeafNum, _nextLeafSlot, _nextValue)))
                            goto err_return;
                    }
                }
            }
            else if(_direction == IX_BTree::LEFT)
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

    return OK_RC;

err_return:
    return (rc);
}


RC IX_BTreeScan::ReadBucket(PageNum iPageNum, RID &rid)
{
    RC rc = OK_RC;

    char *pBuffer;

    if(iPageNum == IX_EMPTY)
    {
        return IX_INVALID_PAGE_NUMBER;
    }


    // get the current node
    if(rc = _pBTree->GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // get the rid
    memcpy((void*) &rid,
           pBuffer + sizeof(IX_PageBucketHdr) + _nextBucketSlot * sizeof(RID), sizeof(RID));


    // set the next parameters
    _nextBucketSlot++;

    if(_nextBucketSlot >= ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots)
    {
        _nextBucketSlot = IX_EMPTY;
        if(rc = _pBTree->ReleaseBuffer(iPageNum, false))
            goto err_return;
        return IX_EOF;
    }



    if(rc = _pBTree->ReleaseBuffer(iPageNum, false))
        goto err_return;

    return rc;

err_return:
    return (rc);
}


// Close index scan
RC IX_BTreeScan::CloseScan()
{
    // delete _value?
    _value = NULL;

    if(_nextValue != NULL)
    {
        delete _nextValue;
        _nextValue = NULL;
    }

    _pBTree = NULL;


    return OK_RC;
}


template <typename T, int n>
RC IX_BTreeScan::ComputePreviousLeafSlot(PageNum &ioLeafNum, int &ioSlotIndex, void *oValue)
{
    RC rc = OK_RC;
    IX_BTree::IX_PageLeaf<T,n> *pBuffer, *pSecondBuffer;

    PageNum initialPageNum = ioLeafNum;

    if(rc = _pBTree->GetLeafPageBuffer(initialPageNum, pBuffer))
        goto err_return;

    _nextBucketSlot = 0;

    if(ioSlotIndex > 0)
    {
        ioSlotIndex--;
        copyGeneric(pBuffer->v[ioSlotIndex], *((T*) oValue));
    }
    else
    {
        ioLeafNum = pBuffer->previous;

        if(ioLeafNum != IX_EMPTY)
        {
            if(rc = _pBTree->GetLeafPageBuffer(ioLeafNum, pSecondBuffer))
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

            if(rc = _pBTree->ReleaseBuffer(ioLeafNum, false))
                goto err_return;
        }
    }

    if(rc = _pBTree->ReleaseBuffer(initialPageNum, false))
        goto err_return;

    return rc;

err_return:
    return rc;
}

template <typename T, int n>
RC IX_BTreeScan::ComputeNextLeafSlot(PageNum &ioLeafNum, int &ioSlotIndex, void *oValue)
{
    RC rc = OK_RC;
    IX_BTree::IX_PageLeaf<T,n> *pBuffer, *pSecondBuffer;

    PageNum initialPageNum = ioLeafNum;

    if(rc = _pBTree->GetLeafPageBuffer(initialPageNum, pBuffer))
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
            if(rc = _pBTree->GetLeafPageBuffer(ioLeafNum, pSecondBuffer))
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

            if(rc = _pBTree->ReleaseBuffer(ioLeafNum, false))
                goto err_return;
        }
    }

    if(rc = _pBTree->ReleaseBuffer(initialPageNum, false))
        goto err_return;

    return rc;

err_return:
    return rc;
}

template <typename T, int n>
RC IX_BTreeScan::ReadFirstValue(PageNum &iLeafNum, int &iSlotIndex, void * oValue)
{
    RC rc = OK_RC;
    IX_BTree::IX_PageLeaf<T,n> *pBuffer;

    if(iLeafNum == IX_EMPTY)
        return IX_EOF;

    if(rc = _pBTree->GetLeafPageBuffer(iLeafNum, pBuffer))
        goto err_return;

    copyGeneric(pBuffer->v[iSlotIndex], *((T*) oValue));

    if(rc = _pBTree->ReleaseBuffer(iLeafNum, false))
        goto err_return;

    return rc;

err_return:
    return rc;
}

template <typename T, int n>
RC IX_BTreeScan::GreaterOnTheRight(const PageNum iLeafNum, const int iSlotIndex, const void * oValue)
{
    RC rc = OK_RC;
    IX_BTree::IX_PageLeaf<T,n> *pBuffer;
    int comparison;

    if(iLeafNum == IX_EMPTY)
        return IX_EOF;

    if(rc = _pBTree->GetLeafPageBuffer(iLeafNum, pBuffer))
        goto err_return;

    comparison = comparisonGeneric(pBuffer->v[iSlotIndex], *((T*) oValue));

    if(comparison < 0 && pBuffer->next == IX_EMPTY)
    {
        if(rc = _pBTree->ReleaseBuffer(iLeafNum, false))
            goto err_return;

        return IX_EOF;
    }

    if(rc = _pBTree->ReleaseBuffer(iLeafNum, false))
        goto err_return;

    return rc;

err_return:
    return rc;
}
