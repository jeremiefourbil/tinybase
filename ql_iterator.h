#ifndef QL_ITERATOR
#define QL_ITERATOR

#include "rm.h"
#include "printer.h"

class QL_Iterator {
  public: 
    QL_Iterator():_bIsOpen(false){}
    virtual ~QL_Iterator(){}

    virtual RC Open() = 0;
    virtual RC GetNext(int &nAttr, DataAttrInfo *&pAttr, char *&pData) = 0;
    virtual RC GetNext(RM_Record &oRecord) = 0;
    virtual RC Close() = 0;
  protected:
    bool _bIsOpen;
};

#endif
