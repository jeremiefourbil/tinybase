#include "it_filescan.h"


IT_FileScan::IT_FileScan(
  RM_Manager &rmm
  ,SM_Manager &smm
  ,const char* relName
  ,ComOp &scanOp
  ,DataAttrInfo &dAttr
  ,void *value
  ,Rc &rc):
    rmm(pRmm)
    ,smm(pSmm)
    ,relName(relName)
    ,scanOp(scanOp)
    ,dAttr(dAttr)
    ,value(value){
}

IT_FileScan::~IT_FileScan(){
  
}

RC IT_FileScan::Open(){
  RC rc;

  if((rc = rmm->OpenFile(relName,rmfh)))
    return rc;
  
  if((rc = rmfs->OpenScan(
                  rmfh
                  ,dAttr.attrType
                  ,dAttr.attrLength
                  ,dAttr.attrOffset
                  ,scanOp
                  ,value)))
    return rc;
}

RC  IT_FileScan::GetNext(DataAttrInfo *&pAttr, int &nAttr, void *&pData){
  RC rc;

  if(!bIsOpen) {
    return QL_ITERATOR_NOT_OPENED;
  }

  rc = rmfs.GetNextRec(rec);
  if (rc != RM_EOF && rc != 0)
    return rc;

  

  if (rc == RM_EOF)
    return QL_EOF;

  return rc;
}
RC IT_FileScan::Close(){
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
