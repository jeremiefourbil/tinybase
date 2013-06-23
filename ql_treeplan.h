#ifndef QL_TREEPLAN_H
#define QL_TREEPLAN_H

#include "redbase.h"

class QL_TreePlan
{
public:
    enum NodeOperation{
        UNION,
        COMPARISON,
        PROJECTION,
        JOIN
    };

    QL_TreePlan();
    ~QL_TreePlan();

    RC PerformNodeOperation();

private:
    QL_TreePlan *_pLc;
    QL_TreePlan *_pRc;
    NodeOperation _nodeOperation;

};

#endif // QL_TREEPLAN_H
