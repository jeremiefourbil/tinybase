#include "ql_treeplan.h"

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

QL_TreePlan::QL_TreePlan(SM_Manager *ipSmm, IX_Manager *ipIxm, RM_Manager *ipRmm)
{
    _pSmm = ipSmm;
    _pIxm = ipIxm;
    _pRmm = ipRmm;

    _pLc = NULL;
    _pRc = NULL;

    _nNodeAttributes = 0;
    _nodeAttributes = NULL;

    _bufferSize = 0;

    _nOperationAttributes = 0;
    _operationAttributes = NULL;

    _scanStatus = SCANCLOSED;
    _pScanIterator = NULL;

    _pJoinData = NULL;
}

QL_TreePlan::~QL_TreePlan()
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

    if(_pJoinData != NULL)
    {
        delete[] _pJoinData;
        _pJoinData = NULL;
    }

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


// *************************
// Setters
// *************************
void QL_TreePlan::SetNodeOperation(NodeOperation iNodeOperation)
{
    _nodeOperation = iNodeOperation;
}
void QL_TreePlan::SetLeftChild(QL_TreePlan *ipTreePlan)
{
    if(_pLc != NULL)
    {
        delete _pLc;
    }

    _pLc = ipTreePlan;
}

void QL_TreePlan::SetRightChild(QL_TreePlan *ipTreePlan)
{
    if(_pRc != NULL)
    {
        delete _pRc;
    }

    _pRc = ipTreePlan;
}

// *************************
// Builders
// *************************

RC QL_TreePlan::BuildFromQuery(const std::vector<RelAttr> &selAttrs,
                               const std::vector<const char*> &relations,
                               const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    // first, check if there is more than one relation involved
    // if so, the node is a join
    if(relations.size() > 1)
    {
        bool cartesianProduct = true;
        for(unsigned int i=0; i<conditions.size(); i++)
        {
            if(conditions[i].bRhsIsAttr)
            {
                cartesianProduct = false;
                break;
            }
        }

        if(cartesianProduct)
        {
            if((rc = BuildFromCartesianProduct(selAttrs, relations, conditions)))
                goto err_return;
        }
        else
        {
            if((rc = BuildFromJoin(selAttrs, relations, conditions)))
                goto err_return;
        }
    }
    else
    {
        //cout << "SINGLE RELATION detected" << endl;
        if((rc = BuildFromSingleRelation(selAttrs, relations, conditions)))
            goto err_return;
    }

    return rc;

err_return:
    return rc;
}

