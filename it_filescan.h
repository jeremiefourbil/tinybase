#ifndef IT_FILESCAN_H
#define IT_FILESCAN_H

#include "ql.h"
#include "ql_iterator.h"
#include "printer.h"

class IT_FileScan : public virtual QL_Iterator {
  public:
    IT_FileScan(RM_Manager *rmm, SM_Manager *smm,
                const char* relName, CompOp scanOp, DataAttrInfo &dAttr, void *value);

    virtual ~IT_FileScan();

    RC Open();
    RC GetNext(int &nAttr, DataAttrInfo *&pAttr, char *pData);
    RC Close();
  private:
    RM_Manager *_pRmm;
    SM_Manager *_pSmm;

    RM_FileHandle _rmfh;
    RM_FileScan _rmfs;

    int _nAttr;
    DataAttrInfo *_dAttr;

    CompOp _scanOp;
    DataAttrInfo _iAttr;
    void *_value;
    const char* _relName;
};

#endif
