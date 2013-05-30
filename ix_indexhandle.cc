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

int comparisonGeneric(const int &v1, const int &v2) { return v1 < v2; }
int comparisonGeneric(const float &v1, const float &v2) { return v1 < v2; }
int comparisonGeneric(const char* v1, const char* v2) { return strcmp(v1,v2) < 0; }


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

    InsertEntryInNode_t<T>(pageNum, pData, rid);

    return rc;

err_unpin:
   pfFileHandle.UnpinPage(pageNum);
err_return:
   return (rc);
}


// Insert a new index entry
template <typename T>
RC IX_IndexHandle::InsertEntryInNode_t(PageNum iPageNum, void *pData, const RID &rid)
{

    RC rc;
    PageNum pageNum;
    PF_PageHandle pageHandle;
    char *pReadData;


   if (rc = pageHandle.GetPageNum(iPageNum))
      goto err_unpin;

   if (rc = pageHandle.GetData(pReadData))
      goto err_unpin;

   for(int i=0; i<5; i++)
   {
       ((IX_PageNode<T> *)pReadData)->child[i] = -1;
   }


   for(int i=0; i<4; i++)
   {
       copyGeneric(((IX_PageNode<T> *)pReadData)->v[i],(*(T *) pData));
   }


   // Mark the page dirty since we changed the next pointer
   if (rc = pfFileHandle.MarkDirty(pageNum))
      // Should not happen
      goto err_unpin;

   // Unpin the page
   if (rc = pfFileHandle.UnpinPage(pageNum))
      goto err_return;

    // Recover from inconsistent state due to unexpected error
 err_unpin:
    pfFileHandle.UnpinPage(pageNum);
 err_return:
    return (rc);

    return OK_RC;
}

template <typename T>
RC IX_IndexHandle::AllocateNodePage_t(NodeType nodeType, PageNum parent, PageNum &oPageNum)
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