RC QL_TreePlan::BuildFromSingleRelation(const std::vector<RelAttr> &selAttrs,
                                        const std::vector<const char*> &relations,
                                        const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    // more than one condition, have to split them
    if(conditions.size() > 1)
    {
        //cout << "Lots of conditions detected" << endl;
        // create the new nodes
        QL_TreePlan *pProjector = new QL_TreePlan(_pSmm, _pIxm, _pRmm);
        QL_TreePlan *pSelect = new QL_TreePlan(_pSmm, _pIxm, _pRmm);
        QL_TreePlan *pComparison = NULL;

        // build the comparisons, the first comparison node is this
        std::vector<Condition> vConditions(conditions);
        pComparison = this;
        if((rc = pComparison->BuildFromComparison(selAttrs, relations, vConditions)))
            return rc;
        vConditions.pop_back();

        QL_TreePlan *pFather = this;
        while(vConditions.size()>1)
        {
            // create the new comparison node
            pComparison = new QL_TreePlan(_pSmm, _pIxm, _pRmm);
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
        QL_TreePlan *pSelect = new QL_TreePlan(_pSmm, _pIxm, _pRmm);

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

RC QL_TreePlan::BuildFromComparison(const std::vector<RelAttr> &selAttrs,
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

err_return:
    return rc;
}

RC QL_TreePlan::BuildFromProjection(const std::vector<RelAttr> &selAttrs,
                                    const std::vector<const char*> &relations,
                                    const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    //cout << "Build from projection" << endl;

    _nodeOperation = PROJECTION;

    if((rc = ComputeAttributesStructure(selAttrs, _nNodeAttributes, _nodeAttributes, _bufferSize)))
        return rc;


    return rc;

err_return:
    return rc;
}

RC QL_TreePlan::BuildFromJoin(const std::vector<RelAttr> &selAttrs,
                              const std::vector<const char*> &relations,
                              const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    cout << "Build from join" << endl;

    _nodeOperation = JOIN;

    if((rc = ComputeAttributesStructure(selAttrs, _nNodeAttributes, _nodeAttributes, _bufferSize)))
        return rc;


    _pLc = new QL_TreePlan(_pSmm, _pIxm, _pRmm);
    _pRc = new QL_TreePlan(_pSmm, _pIxm, _pRmm);

    // initialize children vectors
    std::vector<RelAttr> left_selAttrs;
    std::vector<const char*> left_relations;
    std::vector<Condition> left_conditions;

    std::vector<RelAttr> right_selAttrs;
    std::vector<const char*> right_relations;
    std::vector<Condition> right_conditions;


    // find the relations that appears once
    // initialize the hashmap
    std::map<string, int> relationsNb;
    for(unsigned int i=0; i<relations.size(); i++)
    {
        relationsNb.insert(std::pair<string, int>(relations[i], 0));
    }
    // update the hashmap
    for(unsigned int i=0; i<conditions.size(); i++)
    {
        if(conditions[i].bRhsIsAttr && conditions[i].op == EQ_OP)
        {
            std::map<string,int>::iterator it;
            string str(conditions[i].lhsAttr.relName);
            std::size_t found = str.find_first_of(".");
            it=relationsNb.find(str.substr(0, found));
            it->second++;
            str.assign(conditions[i].rhsAttr.relName);
            found = str.find_first_of(".");
            it=relationsNb.find(str.substr(0, found));
            it->second++;
        }
    }

    // put the iterator on a relation that appears once
    std::map<string,int>::iterator it;
    for (it=relationsNb.begin(); it!=relationsNb.end(); ++it)
    {
        if(it->second == 1)
        {
            break;
        }
    }

    // copy conditions in left & right children
    for(unsigned int i=0;i<conditions.size();i++)
    {
        string str(conditions[i].lhsAttr.relName);
        std::size_t found = str.find_first_of(".");
        // the left hand side matches the iterator
        if(str.substr(0, found).compare(it->first)==0)
        {
            // the right hand side is only a value
            if(!conditions[i].bRhsIsAttr)
            {
                right_conditions.push_back(conditions[i]);
            }
            // the right hand side is an attribute, it corresponds to the join equality
            else
            {
                // join equality treatment
                _operationAttributes = new DataAttrInfo[2];
                _nOperationAttributes = 2;

                // left side
                if((rc = _pSmm->GetAttributeStructure(conditions[i].lhsAttr.relName, conditions[i].lhsAttr.attrName, _operationAttributes[1])))
                    return rc;

                // right side
                if((rc = _pSmm->GetAttributeStructure(conditions[i].rhsAttr.relName, conditions[i].rhsAttr.attrName, _operationAttributes[0])))
                    return rc;
            }
        }
        // the left hand side does not match the iterator
        else
        {
            // the right hand side is only a value
            if(!conditions[i].bRhsIsAttr)
            {
                left_conditions.push_back(conditions[i]);
            }
            // the right hand side is an attribute
            else
            {
                str.assign(conditions[i].rhsAttr.relName);
                found = str.find_first_of(".");
                // it corresponds to the join equality
                if(str.substr(0, found).compare(it->first)==0)
                {
                    // join equality treatment
                    _operationAttributes = new DataAttrInfo[2];
                    _nOperationAttributes = 2;

                    // left side
                    if((rc = _pSmm->GetAttributeStructure(conditions[i].lhsAttr.relName, conditions[i].lhsAttr.attrName, _operationAttributes[0])))
                        return rc;

                    // right side
                    if((rc = _pSmm->GetAttributeStructure(conditions[i].rhsAttr.relName, conditions[i].rhsAttr.attrName, _operationAttributes[1])))
                        return rc;
                }
                // another join is spotted
                else
                {
                    left_conditions.push_back(conditions[i]);
                }
            }
        }
    }

    // copy relations in left & right children
    for(unsigned int i=0;i<relations.size();i++)
    {
        // the relation matches the iterator
        if(strcmp(it->first.c_str(),relations[i])==0)
        {
            right_relations.push_back(relations[i]);
        }
        // the relation does not match the iterator
        else
        {
            left_relations.push_back(relations[i]);
        }
    }

    // copy attributes in left & right children
    for(unsigned int i=0;i<selAttrs.size();i++)
    {
        // the attributes belongs to a relation that matches the iterator
        if(strcmp(it->first.c_str(),selAttrs[i].relName)==0)
        {
            right_selAttrs.push_back(selAttrs[i]);
        }
        // the attributes belongs to a relation that does not match the iterator
        else
        {
            left_selAttrs.push_back(selAttrs[i]);
        }
    }

    // build the left child
    if((rc = _pLc->BuildFromQuery(left_selAttrs, left_relations, left_conditions)))
        goto err_return;

    // build the right child
    if((_pRc->BuildFromQuery(right_selAttrs, right_relations, right_conditions)))
        goto err_return;

    cout << "End Build from join" << endl;

    return rc;

err_return:
    return rc;
}

RC QL_TreePlan::BuildFromSelect(const std::vector<RelAttr> &selAttrs,
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

RC QL_TreePlan::BuildFromCartesianProduct(const std::vector<RelAttr> &selAttrs,
                              const std::vector<const char*> &relations,
                              const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    int relationOccurrences = 0;

//    cout << "Build from cartesian product" << endl;

    _nodeOperation = CARTESIANPRODUCT;

    if((rc = ComputeAttributesStructure(selAttrs, _nNodeAttributes, _nodeAttributes, _bufferSize)))
        return rc;


    _pLc = new QL_TreePlan(_pSmm, _pIxm, _pRmm);
    _pRc = new QL_TreePlan(_pSmm, _pIxm, _pRmm);

    // initialize children vectors
    std::vector<RelAttr> left_selAttrs;
    std::vector<const char*> left_relations;
    std::vector<Condition> left_conditions;

    std::vector<RelAttr> right_selAttrs;
    std::vector<const char*> right_relations;
    std::vector<Condition> right_conditions;


    // copy conditions in left & right children
    for(unsigned int i=0;i<conditions.size();i++)
    {
        string str(conditions[i].lhsAttr.relName);
        std::size_t found = str.find_first_of(".");
        // the left hand side matches the iterator
        if(str.substr(0, found).compare(relations[0])==0)
        {
            right_conditions.push_back(conditions[i]);
        }
        // the left hand side does not match the iterator
        else
        {
            left_conditions.push_back(conditions[i]);
        }
    }

    // copy relations in left & right children
    for(unsigned int i=0;i<relations.size();i++)
    {
        // the relation matches the iterator
        if(strcmp(relations[0],relations[i])==0)
        {
            if(relationOccurrences < 1)
            {
                right_relations.push_back(relations[i]);
            }

            relationOccurrences++;
        }
        // the relation does not match the iterator
        else
        {
            left_relations.push_back(relations[i]);
        }
    }

    // copy attributes in left & right children
    for(unsigned int i=0;i<selAttrs.size();i++)
    {
        // the attributes belongs to a relation that matches the iterator
        if(strcmp(relations[0],selAttrs[i].relName)==0)
        {
            right_selAttrs.push_back(selAttrs[i]);
        }
        // the attributes belongs to a relation that does not match the iterator
        else
        {
            left_selAttrs.push_back(selAttrs[i]);
        }
    }

    if(relationOccurrences > 1)
        return QL_EOF;

//    for(int i=1; i<relationOccurrences; i++)
//    {
//        left_relations.push_back(right_relations[0]);
//        cout << "relation copied" << endl;
//    }

//    if(relationOccurrences > 1)
//    {
//        for(unsigned j=0; j<right_selAttrs.size(); j++)
//        {
//            left_selAttrs.push_back(right_selAttrs[j]);
//        }
//        for(unsigned j=0; j<right_conditions.size(); j++)
//        {
//            left_conditions.push_back(right_conditions[j]);
//        }
//    }

    // build the left child
    if((rc = _pLc->BuildFromQuery(left_selAttrs, left_relations, left_conditions)))
        goto err_return;

    // build the right child
    if((_pRc->BuildFromQuery(right_selAttrs, right_relations, right_conditions)))
        goto err_return;

    return rc;

err_return:
    return rc;
}

// *************************
// Operators
// *************************

RC QL_TreePlan::GetNext(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData)
{
    RC rc = OK_RC;

    switch(_nodeOperation)
    {
    case UNION:
        rc = PerformUnion(nAttributes, tNodeAttributes, pData);
        break;
    case COMPARISON:
        rc = PerformComparison(nAttributes, tNodeAttributes, pData);
        break;
    case PROJECTION:
        rc = PerformProjection(nAttributes, tNodeAttributes, pData);
        break;
    case JOIN:
        rc = PerformJoin(nAttributes, tNodeAttributes, pData);
        break;
    case SELECT:
        rc = PerformSelect(nAttributes, tNodeAttributes, pData);
        break;
    case CARTESIANPRODUCT:
        rc = PerformCartesianProduct(nAttributes, tNodeAttributes, pData);
        break;
    default:
        rc = QL_UNKNOWN_TYPE;
    }

    return rc;
}

RC QL_TreePlan::PerformUnion(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData)
{
    RC rc = OK_RC;

    nAttributes = 0;
    tNodeAttributes = NULL;
    pData = NULL;

    //cout << "Perform UNION" << endl;

    return rc;
}

RC QL_TreePlan::PerformComparison(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData)
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

    // create the output buffer
    pData = new char[_bufferSize];

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

//                            memcpy(strAttrValue,
//                                   left_pData + left_nodeAttributes[i].offset,
//                                   left_nodeAttributes[i].attrLength);
//                            memcpy(strRefValue,
//                                   _conditions[0].rhsValue.data,
//                                   left_nodeAttributes[i].attrLength);


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

                // copy the attribute to the current buffer
                memcpy(pData + _nodeAttributes[index].offset,
                       left_pData + left_nodeAttributes[i].offset,
                       left_nodeAttributes[i].attrLength);
            }
        }

        if(left_pData)
        {
            delete[] left_pData;
            left_pData = NULL;
        }
    }

    // assign the output vars
    nAttributes = _nNodeAttributes;
    tNodeAttributes = _nodeAttributes;

    return rc;
}

