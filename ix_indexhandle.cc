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


// ***********************
// Insertion in tree
// ***********************


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


// templated insertion
template <typename T>
RC IX_IndexHandle::InsertEntry_t(void *pData, const RID &rid)
{
    RC rc;
    PageNum pageNum;
    PageNum newChildPageNum;
    T medianValue;

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

    if(rc = InsertEntryInNode_t<T>(pageNum, pData, rid, newChildPageNum, medianValue))
        goto err_return;

    return rc;

    err_unpin:
    pfFileHandle.UnpinPage(pageNum);
    err_return:
    return (rc);
}


// Node insertion
template <typename T>
RC IX_IndexHandle::InsertEntryInNode_t(PageNum iPageNum, void *pData, const RID &rid, PageNum &newChildPageNum,T &medianValue)
{

    RC rc;
    char *pBuffer;
    int slotIndex;
    int pointerIndex;

    // get the current node
    if(rc = GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the right place to insert the value in the node
    slotIndex = 0;
    while(slotIndex < ((IX_PageNode<T> *)pBuffer)->nbFilledSlots && comparisonGeneric(*((T*) pData), ((IX_PageNode<T> *)pBuffer)->v[slotIndex]) >= 0)
    {
       slotIndex++;
    }

    pointerIndex = slotIndex;
    if(slotIndex == ((IX_PageNode<T> *)pBuffer)->nbFilledSlots-1 && comparisonGeneric(*((T*) pData), ((IX_PageNode<T> *)pBuffer)->v[slotIndex]) >= 0)
        pointerIndex++;

    // add value to the node and increment number of filled slots
    if(slotIndex < 4 && slotIndex == ((IX_PageNode<T> *)pBuffer)->nbFilledSlots)
    {
        copyGeneric(*((T*) pData), ((IX_PageNode<T> *)pBuffer)->v[slotIndex]);
        ((IX_PageNode<T> *)pBuffer)->nbFilledSlots = slotIndex + 1;
        pointerIndex = slotIndex+1;
    }

    // the child is a leaf
    if(((IX_PageNode<T> *)pBuffer)->nodeType == LASTINODE || ((IX_PageNode<T> *)pBuffer)->nodeType == ROOTANDLASTINODE)
    {
        PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[pointerIndex];

        // the leaf is empty
        if(((IX_PageNode<T> *)pBuffer)->child[pointerIndex] == IX_EMPTY)
        {
            if(rc = AllocateLeafPage_t<T>(iPageNum, childPageNum))
                goto err_return;

            ((IX_PageNode<T> *)pBuffer)->child[pointerIndex] = childPageNum;
        }

        if(rc = InsertEntryInLeaf_t<T>(childPageNum, pData, rid, newChildPageNum, medianValue))
            goto err_return;

    }
    // the child is a node
    else
    {
        PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[pointerIndex];

        // the node is empty
        if(childPageNum == IX_EMPTY)
        {

            if(rc = AllocateNodePage_t<T>(LASTINODE, iPageNum, childPageNum))
                goto err_return;

            ((IX_PageNode<T> *)pBuffer)->child[pointerIndex] = childPageNum;
        }

        if(rc = InsertEntryInNode_t<T>(childPageNum, pData, rid, newChildPageNum, medianValue))
            goto err_return;
    }


    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

    err_return:
    return (rc);

    return OK_RC;
}

// Leaf insertion
template <typename T>
RC IX_IndexHandle::InsertEntryInLeaf_t(PageNum iPageNum, void *pData, const RID &rid, PageNum &newChildPageNum,T &medianValue)
{
    RC rc = OK_RC;

    char *pBuffer;
    int slotIndex;
    bool alreadyInLeaf = false;

    PageNum bucketPageNum;

    char *newPageBuffer;

    // fixe la valeur de newChildPageNum s'il n'y a pas de split
    newChildPageNum = IX_EMPTY;

    // get the current leaf
    if(rc = GetPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the right place to insert data in the tree
    for(int i=0; i<((IX_PageLeaf<T> *)pBuffer)->nbFilledSlots; i++)
    {
        if(comparisonGeneric(((IX_PageLeaf<T> *)pBuffer)->v[i], *(T *) pData) == 0)
        {
            slotIndex = i;
            alreadyInLeaf = true;
        }
    }

    if(!alreadyInLeaf)
    {
        slotIndex = ((IX_PageLeaf<T> *)pBuffer)->nbFilledSlots;

        // PB: OVERFLOW !!!!
        if(slotIndex > 3)
        {
        cout << "Overflow" << endl;
        
        PageNum currentPrevPageNum;
        PageNum currentNextPageNum;
        ((IX_PageLeaf<T> *)pBuffer)->next = currentNextPageNum;
        ((IX_PageLeaf<T> *)pBuffer)->previous = currentPrevPageNum;

        // creer une nouvelle page
        if (rc = AllocateLeafPage_t<T>(iPageNum, newChildPageNum))
            goto err_return;

        // on met à jour le lien entre la feuille courante et la nouvelle feuille
        ((IX_PageLeaf<T> *)pBuffer)->next = newChildPageNum;

        // on relâche la feuille courante
        if(rc = ReleaseBuffer(iPageNum, false))
            goto err_return;

        // on charge la nouvelle feuille pour pouvoir mettre à jour les liens
        if(rc = GetPageBuffer(newChildPageNum, newPageBuffer))
            goto err_return;
        ((IX_PageLeaf<T> *)newPageBuffer)->next = currentNextPageNum;
        ((IX_PageLeaf<T> *)newPageBuffer)->previous = bucketPageNum;

        // on rempli la moitié des valeurs de l'ancienne feuille dans la nouvelle feuille

        // on rempli aussi la valeur des buckets pour garder la cohérence

        rc = ReleaseBuffer(newChildPageNum, false);
        return rc;
//        goto err_release;
        }

        ((IX_PageLeaf<T> *)pBuffer)->nbFilledSlots = slotIndex+1;

        copyGeneric(*((T*) pData), ((IX_PageLeaf<T> *)pBuffer)->v[slotIndex]);
    }



    // RID bucket management
    bucketPageNum =  ((IX_PageLeaf<T> *)pBuffer)->bucket[slotIndex];

    if(bucketPageNum == IX_EMPTY)
    {
        if(rc = AllocateBucketPage(iPageNum, bucketPageNum))
            goto err_return;

        ((IX_PageLeaf<T> *)pBuffer)->bucket[slotIndex] = bucketPageNum;
    }

    if(rc = InsertEntryInBucket(bucketPageNum, rid))
        goto err_return;


    // sort the current leaf
    // Val: disabled sort (pb = does not sort the children)
    sortGeneric(((IX_PageLeaf<T> *)pBuffer)->v, ((IX_PageLeaf<T> *)pBuffer)->nbFilledSlots);

    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

    return rc;


    err_release:
        rc = ReleaseBuffer(iPageNum, false);
    err_return:
    return (rc);
}

template <typename T>
RC IX_IndexHandle::RedistributeValuesAndBucket(void *pBuffer1, void *pBuffer2, T &medianValue)
{
    return OK_RC;
}

// bucket insertion
RC IX_IndexHandle::InsertEntryInBucket(PageNum iPageNum, const RID &rid)
{
    RC rc = OK_RC;

    char *pBuffer;

    // get the current bucket
    if(rc = GetPageBuffer(iPageNum, pBuffer))
        goto err_return;


    // WARNING: DOES NOT SUPPORT OVERFLOW

    memcpy(pBuffer + sizeof(IX_PageBucketHdr) + ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots * sizeof(RID),
           (void*) &rid, sizeof(rid));



    ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots = ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots + 1;


    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

    return rc;


    err_release:
        rc = ReleaseBuffer(iPageNum, false);
    err_return:
    return (rc);
}

// ***********************
// Delete an entry
// ***********************

// Delete a new index entry
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid)
{
    return OK_RC;
}


// ***********************
// Force pages
// ***********************

// Force index files to disk
RC IX_IndexHandle::ForcePages()
{
    return OK_RC;
}

// ***********************
// Page allocations
// ***********************

// node page
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

// leaf page
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
    ((IX_PageLeaf<T> *)pReadData)->nbFilledSlots = 0;

    for(int i=0; i<4; i++)
    {
        ((IX_PageLeaf<T> *)pReadData)->bucket[i] = IX_EMPTY;
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

// RID bucket page
RC IX_IndexHandle::AllocateBucketPage(const PageNum parent, PageNum &oPageNum)
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

    ((IX_PageBucketHdr *)pReadData)->parent = parent;
    ((IX_PageBucketHdr *)pReadData)->next = IX_EMPTY;
    ((IX_PageBucketHdr *)pReadData)->nbFilledSlots = 0;

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

// ***********************
// Buffer management
// ***********************

// get the buffer
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

// release the buffer
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

// ***********************
// Display the tree
// ***********************

// Display Tree
RC IX_IndexHandle::DisplayTree()
{
    RC rc = OK_RC;

    switch(fileHdr.attrType)
    {
        case INT:
        rc = DisplayTree_t<int>();
        break;
        case FLOAT:
        rc = DisplayTree_t<float>();
        break;
        // case STRING:
        // rc = DisplayTree_t<char[MAXSTRINGLEN]>();
        // break;
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
    int fatherNodeId = 0;
    int currentEdgeId = 0;
    ofstream xmlFile;

    if(rc = GetPageBuffer(fileHdr.rootNum, pBuffer))
        goto err_return;

    xmlFile.open(XML_FILE);
    xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    xmlFile << "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"  xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns http://www.yworks.com/xml/schema/graphml/1.1/ygraphml.xsd\" xmlns:y=\"http://www.yworks.com/xml/graphml\">" << endl;
    xmlFile << "<key id=\"d0\" for=\"node\" yfiles.type=\"nodegraphics\"/>" << endl;
    xmlFile << "<graph>" << endl;
    xmlFile << "<node id=\"" << currentNodeId << "\"/>" << endl;
    xmlFile.close();

    currentNodeId++;

    DisplayNode_t<T>(fileHdr.rootNum, fatherNodeId, currentNodeId, currentEdgeId);

    xmlFile.open(XML_FILE, ios::app);
    xmlFile << "</graph>" << endl;
    xmlFile << "</graphml>" << endl;
    xmlFile.close();
    
    if(rc = ReleaseBuffer(fileHdr.rootNum, false))
        goto err_return;

    return OK_RC;

    err_return:
    return (rc);
}

// display node
template <typename T>
RC IX_IndexHandle::DisplayNode_t(const PageNum pageNum,const int &fatherNodeId, int &currentNodeId, int &currentEdgeId)
{
    RC rc;
    char *pBuffer;
    int slotIndex;
    int slotChildIndex;
    int newFatherNodeId = currentNodeId;
    ofstream xmlFile;

    // debug
    cout << "(" << pageNum << "," << fatherNodeId << "," << currentNodeId << ")" << endl;

    // get the current node
    if(rc = GetPageBuffer(pageNum, pBuffer))
        goto err_return;

    // XML section
    xmlFile.open(XML_FILE, ios::app);

    xmlFile << "<node id=\"" << currentNodeId << "\">" << endl;

    xmlFile << "<data key=\"d0\"><y:ShapeNode><y:BorderStyle type=\"line\" width=\"1.0\" color=\"#000000\"/>" << endl;
    xmlFile << "<y:Geometry height=\"80.0\" />" << endl;
    xmlFile << "<y:NodeLabel>" << endl;

    for(slotIndex = 0;slotIndex < ((IX_PageNode<T> *)pBuffer)->nbFilledSlots; slotIndex++)
    {
        xmlFile << ((IX_PageNode<T> *)pBuffer)->v[slotIndex] << endl;
    }
    xmlFile << "</y:NodeLabel><y:Shape type=\"rectangle\"/></y:ShapeNode></data></node>" << endl;
    xmlFile << "<edge id=\"" << currentEdgeId <<"\"" << " source=\"" << fatherNodeId << "\" target=\"" << currentNodeId << "\"/>" << endl;

    xmlFile.close();

    for(slotChildIndex = 0; slotChildIndex <= ((IX_PageNode<T> *)pBuffer)->nbFilledSlots; slotChildIndex++)
    {
        // the child is a leaf
        if(((IX_PageNode<T> *)pBuffer)->nodeType == LASTINODE || ((IX_PageNode<T> *)pBuffer)->nodeType == ROOTANDLASTINODE)
        {
            PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[slotChildIndex];
        // the leaf is empty
            if(childPageNum != IX_EMPTY)
            {
                currentNodeId++;
                currentEdgeId++;
                if(rc = DisplayLeaf_t<T>(childPageNum, newFatherNodeId, currentNodeId, currentEdgeId))
                    goto err_return;
            }
        }
    // the child is a node
        else
        {
            PageNum childPageNum = ((IX_PageNode<T> *)pBuffer)->child[slotChildIndex];
        // the node is empty
            if(childPageNum == IX_EMPTY)
            {
                currentNodeId++;
                currentEdgeId++;
                if(rc = DisplayNode_t<T>(childPageNum, newFatherNodeId, currentNodeId, currentEdgeId))
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
RC IX_IndexHandle::DisplayLeaf_t(const PageNum pageNum,const int &fatherNodeId, int &currentNodeId, int &currentEdgeId)
{
    RC rc;
    char *pBuffer;
    int slotIndex;
    ofstream xmlFile;

    cout << "[" << pageNum << "," << fatherNodeId << "," << currentNodeId << "]" << endl;

    if(rc = GetPageBuffer(pageNum, pBuffer))
        goto err_return;

    // XML section
    xmlFile.open(XML_FILE, ios::app);

    xmlFile << "<node id=\"" << currentNodeId << "\">" << endl;

    xmlFile << "<data key=\"d0\"><y:ShapeNode><y:BorderStyle type=\"line\" width=\"1.0\" color=\"#008000\"/>" << endl;
    xmlFile << "<y:Geometry height=\"80.0\" />" << endl;
    xmlFile << "<y:NodeLabel>" << endl;

    for(slotIndex = 0;slotIndex < ((IX_PageLeaf<T> *)pBuffer)->nbFilledSlots; slotIndex++)
    {
        xmlFile << ((IX_PageLeaf<T> *)pBuffer)->v[slotIndex] << endl;
    }
    xmlFile << "</y:NodeLabel><y:Shape type=\"rectangle\"/></y:ShapeNode></data></node>" << endl;

    xmlFile << "<edge id=\"" << currentEdgeId << "\"" << " source=\"" << fatherNodeId << "\" target=\"" << currentNodeId << "\"/>" << endl;
    xmlFile.close();


    if(rc = ReleaseBuffer(pageNum, false))
        goto err_return;

    return OK_RC;

    err_return:
    return (rc);
}
