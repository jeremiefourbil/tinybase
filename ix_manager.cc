#include "ix.h"

IX_Manager::IX_Manager(PF_Manager &pfm) : pPfm(&pfm)
{

}

IX_Manager::~IX_Manager()
{

}

// Create a new Index
RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
               AttrType attrType, int attrLength)
{
    return OK_RC;
}

// Destroy and Index
RC IX_Manager::DestroyIndex(const char *fileName, int indexNo)
{
    return OK_RC;
}

// Open an Index
RC IX_Manager::OpenIndex(const char *fileName, int indexNo,
             IX_IndexHandle &indexHandle)
{
    return OK_RC;
}

// Close an Index
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle)
{
    return OK_RC;
}
