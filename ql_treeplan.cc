#include "ql_treeplan.h"

#include "ql_internal.h"

QL_TreePlan::QL_TreePlan()
{
    _pLc = NULL;
    _pRc = NULL;
}

QL_TreePlan::~QL_TreePlan()
{
    if(_pLc != NULL)
    {
        delete _pLc;
        _pLc = NULL;
    }

    if(_pRc != NULL)
    {
        delete _pRc;
        _pRc = NULL;
    }
}


RC QL_TreePlan::PerformNodeOperation()
{
    RC rc = OK_RC;


    return rc;
}