RC QL_TreePlan::PerformProjection(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData)
{
    RC rc = OK_RC;

//    cout << "Perform PROJECTION" << endl;

    int left_nAttributes;
    DataAttrInfo *left_nodeAttributes = NULL;
    char *left_pData = NULL;

    int index;

    // check if left child exists
    if(_pLc == NULL)
    {
        return QL_NULL_CHILD;
    }

    // get the information from left child
    if((rc = _pLc->GetNext(left_nAttributes, left_nodeAttributes, left_pData)))
        return rc;

    // create the output buffer
    pData = new char[_bufferSize];

    // copy the required attributes in the output buffer
    for(int i=0; i<left_nAttributes; i++)
    {
        if((rc = IsAttributeInList(_nNodeAttributes, _nodeAttributes, left_nodeAttributes[i], index)))
        {
            delete[] pData;
            delete[] left_pData;

            return rc;
        }

        if(index >= 0)
        {
            memcpy(pData + _nodeAttributes[index].offset,
                   left_pData + left_nodeAttributes[i].offset,
                   left_nodeAttributes[i].attrLength);
        }
    }

    // assign the output vars
    nAttributes = _nNodeAttributes;
    tNodeAttributes = _nodeAttributes;

    if(left_pData != NULL)
    {
        delete[] left_pData;
        left_pData = NULL;
    }

    return rc;
}

