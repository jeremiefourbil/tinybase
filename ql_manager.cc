//
// ql_manager_stub.cc
//

// Note that for the SM component (HW3) the QL is actually a
// simple stub that will allow everything to compile.  Without
// a QL stub, we would need two parsers.

#include <cstdio>
#include <iostream>
#include <sys/times.h>
#include <sys/types.h>
#include <cassert>
#include <unistd.h>
#include "redbase.h"
#include "ql.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
#include "sm_internal.h"

using namespace std;

//
// QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm)
//
// Constructor for the QL Manager
//
QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm)
{
    _pSmm = &smm;
    _pIxm = &ixm;
    _pRmm = &rmm;
    _pTreePlan = NULL;
}

//
// QL_Manager::~QL_Manager()
//
// Destructor for the QL Manager
//
QL_Manager::~QL_Manager()
{
    if(_pTreePlan != NULL)
    {
        delete _pTreePlan;
        _pTreePlan = NULL;
    }
}

//
// Handle the select clause
//
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[])
{
    RC rc = OK_RC;
    int i;
    int nbTuples = 0;

    std::vector<RelAttr> vSelAttrs;
    std::vector<const char*> vRelations;
    std::vector<Condition> vConditions;

    cout << "nbAttr: " << nSelAttrs << endl;

    for(int i=0; i<nSelAttrs; i++)
    {
        vSelAttrs.push_back(selAttrs[i]);
    }
    for(int i=0; i<nRelations; i++)
    {
        vRelations.push_back(relations[i]);
    }
    for(int i=0; i<nConditions; i++)
    {
        vConditions.push_back(conditions[i]);
    }

    if((rc = PostParse(vSelAttrs, vRelations, vConditions)))
        return rc;

    cout << "QL Select\n";

    cout << "   nSelAttrs = " << vSelAttrs.size() << "\n";
    DataAttrInfo *tInfos = new DataAttrInfo[vSelAttrs.size()];
    int startOffset = 0;
    for (i = 0; i < vSelAttrs.size(); i++)
    {
        cout << "   selAttrs[" << i << "]:" << vSelAttrs[i] << "\n";

        if((rc = _pSmm->GetAttributeStructure(vSelAttrs[i].relName, vSelAttrs[i].attrName, tInfos[i])))
            return rc;

        tInfos[i].offset = startOffset;
        startOffset += tInfos[i].attrLength;
    }


    cout << "   nRelations = " << vRelations.size() << "\n";
    for (i = 0; i < vRelations.size(); i++)
        cout << "   relations[" << i << "] " << vRelations[i] << "\n";

    cout << "   nCondtions = " << vConditions.size() << "\n";
    for (i = 0; i < vConditions.size(); i++)
        cout << "   conditions[" << i << "]:" << vConditions[i] << "\n";




    // build the tree query plan
    _pTreePlan = new QL_TreePlan(_pSmm, _pIxm, _pRmm);
    if((rc = _pTreePlan->BuildFromQuery(vSelAttrs,vRelations,vConditions)))
        return rc;

    // print the query plan
    _pTreePlan->Print(' ',0);

    // get the information
    int nAttrInfos = 0;
    DataAttrInfo *tAttrInfos = NULL;
    char *pData = NULL;

    // prepare the printer
    Printer printer(tInfos, vSelAttrs.size());
    printer.PrintHeader(cout);

    while(!rc)
    {
        rc = _pTreePlan->GetNext(nAttrInfos, tAttrInfos, pData);

        if(!rc && pData)
        {
            //            cout << "start print" << endl;
            printer.Print(cout, pData);
            //            cout << "end print" << endl;
            if(pData != NULL)
            {
                delete[] pData;
                pData = NULL;
            }

            nbTuples++;
        }
    }

    cout << endl << nbTuples << " tuple(s)" << endl << endl;

    if(rc == QL_EOF)
        rc = OK_RC;

    cout << "delete tInfos" << endl;

    if(tInfos != NULL)
    {
        delete[] tInfos;
        tInfos = NULL;
    }

    cout << "Delete pTree Plan" << endl;

    if(_pTreePlan != NULL)
    {
        delete _pTreePlan;
        _pTreePlan = NULL;
    }

//    cout << "pplan" << endl;

//    for(unsigned int i=0; i<vSelAttrs.size(); i++)
//    {
//        if(vSelAttrs[i].relName != NULL)
//        {
//            delete vSelAttrs[i].relName;
//            vSelAttrs[i].relName = NULL;
//        }

//        if(vSelAttrs[i].attrName != NULL)
//        {
//            delete vSelAttrs[i].attrName;
//            vSelAttrs[i].attrName = NULL;
//        }
//    }

//    cout << "attr" << endl;

//    for(unsigned int i=0; i<vConditions.size(); i++)
//    {
//        if(vConditions[i].rhsAttr.relName != NULL)
//        {
//            delete vConditions[i].rhsAttr.relName;
//            vConditions[i].rhsAttr.relName = NULL;
//        }

//        if(vConditions[i].rhsAttr.attrName != NULL)
//        {
//            delete vConditions[i].rhsAttr.attrName;
//            vConditions[i].rhsAttr.attrName = NULL;
//        }

//        if(vConditions[i].lhsAttr.relName != NULL)
//        {
//            delete vConditions[i].lhsAttr.relName;
//            vConditions[i].lhsAttr.relName = NULL;
//        }

//        if(vConditions[i].lhsAttr.attrName != NULL)
//        {
//            delete vConditions[i].lhsAttr.attrName;
//            vConditions[i].lhsAttr.attrName = NULL;
//        }
//    }


    cout << "END OF QUERY : " << rc << endl;
    return rc;
}

