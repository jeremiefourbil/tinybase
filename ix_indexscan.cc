#include "ix_internal.h"

IX_IndexScan::IX_IndexScan()
{

}

IX_IndexScan::~IX_IndexScan()
{

}

// Open index scan
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
            CompOp compOp,
            void *value,
            ClientHint  pinHint)
{

    _indexHandle = indexHandle;
    _compOp = compOp;
    _value = value;
    _pinHint = pinHint;











    return OK_RC;
}

// Get the next matching entry return IX_EOF if no more matching
// entries.
RC IX_IndexScan::GetNextEntry(RID &rid)
{
    return OK_RC;
}

// Close index scan
RC IX_IndexScan::CloseScan()
{
    return OK_RC;
}
