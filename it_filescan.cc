#include "it_filescan.h"

#include <iostream>

using namespace std;


IT_FileScan::IT_FileScan(RM_Manager *rmm, SM_Manager *smm,
                         const char* relName, CompOp scanOp, DataAttrInfo &dAttr, void *value):
    _pRmm(rmm), _pSmm(smm),
    _relName(relName), _scanOp(scanOp), _iAttr(dAttr), _value(value)
{
    _bIsOpen = false;
}

IT_FileScan::~IT_FileScan()
{

}

RC IT_FileScan::Open()
{
    RC rc;

    if((rc = _pRmm->OpenFile(_relName,_rmfh)))
        return rc;

    if((rc = _rmfs.OpenScan(_rmfh, _iAttr.attrType, _iAttr.attrLength, _iAttr.offset, _scanOp, _value)))
        return rc;

    if((rc = _pSmm->GetRelationStructure(_relName, _dAttr, _nAttr)))
        return rc;

    _bIsOpen = true;

    return rc;
}

RC IT_FileScan::GetNext(int &nAttr, DataAttrInfo *&pAttr, char *&pData)
{
    RC rc;

    RM_Record rec;

    int recordSize;
    char *pRecData;

    if(!_bIsOpen)
    {
        return QL_ITERATOR_NOT_OPENED;
    }

    rc = _rmfs.GetNextRec(rec);
    if (rc != RM_EOF && rc != 0)
        return rc;

    if (rc == RM_EOF)
        return QL_EOF;

    if((rc = rec.GetRecordSize(recordSize)))
        return rc;

    if((rc = rec.GetData(pRecData)))
        return rc;

    pData = new char[recordSize];
    memcpy(pData, pRecData, recordSize);

    nAttr = _nAttr;
    pAttr = _dAttr;

    return rc;
}

RC IT_FileScan::GetNext(RM_Record &oRecord)
{
    RC rc;

    if(!_bIsOpen)
    {
        return QL_ITERATOR_NOT_OPENED;
    }

    rc = _rmfs.GetNextRec(oRecord);
    if (rc != RM_EOF && rc != 0)
        return rc;

    if (rc == RM_EOF)
        return QL_EOF;

    return rc;
}

RC IT_FileScan::Close()
{
    RC rc = OK_RC;

    if(!_bIsOpen)
    {
        return QL_ITERATOR_NOT_OPENED;
    }

    if((rc = _rmfs.CloseScan()))
        return rc;

    if((rc = _pRmm->CloseFile(_rmfh)))
        return rc;

    _bIsOpen = false;
    return rc;
}
