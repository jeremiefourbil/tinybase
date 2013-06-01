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
    int pointerIndex;




    // get the current node
    if(rc = _indexHandle.GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    pointerIndex = 0;
//    // find the next branch
//    if(((IX_PageNode<T> *)pBuffer)->nbFilledSlots == 0 || comparisonGeneric(*((T*) _value), ((IX_PageNode<T> *)pBuffer)->v[pointerIndex]) < 0)
//        pointerIndex = 0;
//    else
//    {
//        while(pointerIndex < ((IX_PageNode<T> *)pBuffer)->nbFilledSlots && comparisonGeneric(*((T*) _value), ((IX_PageNode<T> *)pBuffer)->v[pointerIndex]) > 0)
//        {
//           pointerIndex++;
//        }
//        pointerIndex++;
//    }

    while(pointerIndex < ((IX_PageNode<T> *)pBuffer)->nbFilledSlots && comparisonGeneric(*((T*) _value), ((IX_PageNode<T> *)pBuffer)->v[pointerIndex]) >= 0)
    {
       pointerIndex++;
    }

    if(pointerIndex == 3 && comparisonGeneric(*((T*) _value), ((IX_PageNode<T> *)pBuffer)->v[pointerIndex]) >= 0)
        pointerIndex++;

    cout << "Pointer index: " << pointerIndex << endl;


    // the child is a leaf
    if(((IX_PageNode<T> *)pBuffer)->nodeType == LASTINODE || ((IX_PageNode<T> *)pBuffer)->nodeType == ROOTANDLASTINODE)
    {
        PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[pointerIndex];

        // the leaf is empty
        if(((IX_PageNode<T> *)pBuffer)->child[pointerIndex] == IX_EMPTY)
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
        PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[pointerIndex];

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
    cout << "scan leaf" << endl;

    RC rc = OK_RC;

    char *pBuffer;
    int slotIndex = -1;

    // get the current node
    if(rc = _indexHandle.GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the right place in the tree
    for(int i=0; i<((IX_PageLeaf<T> *)pBuffer)->nbFilledSlots; i++)
    {
        printGeneric(((IX_PageLeaf<T> *)pBuffer)->v[i]);
        if(comparisonGeneric(((IX_PageLeaf<T> *)pBuffer)->v[i], *((T*) _value)) == 0)
        {
            slotIndex = i;
            cout << "found!" << endl;
            break;
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
        cout << "here" << endl;
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
    RC rc = OK_RC;

    switch(_indexHandle.fileHdr.attrType)
    {
    case INT:
        rc = GetNextEntry_t<int>(rid);
        break;
    case FLOAT:
        rc = GetNextEntry_t<float>(rid);
        break;
    case STRING:
        rc = GetNextEntry_t<char[MAXSTRINGLEN]>(rid);
        break;
    default:
        rc = IX_BADTYPE;
    }

    return rc;
}


template <typename T>
RC IX_IndexScan::GetNextEntry_t(RID &rid)
{
    RC rc = OK_RC;

    char *pBuffer;
    PageNum leafNum;

    if(_nextLeafNum == IX_EMPTY)
    {
        return IX_EOF;
    }

    leafNum = _nextLeafNum;


    // get the current node
    if(rc = _indexHandle.GetPageBuffer(leafNum, pBuffer))
        goto err_return;

    // set the right rid
    if(_nextLeafSlot>=0)
    {
        cout << "rid" << endl;
        rid = ((IX_PageLeaf<T> *)pBuffer)->rid[_nextLeafSlot];
    }

    // set the next parameters
    _nextLeafNum = IX_EMPTY;
    _nextLeafSlot = IX_EMPTY;
    _nextRidSlot = IX_EMPTY;



    if(rc = _indexHandle.ReleaseBuffer(leafNum, false))
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
