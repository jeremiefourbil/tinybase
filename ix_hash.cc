#include "ix_hash.h"

#include "ix_internal.h"

#include <cstdio>
#include <iostream>
#include <fstream>

#define XML_FILE "tree.graphml"

using namespace std;

IX_Hash::IX_Hash()
{

}

IX_Hash::~IX_Hash()
{
    _pPfFileHandle = NULL;
}



// ***********************
// Insertion in tree
// ***********************

RC IX_Hash::InsertEntry(void *pData, const RID &rid)
{
    RC rc;

    if (pData == NULL)
        return (IX_NULLPOINTER);

    switch(_pFileHdr->attrType)
    {
    case INT:
        rc = InsertEntry_t<int>(*((int*)pData), rid);
        break;
    case FLOAT:
        rc = InsertEntry_t<float>(*((float*)pData), rid);
        break;
    case STRING:
        rc = InsertEntry_t<char[MAXSTRINGLEN]>((char *) pData, rid);
        break;
    default:
        rc = IX_BADTYPE;
    }

    return rc;
}

// templated insertion
template <typename T>
RC IX_Hash::InsertEntry_t(T iValue, const RID &rid)
{
    RC rc = OK_RC;
    int hash, binary;
    char *pDirBuffer = NULL;
    PageNum bucketNum, newBucketNum;
    bool needToDivide = false;
    int depth, nbSlots, childDepth;

    if(_pFileHdr->directoryNum == IX_EMPTY)
    {
        // create the new page
        if((rc = AllocateDirectoryPage(_pFileHdr->directoryNum)))
        {
            goto err_return;
        }
    }


    // get the directory page
    if(rc = GetPageBuffer(_pFileHdr->directoryNum, pDirBuffer))
        goto err_return;

    // update depth and nbSlots
    depth = ((IX_DirectoryHdr *) pDirBuffer)->depth;
    nbSlots = 1;
    for(int i=2; i<=depth; i++)
    {
        nbSlots *= 2;
    }

    cout << "depth: " << depth << " | nb slots: " << nbSlots << endl;


    // todo: compute hash
    // <----------------

    hash = getHash(iValue);


    // get binary decomposition
    binary = getBinaryDecomposition(hash, depth);

    // get the link
    memcpy(&bucketNum,
           pDirBuffer + sizeof(IX_DirectoryHdr) + binary * sizeof(PageNum),
           sizeof(PageNum));

    // if the page does not exist, create it
    if(bucketNum == IX_EMPTY)
    {
        // create the new page
        if((rc = AllocateBucketPage(depth, bucketNum)))
        {
            ReleaseBuffer(_pFileHdr->directoryNum, false);
            goto err_return;
        }

        // update the directory
        memcpy(pDirBuffer + sizeof(IX_DirectoryHdr) + binary * sizeof(PageNum),
               &bucketNum,
               sizeof(PageNum));
    }


    // see if division is necessary
    if((rc = IsPossibleToInsertInBucket(bucketNum, needToDivide, childDepth)))
    {
        ReleaseBuffer(_pFileHdr->directoryNum, false);
        goto err_return;
    }

    // division is necessary
    if(needToDivide)
    {
        // divide the existing bucket
        if((rc = DivideBucketInTwo(bucketNum, newBucketNum)))
        {
            ReleaseBuffer(_pFileHdr->directoryNum, true);
            goto err_return;
        }

        if(depth == childDepth)
        {
            // NEED TO CHECK IF PAGE SIZE IS REACHED
            memcpy(pDirBuffer + sizeof(IX_DirectoryHdr) + nbSlots * sizeof(PageNum),
                   pDirBuffer + sizeof(IX_DirectoryHdr),
                   nbSlots * sizeof(PageNum));

            ((IX_DirectoryHdr *) pDirBuffer)->depth++;
            depth++;
            nbSlots *= 2;
        }

        // set the right page number for the new bucket
        memcpy(pDirBuffer + sizeof(IX_DirectoryHdr) + (binary + pow2(childDepth)) * sizeof(PageNum),
               &newBucketNum,
               sizeof(PageNum));

        // recompute the binary decomposition
        binary = getBinaryDecomposition(hash, childDepth+1);

        // get the link
        memcpy(&bucketNum,
               pDirBuffer + sizeof(IX_DirectoryHdr) + binary * sizeof(PageNum),
               sizeof(PageNum));
    }

    // insert in bucket
    if((rc = InsertEntryInBucket(bucketNum, rid)))
    {
        ReleaseBuffer(_pFileHdr->directoryNum, false);
        goto err_return;
    }

    // release the directory page
    if(rc = ReleaseBuffer(_pFileHdr->directoryNum, needToDivide))
        goto err_return;


    return rc;

err_return:
    return (rc);
}

