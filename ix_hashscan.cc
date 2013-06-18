#include "ix_hashscan.h"
#include "ix_internal.h"

#include "ix_hash.h"

#include <iostream>

using namespace std;

IX_HashScan::IX_HashScan()
{
    _pHash = NULL;
}

IX_HashScan::~IX_HashScan()
{
    _pHash = NULL;
}

// Open index scan
RC IX_HashScan::OpenScan(IX_Hash * ipHash,
                          CompOp compOp,
                          void *value,
                          ClientHint  pinHint)
{
    RC rc = OK_RC;

    _pHash = ipHash;
    _compOp = compOp;
    _value = value;
    _pinHint = pinHint;

    if(_compOp != EQ_OP)
    {
        return IX_BADOPERATOR;
    }
    else
    {
        if (value == NULL)
            return (IX_NULLPOINTER);

        switch(_pHash->_pFileHdr->attrType)
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
    }

    return rc;
}

template <typename T>
RC IX_HashScan::OpenScan_t()
{
    RC rc = OK_RC;
    PageNum bucketNum;
    int binary;
    char *pDirBuffer = NULL;
    int depth;

    if(_pHash->_pFileHdr->directoryNum == IX_EMPTY)
    {
        _nextBucketNum = IX_EMPTY;
        _nextRidBucketNum = IX_EMPTY;
        _nextRidBucketSlot = IX_EMPTY;

        return OK_RC;
    }


    // get the directory page
    if(rc = _pHash->GetPageBuffer(_pHash->_pFileHdr->directoryNum, pDirBuffer))
        goto err_return;

    // update depth and nbSlots
    depth = ((IX_Hash::IX_DirectoryHdr *) pDirBuffer)->depth;

    // compute hash
    _nextHash = getHash(*((T*)_value));

    // get binary decomposition
    binary = getBinaryDecomposition(_nextHash, depth);

    cout << "Hash: " << _nextHash << " / " << binary << endl;

    // get the link
    memcpy(&bucketNum,
           pDirBuffer + sizeof(IX_Hash::IX_DirectoryHdr) + binary * sizeof(PageNum),
           sizeof(PageNum));

    if((rc = ScanBucket(bucketNum)))
    {
        _pHash->ReleaseBuffer(_pHash->_pFileHdr->directoryNum, false);
        return rc;
    }

    // release the directory page
    if(rc = _pHash->ReleaseBuffer(_pHash->_pFileHdr->directoryNum, false))
        goto err_return;

    return rc;


err_return:
    return (rc);
}

// bucket scan
RC IX_HashScan::ScanBucket(const PageNum iPageNum)
{
    RC rc = OK_RC;
    char *pBuffer;
    IX_Hash::IX_BucketValue tempValue;
    bool alreadyInBucket = false;

    if((rc = _pHash->GetPageBuffer(iPageNum, pBuffer)))
        goto err_return;

    // check if the value is already in there
    for(int i=0; i<((IX_Hash::IX_BucketHdr *)pBuffer)->nbFilledSlots; i++)
    {
        memcpy(&tempValue,
               pBuffer + sizeof(IX_Hash::IX_BucketHdr) + i * sizeof(IX_Hash::IX_BucketValue),
               sizeof(IX_Hash::IX_BucketValue));

        if(_nextHash == tempValue.v)
        {
            alreadyInBucket = true;
            _nextBucketNum = iPageNum;
            _nextRidBucketNum = tempValue.bucketNum;
            _nextRidBucketSlot = 0;
            break;
        }
    }

    if(!alreadyInBucket)
    {
        _nextBucketNum = IX_EMPTY;
        _nextRidBucketNum = IX_EMPTY;
        _nextRidBucketSlot = IX_EMPTY;
    }


    if(rc = _pHash->ReleaseBuffer(iPageNum, false))
        goto err_return;

    return rc;

err_return:
    return rc;
}

// Get the next matching entry
// return IX_EOF if no more matching entries.
RC IX_HashScan::GetNextEntry(RID &rid)
{
    RC rc = OK_RC;

    switch(_pHash->_pFileHdr->attrType)
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
RC IX_HashScan::GetNextEntry_t(RID &rid)
{
    RC rc = OK_RC;

    if(_nextBucketNum == IX_EMPTY || _nextRidBucketNum == IX_EMPTY || _nextRidBucketSlot == IX_EMPTY)
    {
        return IX_EOF;
    }

    // read the rid in the bucket
    if((rc = ReadBucket(_nextBucketNum, rid)))
        goto err_return;

    return rc;

err_return:
    return (rc);
}


RC IX_HashScan::ReadBucket(PageNum iPageNum, RID &rid)
{
    RC rc = OK_RC;

    char *pBuffer;

    if(iPageNum == IX_EMPTY)
    {
        return IX_INVALID_PAGE_NUMBER;
    }


    // get the current node
    if(rc = _pHash->GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // get the rid
    memcpy((void*) &rid,
           pBuffer + sizeof(IX_Hash::IX_RidBucketHdr) + _nextRidBucketSlot * sizeof(RID), sizeof(RID));


    // set the next parameters
    _nextRidBucketSlot++;

    if(_nextRidBucketSlot >= ((IX_Hash::IX_RidBucketHdr *)pBuffer)->nbFilledSlots)
    {
        _nextRidBucketSlot = IX_EMPTY;
        if(rc = _pHash->ReleaseBuffer(iPageNum, false))
            goto err_return;
        return IX_EOF;
    }



    if(rc = _pHash->ReleaseBuffer(iPageNum, false))
        goto err_return;

    return rc;

err_return:
    return (rc);
}


// Close index scan
RC IX_HashScan::CloseScan()
{
    // delete _value?
    _value = NULL;

//    if(_nextValue != NULL)
//    {
//        delete _nextValue;
//        _nextValue = NULL;
//    }

    _pHash = NULL;


    return OK_RC;
}

