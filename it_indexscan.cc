#include "it_indexscan.h"

#include <iostream>

using namespace std;

IT_IndexScan::IT_IndexScan(RM_Manager *rmm, IX_Manager *ixm, SM_Manager *smm,
                           const char* relName, CompOp scanOp, DataAttrInfo &dAttr, void *value):
    _pRmm(rmm), _pIxm(ixm), _pSmm(smm),
    _relName(relName), _scanOp(scanOp), _iAttr(dAttr), _value(value)
{
    _bIsOpen = false;
}

IT_IndexScan::~IT_IndexScan()
{

}

RC IT_IndexScan::Open()
{
  if(_bIsOpen)
  {
    return IX_ALREADY_OPEN;
  }

//  cout << "IXSTARTS" << endl;

  RC rc = OK_RC;

  if((rc = _pRmm->OpenFile(_relName, _rmfh)))
    return rc;

  if((rc = _pIxm->OpenIndex(_relName, _iAttr.indexNo, _ixih)))
    return rc;

  if((rc = _ixis.OpenScan(_ixih, _scanOp, _value)))
    return rc;

  if((rc = _pSmm->GetRelationStructure(_relName, _dAttr, _nAttr)))
      return rc;


  _bIsOpen = true;

  return rc;
}

RC  IT_IndexScan::GetNext(int &nAttr, DataAttrInfo *&pAttr, char *&pData)
{
  RC rc;

  RM_Record rec;

  int recordSize;
  char *pRecData;

  if(!_bIsOpen)
  {
    return QL_ITERATOR_NOT_OPENED;
  }

  RID rid;

  rc = _ixis.GetNextEntry(rid);
  if (rc != IX_EOF && rc != 0)
    return rc;

  if (rc == IX_EOF)
    return QL_EOF;

  rc = _rmfh.GetRec(rid, rec);
  if (rc != 0 ) return rc;

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
RC IT_IndexScan::Close()
{
  RC rc = OK_RC;

  if(!_bIsOpen)
  {
    return QL_ITERATOR_NOT_OPENED;
  }

  if((rc = _ixis.CloseScan()))
    return rc;

  if((rc = _pIxm->CloseIndex(_ixih)))
      return rc;

  if((rc = _pRmm->CloseFile(_rmfh)))
    return rc;

  _bIsOpen = false;
  return rc;
}
