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
        rc = InsertEntry_t<int, order_hash_INT>(*((int*)pData), rid);
        break;
    case FLOAT:
        rc = InsertEntry_t<float, order_hash_FLOAT>(*((float*)pData), rid);
        break;
    case STRING:
        rc = InsertEntry_t<char[MAXSTRINGLEN], order_hash_STRING>((char *) pData, rid);
        break;
    default:
        rc = IX_BADTYPE;
    }

    return rc;
}

// templated insertion
template <typename T,int n>
RC IX_Hash::InsertEntry_t(T iValue, const RID &rid)
{
    RC rc = OK_RC;
    int hash, binary;
    char *pDirBuffer = NULL;
    PageNum bucketNum, newBucketNum;
    bool needToDivide = false;
    bool needToUpdate = false;
    int depth, childDepth;

    if(_pFileHdr->directoryNum == IX_EMPTY)
    {
        // create the new page
        if((rc = AllocateDirectoryPage_t<T,n>(_pFileHdr->directoryNum)))
        {
            goto err_return;
        }
    }


    // get the directory page
    if(rc = GetPageBuffer(_pFileHdr->directoryNum, pDirBuffer))
        goto err_return;

    // update depth and nbSlots
    depth = ((IX_DirectoryHdr *) pDirBuffer)->depth;

    // release the directory page
    if(rc = ReleaseBuffer(_pFileHdr->directoryNum, false))
        goto err_return;

    // compute hash
    hash = getHash(iValue);

    // get binary decomposition
    binary = getBinaryDecomposition(hash, depth);

    // get the link
    if(rc = GetBucketNum(binary, bucketNum))
        goto err_return;


    // if the page does not exist, create it
    if(bucketNum == IX_EMPTY)
    {
        // create the new page
        if((rc = AllocateBucketPage_t<T,n>(depth, bucketNum)))
        {
            goto err_return;
        }

        // update the directory
        memcpy(pDirBuffer + sizeof(IX_DirectoryHdr) + binary * sizeof(PageNum),
               &bucketNum,
               sizeof(PageNum));
    }


    do
    {
        // see if division is necessary
        if((rc = IsPossibleToInsertInBucket_t<T,n>(bucketNum, needToDivide, childDepth)))
        {
            goto err_return;
        }

        // division is necessary
        if(needToDivide)
        {
            needToUpdate = true;

            // divide the existing bucket
            if((rc = DivideBucketInTwo_t<T,n>(bucketNum, newBucketNum)))
            {
                goto err_return;
            }

            if(depth == childDepth)
            {
                if((rc = DoubleDirectorySize_t<T,n>()))
                    goto err_return;

                depth++;
            }

            // recompute the binary decomposition
            binary = getBinaryDecomposition(hash, depth);

            // update the directory for every link to the new bucket page
            int offset = pow2(childDepth);
//            cout << "offset: " << offset << endl;


            // update child depth
            childDepth++;

            if(getLastBit(hash, childDepth) == 0)
            {
                if((rc = UpdateDirectoryEntries(binary, offset, newBucketNum, true)))
                    return rc;
            }
            else
            {
                if((rc = UpdateDirectoryEntries(binary, offset, newBucketNum, false)))
                    return rc;
            }


            // get the link
            if(rc = GetBucketNum(binary, bucketNum))
                goto err_return;
        }
    } while(needToDivide);

    // insert in bucket
    if((rc = InsertEntryInBucket_t<T,n>(bucketNum, iValue, rid)))
    {
        goto err_return;
    }

    return rc;

err_return:
    return (rc);
}



