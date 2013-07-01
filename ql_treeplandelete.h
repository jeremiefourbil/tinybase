#ifndef QL_TREEPLANDELETE_H
#define QL_TREEPLANDELETE_H

#include <vector>
#include <string>

#include "redbase.h"
#include "sm.h"
#include "parser.h"
#include "printer.h"

#include "ql_iterator.h"

class QL_TreePlanDelete
{
public:
    enum NodeOperation {
        COMPARISON,
        SELECT
    };

    enum ScanStatus {
        SCANOPENED,
        SCANCLOSED
    };

    // constructor & destructor
    QL_TreePlanDelete(SM_Manager *ipSmm, IX_Manager *ipIxm, RM_Manager *ipRmm);
    ~QL_TreePlanDelete();

    // getters
    ScanStatus getScanStatus() { return _scanStatus; }

    // setters
    void SetNodeOperation(NodeOperation iNodeOperation);
    void SetLeftChild(QL_TreePlanDelete *ipTreePlan);

    // builder
    RC BuildFromQuery(const std::vector<RelAttr> &selAttrs,
                      const std::vector<const char*> &relations,
                      const std::vector<Condition> &conditions);

    RC BuildFromComparison(const std::vector<RelAttr> &selAttrs,
                      const std::vector<const char*> &relations,
                      const std::vector<Condition> &conditions);

    RC BuildFromSelect(const std::vector<RelAttr> &selAttrs,
                      const std::vector<const char*> &relations,
                      const std::vector<Condition> &conditions);


    // operator
    RC GetNext(RM_Record &oRecord);

    void Print(char prefix, int level);

private:
    RC PerformComparison(RM_Record &oRecord);
    RC PerformSelect(RM_Record &oRecord);

    RC ComputeAttributesStructure(const std::vector<RelAttr> &selAttrs, int &nNodeAttributes, DataAttrInfo *&nodeAttributes, int &bufferSize);
    RC IsAttributeInList(const int nNodeAttributes, const DataAttrInfo *nodeAttributes, const DataAttrInfo &attribute, int &index);

    void Padding(char ch, int n);

    SM_Manager *_pSmm;
    IX_Manager *_pIxm;
    RM_Manager *_pRmm;

    QL_TreePlanDelete *_pLc;
    NodeOperation _nodeOperation;

    int _bufferSize;

    int _nNodeAttributes;
    DataAttrInfo *_nodeAttributes;

    int _nOperationAttributes;
    DataAttrInfo *_operationAttributes;
    std::string _sRelname;

    std::vector<Condition> _conditions;

    ScanStatus _scanStatus;
    QL_Iterator *_pScanIterator;
};

#endif // QL_TreePlanDelete_H
