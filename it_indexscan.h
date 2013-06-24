#ifndef IT_INDEXSCAN_H
#define IT_INDEXSCAN_H

#include "ql.h"
#include "ql_iterator.h"
#include "printer.h"

class IT_IndexScan : public virtual QL_Iterator {
  public:
    IT_IndexScan(
        RM_Manager *rmm
        ,IX_Manager *ixm
        ,SM_Manager *smm
        ,const char* relName
        ,CompOp &scanOp
        ,DataAttrInfo &dAttr
        ,void *value
        ,RC &rc);

    virtual ~IT_IndexScan();

    RC Open();
    RC GetNext(RM_Record &record);
    RC Close();
  private:
    RM_Manager *pRmm;
    IX_Manager *pIxm;
    SM_Manager *pSmm;

    RM_FileHandle rmfh;

    IX_IndexHandle ixih;
    IX_IndexScan ixis;

    int nAttr;
    DataAttrInfo *dAttr;

    CompOp scanOp;
    DataAttrInfo *iAttr;
    void *value;
    const char* relName;
};

#endif
