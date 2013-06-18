#include "ix_internal.h"

#include "ix_btree.h"
#include "ix_hash.h"

#include <cstdio>
#include <iostream>
#include <fstream>

using namespace std;


IX_IndexHandle::IX_IndexHandle()
{
    pBTree = new IX_BTree();

#ifdef IX_USE_HASH
    pHash = new IX_Hash();
#else
    pHash = NULL;
#endif
}

IX_IndexHandle::~IX_IndexHandle()
{
    if(pBTree)
    {
        delete pBTree;
        pBTree = NULL;
    }

#ifdef IX_USE_HASH
    if(pHash)
    {
        delete pHash;
        pHash = NULL;
    }
#endif
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

    if((rc = pBTree->InsertEntry(pData, rid)))
        goto err_return;

#ifdef IX_USE_HASH
    if((rc = pHash->InsertEntry(pData, rid)))
        goto err_return;
#endif

    return rc;

err_return:
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

    if((rc = pBTree->DeleteEntry(pData, rid)))
        goto err_return;

#ifdef IX_USE_HASH
    if((rc = pHash->DeleteEntry(pData, rid)))
        goto err_return;
#endif

    return rc;

err_return:
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

    if((rc = pBTree->DisplayTree()))
        goto err_return;

#ifdef IX_USE_HASH
    if((rc = pHash->DisplayTree()))
        goto err_return;
#endif

    return rc;

err_return:
    return rc;
}
