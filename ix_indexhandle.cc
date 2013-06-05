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
RC IX_IndexHandle::InsertEntry_t(T iValue, const RID &rid)
{
    RC rc;
    PageNum pageNum;

    IX_PageNode<T> *pBuffer, *pNewRootBuffer, *newPageBuffer;

    // Pour la récursion descendante
    PageNum newChildPageNum = IX_EMPTY;
    T medianChildValue;

    PageNum newRootNum = IX_EMPTY;

    pageNum = fileHdr.rootNum;

    // no root, create one
    if(pageNum == IX_EMPTY)
    {
        if(rc = AllocateNodePage_t<T>(ROOTANDLASTINODE, IX_EMPTY, pageNum))
            goto err_return;

        fileHdr.rootNum = pageNum;
    }

    // let's call the recursion method, start with root

    if(rc = InsertEntryInNode_t<T>(pageNum, iValue, rid, newChildPageNum, medianChildValue))
        goto err_return;



    // il y a eu un split de noeuds
    if(newChildPageNum != IX_EMPTY)
    {
        if(rc = GetNodePageBuffer(pageNum, pBuffer))
            goto err_return;


        cout << "la racine doit être splittée" << endl;

            // nouvelle racine
        if(rc = AllocateNodePage_t<T>(ROOT,IX_EMPTY, newRootNum))
            goto err_return;


            // on met à jour la nouvelle racine
        if(rc = GetNodePageBuffer(newRootNum, pNewRootBuffer))
            goto err_return;

        copyGeneric(medianChildValue, pNewRootBuffer->v[0]);
        pNewRootBuffer->nbFilledSlots++;
        pNewRootBuffer->child[0] = pageNum;
        pNewRootBuffer->child[1] = newChildPageNum;

        if(rc = ReleaseBuffer(newRootNum, true))
            return rc;

        fileHdr.rootNum = newRootNum;

        cout << "Root in root" << fileHdr.rootNum << endl;

        bHdrChanged = true;


            // on récupère le buffer du nouveau noeud
        if(rc = GetNodePageBuffer(newChildPageNum, newPageBuffer))
            goto err_return;

            // on met à jour les deux fils
        if(pBuffer->nodeType == ROOTANDLASTINODE)
        {
            pBuffer->nodeType = LASTINODE;
            newPageBuffer->nodeType = LASTINODE;
        }
        else
        {
            pBuffer->nodeType = INODE;
            newPageBuffer->nodeType = INODE;
        }

        if(rc = ReleaseBuffer(newChildPageNum, true))
            return rc;


        if(rc = ReleaseBuffer(pageNum, true))
            return rc;

    }


    return rc;

    err_unpin:
    pfFileHandle.UnpinPage(pageNum);
    err_return:
    return (rc);
}


