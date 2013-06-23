#include "ql_treeplan.h"

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
