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
        rc = InsertEntry_t<int,order_INT>(*((int*)pData), rid);
        break;
        case FLOAT:
        rc = InsertEntry_t<float,order_FLOAT>(*((float*)pData), rid);
        break;
        case STRING:
        rc = InsertEntry_t<char[MAXSTRINGLEN],order_STRING>((char *) pData, rid);
        break;
        default:
        rc = IX_BADTYPE;
    }

    return rc;
}


// templated insertion
template <typename T, int n>
RC IX_IndexHandle::InsertEntry_t(T iValue, const RID &rid)
{
    RC rc;
    PageNum pageNum;

    IX_PageNode<T,n> *pBuffer, *pNewRootBuffer, *newPageBuffer;
    IX_PageLeaf<T,n> *pLeafBuffer;
    // Pour la récursion descendante
    PageNum newChildPageNum = IX_EMPTY;
    T medianChildValue;

    PageNum newRootNum = IX_EMPTY;

    pageNum = fileHdr.rootNum;

    // no root, create one
    if(pageNum == IX_EMPTY)
    {
        if(rc = AllocateLeafPage_t<T,n>(IX_EMPTY, pageNum))
                goto err_return;

        fileHdr.rootNum = pageNum;
        fileHdr.firstLeafNum = pageNum;
    }

    // let's call the recursion method, start with root
    if (fileHdr.height == 0)
    {
        cout << "on insère dans la feuille racine" << endl;
        if(rc = InsertEntryInLeaf_t<T,n>(pageNum, iValue, rid, newChildPageNum, medianChildValue))
            goto err_return;
    }
    else
    {
        cout << "on insère depuis le noeud racine" << endl;
        if(rc = InsertEntryInNode_t<T,n>(pageNum, iValue, rid, newChildPageNum, medianChildValue))
        goto err_return;
    }
    // il y a eu un split de noeuds
    if(newChildPageNum != IX_EMPTY)
    {
        if(fileHdr.height >=1)
        {
            // la racine était un noeud
            if(rc = GetNodePageBuffer(pageNum, pBuffer))
                goto err_return;
            cout << "le noeud racine doit être splittée" << endl;
            // nouvelle racine
            if(rc = AllocateNodePage_t<T,n>(ROOT,IX_EMPTY, newRootNum))
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
            // on met à jour le parents des fils
            pBuffer->parent = newRootNum;
            newPageBuffer->parent = newRootNum;
            if(rc = ReleaseBuffer(newChildPageNum, true))
                return rc;
            if(rc = ReleaseBuffer(pageNum, true))
                return rc;
        } else {
            // la racine était une feuille
            cout << "la feuille racine doit être splittée" << endl;
            // nouvelle racine
            if(rc = AllocateNodePage_t<T,n>(ROOTANDLASTINODE,IX_EMPTY, newRootNum))
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
            // mise à jour des parents pour les enfants
            if(rc = GetLeafPageBuffer(pageNum, pLeafBuffer))
                goto err_return;
            pLeafBuffer->parent = newRootNum;
            if(rc = ReleaseBuffer(pageNum, true))
                return rc;
            if(rc = GetLeafPageBuffer(newChildPageNum, pLeafBuffer))
                goto err_return;
            pLeafBuffer->parent = newRootNum;
            if(rc = ReleaseBuffer(newChildPageNum, true))
                return rc;
            fileHdr.rootNum = newRootNum;

            cout << "Root in root" << fileHdr.rootNum << endl;
        }
        fileHdr.height++;
        bHdrChanged = true;
    }

    cout << "RootNum: " << fileHdr.rootNum << endl;


    return rc;

    err_unpin:
    pfFileHandle.UnpinPage(pageNum);
    err_return:
    return (rc);
}