RC IX_Hash::IsPossibleToInsertInBucket(const PageNum iPageNum, bool &needToDivide, int &childDepth)
{
    RC rc = OK_RC;
    char *pBuffer;

    if((rc = GetPageBuffer(iPageNum, pBuffer)))
        goto err_return;

    childDepth = ((IX_BucketHdr *)pBuffer)->depth;
    needToDivide = ((IX_BucketHdr *)pBuffer)->nbFilledSlots >= ((IX_BucketHdr *)pBuffer)->nbMaxSlots;

    if(rc = ReleaseBuffer(iPageNum, false))
        goto err_return;

    return rc;

err_return:
    return rc;
}

// bucket insertion
RC IX_Hash::InsertEntryInBucket(PageNum iPageNum, const RID &rid)
{
    RC rc = OK_RC;
    char *pBuffer;
    RID tempRID;

    // get the current bucket
    if(rc = GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // overflow detection
    if((((IX_RidBucketHdr *)pBuffer)->nbFilledSlots + 1) * sizeof(RID) > PF_PAGE_SIZE - sizeof(IX_PageBucketHdr))
    {
        if(rc = ReleaseBuffer(iPageNum, false))
            goto err_return;

        return IX_BUCKET_OVERFLOW;
    }

    // check if the RID is already in there
    for(int i=0; i<((IX_RidBucketHdr *)pBuffer)->nbFilledSlots; i++)
    {
        memcpy(pBuffer + sizeof(IX_RidBucketHdr) + i * sizeof(RID),
               &tempRID, sizeof(RID));

        if(rid == tempRID)
        {
            if(rc = ReleaseBuffer(iPageNum, false))
                goto err_return;

            return IX_RID_ALREADY_IN_BUCKET;
        }
    }

    memcpy(pBuffer + sizeof(IX_PageBucketHdr) + ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots * sizeof(RID),
           &rid, sizeof(RID));

    ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots++;

    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

    return rc;

err_release:
    rc = ReleaseBuffer(iPageNum, false);
err_return:
    return (rc);
}

RC IX_Hash::DivideBucketInTwo(const PageNum iBucketNum, PageNum &oBucketNum)
{
    RC rc = OK_RC;


    char *ipBuffer, *opBuffer;
    IX_BucketValue *tValues = NULL;
    int nbFilledSlots;

    // read the existing page
    if((rc = GetPageBuffer(iBucketNum, ipBuffer)))
        goto err_return;

    // create the new page
    if((rc = AllocateBucketPage(((IX_BucketHdr *)ipBuffer)->depth+1, oBucketNum)))
    {
        ReleaseBuffer(iBucketNum, false);
        goto err_return;
    }

    // save the existing bucket data
    nbFilledSlots = ((IX_BucketHdr *)ipBuffer)->nbFilledSlots;
    tValues = new IX_BucketValue[nbFilledSlots];
    for(int i=0; i<nbFilledSlots; i++)
    {
        memcpy(&tValues[i],
               ipBuffer + sizeof(IX_BucketHdr) + i * sizeof(IX_BucketValue),
               sizeof(IX_BucketValue));
    }

    // reset the existing bucket counter
    ((IX_BucketHdr *)ipBuffer)->nbFilledSlots = 0;
    ((IX_BucketHdr *)ipBuffer)->depth++;

    // get the new page buffer
    if((rc = GetPageBuffer(oBucketNum, opBuffer)))
    {
        ReleaseBuffer(iBucketNum, true);
        goto err_return;
    }

    // set the saved data in the appropriate bucket
    for(int i=0; i<nbFilledSlots; i++)
    {
        int d,r,bitNb;

        bitNb = 1;
        d = tValues[i].v;

        do
        {
            getEuclidianDivision(d,d,r);
            bitNb++;
        } while(d>0 && bitNb <= ((IX_BucketHdr *)ipBuffer)->depth);

        if(d == 0)
        {
            memcpy(ipBuffer + sizeof(IX_BucketHdr) + ((IX_BucketHdr *)ipBuffer)->nbFilledSlots * sizeof(IX_BucketValue),
                   &tValues[i],
                   sizeof(IX_BucketValue));
            ((IX_BucketHdr *)ipBuffer)->nbFilledSlots++;
        }
        else
        {
            memcpy(opBuffer + sizeof(IX_BucketHdr) + ((IX_BucketHdr *)opBuffer)->nbFilledSlots * sizeof(IX_BucketValue),
                   &tValues[i],
                   sizeof(IX_BucketValue));
            ((IX_BucketHdr *)opBuffer)->nbFilledSlots++;
        }
    }

    // delete the temporary array
    if(tValues != NULL)
    {
        delete[] tValues;
        tValues = NULL;
    }

    // release the pages
    if((rc = ReleaseBuffer(iBucketNum, true)))
        goto err_return;

    if((rc = ReleaseBuffer(oBucketNum, true)))
        goto err_return;


err_return:
    return rc;
}

// ***********************
// Delete an entry
// ***********************

// Delete a new index entry
RC IX_Hash::DeleteEntry(void *pData, const RID &rid)
{
    RC rc;

    if (pData == NULL)
        return (IX_NULLPOINTER);

    switch(_pFileHdr->attrType)
    {
    case INT:
        rc = DeleteEntry_t<int>(* ((int *) pData), rid);
        break;
    case FLOAT:
        rc = DeleteEntry_t<float>(* ((float *) pData), rid);
        break;
    case STRING:
        rc = DeleteEntry_t<char[MAXSTRINGLEN]>((char*) pData, rid);
        break;
    default:
        rc = IX_BADTYPE;
    }

    return rc;
}

// templated Deletion
template <typename T>
RC IX_Hash::DeleteEntry_t(T iValue, const RID &rid)
{
    RC rc = OK_RC;

    return rc;

err_return:
    return (rc);
}


// bucket Deletion
RC IX_Hash::DeleteEntryInBucket(PageNum &ioPageNum, const RID &rid)
{
    RC rc = OK_RC;

    char *pBuffer;
    RID tempRid;
    int i;
    bool foundRid = false;
    int filledSlots;

    // get the current bucket
    if(rc = GetPageBuffer(ioPageNum, pBuffer))
        goto err_return;

    i=0;
    for(i=0; i<((IX_PageBucketHdr *)pBuffer)->nbFilledSlots; i++)
    {
        memcpy((void*) &tempRid,
               pBuffer + sizeof(IX_PageBucketHdr) + i * sizeof(RID),
               sizeof(RID));

        if(tempRid == rid)
        {
            foundRid = true;
            break;
        }
    }

    // the rid was found, move the last rid to it's place
    if(foundRid && ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots>1)
    {
        memcpy(pBuffer + sizeof(IX_PageBucketHdr) + i * sizeof(RID),
               pBuffer + sizeof(IX_PageBucketHdr) + (((IX_PageBucketHdr *)pBuffer)->nbFilledSlots-1)* sizeof(RID),
               sizeof(RID));
    }
    if(foundRid)
    {
        ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots--;
    }

    if(!foundRid)
    {
        rc = IX_ENTRY_DOES_NOT_EXIST;
        goto err_release;
    }

    filledSlots = ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots;

    if(rc = ReleaseBuffer(ioPageNum, true))
        goto err_return;

    // if the bucket page is empty, delete it
    if(filledSlots == 0)
    {
        if(rc = _pPfFileHandle->DisposePage(ioPageNum))
            goto err_return;

        ioPageNum = IX_EMPTY;
    }

    return rc;

err_release:
    ReleaseBuffer(ioPageNum, false);
err_return:
    return (rc);
}


// ***********************
// Page allocations
// ***********************

// directory page
RC IX_Hash::AllocateDirectoryPage(PageNum &oPageNum)
{
    RC rc;
    PF_PageHandle pageHandle;
    char *pReadData;
    PageNum emptyNum = IX_EMPTY;

    if (rc = _pPfFileHandle->AllocatePage(pageHandle))
        goto err_return;

    if (rc = pageHandle.GetPageNum(oPageNum))
        goto err_unpin;

    if (rc = pageHandle.GetData(pReadData))
        goto err_unpin;

    // Fill node

    ((IX_DirectoryHdr *)pReadData)->depth = 1;
    ((IX_DirectoryHdr *)pReadData)->nbMaximumDepth = 10; // WARNING!!!!!! TO BE CHANGED

    memcpy(pReadData + sizeof(IX_DirectoryHdr),
           &emptyNum,
           sizeof(PageNum));

    memcpy(pReadData + sizeof(IX_DirectoryHdr) + sizeof(PageNum),
           &emptyNum,
           sizeof(PageNum));

    // Mark the page dirty since we changed the next pointer
    if (rc = _pPfFileHandle->MarkDirty(oPageNum))
        goto err_unpin;

    // Unpin the page
    if (rc = _pPfFileHandle->UnpinPage(oPageNum))
        goto err_return;

    // Return ok
    return (0);

err_unpin:
    _pPfFileHandle->UnpinPage(oPageNum);
err_return:
    return (rc);
}

// RID bucket page
RC IX_Hash::AllocateBucketPage(const int depth, PageNum &oPageNum)
{
    RC rc;
    PF_PageHandle pageHandle;
    char *pReadData;

    if (rc = _pPfFileHandle->AllocatePage(pageHandle))
        goto err_return;

    if (rc = pageHandle.GetPageNum(oPageNum))
        goto err_unpin;

    if (rc = pageHandle.GetData(pReadData))
        goto err_unpin;

    // Fill node

    ((IX_BucketHdr *)pReadData)->depth = depth;
    ((IX_BucketHdr *)pReadData)->nbFilledSlots = 0;
    ((IX_BucketHdr *)pReadData)->nbMaxSlots = 5; // WARNING!!!!!! TO BE CHANGED

    // Mark the page dirty since we changed the next pointer
    if (rc = _pPfFileHandle->MarkDirty(oPageNum))
        goto err_unpin;

    // Unpin the page
    if (rc = _pPfFileHandle->UnpinPage(oPageNum))
        goto err_return;

    // Return ok
    return (0);

err_unpin:
    _pPfFileHandle->UnpinPage(oPageNum);
err_return:
    return (rc);
}

// ***********************
// Buffer management
// ***********************

// get the buffer
// DO NOT FORGET TO CLOSE IT !
RC IX_Hash::GetPageBuffer(const PageNum &iPageNum, char * & pBuffer) const
{
    RC rc;
    PF_PageHandle pageHandle;

    if(rc = _pPfFileHandle->GetThisPage(iPageNum, pageHandle))
        goto err_return;

    if (rc = pageHandle.GetData(pBuffer))
        goto err_unpin;

    return (0);

err_unpin:
    _pPfFileHandle->UnpinPage(iPageNum);
err_return:
    return (rc);
}

// release the buffer
RC IX_Hash::ReleaseBuffer(const PageNum &iPageNum, bool isDirty) const
{
    RC rc;

    // Mark the page dirty since we changed the next pointer
    if(isDirty)
    {
        if (rc = _pPfFileHandle->MarkDirty(iPageNum))
            goto err_unpin;
    }

    // Unpin the page
    if (rc = _pPfFileHandle->UnpinPage(iPageNum))
        goto err_return;

    return (0);

err_unpin:
    _pPfFileHandle->UnpinPage(iPageNum);
err_return:
    return (rc);

}

