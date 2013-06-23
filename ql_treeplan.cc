#include "ql_treeplan.h"

#include "ql_internal.h"

#include <stdio.h>
#include <cstddef>

#include <string>
#include <map>

using namespace std;

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
        _nodeOperation = JOIN;
        _pLc = new QL_TreePlan();
        _pRc = new QL_TreePlan();

        std::vector<RelAttr> left_selAttrs;
        std::vector<const char*> left_relations;
        std::vector<Condition> left_conditions;

        std::vector<RelAttr> right_selAttrs;
        std::vector<const char*> right_relations;
        std::vector<Condition> right_conditions;


        // find the relations that appears once
        std::map<string, int> relationsNb;
        for(unsigned int i=0; i<relations.size(); i++)
        {
            relationsNb.insert(std::pair<string, int>(relations[i], 0));
        }

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

        cout << "Begin hash map traversal" << endl;

        std::map<string,int>::iterator it;
        // show content:
        for (it=relationsNb.begin(); it!=relationsNb.end(); ++it)
        {
            cout << it->first << ": " <<  it->second << endl;
            if(it->second == 1)
            {
                break;
            }
        }

        cout << "End hash map traversal" << endl;

        cout << it->second << endl;

        cout << "Begin conditions" << endl;

        for(unsigned int i=0;i<conditions.size();i++)
        {
            string str(conditions[i].lhsAttr.relName);
            std::size_t found = str.find_first_of(".");
            if(str.substr(0, found).compare(it->first)==0)
            {
                if(!conditions[i].bRhsIsAttr)
                {
                    right_conditions.push_back(conditions[i]);
                }
                else
                {
                    _operationAttributes = new DataAttrInfo[2];
                    _nOperationAttributes = 2;
                }
            }
            else
            {
                if(!conditions[i].bRhsIsAttr)
                {
                    left_conditions.push_back(conditions[i]);
                }
                else
                {
                    str.assign(conditions[i].rhsAttr.relName);
                    found = str.find_first_of(".");
                    if(str.substr(0, found).compare(it->first)==0)
                    {

                    }
                    else
                    {
                        left_conditions.push_back(conditions[i]);
                    }
                }
            }
        }

        cout << "End conditions" << endl;
        cout << "Begin relations" << endl;

        // relations
        for(unsigned int i=0;i<relations.size();i++)
        {
            if(strcmp(it->first.c_str(),relations[i])==0)
            {
                right_relations.push_back(relations[i]);
            }
            else
            {
                left_relations.push_back(relations[i]);
            }
        }

        cout << "End relations" << endl;
        cout << "Begin attributes" << endl;

        // attributes
        for(unsigned int i=0;i<selAttrs.size();i++)
        {
            cout << i << ": " << it->first.c_str() << endl;
            cout << i << ": " << selAttrs[i].relName << endl;
            if(strcmp(it->first.c_str(),selAttrs[i].relName)==0)
            {
                cout << "Right" << endl;
                right_selAttrs.push_back(selAttrs[i]);
            }
            else
            {
                cout << "Left" << endl;
                left_selAttrs.push_back(selAttrs[i]);
            }
            cout << i << ": End" << endl;
        }

        cout << "End attributes" << endl;

        cout << "End JOIN for node" << endl;

        cout << "Child Left: " << endl;
        _pLc->BuildFromQuery(left_selAttrs, left_relations, left_conditions);
        cout << "End Child Left: " << endl;

        cout << "Child right: " << endl;
        _pRc->BuildFromQuery(right_selAttrs, right_relations, right_conditions);
        cout << "End child right: " << endl;
    }
    else
    {
        cout << "SELECT detected" << endl;
        _nodeOperation = SELECT;
    }

    return rc;
}

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

// Tree Printer

void QL_TreePlan::Padding (char ch, int n){
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