RC IX_Hash::GetBucketNum(const int binary, PageNum &oBucketNum)
{
    RC rc = OK_RC;
    char *pDirBuffer = NULL;
    PageNum dirNum, nextDirNum;
    int stillToDo = binary;
    bool keepOn = true;
    int depth, nbSlots, visitedSlots;

    dirNum = _pFileHdr->directoryNum;

    if(rc = GetPageBuffer(dirNum, pDirBuffer))
        goto err_return;
    depth = ((IX_DirectoryHdr *)pDirBuffer)->depth;
    if(rc = ReleaseBuffer(dirNum, false))
        goto err_return;
    nbSlots = pow2(depth);

    visitedSlots = 0;
    while(keepOn && visitedSlots <= nbSlots)
    {

        if(dirNum == IX_EMPTY)
        {
            return IX_ENTRY_DOES_NOT_EXIST;
        }

        // get the directory page
        if(rc = GetPageBuffer(dirNum, pDirBuffer))
            goto err_return;

        if(stillToDo >= ((IX_DirectoryHdr *)pDirBuffer)->nbFilledSlots)
        {
            stillToDo = stillToDo - ((IX_DirectoryHdr *)pDirBuffer)->nbFilledSlots;
            nextDirNum = ((IX_DirectoryHdr *)pDirBuffer)->next;
        }
        else
        {
            memcpy(&oBucketNum,
                   pDirBuffer + sizeof(IX_DirectoryHdr) + stillToDo * sizeof(PageNum),
                   sizeof(PageNum));

            keepOn = false;
        }

        visitedSlots += ((IX_DirectoryHdr *)pDirBuffer)->nbFilledSlots;

        if(rc = ReleaseBuffer(dirNum, false))
            goto err_return;

        dirNum = nextDirNum;
    }

    if(visitedSlots > nbSlots)
    {
        cout << "The slot was not found" << endl;
        return 1;
    }

    return rc;

err_return:
    return (rc);
}

template <typename T,int n>
RC IX_Hash::DoubleDirectorySize_t()
{
    RC rc = OK_RC;
    char *pReadBuffer = NULL, *pWriteBuffer = NULL, *pPreviousBuffer = NULL;
    PageNum readNum;
    PageNum nextNum, previousNum;
    PageNum writeNum;
    PageNum lastReadNum, firstWriteNum;

    cout << "Double directory size" << endl;

    readNum = _pFileHdr->directoryNum;

    if(readNum == IX_EMPTY)
    {
        return OK_RC;
    }

    // open the directory
    if(rc = GetPageBuffer(readNum, pReadBuffer))
        goto err_return;

    // the directory's page is sufficient
    if(2 * ((IX_DirectoryHdr *) pReadBuffer)->nbFilledSlots < nbSlotsInDirectory)
    {
        cout << "Sufficient space in directory page" << endl;
        // double the directory size
        memcpy(pReadBuffer + sizeof(IX_DirectoryHdr) + ((IX_DirectoryHdr *) pReadBuffer)->nbFilledSlots * sizeof(PageNum),
               pReadBuffer + sizeof(IX_DirectoryHdr),
               ((IX_DirectoryHdr *) pReadBuffer)->nbFilledSlots * sizeof(PageNum));

        ((IX_DirectoryHdr *) pReadBuffer)->nbFilledSlots *= 2;
        ((IX_DirectoryHdr *) pReadBuffer)->depth++;


        if(rc = ReleaseBuffer(readNum, true))
            goto err_return;
    }
    // impossible to double the size inside the same page
    else
    {
        if(rc = ReleaseBuffer(readNum, false))
            goto err_return;

        cout << "Space in directory page not sufficient, need use new pages" << endl;

        // get the last existing page
        previousNum = readNum;
        nextNum = readNum;
        while(nextNum != IX_EMPTY)
        {
            previousNum = nextNum;
            if(rc = GetPageBuffer(previousNum, pPreviousBuffer))
                goto err_return;

            ((IX_DirectoryHdr *) pPreviousBuffer)->depth++;
            nextNum = ((IX_DirectoryHdr *) pPreviousBuffer)->next;

            if(rc = ReleaseBuffer(previousNum, true))
                goto err_return;
        }

        lastReadNum = previousNum;
        previousNum = IX_EMPTY;


        while(readNum != IX_EMPTY)
        {
            if(rc = GetPageBuffer(readNum, pReadBuffer))
                goto err_return;

            // create the new page
            if((rc = AllocateDirectoryPage_t<T,n>(writeNum)))
            {
                goto err_return;
            }

            if(previousNum == IX_EMPTY)
            {
                firstWriteNum = writeNum;
            }
            else
            {
                if(rc = GetPageBuffer(previousNum, pPreviousBuffer))
                    goto err_return;

                ((IX_DirectoryHdr *) pPreviousBuffer)->next = writeNum;

                if(rc = ReleaseBuffer(previousNum, true))
                    goto err_return;
            }

            if(rc = GetPageBuffer(writeNum, pWriteBuffer))
                goto err_return;

            memcpy(pWriteBuffer,
                   pReadBuffer,
                   sizeof(IX_DirectoryHdr) + (((IX_DirectoryHdr *) pReadBuffer)->nbFilledSlots * sizeof(PageNum)));

            ((IX_DirectoryHdr *) pWriteBuffer)->next = IX_EMPTY;

            nextNum = ((IX_DirectoryHdr *) pReadBuffer)->next;

            if(rc = ReleaseBuffer(writeNum, true))
                goto err_return;

            if(rc = ReleaseBuffer(readNum, false))
                goto err_return;


            previousNum = writeNum;
            readNum = nextNum;

//            cout << "hello" << endl;
        }


        if(rc = GetPageBuffer(lastReadNum, pReadBuffer))
            goto err_return;

        ((IX_DirectoryHdr *) pReadBuffer)->next = firstWriteNum;

        if(rc = ReleaseBuffer(lastReadNum, true))
            goto err_return;

    }



    return rc;

err_return:
    return rc;
}

