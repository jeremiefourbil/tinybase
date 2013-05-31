#include "ix_internal.h"

IX_IndexHandle::IX_IndexHandle()
{

}

IX_IndexHandle::~IX_IndexHandle()
{

}


void copyGeneric(const int &v1, int &v2) { v2 = v1; }
void copyGeneric(const float &v1, float &v2) { v2 = v1; }
void copyGeneric(const char* v1, char* v2) { strcpy(v2, v1); }

int comparisonGeneric(const int &v1, const int &v2) { return (v1 < v2) ? -1 : ((v1==v2) ? 0 : 1); }
int comparisonGeneric(const float &v1, const float &v2) { return (v1 < v2) ? -1 : ((v1==v2) ? 0 : 1); }
int comparisonGeneric(const char* v1, const char* v2) { return strcmp(v1,v2); }


// Insert a new index entry
RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid)
{
    RC rc = OK_RC;

    if (pData == NULL)
        return (IX_NULLPOINTER);


    switch(fileHdr.attrType)
    {
    case INT:
        rc = InsertEntry_t<int>(pData, rid);
    case FLOAT:
        rc = InsertEntry_t<float>(pData, rid);
    case STRING:
        rc = InsertEntry_t<char[MAXSTRINGLEN]>(pData, rid);
    }

    return rc;
}

template <typename T>
RC IX_IndexHandle::InsertEntry_t(void *pData, const RID &rid)
{
    RC rc;
    PageNum pageNum;

    if (pData == NULL)
        return (IX_NULLPOINTER);

    pageNum = fileHdr.rootNum;

    // no root, create one
    if(pageNum == IX_EMPTY)
    {
        if(rc = AllocateNodePage_t<T>(ROOTANDLASTINODE, IX_EMPTY, pageNum))
            goto err_return;
    }

    // let's call the recursion method, start with root
    if(rc = InsertEntryInNode_t<T>(pageNum, pData, rid))
        goto err_return;

    return rc;

err_unpin:
    pfFileHandle.UnpinPage(pageNum);
err_return:
    return (rc);
}


