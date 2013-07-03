#include "ql_treeplandelete.h"

#include <stdio.h>
#include <cstddef>

#include <string>
#include <map>

#include "ql_iterator.h"
#include "it_indexscan.h"
#include "it_filescan.h"

#include "ql_common.h"
#include "ql_internal.h"

using namespace std;

// *************************
// Constructor & destructor
// *************************

QL_TreePlanDelete::QL_TreePlanDelete(SM_Manager *ipSmm, IX_Manager *ipIxm, RM_Manager *ipRmm)
{
    _pSmm = ipSmm;
    _pIxm = ipIxm;
    _pRmm = ipRmm;

    _pLc = NULL;

    _bufferSize = 0;

    _nNodeAttributes = 0;
    _nodeAttributes = NULL;

    _nOperationAttributes = 0;
    _operationAttributes = NULL;

    _scanStatus = SCANCLOSED;
    _pScanIterator = NULL;
}

QL_TreePlanDelete::~QL_TreePlanDelete()
{
    _pSmm = NULL;

    if(_nodeAttributes != NULL)
    {
        delete[] _nodeAttributes;
        _nodeAttributes = NULL;
    }

    if(_operationAttributes != NULL)
    {
        delete[] _operationAttributes;
        _operationAttributes = NULL;
    }

    if(_pLc != NULL)
    {
        delete _pLc;
        _pLc = NULL;
    }
}


// *************************
// Setters
// *************************
void QL_TreePlanDelete::SetNodeOperation(NodeOperation iNodeOperation)
{
    _nodeOperation = iNodeOperation;
}
void QL_TreePlanDelete::SetLeftChild(QL_TreePlanDelete *ipTreePlan)
{
    if(_pLc != NULL)
    {
        delete _pLc;
    }

    _pLc = ipTreePlan;
}

// *************************
// Builders
// *************************

RC QL_TreePlanDelete::BuildFromQuery(const std::vector<RelAttr> &selAttrs,
                                     const std::vector<const char*> &relations,
                                     const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    // more than one condition, have to split them
    if(conditions.size() > 1)
    {
        //cout << "Lots of conditions detected" << endl;
        // create the new nodes
        QL_TreePlanDelete *pSelect = new QL_TreePlanDelete(_pSmm, _pIxm, _pRmm);
        QL_TreePlanDelete *pComparison = NULL;

        // build the comparisons, the first comparison node is this
        std::vector<Condition> vConditions(conditions);
        pComparison = this;
        if((rc = pComparison->BuildFromComparison(selAttrs, relations, vConditions)))
            return rc;
        vConditions.pop_back();

        QL_TreePlanDelete *pFather = this;
        while(vConditions.size()>1)
        {
            // create the new comparison node
            pComparison = new QL_TreePlanDelete(_pSmm, _pIxm, _pRmm);
            if((rc = pComparison->BuildFromComparison(selAttrs, relations, vConditions)))
                return rc;

            // update the father
            pFather->SetLeftChild(pComparison);
            pFather = pComparison;
            vConditions.pop_back();
        }

        // update the last comparison node
        pComparison->SetLeftChild(pSelect);

        // build the selection node
        if((rc = pSelect->BuildFromSelect(selAttrs, relations, vConditions)))
            return rc;
    }
    else
    {
        // build the selection node
        if((rc = this->BuildFromSelect(selAttrs, relations, conditions)))
            return rc;
    }

    return rc;
}


RC QL_TreePlanDelete::BuildFromComparison(const std::vector<RelAttr> &selAttrs,
                                          const std::vector<const char*> &relations,
                                          const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    //cout << "Build from comparison" << endl;

    _nodeOperation = COMPARISON;
    _sRelName.assign(relations[0]);

    if((rc = ComputeAttributesStructure(selAttrs, _nNodeAttributes, _nodeAttributes, _bufferSize)))
        return rc;

    // copy the last condition
    if(conditions.size() > 0)
        _conditions.push_back(conditions.back());

    return rc;
}