RC IX_Hash::UpdateDirectoryEntries(const int iBinary, const int iOffset, const PageNum &newPage, bool updateThisSlot)
{
    RC rc = OK_RC;
    char *pReadBuffer = NULL;
    PageNum readNum, nextNum;
    int p = iBinary / (2 * iOffset);
    int start = iBinary - p * 2 * iOffset;
    int slotCounter = 0;
    int m;

    if(updateThisSlot)
    {
        p = (iBinary - iOffset) / (2 * iOffset);
        start = iBinary - iOffset - p * 2 * iOffset;
        if(start < 0)
        {
            start = iBinary + iOffset;
        }
    }
    else
    {
        p = iBinary / (2 * iOffset);
        start = iBinary - p * 2 * iOffset;
    }

    readNum = _pFileHdr->directoryNum;

    if(readNum == IX_EMPTY)
    {
        return OK_RC;
    }

    while(readNum != IX_EMPTY)
    {

        if(rc = GetPageBuffer(readNum, pReadBuffer))
            goto err_return;

        while(start - slotCounter < ((IX_DirectoryHdr *) pReadBuffer)->nbFilledSlots)
        {
            memcpy(pReadBuffer + sizeof(IX_DirectoryHdr) + (start - slotCounter) * sizeof(PageNum),
                   &newPage,
                   sizeof(PageNum));

            start += 2 * iOffset;
        }

        nextNum = ((IX_DirectoryHdr *) pReadBuffer)->next;

        if(rc = ReleaseBuffer(readNum, true))
            goto err_return;

        readNum = nextNum;
    }


    return rc;

err_return:
    return rc;
}

template <typename T, int n>
RC IX_Hash::IsPossibleToInsertInBucket_t(const PageNum iPageNum, bool &needToDivide, int &childDepth)
{
    RC rc = OK_RC;
    IX_Bucket<T,n> *pBuffer;

    if((rc = GetBucketBuffer<T,n>(iPageNum, pBuffer)))
        goto err_return;

    childDepth = pBuffer->depth;
    needToDivide = (pBuffer->nbFilledSlots >= n);

    //    cout << "depth: " << childDepth << endl;
    //    cout << "ntd: " << needToDivide << endl;
    //    cout << "nb slots: " << pBuffer->nbFilledSlots << endl;
    //    cout << "max slots: " << n << endl;
    //    cout << "max string length: " << MAXSTRINGLEN << endl;

    if(rc = ReleaseBuffer(iPageNum, false))
        goto err_return;

    return rc;

err_return:
    return rc;
}

