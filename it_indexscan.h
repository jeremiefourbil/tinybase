#ifndef IT_INDEXSCAN_H
#define IT_INDEXSCAN_H

#include "ql.h"
#include "ql_iterator.h"
#include "printer.h"

class IT_IndexScan : public virtual QL_Iterator {
  public:
    IT_IndexScan(RM_Manager *rmm, IX_Manager *ixm, SM_Manager *smm,
                    const char* relName, CompOp scanOp, DataAttrInfo &dAttr, void *value);

    virtual ~IT_IndexScan();

    RC Open();
    RC GetNext(int &nAttr, DataAttrInfo *&pAttr, char *pData);
    RC Close();
  private:
    RM_Manager *_pRmm;
    IX_Manager *_pIxm;
    SM_Manager *_pSmm;

    RM_FileHandle _rmfh;

    IX_IndexHandle _ixih;
    IX_IndexScan _ixis;

    int _nAttr;
    DataAttrInfo *_dAttr;

    CompOp _scanOp;
    DataAttrInfo _iAttr;
    void *_value;
    const char* _relName;
};

#endif
