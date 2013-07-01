#include "ql_treeplandelete.h"

#include "ql_internal.h"

#include <stdio.h>
#include <cstddef>

#include <string>
#include <map>

#include "ql_iterator.h"
#include "it_indexscan.h"
#include "it_filescan.h"

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
    cout << "Start delete: " << nodeOperationAsString[_nodeOperation] << endl;
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
        QL_TreePlanDelete *pProjector = new QL_TreePlanDelete(_pSmm, _pIxm, _pRmm);
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
        pComparison->SetLeftChild(pProjector);
        pProjector->SetLeftChild(pSelect);

        // build the projection
        if((rc = pProjector->BuildFromProjection(selAttrs, relations, conditions)))
            return rc;

        // build the selection node
        if((rc = pSelect->BuildFromSelect(selAttrs, relations, vConditions)))
            return rc;
    }
    else
    {
        //cout << "Few conditions detected..." << endl;

        // create the new nodes
        QL_TreePlanDelete *pSelect = new QL_TreePlanDelete(_pSmm, _pIxm, _pRmm);

        // update the nodes
        this->SetLeftChild(pSelect);

        // build the projection
        if((rc = this->BuildFromProjection(selAttrs, relations, conditions)))
            return rc;

        // build the selection node
        if((rc = pSelect->BuildFromSelect(selAttrs, relations, conditions)))
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

    _nodeOperation = SELECT;

    if((rc = ComputeAttributesStructure(selAttrs, _nNodeAttributes, _nodeAttributes, _bufferSize)))
        return rc;

    if(relations.size() == 0)
    {
        return QL_NO_RELATION;
    }

    _sRelname.assign(relations[0]);

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

//    cout << "Perform COMPARISON" << endl;

    int left_nAttributes;
    DataAttrInfo *left_nodeAttributes = NULL;
    char *left_pData = NULL;
    bool isConditionPassed = false;

    int index;

    // check if left child exists
    if(_pLc == NULL)
    {
        return QL_NULL_CHILD;
    }

    while(!isConditionPassed)
    {
        // get the information from left child
        if((rc = _pLc->GetNext(left_nAttributes, left_nodeAttributes, left_pData)))
        {
            if(pData != NULL)
            {
                delete[] pData;
                pData = NULL;
            }

            return rc;
        }

        // copy the required attributes in the output buffer
        for(int i=0; i<left_nAttributes; i++)
        {
            // check if the attribute must be copied
            if((rc = IsAttributeInList(_nNodeAttributes, _nodeAttributes, left_nodeAttributes[i], index)))
                return rc;

            // the attribute must be copied
            if(index >= 0)
            {
                // a condition exists
                if(_conditions.size() > 0)
                {
                    // the left hand side of the condition matches the current attribute
                    if(strcmp(_conditions[0].lhsAttr.relName, left_nodeAttributes[i].relName) == 0 &&
                            strcmp(_conditions[0].lhsAttr.attrName, left_nodeAttributes[i].attrName) == 0)
                    {
                        switch(left_nodeAttributes[i].attrType)
                        {
                        case INT:
                        case FLOAT:
                            float fAttrValue, fRefValue;

                            // subcase: INT
                            if(left_nodeAttributes[i].attrType == INT)
                            {
                                int attrValue, refValue;
                                memcpy(&attrValue,
                                       left_pData + left_nodeAttributes[i].offset,
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
                                       left_pData + left_nodeAttributes[i].offset,
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
                                   left_pData + left_nodeAttributes[i].offset);

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

        if(left_pData)
        {
            delete[] left_pData;
            left_pData = NULL;
        }
    }

    return rc;
}

RC QL_TreePlanDelete::PerformSelect(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData)
{
    RC rc = OK_RC;

    // need to open scan
    if(_scanStatus == SCANCLOSED)
    {
        // with conditions
        if(_conditions.size()>0)
        {
            DataAttrInfo attr;
            if((rc = _pSmm->GetAttributeStructure(_conditions[0].lhsAttr.relName, _conditions[0].lhsAttr.attrName, attr)))
                return rc;

            // Index use
            if(attr.indexNo >= 0)
            {
                _pScanIterator = new IT_IndexScan(_pRmm, _pIxm, _pSmm, attr.relName, _conditions[0].op, attr, _conditions[0].rhsValue.data);
            }
            else
            {
                _pScanIterator = new IT_FileScan(_pRmm, _pSmm, attr.relName, _conditions[0].op, attr, _conditions[0].rhsValue.data);
            }
        }
        else
        {
           _pScanIterator = new IT_FileScan(_pRmm, _pSmm, _sRelname.c_str(), NO_OP, _nodeAttributes[0], NULL);
        }
    }

    if(_scanStatus == SCANCLOSED)
    {
        if((rc = _pScanIterator->Open()))
            return rc;

        _scanStatus = SCANOPENED;
    }

    rc = _pScanIterator->GetNext(nAttributes, tNodeAttributes, pData);
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
    printf ( "%c %s\n", prefix, nodeOperationAsString[_nodeOperation] );

    if(_pRc != NULL)
        _pRc->Print ('\\',level + 1);
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



