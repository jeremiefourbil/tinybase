#include "ix_internal.h"

#include "ix_btree.h"

#include <cstdio>
#include <iostream>
#include <fstream>

using namespace std;


IX_IndexHandle::IX_IndexHandle()
{
    pBTree = new IX_BTree();
}

IX_IndexHandle::~IX_IndexHandle()
{
    if(pBTree)
    {
        delete pBTree;
        pBTree = NULL;
    }
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

    rc = pBTree->InsertEntry(pData, rid);

    return rc;
}


// ***********************
// Delete an entry
// ***********************

// Delete a new index entry
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid)
{
    RC rc;

    if (pData == NULL)
        return (IX_NULLPOINTER);

    rc = pBTree->DeleteEntry(pData, rid);

    return rc;
}


// ***********************
// Force pages
// ***********************


// TO BE VERIFIED


// Force index files to disk
RC IX_IndexHandle::ForcePages()
{
    return pfFileHandle.ForcePages();
}


// ***********************
// Display the tree
// ***********************

// Display Tree
RC IX_IndexHandle::DisplayTree()
{
    RC rc = OK_RC;

    pBTree->DisplayTree();

    return rc;
}