RC QL_TreePlanDelete::BuildFromSelect(const std::vector<RelAttr> &selAttrs,
                                      const std::vector<const char*> &relations,
                                      const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    //cout << "Build from select" << endl;


    if(relations.size() == 0)
    {
        return QL_NO_RELATION;
    }

    _nodeOperation = SELECT;
    _sRelName.assign(relations[0]);

    if((rc = ComputeAttributesStructure(selAttrs, _nNodeAttributes, _nodeAttributes, _bufferSize)))
        return rc;


    if(conditions.size()>0)
    {
        _conditions.push_back(conditions.back());
    }

    return rc;
}

// *************************
// Operators
// *************************

RC QL_TreePlanDelete::GetNext(RM_Record &oRecord)
{
    RC rc = OK_RC;

    switch(_nodeOperation)
    {
    case COMPARISON:
        rc = PerformComparison(oRecord);
        break;
    case SELECT:
        rc = PerformSelect(oRecord);
        break;
    default:
        rc = QL_UNKNOWN_TYPE;
    }

    return rc;
}


RC QL_TreePlanDelete::PerformComparison(RM_Record &oRecord)
{
    RC rc = OK_RC;
    char *pData = NULL;
    bool isConditionPassed = false;


    // check if left child exists
    if(_pLc == NULL)
    {
        return QL_NULL_CHILD;
    }

    while(!isConditionPassed)
    {
        // get the information from left child
        if((rc = _pLc->GetNext(oRecord)))
            return rc;

        oRecord.GetData(pData);

        if(_conditions.size() > 0)
        {

            // copy the required attributes in the output buffer
            for(int i=0; i<_nNodeAttributes; i++)
            {
                // the left hand side of the condition matches the current attribute
                if(strcmp(_sRelName.c_str(), _nodeAttributes[i].relName) == 0 &&
                        strcmp(_conditions[0].lhsAttr.attrName, _nodeAttributes[i].attrName) == 0)
                {
                    switch(_nodeAttributes[i].attrType)
                    {
                    case INT:
                    case FLOAT:
                        float fAttrValue, fRefValue;

                        // subcase: INT
                        if(_nodeAttributes[i].attrType == INT)
                        {
                            int attrValue, refValue;
                            memcpy(&attrValue,
                                   pData + _nodeAttributes[i].offset,
                                   sizeof(int));
                            memcpy(&refValue,
                                   _conditions[0].rhsValue.data,
                                   sizeof(int));

                            fAttrValue = attrValue;
                            fRefValue = refValue;
                        }
                        // subcase: FLOAT
                        else
                        {
                            memcpy(&fAttrValue,
                                   pData + _nodeAttributes[i].offset,
                                   sizeof(float));
                            memcpy(&fRefValue,
                                   _conditions[0].rhsValue.data,
                                   sizeof(float));
                        }

                        switch(_conditions[0].op)
                        {
                        case EQ_OP:
                            isConditionPassed = fAttrValue == fRefValue;
                            break;
                        case LE_OP:
                            isConditionPassed = fAttrValue <= fRefValue;
                            break;
                        case LT_OP:
                            isConditionPassed = fAttrValue < fRefValue;
                            break;
                        case GE_OP:
                            isConditionPassed = fAttrValue >= fRefValue;
                            break;
                        case GT_OP:
                            isConditionPassed = fAttrValue > fRefValue;
                            break;
                        case NE_OP:
                            isConditionPassed = fAttrValue != fRefValue;
                            break;
                        default:
                            isConditionPassed = true;
                        }

                        break;
                    case STRING:
                        char strAttrValue[MAXSTRINGLEN], strRefValue[MAXSTRINGLEN];

                        strcpy(strAttrValue,
                               pData + _nodeAttributes[i].offset);

                        strcpy(strRefValue,
                               (char*) _conditions[0].rhsValue.data);


                        switch(_conditions[0].op)
                        {
                        case EQ_OP:
                            isConditionPassed = strcmp(strAttrValue, strRefValue) == 0;
                            break;
                        case LE_OP:
                            isConditionPassed = strcmp(strAttrValue, strRefValue) <= 0;
                            break;
                        case LT_OP:
                            isConditionPassed = strcmp(strAttrValue, strRefValue) < 0;
                            break;
                        case GE_OP:
                            isConditionPassed = strcmp(strAttrValue, strRefValue) >= 0;
                            break;
                        case GT_OP:
                            isConditionPassed = strcmp(strAttrValue, strRefValue) > 0;
                            break;
                        case NE_OP:
                            isConditionPassed = strcmp(strAttrValue, strRefValue) != 0;
                            break;
                        default:
                            isConditionPassed = true;
                        }

                        break;
                    default:
                        isConditionPassed = true;
                        break;
                    }

                }
            }
        }
    }

    // no need to delete, done with RM_Record destructor
    pData = NULL;

    return rc;
}

