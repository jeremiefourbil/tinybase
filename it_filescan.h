#ifndef IT_FILESCAN_H
#define IT_FILESCAN_H

#include "ql.h"
#include "ql_iterator.h"

class IT_FileScan : public virtual QL_Iterator {
  public:
    IT_FileScan(
      RM_Manager &rmm
      ,SM_Manager &smm
      ,const char* relName
      ,ComOp &scanOp
      ,DataAttrInfo &dAttr
      ,void *value
      ,Rc &rc);

    virtual ~IT_FileScan();

    RC Open();
    RC GetNext(RM_Record &rec);
    RC Close();
  private:
    RM_Manager *pRmm;
    SM_Manager *pSmm;

    RM_FileHandle rmfh;
    RM_FileScan rmfs;

    DataAttrInfo *dAttr;

    CompOp scanOp;
    void *value;
    const char* relName;

    DataAttrInfo *pAttr;
    int nAttr;
};

#endif