// bucket insertion
template <typename T, int n>
RC IX_Hash::InsertEntryInBucket_t(PageNum iPageNum, T iValue, const RID &rid)
{
    RC rc = OK_RC;
    IX_Bucket<T,n> *pBuffer;
    bool alreadyInBucket = false;
    bool needToUpdate = false;
    PageNum ridBucketNum;
    int index=0;

    // get the current bucket
    if(rc = GetBucketBuffer<T,n>(iPageNum, pBuffer))
        goto err_return;

    // check if the value is already in there
    for(index=0; index<pBuffer->nbFilledSlots; index++)
    {
        if(comparisonGeneric(pBuffer->v[index], iValue) == 0)
        {
            alreadyInBucket = true;
            ridBucketNum = pBuffer->child[index];
            break;
        }
    }

    // need to add an entry in the bucket
    if(!alreadyInBucket)
    {
        if((rc = AllocateRidBucketPage(ridBucketNum)))
        {
            ReleaseBuffer(iPageNum, false);
            return rc;
        }

        copyGeneric(iValue, pBuffer->v[pBuffer->nbFilledSlots]);
        pBuffer->child[pBuffer->nbFilledSlots] = ridBucketNum;

        pBuffer->nbFilledSlots++;

        needToUpdate = true;
    }

    if(ridBucketNum == IX_EMPTY)
    {
        if((rc = AllocateRidBucketPage(ridBucketNum)))
        {
            ReleaseBuffer(iPageNum, false);
            return rc;
        }

        pBuffer->child[index] = ridBucketNum;

        needToUpdate = true;
    }

    // insert in the rid bucket
    if((rc = InsertEntryInRidBucket(ridBucketNum, rid)))
    {
        ReleaseBuffer(iPageNum, !alreadyInBucket);
        return rc;
    }

    if(rc = ReleaseBuffer(iPageNum, needToUpdate))
        goto err_return;

    return rc;

err_return:
    return (rc);
}

RC IX_Hash::InsertEntryInRidBucket(PageNum iPageNum, const RID &rid)
{
    RC rc = OK_RC;
    char *pBuffer;
    RID tempRID;

    // get the current bucket
    if(rc = GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // overflow detection
    if((((IX_RidBucketHdr *)pBuffer)->nbFilledSlots + 1) * sizeof(RID) > PF_PAGE_SIZE - sizeof(IX_RidBucketHdr))
    {
        if(rc = ReleaseBuffer(iPageNum, false))
            goto err_return;

        return IX_BUCKET_OVERFLOW;
    }

    // check if the RID is already in there
    for(int i=0; i<((IX_RidBucketHdr *)pBuffer)->nbFilledSlots; i++)
    {
        memcpy(&tempRID,
               pBuffer + sizeof(IX_RidBucketHdr) + i * sizeof(RID),
               sizeof(RID));

        if(rid == tempRID)
        {
            if(rc = ReleaseBuffer(iPageNum, false))
                goto err_return;

            return IX_RID_ALREADY_IN_BUCKET;
        }
    }

    memcpy(pBuffer + sizeof(IX_RidBucketHdr) + ((IX_RidBucketHdr *)pBuffer)->nbFilledSlots * sizeof(RID),
           &rid, sizeof(RID));

    ((IX_RidBucketHdr *)pBuffer)->nbFilledSlots++;

    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

    return rc;

err_release:
    rc = ReleaseBuffer(iPageNum, false);
err_return:
    return (rc);
}

template <typename T, int n>
RC IX_Hash::DivideBucketInTwo_t(const PageNum iBucketNum, PageNum &oBucketNum)
{
    RC rc = OK_RC;


    IX_Bucket<T,n> *ipBuffer, *opBuffer;
    IX_Bucket<T,n> tempBuffer;

    // read the existing page
    if((rc = GetBucketBuffer<T,n>(iBucketNum, ipBuffer)))
        goto err_return;

    // create the new page
    if((rc = AllocateBucketPage_t<T,n>(ipBuffer->depth+1, oBucketNum)))
    {
        ReleaseBuffer(iBucketNum, false);
        goto err_return;
    }

    //    cout << "BEGIN Bucket division" << endl;

    // save the existing bucket data
    tempBuffer.nbFilledSlots = ipBuffer->nbFilledSlots;

    for(int i=0; i<tempBuffer.nbFilledSlots; i++)
    {
        copyGeneric(ipBuffer->v[i], tempBuffer.v[i]);
        tempBuffer.child[i] = ipBuffer->child[i];
    }

    // reset the existing bucket counter
    ipBuffer->nbFilledSlots = 0;
    ipBuffer->depth++;

    // get the new page buffer
    if((rc = GetBucketBuffer<T,n>(oBucketNum, opBuffer)))
    {
        ReleaseBuffer(iBucketNum, true);
        goto err_return;
    }

    // set the saved data in the appropriate bucket
    for(int i=0; i<tempBuffer.nbFilledSlots; i++)
    {
        int hash = getHash(tempBuffer.v[i]);
        int r = getLastBit(hash, ipBuffer->depth);

        if(r == 0)
        {
            copyGeneric(tempBuffer.v[i], ipBuffer->v[ipBuffer->nbFilledSlots]);
            ipBuffer->child[ipBuffer->nbFilledSlots] = tempBuffer.child[i];
            ipBuffer->nbFilledSlots++;
        }
        else
        {
            copyGeneric(tempBuffer.v[i], opBuffer->v[opBuffer->nbFilledSlots]);
            opBuffer->child[opBuffer->nbFilledSlots] = tempBuffer.child[i];
            opBuffer->nbFilledSlots++;
        }
    }


    //    cout << "one: " << endl;
    //    for(int i=0; i<((IX_BucketHdr *)ipBuffer)->nbFilledSlots; i++)
    //    {
    //        memcpy(&tValues[i],
    //               ipBuffer + sizeof(IX_BucketHdr) + i * sizeof(IX_BucketValue),
    //               sizeof(IX_BucketValue));
    //        cout << tValues[i].v << endl;
    //    }

    //    cout << "two: " << endl;
    //    for(int i=0; i<((IX_BucketHdr *)opBuffer)->nbFilledSlots; i++)
    //    {
    //        memcpy(&tValues[i],
    //               opBuffer + sizeof(IX_BucketHdr) + i * sizeof(IX_BucketValue),
    //               sizeof(IX_BucketValue));
    //        cout << tValues[i].v << endl;
    //    }


    //    cout << "END Bucket division" << endl;

    // release the pages
    if((rc = ReleaseBuffer(iBucketNum, true)))
        goto err_return;

    if((rc = ReleaseBuffer(oBucketNum, true)))
        goto err_return;

    return rc;


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
        rc = DeleteEntry_t<int, order_hash_INT>(* ((int *) pData), rid);
        break;
    case FLOAT:
        rc = DeleteEntry_t<float, order_hash_FLOAT>(* ((float *) pData), rid);
        break;
    case STRING:
        rc = DeleteEntry_t<char[MAXSTRINGLEN], order_hash_STRING>((char*) pData, rid);
        break;
    default:
        rc = IX_BADTYPE;
    }

    return rc;
}