RC QL_TreePlanDelete::PerformSelect(RM_Record &oRecord)
{
    RC rc = OK_RC;


    // need to open scan
    if(_scanStatus == SCANCLOSED)
    {
        // with conditions
        if(_conditions.size()>0)
        {
            DataAttrInfo attr;
            if((rc = _pSmm->GetAttributeStructure(_sRelName.c_str(), _conditions[0].lhsAttr.attrName, attr)))
                return rc;

            if(_conditions[0].rhsValue.type == STRING)
            {
                fillString((char*) _conditions[0].rhsValue.data, attr.attrLength);
            }

            // Index use
            //            if(attr.indexNo >= 0)
            //            {
            //                cout << "index scan" << endl;
            //                _pScanIterator = new IT_IndexScan(_pRmm, _pIxm, _pSmm, _sRelName.c_str(), _conditions[0].op, attr, _conditions[0].rhsValue.data);
            //            }
            //            else
            //            {
            _pScanIterator = new IT_FileScan(_pRmm, _pSmm, _sRelName.c_str(), _conditions[0].op, attr, _conditions[0].rhsValue.data);
            //            }
        }
        else
        {
            _pScanIterator = new IT_FileScan(_pRmm, _pSmm, _sRelName.c_str(), NO_OP, _nodeAttributes[0], NULL);
        }
    }

    if(_scanStatus == SCANCLOSED)
    {
        if((rc = _pScanIterator->Open()))
            return rc;

        _scanStatus = SCANOPENED;
    }

    rc = _pScanIterator->GetNext(oRecord);
    if(rc == QL_EOF)
    {
        if((rc = _pScanIterator->Close()))
            return rc;

        if(_pScanIterator != NULL)
        {
            delete _pScanIterator;
            _pScanIterator = NULL;
        }

        _scanStatus = SCANCLOSED;

        return QL_EOF;
    }


    return rc;
}


// *************************
// Printer
// *************************

void QL_TreePlanDelete::Padding (char ch, int n)
{
    int i;
    for(i=0;i<n;i++){
        putchar(ch);
    }
}


void QL_TreePlanDelete::Print (char prefix ,int level)
{
    if(_pLc != NULL)
        _pLc->Print ('/',level + 1);
    Padding('\t', level);
    printf ( "%c %s\n", prefix, nodeDeleteOperationAsString[_nodeOperation] );
}

// *************************
// Helper
// *************************

RC QL_TreePlanDelete::ComputeAttributesStructure(const std::vector<RelAttr> &selAttrs, int &nNodeAttributes, DataAttrInfo *&nodeAttributes, int &bufferSize)
{
    RC rc = OK_RC;
    bufferSize = 0;

    nNodeAttributes = selAttrs.size();
    nodeAttributes = new DataAttrInfo[nNodeAttributes];

    for(unsigned int i=0; i<nNodeAttributes; i++)
    {
        if((rc = _pSmm->GetAttributeStructure(selAttrs[i].relName, selAttrs[i].attrName, nodeAttributes[i])))
            return rc;

        nodeAttributes[i].offset = bufferSize;
        bufferSize += nodeAttributes[i].attrLength;
    }

    return rc;
}

RC QL_TreePlanDelete::IsAttributeInList(const int nNodeAttributes, const DataAttrInfo *nodeAttributes, const DataAttrInfo &attribute, int &index)
{
    RC rc = OK_RC;

    bool isIn = false;

    for(index=0; index<nNodeAttributes; index++)
    {
        if(strcmp(nodeAttributes[index].relName, attribute.relName) == 0 &&
                strcmp(nodeAttributes[index].attrName, attribute.attrName) == 0)
        {
            isIn = true;
            break;
        }
    }

    if(!isIn)
        index = -1;

    return rc;
}



