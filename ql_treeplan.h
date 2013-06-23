#ifndef QL_TREEPLAN_H
#define QL_TREEPLAN_H

#include <vector>
#include "redbase.h"
#include "parser.h"
#include "printer.h"

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

    RC BuildFromQuery(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[]
                      );

    RC PerformNodeOperation(int nAttributes, DataAttrInfo *tNodeAttributes,
                      void * pData);

    RC PerformUnion();
    RC PerformComparison();
    RC PerformProjection();
    RC PerformJoin();
    void Print(QL_TreePlan * x, char prefix ,int level);

private:
    QL_TreePlan *_pLc;
    QL_TreePlan *_pRc;
    NodeOperation _nodeOperation;
    void Padding(char ch, int n);
    int _nNodeAtributes;
    DataAttrInfo *_nodeAttributes;
    int _nOperationAttributes;
    DataAttrInfo *_operationAttributes;
};

#endif // QL_TREEPLAN_H