// Insert a new data in the index
template <typename T>
RC IX_IndexHandle::InsertEntryInNode_t(PageNum iPageNum, void *pData, const RID &rid)
{

    RC rc;
    char *pBuffer;
    int slotIndex;

    // get the current node
    if(rc = GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the right place to insert data in the tree
    slotIndex = 0;
    while( slotIndex < ((IX_PageNode<T> *)pBuffer)->nbFilledSlots
            && comparisonGeneric(((IX_PageNode<T> *)pBuffer)->v[slotIndex], *((T*) pData)) > 0 )
    {
        ++slotIndex;
    }


    // add value to the node and increment number of filled slots
    if(slotIndex == ((IX_PageNode<T> *)pBuffer)->nbFilledSlots)
    {
        copyGeneric(*((T*) pData), ((IX_PageNode<T> *)pBuffer)->v[slotIndex]);
        ((IX_PageNode<T> *)pBuffer)->nbFilledSlots = slotIndex;
    }


    // the child is a leaf
    if(((IX_PageNode<T> *)pBuffer)->nodeType == LASTINODE || ((IX_PageNode<T> *)pBuffer)->nodeType == ROOTANDLASTINODE)
    {
        PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[slotIndex];

        // the leaf is empty
        if(((IX_PageNode<T> *)pBuffer)->child[slotIndex] == IX_EMPTY)
        {
            if(rc = AllocateLeafPage_t<T>(iPageNum, childPageNum))
                goto err_return;

            ((IX_PageNode<T> *)pBuffer)->child[slotIndex] = childPageNum;
        }

        if(rc = InsertEntryInLeaf_t<T>(childPageNum, pData, rid))
            goto err_return;

    }
    // the child is a node
    else
    {
        PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[slotIndex];

        // the node is empty
        if(childPageNum == IX_EMPTY)
        {

            if(rc = AllocateNodePage_t<T>(LASTINODE, iPageNum, childPageNum))
                goto err_return;

            ((IX_PageNode<T> *)pBuffer)->child[slotIndex] = childPageNum;
        }

        if(rc = InsertEntryInNode_t<T>(childPageNum, pData, rid))
            goto err_return;
    }


    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

err_return:
    return (rc);

    return OK_RC;
}

// Insert a new data in the index
template <typename T>
RC IX_IndexHandle::InsertEntryInLeaf_t(PageNum iPageNum, void *pData, const RID &rid)
{

    RC rc;
    char *pBuffer;
    int slotIndex;

    // get the current node
    if(rc = GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the right place to insert data in the tree
    slotIndex = ((IX_PageLeaf<T> *)pBuffer)->nbFilledSlots;

    // PB: OVERFLOW !!!!
    if(slotIndex > 3)
        return 1;

    copyGeneric(*((T*) pData), ((IX_PageLeaf<T> *)pBuffer)->v[slotIndex]);


    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

err_return:
    return (rc);

    return OK_RC;
}

template <typename T>
RC IX_IndexHandle::AllocateNodePage_t(const NodeType nodeType, const PageNum parent, PageNum &oPageNum)
{
    RC rc;
    PF_PageHandle pageHandle;
    char *pReadData;

    if (rc = pfFileHandle.AllocatePage(pageHandle))
        goto err_return;

    if (rc = pageHandle.GetPageNum(oPageNum))
        goto err_unpin;

    if (rc = pageHandle.GetData(pReadData))
        goto err_unpin;

    // Fill node

    ((IX_PageNode<T> *)pReadData)->parent = parent;
    ((IX_PageNode<T> *)pReadData)->nodeType = nodeType;
    ((IX_PageNode<T> *)pReadData)->nbFilledSlots = 0;

    for(int i=0; i<5; i++)
    {
        ((IX_PageNode<T> *)pReadData)->child[i] = IX_EMPTY;
    }



    // Mark the page dirty since we changed the next pointer
    if (rc = pfFileHandle.MarkDirty(oPageNum))
        goto err_unpin;

    // Unpin the page
    if (rc = pfFileHandle.UnpinPage(oPageNum))
        goto err_return;

    bHdrChanged = TRUE;

    // Return ok
    return (0);

err_unpin:
    pfFileHandle.UnpinPage(oPageNum);
err_return:
    return (rc);
}

template <typename T>
RC IX_IndexHandle::AllocateLeafPage_t(const PageNum parent, PageNum &oPageNum)
{
    RC rc;
    PF_PageHandle pageHandle;
    char *pReadData;

    if (rc = pfFileHandle.AllocatePage(pageHandle))
        goto err_return;

    if (rc = pageHandle.GetPageNum(oPageNum))
        goto err_unpin;

    if (rc = pageHandle.GetData(pReadData))
        goto err_unpin;

    // Fill node

    ((IX_PageLeaf<T> *)pReadData)->parent = parent;
    ((IX_PageLeaf<T> *)pReadData)->previous = IX_EMPTY;
    ((IX_PageLeaf<T> *)pReadData)->next = IX_EMPTY;
    ((IX_PageNode<T> *)pReadData)->nbFilledSlots = 0;

    for(int i=0; i<5; i++)
    {
        ((IX_PageNode<T> *)pReadData)->child[i] = IX_EMPTY;
    }

    // Mark the page dirty since we changed the next pointer
    if (rc = pfFileHandle.MarkDirty(oPageNum))
        goto err_unpin;

    // Unpin the page
    if (rc = pfFileHandle.UnpinPage(oPageNum))
        goto err_return;

    bHdrChanged = TRUE;

    // Return ok
    return (0);

err_unpin:
    pfFileHandle.UnpinPage(oPageNum);
err_return:
    return (rc);
}

// DO NOT FORGET TO CLOSE IT !
RC IX_IndexHandle::GetPageBuffer(const PageNum &iPageNum, char * & pBuffer) const
{
    RC rc;
    PF_PageHandle pageHandle;

    if(rc = pfFileHandle.GetThisPage(iPageNum, pageHandle))
        goto err_return;

    if (rc = pageHandle.GetData(pBuffer))
        goto err_unpin;

    return (0);

err_unpin:
    pfFileHandle.UnpinPage(iPageNum);
err_return:
    return (rc);
}

RC IX_IndexHandle::ReleaseBuffer(const PageNum &iPageNum, bool isDirty) const
{
    RC rc;

    // Mark the page dirty since we changed the next pointer
    if(isDirty)
    {
        if (rc = pfFileHandle.MarkDirty(iPageNum))
            goto err_unpin;
    }

    // Unpin the page
    if (rc = pfFileHandle.UnpinPage(iPageNum))
        goto err_return;

    return (0);

err_unpin:
    pfFileHandle.UnpinPage(iPageNum);
err_return:
    return (rc);

}




// Delete a new index entry
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid)
{
    return OK_RC;
}

// Force index files to disk
RC IX_IndexHandle::ForcePages()
{
    return OK_RC;
}