//
// Insert the values into relName
//
RC QL_Manager::Insert(const char *relName,
                      int nValues, const Value values[])
{
    int i;

    cout << "QL Insert\n";

    cout << "   relName = " << relName << "\n";
    cout << "   nValues = " << nValues << "\n";
    for (i = 0; i < nValues; i++)
        cout << "   values[" << i << "]:" << values[i] << "\n";

    return 0;
}

//
// Delete from the relName all tuples that satisfy conditions
//
RC QL_Manager::Delete(const char *relName,
                      int nConditions, const Condition conditions[])
{
    int i;

    cout << "QL Delete\n";

    cout << "   relName = " << relName << "\n";
    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    return 0;
}


//
// Update from the relName all tuples that satisfy conditions
//
RC QL_Manager::Update(const char *relName,
                      const RelAttr &updAttr,
                      const int bIsValue,
                      const RelAttr &rhsRelAttr,
                      const Value &rhsValue,
                      int nConditions, const Condition conditions[])
{
    int i;

    cout << "QL Update\n";

    cout << "   relName = " << relName << "\n";
    cout << "   updAttr:" << updAttr << "\n";
    if (bIsValue)
        cout << "   rhs is value: " << rhsValue << "\n";
    else
        cout << "   rhs is attribute: " << rhsRelAttr << "\n";

    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    return 0;
}


RC QL_Manager::PostParse(std::vector<RelAttr> &vSelAttrs,
                         std::vector<const char*> &vRelations,
                         std::vector<Condition> &vConditions)
{
    RC rc = OK_RC;

    // select *
    if(vSelAttrs.size() == 1 && strcmp(vSelAttrs[0].attrName, "*") == 0)
    {
        vSelAttrs.pop_back();

        for(unsigned int i=0; i<vRelations.size(); i++)
        {
            int nAttr;
            DataAttrInfo *attr = NULL;

            cout << vRelations[i] << endl;

            if((rc = _pSmm->GetRelationStructure(vRelations[i], attr, nAttr)))
                return rc;

            for(int j=0; j<nAttr; j++)
            {
                cout << "j " << j << endl;
                RelAttr relAttr;
                relAttr.relName = new char[MAXSTRINGLEN];
                relAttr.attrName = new char[MAXSTRINGLEN];
                strcpy(relAttr.relName, vRelations[i]);
                strcpy(relAttr.attrName, attr[j].attrName);

                vSelAttrs.push_back(relAttr);
            }

            if(attr != NULL)
            {
                delete[] attr;
                attr = NULL;
            }
        }
    }

    //    if(vRelations.size() == 1)
    //    {
    //        for(unsigned int i=0; i<vSelAttrs.size(); i++)
    //        {
    //            if(vSelAttrs[i].relName == NULL)
    //            {
    //                vSelAttrs[i].relName = new char[MAXSTRINGLEN];
    //                strcpy(vSelAttrs[i].relName, vRelations[0]);
    //            }
    //        }

    //        for(unsigned int i=0; i<vConditions.size(); i++)
    //        {
    //            if(vConditions[i].lhsAttr.relName == NULL)
    //            {
    //                vConditions[i].lhsAttr.relName = new char[MAXSTRINGLEN];
    //                strcpy(vConditions[i].lhsAttr.relName, vRelations[0]);
    //            }

    //            if(vConditions[i].bRhsIsAttr && vConditions[i].rhsAttr.relName == NULL)
    //            {
    //                vConditions[i].rhsAttr.relName = new char[MAXSTRINGLEN];
    //                strcpy(vConditions[i].rhsAttr.relName, vRelations[0]);
    //            }
    //        }

    //    }

    return rc;
}


//
// void QL_PrintError(RC rc)
//
// This function will accept an Error code and output the appropriate
// error.
//
void QL_PrintError(RC rc)
{
    cout << "QL_PrintError\n   rc=" << rc << "\n";
}