// templated Deletion
template <typename T, int n>
RC IX_Hash::DeleteEntry_t(T iValue, const RID &rid)
{
    RC rc = OK_RC;
    int hash, binary;
    char *pDirBuffer = NULL;
    PageNum bucketNum;
    bool needToUpdate = false;
    int depth, nbSlots;

    if(_pFileHdr->directoryNum == IX_EMPTY)
    {
        return IX_ENTRY_DOES_NOT_EXIST;
    }


    // get the directory page
    if(rc = GetPageBuffer(_pFileHdr->directoryNum, pDirBuffer))
        goto err_return;

    // update depth and nbSlots
    depth = ((IX_DirectoryHdr *) pDirBuffer)->depth;
    nbSlots = 1;
    for(int i=1; i<=depth; i++)
    {
        nbSlots *= 2;
    }


    // compute hash
    hash = getHash(iValue);

    // get binary decomposition
    binary = getBinaryDecomposition(hash, depth);

    // get the link
    memcpy(&bucketNum,
           pDirBuffer + sizeof(IX_DirectoryHdr) + binary * sizeof(PageNum),
           sizeof(PageNum));

    // the page does not exist
    if(bucketNum == IX_EMPTY)
    {
        ReleaseBuffer(_pFileHdr->directoryNum, false);
        return IX_ENTRY_DOES_NOT_EXIST;
    }

    // delete in bucket
    if((rc = DeleteEntryInBucket_t<T,n>(bucketNum, iValue, rid)))
    {
        ReleaseBuffer(_pFileHdr->directoryNum, false);
        goto err_return;
    }

    // release the directory page
    if(rc = ReleaseBuffer(_pFileHdr->directoryNum, needToUpdate))
        goto err_return;


    return rc;

err_return:
    return (rc);
}