// Node insertion
template <typename T>
RC IX_IndexHandle::InsertEntryInNode_t(PageNum iPageNum, T iValue, const RID &rid, PageNum &newNodePageNum,T &medianParentValue)
{

    RC rc;
    IX_PageNode<T> *pBuffer;
    int pointerIndex;
    IX_PageNode<T> *newPageBuffer; // pour le possible split

    PageNum newChildPageNum = IX_EMPTY;
    T medianChildValue;


    // get the current node
    if(rc = GetNodePageBuffer(iPageNum, pBuffer))
        goto err_return;

    if(pBuffer->nodeType == ROOTANDLASTINODE && pBuffer->nbFilledSlots == 0)
    {
        copyGeneric(iValue, pBuffer->v[0]);
        pBuffer->nbFilledSlots++;
    }

    pointerIndex = 0;
    while(pointerIndex < pBuffer->nbFilledSlots && comparisonGeneric(iValue, pBuffer->v[pointerIndex]) >= 0)
    {
        pointerIndex++;
    }

    if(pointerIndex == pBuffer->nbFilledSlots-1 && comparisonGeneric(iValue, pBuffer->v[pointerIndex]) >= 0)
        pointerIndex++;

    // the child is a leaf
    if(pBuffer->nodeType == LASTINODE || pBuffer->nodeType == ROOTANDLASTINODE)
    {
        PageNum childPageNum = pBuffer->child[pointerIndex];

        // the leaf is empty
        if(pBuffer->child[pointerIndex] == IX_EMPTY)
        {
            if(rc = AllocateLeafPage_t<T>(iPageNum, childPageNum))
                goto err_return;

            pBuffer->child[pointerIndex] = childPageNum;
        }

        if(rc = InsertEntryInLeaf_t<T>(childPageNum, iValue, rid, newChildPageNum, medianChildValue))
            goto err_return;
    }
    // the child is a node
    else
    {
        PageNum childPageNum = pBuffer->child[pointerIndex];

        // the node is empty
        if(childPageNum == IX_EMPTY)
        {
            // Todo : error state
            cout << "Error: Empty Node child" << endl;
        }

        if(rc = InsertEntryInNode_t<T>(childPageNum, iValue, rid, newChildPageNum, medianChildValue))
            goto err_return;
    }

    // il y a eu un split à cause de l'insertion de la valeur
    if(newChildPageNum != IX_EMPTY)
    {
        // on essaye d'insérer la valeur médiane dans le noeud courant
        cout << "split en dessous" << endl;
        if(pBuffer->nbFilledSlots<IX_MAX_NUMBER_OF_VALUES)
        {
            cout << "le noeud courant a de la place :" << endl;
            // on insère la valeur médiane liée à son fils droit
            copyGeneric(medianChildValue,pBuffer->v[pBuffer->nbFilledSlots]);
            // on incrémente le nombre de slots occupés
            pBuffer->nbFilledSlots++;
            // on ajoute le fils droit correspondant au nouveau slot
            pBuffer->child[pBuffer->nbFilledSlots] = newChildPageNum;
            // il faut ordonner les valeurs du noeud
            sortNodeGeneric(pBuffer->v,pBuffer->child, pBuffer->nbFilledSlots);
        }
        else
        {
            cout << "le noeud courant doit être splité" << endl;
            // le noeud courant est plein donc on va créer un nouveau noeud
            if(rc = AllocateNodePage_t<T>(pBuffer->nodeType,pBuffer->parent, newNodePageNum))
                goto err_return;
            // on récupère le buffer du nouveau noeud
            if(rc = GetNodePageBuffer(newNodePageNum, newPageBuffer))
                goto err_return;

            // redistribution entre le noeud courant et le nouveau noeud
            if(rc = RedistributeValuesAndChildren<T>(pBuffer, newPageBuffer, medianChildValue, medianParentValue, newChildPageNum))
                goto err_return;

            if(rc = ReleaseBuffer(newNodePageNum, true))
                return rc;
        }
    }


    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

    err_return:
    return (rc);

    return OK_RC;
}

