#include "it_indexscan.h"


IT_IndexScan::IT_IndexScan(RM_Manager *rmm, IX_Manager *ixm, SM_Manager *smm,
                           const char* relName, CompOp scanOp, DataAttrInfo &dAttr, void *value):
    pRmm(rmm), pIxm(ixm), pSmm(smm),
    relName(relName), scanOp(scanOp), iAttr(&dAttr), value(value)
{

}

IT_IndexScan::~IT_IndexScan()
{

}

RC IT_IndexScan::Open()
{
  if(bIsOpen)
  {
    return IX_ALREADY_OPEN;
  }

  RC rc = OK_RC;

  if((rc = pRmm->OpenFile(relName, rmfh)))
    return rc;

  if((rc = pIxm->OpenIndex(relName, iAttr->indexNo, ixih)))
    return rc;

  if((rc = ixis.OpenScan(ixih, scanOp, value)))
    return rc;

  if((rc = pSmm->GetRelationStructure(relName, dAttr, nAttr)))
      return rc;


  bIsOpen = true;

  return rc;
}

RC  IT_IndexScan::GetNext(int &nAttr, DataAttrInfo *&pAttr, char *pData)
{
  RC rc;

  RM_Record rec;

  int recordSize;
  char *pRecData;

  if(!bIsOpen)
  {
    return QL_ITERATOR_NOT_OPENED;
  }

  RID rid;

  rc = ixis.GetNextEntry(rid);
  if (rc != IX_EOF && rc != 0)
    return rc;

  if (rc == IX_EOF)
    return QL_EOF;

  rc = rmfh.GetRec(rid, rec);
  if (rc != 0 ) return rc;

  if((rc = rec.GetRecordSize(recordSize)))
      return rc;

  if((rc = rec.GetData(pRecData)))
      return rc;

  pData = new char[recordSize];
  memcpy(pData, pRecData, recordSize);

  return rc;
}
RC IT_IndexScan::Close()
{
  RC rc = OK_RC;

  if(!bIsOpen)
  {
    return QL_ITERATOR_NOT_OPENED;
  }

  if((rc = ixis.CloseScan()))
    return rc;

  if((rc = pRmm->CloseFile(rmfh)))
    return rc;

  bIsOpen = false;
  return rc;
}
