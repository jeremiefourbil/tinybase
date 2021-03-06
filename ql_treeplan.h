#ifndef QL_TREEPLAN_H
#define QL_TREEPLAN_H

#include <vector>
#include <string>

#include "redbase.h"
#include "sm.h"
#include "parser.h"
#include "printer.h"

#include "ql_iterator.h"

class QL_TreePlan
{
public:
    enum NodeOperation {
        UNION,
        COMPARISON,
        PROJECTION,
        JOIN,
        SELECT,
        CARTESIANPRODUCT
    };

    enum ScanStatus {
        SCANOPENED,
        SCANCLOSED
    };

    // constructor & destructor
    QL_TreePlan(SM_Manager *ipSmm, IX_Manager *ipIxm, RM_Manager *ipRmm);
    ~QL_TreePlan();

    // getters
    ScanStatus getScanStatus() { return _scanStatus; }

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


    RC BuildFromCartesianProduct(const std::vector<RelAttr> &selAttrs,
                      const std::vector<const char*> &relations,
                      const std::vector<Condition> &conditions);

    // operator
    RC GetNext(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData);

    void Print(char prefix, int level);

private:
    RC PerformUnion(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData);
    RC PerformComparison(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData);
    RC PerformProjection(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData);
    RC PerformJoin(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData);
    RC PerformSelect(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData);
    RC PerformCartesianProduct(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData);

    RC ComputeAttributesStructure(const std::vector<RelAttr> &selAttrs, int &nNodeAttributes, DataAttrInfo *&nodeAttributes, int &bufferSize);
    RC IsAttributeInList(const int nNodeAttributes, const DataAttrInfo *nodeAttributes, const DataAttrInfo &attribute, int &index);

    void Padding(char ch, int n);

    SM_Manager *_pSmm;
    IX_Manager *_pIxm;
    RM_Manager *_pRmm;

    QL_TreePlan *_pLc;
    QL_TreePlan *_pRc;
    NodeOperation _nodeOperation;

    int _nNodeAttributes;
    DataAttrInfo *_nodeAttributes;

    int _bufferSize;

    int _nOperationAttributes;
    DataAttrInfo *_operationAttributes;
    std::string _sRelName;

    std::vector<Condition> _conditions;

    ScanStatus _scanStatus;
    QL_Iterator *_pScanIterator;

    char * _pJoinData;
    int _nJoinAttributes;
    DataAttrInfo *_nodeJoinAttributes;
};

#endif // QL_TREEPLAN_H
