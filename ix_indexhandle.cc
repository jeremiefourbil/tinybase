#include "ix_internal.h"

#include <cstdio>
#include <iostream>
#include <fstream>

using namespace std;

#define XML_FILE "tree.graphml"

IX_IndexHandle::IX_IndexHandle()
{

}

IX_IndexHandle::~IX_IndexHandle()
{

}


// Insert a new index entry
RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid)
{
    RC rc;

    if (pData == NULL)
        return (IX_NULLPOINTER);


    switch(fileHdr.attrType)
    {
        case INT:
        rc = InsertEntry_t<int>(pData, rid);
        break;
    case FLOAT:
        rc = InsertEntry_t<float>(pData, rid);
        break;
    case STRING:
        rc = InsertEntry_t<char[MAXSTRINGLEN]>(pData, rid);
        break;
    default:
        rc = IX_BADTYPE;
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

        fileHdr.rootNum = pageNum;
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
    ((IX_PageLeaf<T> *)pBuffer)->rid[slotIndex] = rid;


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


// Display Tree
RC IX_IndexHandle::DisplayTree()
{
    RC rc = OK_RC;

    int currentNodeId = 0;

    switch(fileHdr.attrType)
    {
        case INT:
        rc = DisplayTree_t<int>();
        case FLOAT:
        rc = DisplayTree_t<float>();
        case STRING:
        rc = DisplayTree_t<char[MAXSTRINGLEN]>();
    }

    return rc;
}

// display tree
template <typename T>
RC IX_IndexHandle::DisplayTree_t()
{
    RC rc;
    char *pBuffer;
    int currentNodeId = 0;
    int currentEdgeId = 0;
    ofstream xmlFile;

    if(rc = GetPageBuffer(fileHdr.rootNum, pBuffer))
        goto err_return;

    xmlFile.open(XML_FILE);
    xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    xmlFile << "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"  xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns http://www.yworks.com/xml/schema/graphml/1.1/ygraphml.xsd\"";

    xmlFile.close();

    DisplayNode_t<T>(fileHdr.rootNum, currentNodeId, currentEdgeId);

    if(rc = ReleaseBuffer(fileHdr.rootNum, false))
        goto err_return;

    return OK_RC;

    err_return:
    return (rc);
}

// display node
template <typename T>
RC IX_IndexHandle::DisplayNode_t(const PageNum pageNum, int &fatherNodeId, int &currentEdgeId)
{
    RC rc;
    char *pBuffer;
    int slotIndex;
    int slotChildIndex;
    ofstream xmlFile;

    // get the current node
    if(rc = GetPageBuffer(pageNum, pBuffer))
        goto err_return;

    // XML section
    xmlFile.open(XML_FILE);

    xmlFile << "<node id=" << fatherNodeId +1 << "/>";

    // for(slotIndex = 0;slotIndex < ((IX_PageNode<T> *)pBuffer)->nbFilledSlots; slotIndex++)
    // {
        // TODO display the value of the key
    // }
    // edge between the current node and the father

    xmlFile << "<edge id=\"" << currentEdgeId <<"\"" << " source=\"" << fatherNodeId << "\" target=\"" << fatherNodeId+1 << "\">";

    xmlFile.close();

    for(slotChildIndex = 0; slotChildIndex <= ((IX_PageNode<T> *)pBuffer)->nbFilledSlots; slotChildIndex++)
    {
        // the child is a leaf
        if(((IX_PageNode<T> *)pBuffer)->nodeType == LASTINODE || ((IX_PageNode<T> *)pBuffer)->nodeType == ROOTANDLASTINODE)
        {
            PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[slotIndex];
        // the leaf is empty
            if(childPageNum != IX_EMPTY)
            {
                fatherNodeId++;
                currentEdgeId++;
                if(rc = DisplayLeaf_t<T>(childPageNum, fatherNodeId, currentEdgeId))
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
                fatherNodeId++;
                currentEdgeId++;
                if(rc = DisplayNode_t<T>(childPageNum, fatherNodeId, currentEdgeId))
                    goto err_return;
            }
        }
    }

    if(rc = ReleaseBuffer(pageNum, false))
        goto err_return;

    return OK_RC;

    err_return:
    return (rc);
}

// display leaf
template <typename T>
RC IX_IndexHandle::DisplayLeaf_t(const PageNum pageNum, int &fatherNodeId, int &currentEdgeId)
{
    RC rc;
    char *pBuffer;
    int slotIndex;
    ofstream xmlFile;

    // XML section
    xmlFile.open(XML_FILE);

    xmlFile << "<node id=\"" << fatherNodeId+1 << "/>";

    // for(slotIndex = 0;slotIndex < ((IX_PageNode<T> *)pBuffer)->nbFilledSlots; slotIndex++)
    // {
        // TODO display the value of the key
    // }
    // edge between the current node and the father

    xmlFile << "<edge id=\"" << currentEdgeId << "\"" << " source=\"" << fatherNodeId << "\" target=\"" << fatherNodeId+1 << "\">";
    xmlFile.close();

    if(rc = GetPageBuffer(pageNum, pBuffer))
        goto err_return;
    for(slotIndex = 0; slotIndex < ((IX_PageLeaf<T> *)pBuffer)->nbFilledSlots; slotIndex++)
    {
        // print xml line
    }

    if(rc = ReleaseBuffer(pageNum, false))
        goto err_return;

    return OK_RC;

    err_return:
    return (rc);
}