// Leaf insertion
template <typename T>
RC IX_IndexHandle::InsertEntryInLeaf_t(PageNum iPageNum, T iValue, const RID &rid, PageNum &newChildPageNum,T &medianValue)
{
    RC rc = OK_RC;
    IX_PageLeaf<T> *pBuffer, *newPageBuffer;
    PageNum bucketPageNum = IX_EMPTY;
    int slotIndex;
    bool alreadyInLeaf = false;

    // fixe la valeur de newChildPageNum s'il n'y a pas de split
    newChildPageNum = IX_EMPTY;

    // get the current leaf
    if(rc = GetLeafPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the right place to insert data in the tree
    for(int i=0; i<pBuffer->nbFilledSlots; i++)
    {
        if(comparisonGeneric(pBuffer->v[i], iValue) == 0)
        {
            slotIndex = i;
            alreadyInLeaf = true;
            break;
        }
    }

    if(!alreadyInLeaf)
    {
        if(pBuffer->nbFilledSlots == IX_MAX_NUMBER_OF_VALUES)
        {
            cout << "Overflow" << endl;

            PageNum currentNextPageNum = pBuffer->next;

            // créer une nouvelle page
            if (rc = AllocateLeafPage_t<T>(pBuffer->parent, newChildPageNum))
                goto err_return;

            // on alloue une page pour le bucket
            if(rc = AllocateBucketPage(newChildPageNum, bucketPageNum))
                goto err_return;

            // on met à jour le lien entre la feuille courante et la nouvelle feuille
            pBuffer->next = newChildPageNum;

            // on charge la nouvelle feuille pour pouvoir mettre à jour les liens
            if(rc = GetLeafPageBuffer(newChildPageNum, newPageBuffer))
                goto err_return;
            newPageBuffer->next = currentNextPageNum;
            newPageBuffer->previous = iPageNum;

            // on remplit la moitié des valeurs de l'ancienne feuille dans la nouvelle feuille
            if(rc = RedistributeValuesAndBuckets<T>(pBuffer, newPageBuffer, iValue, medianValue, bucketPageNum))
                goto err_return;
            // on relâche la nouvelle feuille
            if(rc = ReleaseBuffer(newChildPageNum, true))
                goto err_return;
        } else {
            // des slots sont disponibles alors on insère la valeur dans la feuille

            if(rc = AllocateBucketPage(iPageNum, bucketPageNum))
                goto err_return;

            copyGeneric(iValue, pBuffer->v[pBuffer->nbFilledSlots]);

            pBuffer->bucket[pBuffer->nbFilledSlots] = bucketPageNum;
            pBuffer->nbFilledSlots++;

            sortGeneric(pBuffer->v, pBuffer->bucket, pBuffer->nbFilledSlots);
        }
    }
    else
    {
        bucketPageNum =  pBuffer->bucket[slotIndex];
    }

    // error
    if(bucketPageNum == IX_EMPTY)
    {
        cout << "Error: bucket page empty" << endl;
        return 1;
    }

    // insert the value in the bucket
    if(rc = InsertEntryInBucket(bucketPageNum, rid))
        goto err_return;

    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

    return rc;


    err_release:
    rc = ReleaseBuffer(iPageNum, false);
    err_return:
    return (rc);
}

template <typename T>
RC IX_IndexHandle::RedistributeValuesAndChildren(IX_PageNode<T> *pBufferCurrentNode, IX_PageNode<T> *pBufferNewNode,T &medianChildValue, T &medianParentValue,const PageNum &newNodePageNum)
{
    T array[5];
    PageNum child[5];

    // on remplit le nouveau tableau avec les 5 valeurs
    for(int i=0;i<pBufferCurrentNode->nbFilledSlots;i++)
    {
        copyGeneric(pBufferCurrentNode->v[i], array[i]);
        // offset pour ne pas prendre le fils de gauche
        child[i] = pBufferCurrentNode->child[i+1];
    }
    // on place à la fin du tableau la nouvelle valeur
    copyGeneric(medianChildValue, array[4]);
    // on remplit la page du nouveau 
    child[4] = newNodePageNum;
    // on ordonne le nouveau tableau
    sortGeneric(array, child, 5);
    // on récupère la valeur médiane
    copyGeneric(array[2],medianParentValue);
    // on initialise le nombre de slots pour chaque feuille
    pBufferCurrentNode->nbFilledSlots = 0;
    pBufferNewNode->nbFilledSlots = 0;

    // redistribution des valeurs
    for(int j=0;j<5;j++)
    {
        if(j<2){
            copyGeneric(array[j], pBufferCurrentNode->v[j]);
            // on met à jour les buckets pour être cohérent
            pBufferCurrentNode->child[j+1] = child[j];
            // on incrémente le nombre slots remplis
            pBufferCurrentNode->nbFilledSlots++;
        } else {
        // on copie la valeur dans la nouvelle feuille
            copyGeneric(array[j], pBufferNewNode->v[j-2]);
        // on met à jour les buckets pour être cohérent
            pBufferNewNode->child[j-1] = child[j];
        // on incrémente le nombre slots remplis
            pBufferNewNode->nbFilledSlots++;
        }
    }
    return OK_RC;
}

template <typename T>
RC IX_IndexHandle::RedistributeValuesAndBuckets(IX_PageLeaf<T> *pBufferCurrentLeaf, IX_PageLeaf<T> *pBufferNewLeaf, T iValue, T &medianValue,const PageNum &bucketPageNum)
{
    T array[5];
    PageNum bucket[5];

    cout << "--- redistribution --" << endl;
    cout << "--- before :" << endl;

    // on remplit le nouveau tableau avec les 5 valeurs
    for(int i=0;i<pBufferCurrentLeaf->nbFilledSlots;i++)
    {
        copyGeneric(pBufferCurrentLeaf->v[i], array[i]);
        bucket[i] = pBufferCurrentLeaf->bucket[i];
        cout << "(" << array[i] << "," << bucket[i] <<")" ;
    }
    cout << endl;
    // on place à la fin du tableau la nouvelle valeur
    copyGeneric(iValue, array[4]);
    // on remplit la page du nouveau bucket
    bucket[4] = bucketPageNum;
    // on ordonne le nouveau tableau
    sortGeneric(array, bucket, 5);
    // on récupère la valeur médiane
    copyGeneric(array[2],medianValue);
    // on initialise le nombre de slots pour chaque feuille
    pBufferCurrentLeaf->nbFilledSlots = 0;
    pBufferNewLeaf->nbFilledSlots = 0;
    // redistribution des valeurs
    cout << "--- after :" << endl;
    for(int j=0;j<5;j++)
    {
        if(j<2){
            copyGeneric(array[j], pBufferCurrentLeaf->v[j]);
            // on met à jour les buckets pour être cohérent
            pBufferCurrentLeaf->bucket[j] = bucket[j];
            // on incrémente le nombre slots remplis
            pBufferCurrentLeaf->nbFilledSlots++;
            cout << "(" << array[j] << "," << bucket[j] <<")" ;
        } else {
        // on copie la valeur dans la nouvelle feuille
            copyGeneric(array[j], pBufferNewLeaf->v[j-2]);
        // on met à jour les buckets pour être cohérent
            pBufferNewLeaf->bucket[j-2] = bucket[j];
        // on incrémente le nombre slots remplis
            pBufferNewLeaf->nbFilledSlots++;
            cout << "(" << array[j] << "," << bucket[j] <<")" ;
        }
    }
    cout << endl;
    return OK_RC;
}

// bucket insertion
RC IX_IndexHandle::InsertEntryInBucket(PageNum iPageNum, const RID &rid)
{
    RC rc = OK_RC;
    cout << iPageNum << endl;
    char *pBuffer;
    // get the current bucket
    if(rc = GetPageBuffer(iPageNum, pBuffer))
        goto err_return;


    // WARNING: DOES NOT SUPPORT OVERFLOW

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

// ***********************
// Delete an entry
// ***********************

// Delete a new index entry
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid)
{
    RC rc;

    if (pData == NULL)
        return (IX_NULLPOINTER);


    switch(fileHdr.attrType)
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
RC IX_IndexHandle::DeleteEntry_t(T iValue, const RID &rid)
{
    RC rc;
    PageNum pageNum;
    IX_PageNode<T> * pBuffer;
    T updatedChildValue;
    bool updateChildIndex = false;
    int slotIndex = 0;

    pageNum = fileHdr.rootNum;

    // no root, create one
    if(pageNum == IX_EMPTY)
    {
        return(IX_ENTRY_DOES_NOT_EXIST);
    }

    // let's call the recursion method, start with root

    if(rc = DeleteEntryInNode_t<T>(pageNum, iValue, rid, updatedChildValue, updateChildIndex))
        goto err_return;
    if(updateChildIndex)
    {
        // get the current node
        if(rc = GetNodePageBuffer(pageNum, pBuffer))
            goto err_return;
        slotIndex = 0;
        while(slotIndex < pBuffer->nbFilledSlots && comparisonGeneric(iValue, pBuffer->v[slotIndex]) >= 0)
        {
            slotIndex++;
        }
        copyGeneric(updatedChildValue, pBuffer->v[slotIndex]);
    }
    return rc;

    err_return:
    return (rc);
}

// Node Deletion
template <typename T>
RC IX_IndexHandle::DeleteEntryInNode_t(PageNum iPageNum, T iValue, const RID &rid, T &updatedParentValue, bool &updateParentIndex)
{

    RC rc;
    IX_PageNode<T> * pBuffer;
    int slotIndex;
    int pointerIndex;
    bool updateChildIndex = false;
    T updatedChildValue;

    // get the current node
    if(rc = GetNodePageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the right branch
    slotIndex = 0;
    while(slotIndex < pBuffer->nbFilledSlots && comparisonGeneric(iValue, pBuffer->v[slotIndex]) >= 0)
    {
       slotIndex++;
   }

   pointerIndex = slotIndex;
   if(slotIndex == pBuffer->nbFilledSlots-1 && comparisonGeneric(iValue, pBuffer->v[slotIndex]) >= 0)
    pointerIndex++;

    // the child is a leaf
    if(pBuffer->nodeType == LASTINODE || pBuffer->nodeType == ROOTANDLASTINODE)
    {
        PageNum childPageNum = pBuffer->child[pointerIndex];

            // the leaf is empty
        if(pBuffer->child[pointerIndex] == IX_EMPTY)
        {
            rc = IX_ENTRY_DOES_NOT_EXIST;
            goto err_release;
        }

        if(rc = DeleteEntryInLeaf_t<T>(childPageNum, iValue, rid, updatedChildValue, updateChildIndex))
            goto err_return;
        // mettre à jour l'index car la valeur minimale de la feuille a changé
        if(updateChildIndex)
        {
            copyGeneric(updatedChildValue, pBuffer->v[slotIndex]);
            if(slotIndex == 0){
                copyGeneric(pBuffer->v[slotIndex], updatedParentValue);
                updateParentIndex = true;
            }
        }
    }
        // the child is a node
    else
    {
        PageNum childPageNum = pBuffer->child[pointerIndex];

            // the node is empty
        if(childPageNum == IX_EMPTY)
        {
            rc = IX_ENTRY_DOES_NOT_EXIST;
            goto err_release;
        }

        if(rc = DeleteEntryInNode_t<T>(childPageNum, iValue, rid, updatedChildValue, updateChildIndex))
            goto err_return;
        if(updateChildIndex)
        {
            copyGeneric(updatedChildValue, pBuffer->v[slotIndex]);
            if(slotIndex == 0){
                copyGeneric(pBuffer->v[slotIndex], updatedParentValue);
                updateParentIndex = true;
            }
        }
    }


    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

    return rc;

    err_release:
    ReleaseBuffer(iPageNum, false);
    err_return:
    return (rc);

    return OK_RC;
}

// Leaf Deletion
template <typename T>
RC IX_IndexHandle::DeleteEntryInLeaf_t(PageNum iPageNum, T iValue, const RID &rid, T &updatedParentValue, bool &updateParentIndex)
{
    RC rc = OK_RC;

    IX_PageLeaf<T> *pBuffer;
    int slotIndex;
    bool foundInLeaf = false;
    PageNum bucketPageNum;

    // get the current leaf
    if(rc = GetLeafPageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the right place to Delete data in the tree
    for(int i=0; i<pBuffer->nbFilledSlots; i++)
    {
        if(comparisonGeneric(pBuffer->v[i], iValue) == 0)
        {
            slotIndex = i;
            foundInLeaf = true;
        }
    }

    if(!foundInLeaf)
    {
        rc = IX_ENTRY_DOES_NOT_EXIST;
        goto err_release;
    }

    bucketPageNum = pBuffer->bucket[slotIndex];
    if(rc = DeleteEntryInBucket(bucketPageNum, rid))
    {
        goto err_release;
    }

    // have to delete bucket page and leaf value
    if(bucketPageNum = IX_EMPTY)
    {
        rc = pfFileHandle.DisposePage(pBuffer->bucket[slotIndex]);
        pBuffer->bucket[slotIndex] = IX_EMPTY;

        // swap the last record and the deleted record
        swapLeafEntries<T>(slotIndex, pBuffer, pBuffer->nbFilledSlots-1, pBuffer);
        pBuffer->nbFilledSlots--;
        sortLeaf(pBuffer);
    }

    // trois cas se présentent
    // 1. cette feuille est la racine et elle est devenue vide
    // on la supprime et le btree devient vide.
    // IMPOSSIBLE pour l'instant car la la racine ne peut pas être une feuille
    if(pBuffer->nbFilledSlots < IX_MAX_NUMBER_OF_VALUES/2)
    {
        // 2. la feuille compte moins de IX_MAX_VALUES / 2
    }
    else
    {
        // 3. la feuille a au moins la moitié de ses slots remplis
        // on met à jour l'index en remontant la valeur.
        if(slotIndex == 0)
        {
            copyGeneric(pBuffer->v[0], updatedParentValue);
            updateParentIndex = true;
        }
    }   

    if(rc = ReleaseBuffer(iPageNum, true))
        goto err_return;

    return rc;

    err_release:
    ReleaseBuffer(iPageNum, false);
    err_return:
    return (rc);
}

// bucket Deletion
RC IX_IndexHandle::DeleteEntryInBucket(PageNum &ioPageNum, const RID &rid)
{
    RC rc = OK_RC;

    char *pBuffer;
    RID tempRid;
    int i;
    bool foundRid = false;

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
    if(foundRid && ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots > 1)
    {
        memcpy(pBuffer + sizeof(IX_PageBucketHdr) + i * sizeof(RID),
           pBuffer + sizeof(IX_PageBucketHdr) + ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots * sizeof(RID),
           sizeof(rid));

        ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots = ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots - 1;
    }

    if(!foundRid)
    {
        rc = IX_ENTRY_DOES_NOT_EXIST;
        goto err_release;
    }

    if(((IX_PageBucketHdr *)pBuffer)->nbFilledSlots == 0)
    {
        ioPageNum = IX_EMPTY;
    }

    if(rc = ReleaseBuffer(ioPageNum, true))
        goto err_return;

    return rc;


    err_release:
    ReleaseBuffer(ioPageNum, false);
    err_return:
    return (rc);
}


// ***********************
// Force pages
// ***********************


// TO BE VERIFIED


// Force index files to disk
RC IX_IndexHandle::ForcePages()
{
    RC rc;

    // Write back the file header if any changes made to the header
    // while the file is open
    if (bHdrChanged) {
       PF_PageHandle pageHandle;
       char* pData;

       // Get the header page
       if (rc = pfFileHandle.GetFirstPage(pageHandle))
          // Test: unopened(closed) fileHandle, invalid file
          goto err_return;

       // Get a pointer where header information will be written
       if (rc = pageHandle.GetData(pData))
          // Should not happen
          goto err_unpin;

       // Write the file header (to the buffer pool)
       memcpy(pData, &fileHdr, sizeof(fileHdr));

       // Mark the header page as dirty
       if (rc = pfFileHandle.MarkDirty(IX_HEADER_PAGE_NUM))
          // Should not happen
          goto err_unpin;

       // Unpin the header page
       if (rc = pfFileHandle.UnpinPage(IX_HEADER_PAGE_NUM))
          // Should not happen
          goto err_return;

       if (rc = pfFileHandle.ForcePages(IX_HEADER_PAGE_NUM))
          // Should not happen
          goto err_return;

       // Set file header to be not changed
       bHdrChanged = FALSE;
    }

    // Call PF_FileHandle::ForcePages()
    if (rc = pfFileHandle.ForcePages(IX_HEADER_PAGE_NUM))
       goto err_return;

    // Return ok
    return (0);

    // Recover from inconsistent state due to unexpected error
 err_unpin:
    pfFileHandle.UnpinPage(IX_HEADER_PAGE_NUM);
 err_return:
    // Return error
    return (rc);
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

template <typename T>
RC IX_IndexHandle::GetNodePageBuffer(const PageNum &iPageNum, IX_PageNode<T> * & pBuffer) const
{
    RC rc;
    char * pData;

    rc = GetPageBuffer(iPageNum, pData);

    pBuffer = (IX_PageNode<T> *) pData;

    return rc;
}

template <typename T>
RC IX_IndexHandle::GetLeafPageBuffer(const PageNum &iPageNum, IX_PageLeaf<T> * & pBuffer) const
{
    RC rc;
    char * pData;

    rc = GetPageBuffer(iPageNum, pData);

    pBuffer = (IX_PageLeaf<T> *) pData;

    return rc;
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
        case STRING:
        rc = DisplayTree_t<char[MAXSTRINGLEN]>();
        break;
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

    cout << "Root in display" << fileHdr.rootNum << endl;

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
RC IX_IndexHandle::DisplayNode_t(const PageNum pageNum, const int fatherNodeId, int &currentNodeId, int &currentEdgeId)
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
        // debug
        printGeneric(((IX_PageNode<T> *)pBuffer)->v[slotIndex]);
    }
    for(slotIndex = 0;slotIndex < 4; slotIndex++)
    {
        printGeneric(((IX_PageNode<T> *)pBuffer)->v[slotIndex]);
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
            if(childPageNum != IX_EMPTY)
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
RC IX_IndexHandle::DisplayLeaf_t(const PageNum pageNum,const int fatherNodeId, int &currentNodeId, int &currentEdgeId)
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

        xmlFile << ((IX_PageLeaf<T> *)pBuffer)->v[slotIndex] << "-" << ((IX_PageLeaf<T> *)pBuffer)->bucket[slotIndex] << endl;
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





template <typename T>
void swapLeafEntries(int i, IX_PageLeaf<T> * pBuffer1, int j, IX_PageLeaf<T> *pBuffer2)
{
    T tempV;
    PageNum tempNum;

    copyGeneric(pBuffer1->v[i], tempV);
    tempNum = pBuffer1->bucket[i];

    copyGeneric(pBuffer2->v[j], pBuffer1->v[i]);
    pBuffer1->bucket[i] = pBuffer2->bucket[j];

    copyGeneric(tempV, pBuffer2->v[j]);
    pBuffer2->bucket[j] = tempNum;
}


template <typename T>
void sortLeaf(IX_PageLeaf<T> * pBuffer)
{
    sortGeneric(pBuffer->v, pBuffer->bucket, pBuffer->nbFilledSlots);
}