// Rid bucket Deletion
template <typename T, int n>
RC IX_Hash::DeleteEntryInBucket_t(PageNum iPageNum, T iValue, const RID &rid)
{
    RC rc = OK_RC;
    IX_Bucket<T,n> *pBuffer;
    bool alreadyInBucket = false;
    PageNum ridBucketNum;
    int index = 0;

    // get the current bucket
    if(rc = GetBucketBuffer<T,n>(iPageNum, pBuffer))
        goto err_return;

    // check if the value is already in there
    for(index=0; index<pBuffer->nbFilledSlots; index++)
    {
        if(comparisonGeneric(pBuffer->v[index], iValue) == 0)
        {
            alreadyInBucket = true;
            ridBucketNum = pBuffer->child[index];
            break;
        }
    }

    // the value does not exist
    if(!alreadyInBucket)
    {
        ReleaseBuffer(iPageNum, false);
        return IX_ENTRY_DOES_NOT_EXIST;
    }

    // delete in the rid bucket
    if((rc = DeleteEntryInRidBucket(ridBucketNum, rid)))
    {
        ReleaseBuffer(iPageNum, false);
        return rc;
    }

    // the rid bucket is now empty
    if(ridBucketNum == IX_EMPTY)
    {
        pBuffer->child[index] = IX_EMPTY;

        if(rc = ReleaseBuffer(iPageNum, true))
            goto err_return;
    }
    else
    {
        if(rc = ReleaseBuffer(iPageNum, false))
            goto err_return;
    }

    return rc;

err_return:
    return (rc);
}

