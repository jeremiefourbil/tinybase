#include "it_indexscan.h"


IT_IndexScan::IT_IndexScan(
  RM_Manager *rmm
  ,IX_Manager *ixm
  ,SM_Manager *smm
  ,const char* relName
  ,CompOp &scanOp
  ,DataAttrInfo &dAttr
  ,void *value
  ,RC &rc):
    rmm(rmm)
    ,ixm(ixm)
    ,smm(smm)
    ,relName(relName)
    ,scanOp(scanOp)
    ,dAttr(dAttr)
    ,value(value){
}

IT_IndexScan::~IT_IndexScan(){

}

RC IT_IndexScan::Open(){
  if(bIsOpen) {
    return IX_ALREADY_OPEN;
  }

  RC rc = OK_RC;

  if((rc = rmm->OpenFile(relName, rmfh)))
    return rc;

  if((rc = ixm->OpenIndex(relName, dAttr.indexNo, ixih)))
    return rc;

  if((rc = ixis.OpenScan(ixih, scanOp, value)))
    return rc;

  bIsOpen = true;

  return rc;
}

RC  IT_IndexScan::GetNext(RM_Record &rec){
  RC rc;

  if(!bIsOpen) {
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

  return rc;
}
RC IT_IndexScan::Close(){
  RC rc = OK_RC;

  if(!bIsOpen) {
    return QL_ITERATOR_NOT_OPENED;
  }

  if((rc = rmfs.CloseScan()))
    return rc;

  if((rc = pRmm->CloseFile(rmfh)))
    return rc;

  bIsOpen = false;
  return rc;
}