RC QL_TreePlan::PerformJoin(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData)
{
    RC rc = OK_RC;

    int right_nAttributes;
    DataAttrInfo *right_nodeAttributes = NULL;
    char *right_pData = NULL;
    bool joinCondition = false;
    int index;

//    cout << "Perform JOIN" << endl;

    // check if left child exists
    if(_pLc == NULL)
    {
        return QL_NULL_CHILD;
    }

    if(_pJoinData == NULL)
    {
        if((rc = _pLc->GetNext(_nJoinAttributes, _nodeJoinAttributes, _pJoinData)))
            return rc;
    }



    // get the information from left child
    while(!joinCondition)
    {
        // delete the previous right child
        if(right_pData != NULL)
        {
            delete[] right_pData;
            right_pData = NULL;
        }

        // get the next right child
        if((rc = _pRc->GetNext(right_nAttributes, right_nodeAttributes, right_pData)))
        {
            // delete the previous left child
            if(_pJoinData != NULL)
            {
                delete[] _pJoinData;
                _pJoinData = NULL;
                _nodeJoinAttributes = NULL;
            }

            // get the next left child
            if((rc = _pLc->GetNext(_nJoinAttributes, _nodeJoinAttributes, _pJoinData)))
            {
                if(right_pData != NULL)
                {
                    delete[] right_pData;
                    right_nodeAttributes = NULL;
                }
                return rc;
            }
        }
        else
        {
            // check the join condition
            int left_index, right_index;
            if((rc = IsAttributeInList(_nJoinAttributes, _nodeJoinAttributes, _operationAttributes[0], left_index)))
                return rc;
            if((rc = IsAttributeInList(right_nAttributes, right_nodeAttributes, _operationAttributes[1], right_index)))
                return rc;
            switch (_operationAttributes[0].attrType)
            {
            case INT:
                int tempLeft, tempRight;
                memcpy(&tempLeft, _pJoinData+_nodeJoinAttributes[left_index].offset, sizeof(int));
                memcpy(&tempRight, right_pData+right_nodeAttributes[right_index].offset, sizeof(int));
                joinCondition = tempLeft == tempRight;
                break;
            case FLOAT:
                float fTempLeft, fTempRight;
                memcpy(&fTempLeft, _pJoinData+_nodeJoinAttributes[left_index].offset, sizeof(float));
                memcpy(&fTempRight, right_pData+right_nodeAttributes[right_index].offset, sizeof(float));
                joinCondition = fTempLeft == fTempRight;
                break;
            case STRING:
                char sTempLeft[MAXSTRINGLEN], sTempRight[MAXSTRINGLEN];
                strcpy(sTempLeft, _pJoinData+_nodeJoinAttributes[left_index].offset);
                strcpy(sTempRight, right_pData+right_nodeAttributes[right_index].offset);
                joinCondition = strcmp(sTempLeft,sTempRight) == 0;
                break;
            default:
                break;
            }
        }
    }

    // create the output buffer
    pData = new char[_bufferSize];

    // copy the required attributes in the output buffer
    for(int i=0; i<_nJoinAttributes; i++)
    {
        if((rc = IsAttributeInList(_nNodeAttributes, _nodeAttributes, _nodeJoinAttributes[i], index)))
            return rc;

        if(index >= 0)
        {
            memcpy(pData + _nodeAttributes[index].offset,
                   _pJoinData + _nodeJoinAttributes[i].offset,
                   _nodeJoinAttributes[i].attrLength);
        }
    }

    for(int i=0; i<right_nAttributes; i++)
    {
        if((rc = IsAttributeInList(_nNodeAttributes, _nodeAttributes, right_nodeAttributes[i], index)))
            return rc;

        if(index >= 0)
        {
            memcpy(pData + _nodeAttributes[index].offset,
                   right_pData + right_nodeAttributes[i].offset,
                   right_nodeAttributes[i].attrLength);
        }
    }

    // assign the output vars
    nAttributes = _nNodeAttributes;
    tNodeAttributes = _nodeAttributes;

    // delete the children buffers
    if(right_pData != NULL)
    {
        delete[] right_pData;
        right_pData = NULL;
    }

    return rc;
}