// Rid bucket Deletion
RC IX_Hash::DeleteEntryInRidBucket(PageNum &ioPageNum, const RID &rid)
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
    for(i=0; i<((IX_RidBucketHdr *)pBuffer)->nbFilledSlots; i++)
    {
        memcpy((void*) &tempRid,
               pBuffer + sizeof(IX_RidBucketHdr) + i * sizeof(RID),
               sizeof(RID));

        if(tempRid == rid)
        {
            foundRid = true;
            break;
        }
    }

    // the rid was found, move the last rid to it's place
    if(foundRid && ((IX_RidBucketHdr *)pBuffer)->nbFilledSlots>1)
    {
        memcpy(pBuffer + sizeof(IX_RidBucketHdr) + i * sizeof(RID),
               pBuffer + sizeof(IX_RidBucketHdr) + (((IX_RidBucketHdr *)pBuffer)->nbFilledSlots-1)* sizeof(RID),
               sizeof(RID));
    }
    if(foundRid)
    {
        ((IX_RidBucketHdr *)pBuffer)->nbFilledSlots--;
    }

    if(!foundRid)
    {
        rc = IX_ENTRY_DOES_NOT_EXIST;
        goto err_release;
    }

    filledSlots = ((IX_RidBucketHdr *)pBuffer)->nbFilledSlots;

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
template <typename T, int n>
RC IX_Hash::AllocateDirectoryPage_t(PageNum &oPageNum)
{
    RC rc;
    PF_PageHandle pageHandle;
    char *pReadData;
    PageNum firstNum, secondNum;

    if (rc = _pPfFileHandle->AllocatePage(pageHandle))
        goto err_return;

    if (rc = pageHandle.GetPageNum(oPageNum))
        goto err_unpin;

    if (rc = pageHandle.GetData(pReadData))
        goto err_unpin;

    // Fill node
    ((IX_DirectoryHdr *)pReadData)->depth = 1;
    ((IX_DirectoryHdr *)pReadData)->nbFilledSlots = 2;
    ((IX_DirectoryHdr *)pReadData)->next = IX_EMPTY;

    // create the new pages
    if((rc = AllocateBucketPage_t<T,n>(((IX_DirectoryHdr *)pReadData)->depth, firstNum)))
        goto err_return;

    if((rc = AllocateBucketPage_t<T,n>(((IX_DirectoryHdr *)pReadData)->depth, secondNum)))
        goto err_return;

    // update the directory
    memcpy(pReadData + sizeof(IX_DirectoryHdr),
           &firstNum,
           sizeof(PageNum));

    memcpy(pReadData + sizeof(IX_DirectoryHdr) + sizeof(PageNum),
           &secondNum,
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

// Bucket page
template <typename T, int n>
RC IX_Hash::AllocateBucketPage_t(const int depth, PageNum &oPageNum)
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

    ((IX_Bucket<T,n> *)pReadData)->depth = depth;
    ((IX_Bucket<T,n> *)pReadData)->nbFilledSlots = 0;

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
RC IX_Hash::AllocateRidBucketPage(PageNum &oPageNum)
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

    ((IX_RidBucketHdr *)pReadData)->nbFilledSlots = 0;

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

template <typename T, int n>
RC IX_Hash::GetBucketBuffer(const PageNum &iPageNum, IX_Bucket<T,n> * & pBuffer) const
{
    RC rc;
    char * pData;

    rc = GetPageBuffer(iPageNum, pData);

    pBuffer = (IX_Bucket<T,n> *) pData;

    return rc;
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



// Display
RC IX_Hash::DisplayTree()
{
    RC rc;

    switch(_pFileHdr->attrType)
    {
    case INT:
        rc = DisplayTree_t<int, order_hash_INT>();
        break;
    case FLOAT:
        rc = DisplayTree_t<float, order_hash_FLOAT>();
        break;
    case STRING:
        rc = DisplayTree_t<char[MAXSTRINGLEN], order_hash_STRING>();
        break;
    default:
        rc = IX_BADTYPE;
    }

    return rc;
}


template <typename T, int n>
RC IX_Hash::DisplayTree_t()
{
    RC rc = OK_RC;
    char *pDirBuffer;

    int depth, nbSlots;
    PageNum currentNum;

    if(_pFileHdr->directoryNum == IX_EMPTY)
    {
        return IX_INVALID_PAGE_NUMBER;
    }

    // get the directory page
    if(rc = GetPageBuffer(_pFileHdr->directoryNum, pDirBuffer))
        goto err_return;

    // update depth and nbSlots
    depth = ((IX_DirectoryHdr *) pDirBuffer)->depth;
    nbSlots = 1;
    for(int i=1; i<=depth; i++)
    {
        nbSlots *= 2;
    }

    cout << "#####################################" << endl;
    cout << "Depth: " << depth << endl;
    cout << "Nb Slots: " << nbSlots << endl;

    // browse each directory slot
    for(int i=0; i<nbSlots; i++)
    {
        if((rc = GetBucketNum(i, currentNum)))
            return rc;

//        memcpy(&currentNum,
//               pDirBuffer + sizeof(IX_DirectoryHdr) + i * sizeof(PageNum),
//               sizeof(PageNum));

        cout << "------------------------------" << endl;
        cout << "# slot: " << i;
        printDecomposition(i);
        cout << endl;
//        cout << "# page: " << currentNum << endl;

        if(((rc = DisplayBucket_t<T,n>(currentNum))))
        {
            ReleaseBuffer(_pFileHdr->directoryNum, false);
            goto err_return;
        }
    }

    cout << "#####################################" << endl;


    if(rc = ReleaseBuffer(_pFileHdr->directoryNum, false))
        goto err_return;


    return rc;

err_return:
    return rc;
}

template <typename T, int n>
RC IX_Hash::DisplayBucket_t(const PageNum iPageNum)
{
    RC rc = OK_RC;
    IX_Bucket<T,n> *pBucketBuffer;

    if(iPageNum == IX_EMPTY)
    {
        return IX_INVALID_PAGE_NUMBER;
    }

    // get the bucket page
    if(rc = GetBucketBuffer(iPageNum, pBucketBuffer))
        goto err_return;

    cout << "# bucket depth: " << pBucketBuffer->depth << endl;
//    cout << "# nb filled slots: " << pBucketBuffer->nbFilledSlots << endl;
//    cout << "# max slots: " << n << endl;

    for(int i=0; i<pBucketBuffer->nbFilledSlots; i++)
    {
        printDecomposition(getHash(pBucketBuffer->v[i]));
        cout << " -> ";
        printGeneric(pBucketBuffer->v[i]);
    }


    if(rc = ReleaseBuffer(iPageNum, false))
        goto err_return;

    return rc;

err_return:
    return rc;

}
