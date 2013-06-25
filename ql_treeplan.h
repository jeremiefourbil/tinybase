#ifndef QL_TREEPLAN_H
#define QL_TREEPLAN_H

#include <vector>

#include "redbase.h"
#include "sm.h"
#include "parser.h"
#include "printer.h"

class QL_TreePlan
{
public:
    enum NodeOperation{
        UNION,
        COMPARISON,
        PROJECTION,
        JOIN,
        SELECT
    };

    // constructor & destructor
    QL_TreePlan(SM_Manager *ipSmm);
    ~QL_TreePlan();

    // setters
    void SetNodeOperation(NodeOperation iNodeOperation);
    void SetLeftChild(QL_TreePlan *ipTreePlan);
    void SetRightChild(QL_TreePlan *ipTreePlan);

    // builder
    RC BuildFromQuery(const std::vector<RelAttr> &selAttrs,
                      const std::vector<const char*> &relations,
                      const std::vector<Condition> &conditions);

    RC BuildFromSingleRelation(const std::vector<RelAttr> &selAttrs,
                                   const std::vector<const char*> &relations,
                                   const std::vector<Condition> &conditions);

    RC BuildFromComparison(const std::vector<RelAttr> &selAttrs,
                      const std::vector<const char*> &relations,
                      const std::vector<Condition> &conditions);

    RC BuildFromProjection(const std::vector<RelAttr> &selAttrs,
                      const std::vector<const char*> &relations,
                      const std::vector<Condition> &conditions);

    RC BuildFromJoin(const std::vector<RelAttr> &selAttrs,
                      const std::vector<const char*> &relations,
                      const std::vector<Condition> &conditions);

    RC BuildFromSelect(const std::vector<RelAttr> &selAttrs,
                      const std::vector<const char*> &relations,
                      const std::vector<Condition> &conditions);

    // operator
    RC PerformNodeOperation(int nAttributes, DataAttrInfo *tNodeAttributes,
                      void * pData);

    RC PerformUnion();
    RC PerformComparison();
    RC PerformProjection();
    RC PerformJoin();

    void Print(char prefix, int level);

    RC ComputeAttributesStructure(const std::vector<RelAttr> &selAttrs, int &nNodeAttributes, DataAttrInfo *&nodeAttributes);

private:
    SM_Manager *_pSmm;
    QL_TreePlan *_pLc;
    QL_TreePlan *_pRc;
    NodeOperation _nodeOperation;
    void Padding(char ch, int n);
    int _nNodeAttributes;
    DataAttrInfo *_nodeAttributes;
    int _nOperationAttributes;
    DataAttrInfo *_operationAttributes;
};

#endif // QL_TREEPLAN_H
