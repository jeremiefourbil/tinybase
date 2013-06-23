#include "ql_treeplan.h"

#include "ql_internal.h"

QL_TreePlan::QL_TreePlan()
{
    _pLc = NULL;
    _pRc = NULL;
    _nodeAttributes = NULL;
    _operationAttributes = NULL;
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

    if(_nodeAttributes != NULL)
    {
        delete _nodeAttributes;
        _nodeAttributes = NULL;
    }

    if(_operationAttributes != NULL)
    {
        delete _operationAttributes;
        _operationAttributes = NULL;
    }
}

RC QL_TreePlan::BuildFromQuery(int nSelAttrs, const RelAttr selAttrs[],
                               int nRelations, const char * const relations[],
                               int nConditions, const Condition conditions[])
{
    RC rc = OK_RC;


    return rc;
}

RC QL_TreePlan::PerformNodeOperation()
{
    RC rc = OK_RC;

    switch(_nodeOperation)
    {
    case UNION:
        rc = PerformUnion();
        break;
    case COMPARISON:
        rc = PerformComparison();
        break;
    case PROJECTION:
        rc = PerformProjection();
        break;
    case JOIN:
        rc = PerformJoin();
        break;
    default:
        rc = QL_UNKNOWN_TYPE;
    }

    return rc;
}

RC QL_TreePlan::PerformUnion()
{
    RC rc = OK_RC;


    return rc;
}

RC QL_TreePlan::PerformComparison()
{
    RC rc = OK_RC;


    return rc;
}

RC QL_TreePlan::PerformProjection()
{
    RC rc = OK_RC;


    return rc;
}

RC QL_TreePlan::PerformJoin()
{
    RC rc = OK_RC;


    return rc;
}
