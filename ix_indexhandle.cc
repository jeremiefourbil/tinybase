#include "ix_internal.h"

IX_IndexHandle::IX_IndexHandle()
{

}

IX_IndexHandle::~IX_IndexHandle()
{

}

// Insert a new index entry
RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid)
{
    return OK_RC;
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