// Node insertion
template <typename T, int n>
RC IX_IndexHandle::InsertEntryInNode_t(PageNum iPageNum, T iValue, const RID &rid, PageNum &newNodePageNum,T &medianParentValue)
{
    RC rc;
    IX_PageNode<T,n> *pBuffer;
    int pointerIndex;
    IX_PageNode<T,n> *newPageBuffer; // pour le possible split

    PageNum newChildPageNum = IX_EMPTY;
    T medianChildValue;

    IX_PageLeaf<T,n> *tmpLeafBuffer;
    IX_PageNode<T,n> *tmpNodeBuffer;

    // get the current node
    if(rc = GetNodePageBuffer(iPageNum, pBuffer))
        goto err_return;

    if(pBuffer->nodeType == ROOTANDLASTINODE && pBuffer->nbFilledSlots == 0)
    {
        copyGeneric(iValue, pBuffer->v[0]);
        pBuffer->nbFilledSlots++;
    }

    getPointerIndex(pBuffer->v, pBuffer->nbFilledSlots, iValue, pointerIndex);

    // the child is a leaf
    if(pBuffer->nodeType == LASTINODE || pBuffer->nodeType == ROOTANDLASTINODE )
    {
        PageNum childPageNum = pBuffer->child[pointerIndex];

        // the leaf is empty
        if(pBuffer->child[pointerIndex] == IX_EMPTY)
        {
            if(rc = AllocateLeafPage_t<T,n>(iPageNum, childPageNum))
                goto err_return;

            pBuffer->child[pointerIndex] = childPageNum;
        }

        if(rc = InsertEntryInLeaf_t<T,n>(childPageNum, iValue, rid, newChildPageNum, medianChildValue))
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

        if(rc = InsertEntryInNode_t<T,n>(childPageNum, iValue, rid, newChildPageNum, medianChildValue))
            goto err_return;
    }

    // il y a eu un split à cause de l'insertion de la valeur
    if(newChildPageNum != IX_EMPTY)
    {
        // on essaye d'insérer la valeur médiane dans le noeud courant
        cout << "split en dessous" << endl;
        if(pBuffer->nbFilledSlots<n)
        {
            cout << "le noeud courant a de la place :" << endl;
            // on insère la valeur médiane liée à son fils droit
            copyGeneric(medianChildValue,pBuffer->v[pBuffer->nbFilledSlots]);
            // on incrémente le nombre de slots occupés
            pBuffer->nbFilledSlots++;
            // on ajoute le fils droit correspondant au nouveau slot
            pBuffer->child[pBuffer->nbFilledSlots] = newChildPageNum;
            // il faut ordonner les valeurs du noeud
            sortNodeGeneric<n>(pBuffer->v,pBuffer->child, pBuffer->nbFilledSlots);
        }
        else
        {
            cout << "le noeud courant doit être splité" << endl;
            // le noeud courant est plein donc on va créer un nouveau noeud
            if(rc = AllocateNodePage_t<T,n>(pBuffer->nodeType,pBuffer->parent, newNodePageNum))
                goto err_return;
            // on récupère le buffer du nouveau noeud
            if(rc = GetNodePageBuffer(newNodePageNum, newPageBuffer))
                goto err_return;

            // redistribution entre le noeud courant et le nouveau noeud
            if(rc = RedistributeValuesAndChildren<T,n>(pBuffer, newPageBuffer, medianChildValue, medianParentValue, newChildPageNum))
                goto err_return;
            cout << "mise à jour du parent des enfants" << endl;
            // on met à jour le parent des enfants du nouveau noeuds
            for(int i=0;i<newPageBuffer->nbFilledSlots;i++)
            {
                // on vérifie que le fils existe droit
                if(newPageBuffer->child[i+1] != IX_EMPTY)
                {
                    if(newPageBuffer->nodeType == LASTINODE || newPageBuffer->nodeType == ROOTANDLASTINODE)
                    {
                        // le fils est une feuillle
                        if(rc = GetLeafPageBuffer(newPageBuffer->child[i+1], tmpLeafBuffer))
                            goto err_return;
                        tmpLeafBuffer->parent = newNodePageNum;
                        if(rc = ReleaseBuffer(newPageBuffer->child[i+1], true))
                            return rc;
                    }
                    else if (newPageBuffer->nodeType == INODE)
                    {
                        // le fils est un noeud
                        if(rc = GetNodePageBuffer(newPageBuffer->child[i+1], tmpNodeBuffer))
                            goto err_return;
                        tmpNodeBuffer->parent = newNodePageNum;
                        if(rc = ReleaseBuffer(newPageBuffer->child[i+1], true))
                            return rc;
                    }
                    else
                    {
                        // ne doit pas se produire
                    }
                } else {
                    // ne doit pas se produire
                    cout << "le fils droit est vide" << endl;
                }
            }
            cout << endl;
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
template <typename T, int n>
RC IX_IndexHandle::InsertEntryInLeaf_t(PageNum iPageNum, T iValue, const RID &rid, PageNum &newChildPageNum,T &medianValue)
{
    RC rc = OK_RC;
    IX_PageLeaf<T,n> *pBuffer, *newPageBuffer, *oldNextPageBuffer;
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
        if(pBuffer->nbFilledSlots == n)
        {
            cout << "Overflow" << endl;

            // créer une nouvelle page
            if (rc = AllocateLeafPage_t<T,n>(pBuffer->parent, newChildPageNum))
                goto err_return;

            // on alloue une page pour le bucket
            if(rc = AllocateBucketPage(newChildPageNum, bucketPageNum))
                goto err_return;


            // on charge la nouvelle feuille pour pouvoir mettre à jour les liens
            if(rc = GetLeafPageBuffer(newChildPageNum, newPageBuffer))
                goto err_return;
            // on charge l'ancienne prochaine page pour mettre à jour son lien previous
            if(pBuffer->next != IX_EMPTY)
            {
                if(rc = GetLeafPageBuffer(pBuffer->next, oldNextPageBuffer))
                    goto err_return;
                oldNextPageBuffer->previous = newChildPageNum;
                if(rc = ReleaseBuffer(pBuffer->next, true))
                    goto err_return;
            }
            newPageBuffer->next = pBuffer->next;
            pBuffer->next = newChildPageNum;
            newPageBuffer->previous = iPageNum;
            // on remplit la moitié des valeurs de l'ancienne feuille dans la nouvelle feuille
            if(rc = RedistributeValuesAndBuckets<T,n>(pBuffer, newPageBuffer, iValue, medianValue, bucketPageNum, n+1, false))
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

template <typename T, int n>
RC IX_IndexHandle::RedistributeValuesAndChildren(IX_PageNode<T,n> *pBufferCurrentNode, IX_PageNode<T,n> *pBufferNewNode,T &medianChildValue, T &medianParentValue,const PageNum &newNodePageNum)
{
    T array[n+1];
    PageNum child[n+1];

    int slotOffset = n / 2;

    // on remplit le nouveau tableau avec les 5 valeurs
    for(int i=0;i<pBufferCurrentNode->nbFilledSlots;i++)
    {
        copyGeneric(pBufferCurrentNode->v[i], array[i]);
        // offset pour ne pas prendre le fils de gauche
        child[i] = pBufferCurrentNode->child[i+1];
    }
    // on place à la fin du tableau la nouvelle valeur
    copyGeneric(medianChildValue, array[n]);
    // on remplit la page du nouveau 
    child[n] = newNodePageNum;
    // on ordonne le nouveau tableau
    sortGeneric(array, child, n+1);
    // on récupère la valeur médiane
    copyGeneric(array[slotOffset],medianParentValue);
    // on initialise le nombre de slots pour chaque feuille
    pBufferCurrentNode->nbFilledSlots = 0;
    pBufferNewNode->nbFilledSlots = 0;

    // redistribution des valeurs
    for(int j=0;j<n+1;j++)
    {
        if(j<slotOffset){
            copyGeneric(array[j], pBufferCurrentNode->v[j]);
            // on met à jour les buckets pour être cohérent
            pBufferCurrentNode->child[j+1] = child[j];
            // on incrémente le nombre slots remplis
            pBufferCurrentNode->nbFilledSlots++;
        } else {
        // on copie la valeur dans la nouvelle feuille
            copyGeneric(array[j], pBufferNewNode->v[j-slotOffset]);
        // on met à jour les buckets pour être cohérent
            pBufferNewNode->child[j-slotOffset+1] = child[j];
        // on incrémente le nombre slots remplis
            pBufferNewNode->nbFilledSlots++;
        }
    }
    return OK_RC;
}

template <typename T, int n>
RC IX_IndexHandle::RedistributeValuesAndBuckets(IX_PageLeaf<T,n> *pBufferCurrentLeaf, IX_PageLeaf<T,n> *pBufferNewLeaf, T iValue, T &medianValue,const PageNum &bucketPageNum, const int nEntries, bool redistributionOnly)
{
    T arr[nEntries];
    PageNum buck[nEntries];

    int slotOffset = nEntries/2;

    cout << "--- redistribution --" << endl;
    cout << "--- before :" << endl;

    // on remplit le nouveau tableau avec les valeurs de la feuille gauche
    int i;

    for(i=0;i<pBufferCurrentLeaf->nbFilledSlots;i++)
    {
        copyGeneric(pBufferCurrentLeaf->v[i], arr[i]);
        buck[i] = pBufferCurrentLeaf->bucket[i];
        cout << i << ":(" << arr[i] << "," << buck[i] << ") " ;
    }
    cout << "i:"<< i << " ";
    // on remplit le nouveau tableau avec les valeurs de la feuille droite
    for(int j=0;j<pBufferNewLeaf->nbFilledSlots;j++)
    {
        copyGeneric(pBufferNewLeaf->v[j], arr[j+i]);
        buck[i+j] = pBufferNewLeaf->bucket[j];
        cout << i+j << ":(" << arr[i+j] << "," << buck[i+j] <<") " ;
    }
    cout << endl;
    // mode pour la suppresion sans ajouter la valeur qui fait splitter
    if(!redistributionOnly)
    {
        // on place à la fin du tableau la nouvelle valeur
        copyGeneric(iValue, arr[nEntries-1]);
        // on remplit la page du nouveau bucket
        buck[nEntries-1] = bucketPageNum;
    }
    // on ordonne le nouveau tableau
    sortGeneric(arr, buck, nEntries);
    // on récupère la valeur médiane
    copyGeneric(arr[slotOffset],medianValue);
    // on initialise le nombre de slots pour chaque feuille
    pBufferCurrentLeaf->nbFilledSlots = 0;
    pBufferNewLeaf->nbFilledSlots = 0;
    // redistribution des valeurs
    cout << "--- after :" << endl;
    for(int j=0;j<nEntries;j++)
    {
        if(j<slotOffset){
            copyGeneric(arr[j], pBufferCurrentLeaf->v[j]);
            // on met à jour les buckets pour être cohérent
            pBufferCurrentLeaf->bucket[j] = buck[j];
            // on incrémente le nombre slots remplis
            pBufferCurrentLeaf->nbFilledSlots++;
            cout << "(" << arr[j] << "," << buck[j] <<")" ;
        } else {
        // on copie la valeur dans la nouvelle feuille
            copyGeneric(arr[j], pBufferNewLeaf->v[j-slotOffset]);
        // on met à jour les buckets pour être cohérent
            pBufferNewLeaf->bucket[j-slotOffset] = buck[j];
        // on incrémente le nombre slots remplis
            pBufferNewLeaf->nbFilledSlots++;
            cout << "(" << arr[j] << "," << buck[j] <<")" ;
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
        rc = DeleteEntry_t<int,order_INT>(* ((int *) pData), rid);
        break;
        case FLOAT:
        rc = DeleteEntry_t<float,order_FLOAT>(* ((float *) pData), rid);
        break;
        case STRING:
        rc = DeleteEntry_t<char[MAXSTRINGLEN],order_STRING>((char*) pData, rid);
        break;
        default:
        rc = IX_BADTYPE;
    }

    return rc;
}

// templated Deletion
template <typename T, int n>
RC IX_IndexHandle::DeleteEntry_t(T iValue, const RID &rid)
{
    RC rc;
    PageNum pageNum;
    IX_PageNode<T,n> * pBuffer;
    T updatedChildValue;
    DeleteStatus childStatus = NOTHING;
    int slotIndex = 0;

    pageNum = fileHdr.rootNum;

    // no root, create one
    if(pageNum == IX_EMPTY)
    {
        return(IX_ENTRY_DOES_NOT_EXIST);
    }

    if(fileHdr.height == 0)
    {
        if(rc = DeleteEntryInLeaf_t<T,n>(pageNum, iValue, rid, updatedChildValue, childStatus))
            goto err_return;
    }
    else
    {
        // let's call the recursion method, start with root
        if(rc = DeleteEntryInNode_t<T,n>(pageNum, iValue, rid, updatedChildValue, childStatus))
            goto err_return;
        if(childStatus == UPDATE_ONLY)
        {
            // get the current node
            cout << "delete : need update" << endl;
            if(rc = GetNodePageBuffer(pageNum, pBuffer))
                goto err_return;
            slotIndex = 0;
            while(slotIndex < pBuffer->nbFilledSlots && comparisonGeneric(iValue, pBuffer->v[slotIndex]) >= 0)
            {
                slotIndex++;
            }
            copyGeneric(updatedChildValue, pBuffer->v[slotIndex]);
            if(rc = ReleaseBuffer(pageNum, true))
                goto err_return;
        }
    }
    return rc;

    err_return:
    return (rc);
}

// Node Deletion
template <typename T, int n>
RC IX_IndexHandle::DeleteEntryInNode_t(PageNum iPageNum, T iValue, const RID &rid, T &updatedParentValue, DeleteStatus &parentStatus)
{

    RC rc;
    IX_PageNode<T,n> *pBuffer, *pChildBuffer, *pSecondChildBuffer;
    IX_PageLeaf<T,n> *pRedistBuffer, *pRedistBufferBis;
    int slotIndex;
    int pointerIndex;
    DeleteStatus childStatus = NOTHING;
    T updatedChildValue;
    bool problemSolved;
    int index;

    // get the current node
    if(rc = GetNodePageBuffer(iPageNum, pBuffer))
        goto err_return;

    // find the right slot
    getSlotIndex(pBuffer->v, pBuffer->nbFilledSlots, iValue, pointerIndex);

    // find the right branch
    getPointerIndex(pBuffer->v, pBuffer->nbFilledSlots, iValue, pointerIndex);

    // the child is a leaf
    if(pBuffer->nodeType == LASTINODE || pBuffer->nodeType == ROOTANDLASTINODE )
    {
        PageNum childPageNum = pBuffer->child[pointerIndex];

            // the leaf is empty
        if(pBuffer->child[pointerIndex] == IX_EMPTY)
        {
            rc = IX_ENTRY_DOES_NOT_EXIST;
            goto err_release;
        }

        if(rc = DeleteEntryInLeaf_t<T,n>(childPageNum, iValue, rid, updatedChildValue, childStatus))
            goto err_return;
        // mettre à jour l'index car la valeur minimale de la feuille a changé
        if(childStatus == UPDATE_ONLY)
        {
            cout << "update from leaf :" << slotIndex << "- pointer:" << pointerIndex << endl;
            if(slotIndex == pointerIndex)
            {
                if(slotIndex != 0)
                {
                    copyGeneric(updatedChildValue, pBuffer->v[slotIndex-1]);
                    if(slotIndex == 0)
                    {
                        copyGeneric(pBuffer->v[slotIndex], updatedParentValue);
                        parentStatus = UPDATE_ONLY;
                    }
                }
            }
            else
            {
                copyGeneric(updatedChildValue, pBuffer->v[slotIndex]);
                if(slotIndex == 0 && (pointerIndex == 1) ){
                    copyGeneric(pBuffer->v[slotIndex], updatedParentValue);
                    parentStatus = UPDATE_ONLY;
                }
            }
        }
        else if(childStatus == REDISTRIBUTION_RIGHT)
        {
            if(slotIndex == pointerIndex)
            {
                if(slotIndex == 0)
                {
                    // on teste unique la valeur du fils de droite
                    if(rc = GetLeafPageBuffer(pBuffer->child[pointerIndex + 1], pRedistBuffer))
                        goto err_return;

                    if(pRedistBuffer->v[0] != pBuffer->v[slotIndex])
                    {
                        copyGeneric(pRedistBuffer->v[0], pBuffer->v[slotIndex]);
                        if(rc = ReleaseBuffer(pBuffer->child[pointerIndex + 1], true))
                        goto err_return;
                        // on previent le noeud parent
                        parentStatus = UPDATE_ONLY;
                        copyGeneric(pBuffer->v[slotIndex], updatedParentValue);
                    }
                    else
                    {
                        if(rc = ReleaseBuffer(pBuffer->child[pointerIndex + 1], false))
                            goto err_return;
                    }
                }
                else
                {
                    // on teste les slots à gauche du pointerIndex
                    // premier fils
                    if(rc = GetLeafPageBuffer(pBuffer->child[pointerIndex], pRedistBuffer))
                        goto err_return;

                    if(pRedistBuffer->v[0] != pBuffer->v[slotIndex])
                    {
                        copyGeneric(pRedistBuffer->v[0], pBuffer->v[slotIndex - 1]);
                        if(rc = ReleaseBuffer(pBuffer->child[pointerIndex], true))
                            goto err_return;
                    }
                    else
                    {
                        if(rc = ReleaseBuffer(pBuffer->child[pointerIndex], false))
                            goto err_return;
                    }

                    // deuxième fils
                    if(rc = GetLeafPageBuffer(pBuffer->child[pointerIndex+1], pRedistBuffer))
                        goto err_return;

                    if(pRedistBuffer->v[0] != pBuffer->v[slotIndex+1])
                    {
                        copyGeneric(pRedistBuffer->v[0], pBuffer->v[slotIndex]);
                        if(rc = ReleaseBuffer(pBuffer->child[pointerIndex+1], true))
                            goto err_return;
                    }
                    else
                    {
                        if(rc = ReleaseBuffer(pBuffer->child[pointerIndex+1], false))
                            goto err_return;
                    }
                }
            }
            else
            {
                // on traite que les fils de droite car slotIndex < pointerIndex
                // premier fils
                if(rc = GetLeafPageBuffer(pBuffer->child[pointerIndex], pRedistBuffer))
                    goto err_return;

                if(pRedistBuffer->v[0] != pBuffer->v[slotIndex])
                {
                    copyGeneric(pRedistBuffer->v[0], pBuffer->v[slotIndex]);
                    if(rc = ReleaseBuffer(pBuffer->child[pointerIndex], true))
                        goto err_return;
                    if(slotIndex == 0)
                    {
                        parentStatus = UPDATE_ONLY;
                        copyGeneric(pBuffer->v[slotIndex], updatedParentValue);
                    }
                }
                else
                {
                    if(rc = ReleaseBuffer(pBuffer->child[pointerIndex], false))
                        goto err_return;
                }
                // deuxième fils
                if(rc = GetLeafPageBuffer(pBuffer->child[pointerIndex+1], pRedistBuffer))
                    goto err_return;
                if(pRedistBuffer->v[0] != pBuffer->v[slotIndex + 1])
                {
                    copyGeneric(pRedistBuffer->v[0], pBuffer->v[slotIndex + 1]);
                    if(rc = ReleaseBuffer(pBuffer->child[pointerIndex+1], true))
                        goto err_return;
                }
                else
                {
                    if(rc = ReleaseBuffer(pBuffer->child[pointerIndex+1], false))
                        goto err_return;
                }
            }

        }
        else if (childStatus == REDISTRIBUTION_LEFT)
        {
            // on va verifier le pointeur courant ainsi que le fils à gauche pour mettre à jour les valeurs de l'index
            // on traite que les fils de droite car slotIndex < pointerIndex
            // premier fils
            if(rc = GetLeafPageBuffer(pBuffer->child[pointerIndex], pRedistBuffer))
                goto err_return;
            if(pRedistBuffer->v[0] != pBuffer->v[slotIndex])
            {
                copyGeneric(pRedistBuffer->v[0], pBuffer->v[slotIndex]);
                if(rc = ReleaseBuffer(pBuffer->child[pointerIndex], true))
                    goto err_return;
            }
            else
            {
                if(rc = ReleaseBuffer(pBuffer->child[pointerIndex], false))
                    goto err_return;
            }
        }
        else if (childStatus == MERGE_LEFT)
        {
            // mettre à jour les liens vers le suivant et le précédent avant de supprimer la page
            if(rc = GetLeafPageBuffer(pBuffer->child[pointerIndex], pRedistBuffer))
                goto err_return;


            // le suivant du précédent c'est le suivant de l'actuel
            if(rc = GetLeafPageBuffer(pRedistBuffer->previous, pRedistBufferBis))
                goto err_return;
            pRedistBufferBis->next = pRedistBuffer->next;
            if(rc = ReleaseBuffer(pRedistBuffer->previous, true))
                goto err_return;


            // le précédent du suivant s'il existe c'est le précédent de l'actuel
            if(pRedistBuffer->next != IX_EMPTY)
            {
                if(rc = GetLeafPageBuffer(pRedistBuffer->next, pRedistBufferBis))
                    goto err_return;
                pRedistBufferBis->previous = pRedistBuffer->previous;
                if(rc = ReleaseBuffer(pRedistBuffer->next, true))
                    goto err_return;
            }

            if(rc = ReleaseBuffer(pBuffer->child[pointerIndex], false))
                goto err_return;
            // supprimer la page fils
            cout << "supprimer la page fils" << endl;
            if(rc = pfFileHandle.DisposePage(pBuffer->child[pointerIndex]))
                goto err_return;



            DeleteNodeValue<T,n>(pBuffer, pointerIndex-1, pBuffer->nbFilledSlots);
            if(slotIndex == 0)
            {
                parentStatus = UPDATE_ONLY;
                copyGeneric(pBuffer->v[slotIndex], updatedParentValue);
            }

            // val: le noeud n'est pas assez rempli
            if(pBuffer->nbFilledSlots < n/2)
            {
                parentStatus = TOO_EMPTY_NODE;
            }

        }
        else if (childStatus == MERGE_RIGHT)
        {




            // val: le noeud n'est pas assez rempli
            if(pBuffer->nbFilledSlots < n/2)
            {
                parentStatus = TOO_EMPTY_NODE;
            }

        } else {
            // impossible comme cas
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

        if(rc = DeleteEntryInNode_t<T,n>(childPageNum, iValue, rid, updatedChildValue, childStatus))
            goto err_return;
        // if(updateChildIndex)
        if(childStatus == UPDATE_ONLY)
        {
            cout << "update from node :" << slotIndex << "- pointer:" << pointerIndex << endl;
            if(slotIndex == pointerIndex)
            {
                if(slotIndex != 0)
                {
                    copyGeneric(updatedChildValue, pBuffer->v[slotIndex-1]);
                    if(slotIndex == 0)
                    {
                        copyGeneric(pBuffer->v[slotIndex], updatedParentValue);
                        parentStatus = UPDATE_ONLY;
                    }
                }
            }
            else
            {
                copyGeneric(updatedChildValue, pBuffer->v[slotIndex]);
                if(slotIndex == 0 && (pointerIndex == 1) ){
                    copyGeneric(pBuffer->v[slotIndex], updatedParentValue);
                    parentStatus = UPDATE_ONLY;
                }
            }
        }
        // val: le noeud fils n'est pas assez rempli
        // le problème se règle ici, au niveau de son noeud père
        else if(childStatus == TOO_EMPTY_NODE)
        {
            problemSolved = false;

            if(rc = GetNodePageBuffer(pBuffer->child[pointerIndex], pChildBuffer))
                goto err_return;

            // 1er cas: tentative de redistribution à gauche
            if(pointerIndex > 0)
            {
                if(rc = GetNodePageBuffer(pBuffer->child[pointerIndex-1], pSecondChildBuffer))
                    goto err_return;

                if(pSecondChildBuffer->nbFilledSlots > n/2 + 1)
                {
                    // start the redistribution at the following index
                    int index = (pChildBuffer->nbFilledSlots + pSecondChildBuffer->nbFilledSlots)/2;

                    // utiliser fonction de décallage

                    // TO BE CONTINUED

                    problemSolved = true;
                }

                if(rc = ReleaseBuffer(pBuffer->child[pointerIndex-1], false))
                    goto err_return;
            }

            // 2e cas: tentative de redistribution à droite
            if(!problemSolved && pointerIndex < pBuffer->nbFilledSlots)
            {
                if(rc = GetNodePageBuffer(pBuffer->child[pointerIndex+1], pSecondChildBuffer))
                    goto err_return;

                if(pSecondChildBuffer->nbFilledSlots > n/2 + 1)
                {
                    // Write the solution

                    // End

                    problemSolved = true;
                }

                if(rc = ReleaseBuffer(pBuffer->child[pointerIndex+1], false))
                    goto err_return;
            }

            // 3e cas: pas de redistribution possible, on tente une fusion à gauche
            if(!problemSolved && pointerIndex > 0)
            {
                if(rc = GetNodePageBuffer(pBuffer->child[pointerIndex-1], pSecondChildBuffer))
                    goto err_return;

                // Write the solution

                // End

                problemSolved = true;

                if(rc = ReleaseBuffer(pBuffer->child[pointerIndex-1], false))
                    goto err_return;
            }

            // 4e cas: fusion à droite
            if(!problemSolved && pointerIndex < pBuffer->nbFilledSlots)
            {
                if(rc = GetNodePageBuffer(pBuffer->child[pointerIndex+1], pSecondChildBuffer))
                    goto err_return;

                // Write the solution

                // End

                problemSolved = true;

                if(rc = ReleaseBuffer(pBuffer->child[pointerIndex+1], false))
                    goto err_return;
            }


            if(rc = ReleaseBuffer(pBuffer->child[pointerIndex], false))
                goto err_return;
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
template <typename T, int n>
RC IX_IndexHandle::DeleteEntryInLeaf_t(PageNum iPageNum, T iValue, const RID &rid, T &updatedParentValue, DeleteStatus &parentStatus)
{
    RC rc = OK_RC;

    IX_PageLeaf<T,n> *pBuffer;
    IX_PageLeaf<T,n> *pNeighborBuffer;
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
        pBuffer->bucket[slotIndex] = IX_EMPTY;

        // swap the last record and the deleted record
        swapLeafEntries<T,n>(slotIndex, pBuffer, pBuffer->nbFilledSlots-1, pBuffer);
        pBuffer->nbFilledSlots--;
        sortLeaf(pBuffer);
    }

    // trois cas se présentent
    // 1. cette feuille est la racine et elle est devenue vide
    // on la supprime et le btree devient vide.
    // IMPOSSIBLE pour l'instant car la la racine ne peut pas être une feuille
    if(pBuffer->nbFilledSlots < n/2)
    {
        // 2. la feuille compte moins de IX_MAX_VALUES / 2
        // on cherche une feuille voisine appartenant au même parent
        if(pBuffer->next != IX_EMPTY)
        {
            if(rc = GetLeafPageBuffer(pBuffer->next, pNeighborBuffer))
                goto err_return;

            if(pNeighborBuffer->parent == pBuffer->parent)
            {
                // la feuille voisine droite a le même parent
                if(pNeighborBuffer->nbFilledSlots > n/2)
                {
                    // on redistribue avec le voisin droit
                    cout << "redistribution avec le voisin de droite" << endl;
                    if(rc = RedistributeValuesAndBuckets<T,n>(pBuffer, pNeighborBuffer, iValue, updatedParentValue, bucketPageNum, pNeighborBuffer->nbFilledSlots+pBuffer->nbFilledSlots, true))
                        return rc;
                    parentStatus = REDISTRIBUTION_RIGHT;
                    if(rc = ReleaseBuffer(pBuffer->next, true))
                        goto err_return;
                }
                else
                {
                    if(rc = ReleaseBuffer(pBuffer->next, false))
                        goto err_return;
                }   
            } else {
                // on relâche le voisin droit pour charger l'autre
                if(rc = ReleaseBuffer(pBuffer->next, false))
                    goto err_return;
            }
        }
        if(parentStatus == NOTHING && pBuffer->previous != IX_EMPTY)
        {
            if(rc = GetLeafPageBuffer(pBuffer->previous, pNeighborBuffer))
                goto err_return;
            if(pNeighborBuffer->parent == pBuffer->parent)
            {
                // la feuille voisine gauche a le même parent
                if(pNeighborBuffer->nbFilledSlots > n/2)
                {
                    // on redistribue avec le voisin gauche
                    cout << "redistribution avec le voisin de gauche" << endl;
                    if(rc = RedistributeValuesAndBuckets<T,n>(pNeighborBuffer,pBuffer, iValue, updatedParentValue, bucketPageNum, pNeighborBuffer->nbFilledSlots + pBuffer->nbFilledSlots, true))
                        goto err_return;
                    parentStatus = REDISTRIBUTION_LEFT;
                    if(rc = ReleaseBuffer(pBuffer->previous, true))
                        goto err_return;
                }
                else
                {
                    if(rc = ReleaseBuffer(pBuffer->previous, false))
                        goto err_return;
                }   
            } else {
                // il faut remonter pour faire faire proprement la suppression
                // on relâche le voisin droit pour charger l'autre
                if(rc = ReleaseBuffer(pBuffer->previous, false))
                    goto err_return;
            }
        }
        if(parentStatus == NOTHING && pBuffer->next != IX_EMPTY)
        {
            if(rc = GetLeafPageBuffer(pBuffer->next, pNeighborBuffer))
                goto err_return;
            if(pNeighborBuffer->parent == pBuffer->parent)
            {
                // la feuille voisine droite a le même parent
                if(pNeighborBuffer->nbFilledSlots <= n/2)
                {
                    cout << "merge avec le voisin de droite" << endl;

                    if(rc = ReleaseBuffer(pBuffer->next, false))
                        goto err_return;
                }
                else
                {
                    if(rc = ReleaseBuffer(pBuffer->next, false))
                        goto err_return;
                }   
            } else {
                // on relâche le voisin droit pour charger l'autre
                if(rc = ReleaseBuffer(pBuffer->next, false))
                    goto err_return;
            }
        }
        if(parentStatus == NOTHING && pBuffer->previous != IX_EMPTY)
        {
            if(rc = GetLeafPageBuffer(pBuffer->previous, pNeighborBuffer))
                goto err_return;
            if(pNeighborBuffer->parent == pBuffer->parent)
            {
                // la feuille voisine gauche a le même parent

                if(pNeighborBuffer->nbFilledSlots <= n/2)
                {
                    // merge à gauche
                    cout << "merge avec le voisin de gauche" << endl;
                    if(rc = MergeValuesAndBuckets<T,n>(pBuffer, pNeighborBuffer, pBuffer->nbFilledSlots + pNeighborBuffer->nbFilledSlots))
                        goto err_return;
                    if(rc = ReleaseBuffer(pBuffer->previous, true))
                        goto err_return;
                    cout << "toto" << endl;
                    parentStatus = MERGE_LEFT;
                }
                else
                {
                    if(rc = ReleaseBuffer(pBuffer->previous, false))
                        goto err_return;
                }   
            } else {
                // il faut remonter pour faire faire proprement la suppression
                // on relâche le voisin droit pour charger l'autre
                if(rc = ReleaseBuffer(pBuffer->previous, false))
                    goto err_return;
            }
        }
    }
    else
    {
        // 3. la feuille a au moins la moitié de ses slots remplis
        // on met à jour l'index en remontant la valeur.
        if(slotIndex == 0)
        {
            copyGeneric(pBuffer->v[0], updatedParentValue);
            // updateParentIndex = true;
            parentStatus = UPDATE_ONLY;
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

template <typename T, int n>
RC IX_IndexHandle::MergeValuesAndBuckets(IX_PageLeaf<T,n> *pBufferCurrentLeaf, IX_PageLeaf<T,n> *pBufferNewLeaf, const int nEntries)
{
    T arr[nEntries];
    PageNum buck[nEntries];

    cout << "--- merge --" << endl;
    cout << "--- before :" << endl;

    // on remplit le nouveau tableau avec les valeurs de la feuille gauche
    int i;

    for(i=0;i<pBufferCurrentLeaf->nbFilledSlots;i++)
    {
        copyGeneric(pBufferCurrentLeaf->v[i], arr[i]);
        buck[i] = pBufferCurrentLeaf->bucket[i];
        cout << i << ":(" << arr[i] << "," << buck[i] << ") " ;
    }
    cout << "i:"<< i << " ";
    // on remplit le nouveau tableau avec les valeurs de la feuille droite
    for(int j=0;j<pBufferNewLeaf->nbFilledSlots;j++)
    {
        copyGeneric(pBufferNewLeaf->v[j], arr[j+i]);
        buck[i+j] = pBufferNewLeaf->bucket[j];
        cout << i+j << ":(" << arr[i+j] << "," << buck[i+j] <<") " ;
    }
    cout << endl;
    sortGeneric(arr, buck, nEntries);

    pBufferNewLeaf->nbFilledSlots = 0;
    // on remplit la cible
    cout << "--- after :" << endl;
    for(int j=0;j<nEntries;j++)
    {
        copyGeneric(arr[j], pBufferNewLeaf->v[j]);
        // on met à jour les buckets pour être cohérent
        pBufferNewLeaf->bucket[j] = buck[j];
        // on incrémente le nombre slots remplis
        pBufferNewLeaf->nbFilledSlots++;
        cout << "(" << arr[j] << "," << buck[j] <<")" ;
    }
    cout << endl;
    return OK_RC;
}

// Delete value of a node
template <typename T, int n>
RC IX_IndexHandle::DeleteNodeValue(IX_PageNode<T,n> *pBuffer, const int & slotIndex, const int nEntries)
{
    T arr[nEntries-1];
    PageNum child[nEntries];
    int j=0;
    cout << "---- Delete node value ----" << endl;
    cout << "---- before ----" << endl;
    for(int i=0;i<pBuffer->nbFilledSlots;i++)
    {
        if(slotIndex != i)
        {
            copyGeneric(pBuffer->v[i], arr[j]);
            child[j] = pBuffer->child[i+1];
            cout << j << ":(" << arr[j] << "," << child[j] << ") " ;
            j++;
        }
    }
    sortGeneric(arr, child, nEntries-1);
    pBuffer->nbFilledSlots = nEntries-1;
    for(int i=0;i<nEntries-1;i++)
    {
        copyGeneric(arr[i],pBuffer->v[i]);
        pBuffer->child[i+1] = child[i];
    }
    return OK_RC;
}

// bucket Deletion
RC IX_IndexHandle::DeleteEntryInBucket(PageNum &ioPageNum, const RID &rid)
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
    if(foundRid && ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots > 1)
    {
        memcpy(pBuffer + sizeof(IX_PageBucketHdr) + i * sizeof(RID),
           pBuffer + sizeof(IX_PageBucketHdr) + (((IX_PageBucketHdr *)pBuffer)->nbFilledSlots -1 )* sizeof(RID),
           sizeof(rid));

        ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots = ((IX_PageBucketHdr *)pBuffer)->nbFilledSlots - 1;
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
        if(rc = pfFileHandle.DisposePage(ioPageNum))
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
// Force pages
// ***********************


// TO BE VERIFIED


// Force index files to disk
RC IX_IndexHandle::ForcePages()
{
    return pfFileHandle.ForcePages();
}

// ***********************
// Page allocations
// ***********************

// node page
template <typename T, int n>
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

    ((IX_PageNode<T,n> *)pReadData)->parent = parent;
    ((IX_PageNode<T,n> *)pReadData)->nodeType = nodeType;
    ((IX_PageNode<T,n> *)pReadData)->nbFilledSlots = 0;

    for(int i=0; i<n+1; i++)
    {
        ((IX_PageNode<T,n> *)pReadData)->child[i] = IX_EMPTY;
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
template <typename T, int n>
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

    ((IX_PageLeaf<T,n> *)pReadData)->parent = parent;
    ((IX_PageLeaf<T,n> *)pReadData)->previous = IX_EMPTY;
    ((IX_PageLeaf<T,n> *)pReadData)->next = IX_EMPTY;
    ((IX_PageLeaf<T,n> *)pReadData)->nbFilledSlots = 0;

    for(int i=0; i<n; i++)
    {
        ((IX_PageLeaf<T,n> *)pReadData)->bucket[i] = IX_EMPTY;
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

template <typename T, int n>
RC IX_IndexHandle::GetNodePageBuffer(const PageNum &iPageNum, IX_PageNode<T,n> * & pBuffer) const
{
    RC rc;
    char * pData;

    rc = GetPageBuffer(iPageNum, pData);

    pBuffer = (IX_PageNode<T,n> *) pData;

    return rc;
}

template <typename T, int n>
RC IX_IndexHandle::GetLeafPageBuffer(const PageNum &iPageNum, IX_PageLeaf<T,n> * & pBuffer) const
{
    RC rc;
    char * pData;

    rc = GetPageBuffer(iPageNum, pData);

    pBuffer = (IX_PageLeaf<T,n> *) pData;

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
        rc = DisplayTree_t<int,order_INT>();
        break;
        case FLOAT:
        rc = DisplayTree_t<float,order_FLOAT>();
        break;
        case STRING:
        rc = DisplayTree_t<char[MAXSTRINGLEN],order_STRING>();
        break;
    }

    return rc;
}

// display tree
template <typename T, int n>
RC IX_IndexHandle::DisplayTree_t()
{
    RC rc;
    char *pBuffer;
    int currentNodeId = 0;
    int fatherNodeId = 0;
    int currentEdgeId = 0;
    ofstream xmlFile;

    xmlFile.open(XML_FILE);
    xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    xmlFile << "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"  xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns http://www.yworks.com/xml/schema/graphml/1.1/ygraphml.xsd\" xmlns:y=\"http://www.yworks.com/xml/graphml\">" << endl;
    xmlFile << "<key id=\"d0\" for=\"node\" yfiles.type=\"nodegraphics\"/>" << endl;
    xmlFile << "<graph>" << endl;
    xmlFile << "<node id=\"" << currentNodeId << "\"/>" << endl;
    xmlFile.close();

    currentNodeId++;

    cout << "Root in display" << fileHdr.rootNum << "- height: "<< fileHdr.height << endl;

    if (fileHdr.height == 0)
    {
        DisplayLeaf_t<T,n>(fileHdr.rootNum, fatherNodeId, currentNodeId, currentEdgeId);
    } 
    else
    {
        DisplayNode_t<T,n>(fileHdr.rootNum, fatherNodeId, currentNodeId, currentEdgeId);
    }
    xmlFile.open(XML_FILE, ios::app);
    xmlFile << "</graph>" << endl;
    xmlFile << "</graphml>" << endl;
    xmlFile.close();

    return OK_RC;

    err_return:
    return (rc);
}

// display node
template <typename T, int n>
RC IX_IndexHandle::DisplayNode_t(const PageNum pageNum, const int fatherNodeId, int &currentNodeId, int &currentEdgeId)
{
    RC rc;
    char *pBuffer;
    int slotIndex;
    int slotChildIndex;
    int newFatherNodeId = currentNodeId;
    ofstream xmlFile;

    // get the current node
    if(rc = GetPageBuffer(pageNum, pBuffer))
        goto err_return;

    // XML section
    xmlFile.open(XML_FILE, ios::app);

    xmlFile << "<node id=\"" << currentNodeId << "\">" << endl;

    xmlFile << "<data key=\"d0\"><y:ShapeNode><y:BorderStyle type=\"line\" width=\"1.0\" color=\"#000000\"/>" << endl;
    xmlFile << "<y:Geometry height=\"80.0\" width=\"150\" />" << endl;
    xmlFile << "<y:NodeLabel>" << endl;
    xmlFile << "parent:" << ((IX_PageNode<T,n> *)pBuffer)->parent << " - " << "page:" << pageNum <<endl;
    for(slotIndex = 0;slotIndex < ((IX_PageNode<T,n> *)pBuffer)->nbFilledSlots; slotIndex++)
    {
        xmlFile << ((IX_PageNode<T,n> *)pBuffer)->v[slotIndex] << endl;
        // debug
        printGeneric(((IX_PageNode<T,n> *)pBuffer)->v[slotIndex]);
    }
    for(slotIndex = 0;slotIndex < n; slotIndex++)
    {
        printGeneric(((IX_PageNode<T,n> *)pBuffer)->v[slotIndex]);
    }
    xmlFile << "</y:NodeLabel><y:Shape type=\"rectangle\"/></y:ShapeNode></data></node>" << endl;
    xmlFile << "<edge id=\"" << currentEdgeId <<"\"" << " source=\"" << fatherNodeId << "\" target=\"" << currentNodeId << "\"/>" << endl;

    xmlFile.close();

    for(slotChildIndex = 0; slotChildIndex <= ((IX_PageNode<T,n> *)pBuffer)->nbFilledSlots; slotChildIndex++)
    {
        // the child is a leaf
        if(((IX_PageNode<T,n> *)pBuffer)->nodeType == LASTINODE || ((IX_PageNode<T,n> *)pBuffer)->nodeType == ROOTANDLASTINODE)
        {
            PageNum childPageNum = ((IX_PageNode<T,n> *)pBuffer)->child[slotChildIndex];
        // the leaf is empty
            if(childPageNum != IX_EMPTY)
            {
                currentNodeId++;
                currentEdgeId++;
                if(rc = DisplayLeaf_t<T,n>(childPageNum, newFatherNodeId, currentNodeId, currentEdgeId))
                    goto err_return;
            }
        }
    // the child is a node
        else
        {
            PageNum childPageNum = ((IX_PageNode<T,n> *)pBuffer)->child[slotChildIndex];
        // the node is empty
            if(childPageNum != IX_EMPTY)
            {
                currentNodeId++;
                currentEdgeId++;
                if(rc = DisplayNode_t<T,n>(childPageNum, newFatherNodeId, currentNodeId, currentEdgeId))
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
template <typename T, int n>
RC IX_IndexHandle::DisplayLeaf_t(const PageNum pageNum,const int fatherNodeId, int &currentNodeId, int &currentEdgeId)
{
    RC rc;
    char *pBuffer, *pBucketBuffer;
    int slotIndex;
    ofstream xmlFile;
    RID rid;

    if(rc = GetPageBuffer(pageNum, pBuffer))
        goto err_return;

    // XML section
    xmlFile.open(XML_FILE, ios::app);

    xmlFile << "<node id=\"" << currentNodeId << "\">" << endl;

    xmlFile << "<data key=\"d0\"><y:ShapeNode><y:BorderStyle type=\"line\" width=\"1.0\" color=\"#008000\"/>" << endl;
    xmlFile << "<y:Geometry height=\"100.0\" width=\"150\" />" << endl;
    xmlFile << "<y:NodeLabel>" << endl;
    xmlFile << "parent : " << ((IX_PageLeaf<T,n> *)pBuffer)->parent << endl;
    xmlFile << "(==" << ((IX_PageLeaf<T,n> *)pBuffer)->previous << " ; " << pageNum <<" ; " << ((IX_PageLeaf<T,n> *)pBuffer)->next << "==)" << endl;
    for(slotIndex = 0;slotIndex < ((IX_PageLeaf<T,n> *)pBuffer)->nbFilledSlots; slotIndex++)
    {
        int rid_pnum;
        int rid_snum;

        if(rc = GetPageBuffer(((IX_PageLeaf<T,n> *)pBuffer)->bucket[slotIndex], pBucketBuffer))
            goto err_return;

        memcpy((void*) &rid,
               pBucketBuffer + sizeof(IX_PageBucketHdr), sizeof(RID));

        rid.GetPageNum(rid_pnum);
        rid.GetSlotNum(rid_snum);


        xmlFile << ((IX_PageLeaf<T,n> *)pBuffer)->v[slotIndex] << "-" << ((IX_PageBucketHdr *)pBucketBuffer)->nbFilledSlots << "(" << rid_pnum << ";" << rid_snum << ")" << endl;

        if(rc = ReleaseBuffer(((IX_PageLeaf<T,n> *)pBuffer)->bucket[slotIndex], false))
            goto err_return;



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



template <int n>
void sortNodeGeneric(int *array, PageNum child[n+1], const int arrayLength)
{
  // on crée un tableau temporaire avec uniquement les fils droits
  PageNum temporaryChild[n];
  // on le remplit
  for(int k= 1; k< n+1; ++k)
  {
    temporaryChild[k-1] = child[k];
  }
  for (int i = 1; i < arrayLength; i++)
  {
    int tmp = array[i];
    PageNum tmpChild = temporaryChild[i];
    int j = i;
    for (; j && tmp < array[j - 1]; --j)
    {
        array[j] = array[j - 1];
        temporaryChild[j] = temporaryChild[j - 1];
    }
    array[j] = tmp;
    temporaryChild[j] = tmpChild;
  }
  // on remplit les valeurs triées que l'on réinjecte dans child[]
  for(int k=0; k<n;++k)
  {
    child[k+1] = temporaryChild[k];
  }
}

template <int n>
void sortNodeGeneric(float *array, PageNum child[n+1], const int arrayLength)
{
  // on crée un tableau temporaire avec uniquement les fils droits
  PageNum temporaryChild[n];
  // on le remplit
  for(int k= 1; k< n+1; ++k)
  {
    temporaryChild[k-1] = child[k];
  }
  for (int i = 1; i < arrayLength; i++)
  {
    float tmp = array[i];
    PageNum tmpChild = temporaryChild[i];
    int j = i;
    for (; j && tmp < array[j - 1]; --j)
    {
        array[j] = array[j - 1];
        temporaryChild[j] = temporaryChild[j - 1];
    }
    array[j] = tmp;
    temporaryChild[j] = tmpChild;
  }
  // on remplit les valeurs triées que l'on réinjecte dans child[]
  for(int k=0; k<n;++k)
  {
    child[k+1] = temporaryChild[k];
  }
}

template <int n>
void sortNodeGeneric(char array[][MAXSTRINGLEN], PageNum child[n+1], const int arrayLength)
{
  // on crée un tableau temporaire avec uniquement les fils droits
  PageNum temporaryChild[n];
  // on le remplit
  for(int k= 1; k< n+1; ++k)
  {
    temporaryChild[k-1] = child[k];
  }
  for (int i = 1; i < arrayLength; i++)
  {
    char tmp[MAXSTRINGLEN];
    PageNum tmpChild = temporaryChild[i];
    strcpy(tmp, array[i]);
    int j = i;
    for (; j && strcmp(tmp,array[j - 1]) < 0; --j)
    {
        strcpy(array[j], array[j - 1]);
        temporaryChild[j] = temporaryChild[j - 1];
    }
    strcpy(array[j],tmp);
    temporaryChild[j] = tmpChild;
  }
  // on remplit les valeurs triées que l'on réinjecte dans child[]
  for(int k=0; k<n;++k)
  {
    child[k+1] = temporaryChild[k];
  }
}

template <typename T, int n>
void swapLeafEntries(int i, IX_PageLeaf<T,n> * pBuffer1, int j, IX_PageLeaf<T,n> *pBuffer2)
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


template <typename T, int n>
void sortLeaf(IX_PageLeaf<T,n> * pBuffer)
{
    sortGeneric(pBuffer->v, pBuffer->bucket, pBuffer->nbFilledSlots);
}