RC QL_TreePlan::PerformSelect(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData)
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


            if(_conditions[0].rhsValue.type == STRING)
            {
                fillString((char*) _conditions[0].rhsValue.data, attr.attrLength);
            }


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


RC QL_TreePlan::PerformCartesianProduct(int &nAttributes, DataAttrInfo *&tNodeAttributes, char * &pData)
{
    RC rc = OK_RC;

    int right_nAttributes;
    DataAttrInfo *right_nodeAttributes;
    char *right_pData;

    int index;

    // check if children do exist
    if(_pLc == NULL || _pRc == NULL)
    {
        return QL_NULL_CHILD;
    }

    if(_pJoinData == NULL)
    {
        if((rc = _pLc->GetNext(_nJoinAttributes, _nodeJoinAttributes, _pJoinData)))
            return rc;
    }


    if((rc = _pRc->GetNext(right_nAttributes, right_nodeAttributes, right_pData)))
    {
        if(_pJoinData != NULL)
        {
            delete[] _pJoinData;
            _nodeJoinAttributes = NULL;
        }

        if((rc = _pLc->GetNext(_nJoinAttributes, _nodeJoinAttributes, _pJoinData)))
        {
            if(right_pData != NULL)
            {
                delete[] right_pData;
                right_nodeAttributes = NULL;
            }
            return rc;
        }

        if((rc = _pRc->GetNext(right_nAttributes, right_nodeAttributes, right_pData)))
        {
            if(_pJoinData != NULL)
            {
                delete[] _pJoinData;
                _nodeJoinAttributes = NULL;
            }

            return rc;
        }
    }

    // create the output buffer
    pData = new char[_bufferSize];

    // copy the required attributes in the output buffer
    for(int i=0; i<_nJoinAttributes; i++)
    {
        if((rc = IsAttributeInList(_nNodeAttributes, _nodeAttributes, _nodeJoinAttributes[i], index)))
            return rc;

        if(index >= 0)
        {
            memcpy(pData + _nodeAttributes[index].offset,
                   _pJoinData + _nodeJoinAttributes[i].offset,
                   _nodeJoinAttributes[i].attrLength);
        }
    }

    for(int i=0; i<right_nAttributes; i++)
    {
        if((rc = IsAttributeInList(_nNodeAttributes, _nodeAttributes, right_nodeAttributes[i], index)))
            return rc;

        if(index >= 0)
        {
            memcpy(pData + _nodeAttributes[index].offset,
                   right_pData + right_nodeAttributes[i].offset,
                   right_nodeAttributes[i].attrLength);
        }
    }

    // assign the output vars
    nAttributes = _nNodeAttributes;
    tNodeAttributes = _nodeAttributes;

    // release
    if(right_pData != NULL)
    {
        delete[] right_pData;
        right_pData = NULL;
    }

    return rc;
}

// *************************
// Printer
// *************************

void QL_TreePlan::Padding (char ch, int n)
{
    int i;
    for(i=0;i<n;i++){
        putchar(ch);
    }
}


void QL_TreePlan::Print (char prefix ,int level)
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

RC QL_TreePlan::ComputeAttributesStructure(const std::vector<RelAttr> &selAttrs, int &nNodeAttributes, DataAttrInfo *&nodeAttributes, int &bufferSize)
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

RC QL_TreePlan::IsAttributeInList(const int nNodeAttributes, const DataAttrInfo *nodeAttributes, const DataAttrInfo &attribute, int &index)
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



