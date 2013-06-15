#ifndef IX_BTREE_H
#define IX_BTREE_H

#include "pf.h"
#include "rm_rid.h"
//#include "ix_internal.h"

class IX_BTree
{
    friend class IX_IndexHandle;
    friend class IX_Manager;
    friend class IX_IndexScan;
public:
    IX_BTree();
    ~IX_BTree();


    enum NodeType {
        ROOT,
        INODE,
        LASTINODE,
        ROOTANDLASTINODE
    };

    enum Direction {
        LEFT,
        RIGHT,
        DONTMOVE
    };

    enum DeleteStatus {
        NOTHING, // rien à faire
        UPDATE_ONLY, // simple mise à jour de l'index
        REDISTRIBUTION_LEFT, // impliquant une redistribution gauche
        REDISTRIBUTION_RIGHT, // impliquant une redistributtion droite
        MERGE_LEFT, // impliquant une fusion à gauch
        MERGE_RIGHT, // impliquant une fusion à droite
        TOO_EMPTY_NODE,
        TOO_EMPTY_NODE_WITH_UPDATE
    };

    struct IX_FileHdr {
        PageNum rootNum;
        AttrType attrType;
        int attrLength;
        int height;
        PageNum firstLeafNum;
    };


    template <typename T, int n>
    struct IX_PageNode {
        PageNum parent;
        NodeType nodeType;

        int nbFilledSlots;
        T v[n];

        PageNum child[n+1];
    };

    template <typename T, int n>
    struct IX_PageLeaf {
        PageNum parent;

        PageNum previous;
        PageNum next;

        int nbFilledSlots;
        T v[n];
        PageNum bucket[n];
    };

    void setFileHandle(PF_FileHandle & iPfFileHandle) { _pPfFileHandle = &iPfFileHandle; }

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Display
    RC DisplayTree();

private:

    // insertion
    template <typename T, int n>
    RC InsertEntry_t(T iValue, const RID &rid);

    template <typename T, int n>
    RC InsertEntryInNode_t(PageNum iPageNum, T iValue, const RID &rid, PageNum &newChildPageNum,T &medianChildValue);

    template <typename T, int n>
    RC InsertEntryInLeaf_t(PageNum iPageNum, T iValue, const RID &rid, PageNum &newChildPageNum,T &medianValue);

    RC InsertEntryInBucket(PageNum iPageNum, const RID &rid);

    // deletion
    template <typename T, int n>
    RC DeleteEntry_t(T iValue, const RID &rid);

    template <typename T, int n>
    RC DeleteEntryInNode_t(PageNum iPageNum, T iValue, const RID &rid, T &updatedParentValue, IX_BTree::DeleteStatus &parentStatus);

    template <typename T, int n>
    RC DeleteEntryInLeaf_t(PageNum iPageNum, T iValue, const RID &rid, T &updatedParentValue, IX_BTree::DeleteStatus &parentStatus);

    RC DeleteEntryInBucket(PageNum &ioPageNum, const RID &rid);

    // allocation
    template <typename T, int n>
    RC AllocateNodePage_t(const IX_BTree::NodeType, const PageNum parent, PageNum &oPageNum);

    template <typename T, int n>
    RC AllocateLeafPage_t(const PageNum parent, PageNum &oPageNum);

    RC AllocateBucketPage(const PageNum parent, PageNum &oPageNum);

    // page management
    RC GetPageBuffer(const PageNum &iPageNum, char * &pBuffer) const;

    template <typename T, int n>
    RC GetNodePageBuffer(const PageNum &iPageNum, IX_BTree::IX_PageNode<T,n> * & pBuffer) const;

    template <typename T, int n>
    RC GetLeafPageBuffer(const PageNum &iPageNum, IX_BTree::IX_PageLeaf<T,n> * & pBuffer) const;

    RC ReleaseBuffer(const PageNum &iPageNum, bool isDirty) const;

    // pour les noeuds

    template <typename T, int n>
    RC RedistributeValuesAndChildren(IX_BTree::IX_PageNode<T,n> *pBufferCurrentNode, IX_BTree::IX_PageNode<T,n> *pBufferNewNode,T &medianChildValue, T &medianParentValue,const PageNum &newNodePageNum);
    template <typename T, int n>
    RC DeleteNodeValue(IX_BTree::IX_PageNode<T,n> *pBuffer, const int & slotIndex, const int nEntries);

    // pour les feuilles
    template <typename T, int n>
    RC RedistributeValuesAndBuckets(IX_BTree::IX_PageLeaf<T,n> *pBufferCurrentLeaf, IX_BTree::IX_PageLeaf<T,n> *pBufferNewLeaf, T iValue, T &medianValue, const PageNum &bucketPageNum, const int nEntries, bool redistributionOnly);
    template <typename T, int n>
    RC MergeValuesAndBuckets(IX_BTree::IX_PageLeaf<T,n> *pBufferCurrentLeaf, IX_BTree::IX_PageLeaf<T,n> *pBufferNewLeaf, const int nEntries);
    // debugging

    template <typename T, int n>
    RC DisplayTree_t();
    template <typename T, int n>
    RC DisplayNode_t(const PageNum pageNum,const int fatherNodeId, int &currentNodeId,int &currentEdgeId);
    template <typename T, int n>
    RC DisplayLeaf_t(const PageNum page,const int fatherNodeId, int &currentNodeId,int &currentEdgeId);





    template <int n>
    void sortNodeGeneric(int *array, PageNum child[n+1], const int arrayLength);

    template <int n>
    void sortNodeGeneric(float *array, PageNum child[n+1], const int arrayLength);

    template <int n>
    void sortNodeGeneric(char array[][MAXSTRINGLEN], PageNum child[n+1], const int arrayLength);

    template <typename T, int n>
    void swapLeafEntries(int i, IX_PageLeaf<T,n> * pBuffer1, int j, IX_PageLeaf<T,n> *pBuffer2);
    template <typename T, int n>
    void sortLeaf(IX_PageLeaf<T,n> * pBuffer);

    template <typename T, int n>
    void setOffsetInNode(IX_PageNode<T,n> *pBuffer, const int offset);

    template <typename T, int n>
    void removeFirstChildrenInNode(IX_PageNode<T,n> *pBuffer, const int offset);
    template <typename T, int n>
    void removeInNode(IX_PageNode<T,n> *pBuffer, const int slotIndex, const int pointerIndex);





    PF_FileHandle *_pPfFileHandle;
    IX_FileHdr fileHdr;
};

#endif // IX_BTREE_H
