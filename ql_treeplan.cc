#include "ql_treeplan.h"

#include "ql_internal.h"

#include <stdio.h>
#include <cstddef>

#include <string>
#include <map>

using namespace std;

// *************************
// Constructor & destructor
// *************************

QL_TreePlan::QL_TreePlan(SM_Manager *ipSmm)
{
    _pSmm = ipSmm;
    _pLc = NULL;
    _pRc = NULL;
    _nodeAttributes = NULL;
    _operationAttributes = NULL;
}

QL_TreePlan::~QL_TreePlan()
{
    _pSmm = NULL;

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
        delete[] _nodeAttributes;
        _nodeAttributes = NULL;
    }

    if(_operationAttributes != NULL)
    {
        delete[] _operationAttributes;
        _operationAttributes = NULL;
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
        cout << "JOIN detected" << endl;
        if((rc = BuildFromJoin(selAttrs, relations, conditions)))
            goto err_return;
    }
    else
    {
        cout << "SINGLE RELATION detected" << endl;
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
        // create the new nodes
        QL_TreePlan *pProjector = new QL_TreePlan(_pSmm);
        QL_TreePlan *pSelect = new QL_TreePlan(_pSmm);
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
            pComparison = new QL_TreePlan(_pSmm);
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
            goto err_return;

        // build the selection node
        if((rc = pSelect->BuildFromSelect(selAttrs, relations, vConditions)))
            goto err_return;
    }
    else
    {
        QL_TreePlan *pSelect = new QL_TreePlan(_pSmm);

        this->SetNodeOperation(PROJECTION);
        pSelect->SetNodeOperation(SELECT);

        this->SetLeftChild(pSelect);
    }



    return rc;

err_return:
    return rc;
}

RC QL_TreePlan::BuildFromComparison(const std::vector<RelAttr> &selAttrs,
                                    const std::vector<const char*> &relations,
                                    const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    _nodeOperation = COMPARISON;

    if((rc = ComputeAttributesStructure(selAttrs, _nNodeAttributes, _nodeAttributes)))
        return rc;

    return rc;

err_return:
    return rc;
}

RC QL_TreePlan::BuildFromProjection(const std::vector<RelAttr> &selAttrs,
                                    const std::vector<const char*> &relations,
                                    const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    _nodeOperation = PROJECTION;

    if((rc = ComputeAttributesStructure(selAttrs, _nNodeAttributes, _nodeAttributes)))
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

    _nodeOperation = JOIN;
    _pLc = new QL_TreePlan(_pSmm);
    _pRc = new QL_TreePlan(_pSmm);

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

    return rc;

err_return:
    return rc;
}

RC QL_TreePlan::BuildFromSelect(const std::vector<RelAttr> &selAttrs,
                               const std::vector<const char*> &relations,
                               const std::vector<Condition> &conditions)
{
    RC rc = OK_RC;

    _nodeOperation = SELECT;

    if((rc = ComputeAttributesStructure(selAttrs, _nNodeAttributes, _nodeAttributes)))
        return rc;

    return rc;

err_return:
    return rc;
}

// *************************
// Operators
// *************************

RC QL_TreePlan::PerformNodeOperation(int nAttributes, DataAttrInfo *tNodeAttributes,void * pData)
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

RC QL_TreePlan::ComputeAttributesStructure(const std::vector<RelAttr> &selAttrs, int &nNodeAttributes, DataAttrInfo *&nodeAttributes)
{
    RC rc = OK_RC;

    nNodeAttributes = selAttrs.size();
    nodeAttributes = new DataAttrInfo[nNodeAttributes];

    for(unsigned int i=0; i<nNodeAttributes; i++)
    {
        if((rc = _pSmm->GetAttributeStructure(selAttrs[i].relName, selAttrs[i].attrName, nodeAttributes[i])))
            return rc;
    }

    return rc;
}



