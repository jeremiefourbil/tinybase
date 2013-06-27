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

    cout << "QL Select\n";

    cout << "   nSelAttrs = " << nSelAttrs << "\n";
    DataAttrInfo *tInfos = new DataAttrInfo[nSelAttrs];
    int startOffset = 0;
    for (i = 0; i < nSelAttrs; i++)
    {
        cout << "   selAttrs[" << i << "]:" << selAttrs[i] << "\n";

        if((rc = _pSmm->GetAttributeStructure(selAttrs[i].relName, selAttrs[i].attrName, tInfos[i])))
            return rc;

        tInfos[i].offset = startOffset;
        startOffset += tInfos[i].attrLength;
    }


    cout << "   nRelations = " << nRelations << "\n";
    for (i = 0; i < nRelations; i++)
        cout << "   relations[" << i << "] " << relations[i] << "\n";

    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";




    // buil the tree query plan
    std::vector<RelAttr> vSelAttrs;
    std::vector<const char*> vRelations;
    std::vector<Condition> vConditions;
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
    Printer printer(tInfos, nSelAttrs);
    printer.PrintHeader(cout);

    for(int k=0; k<10000 && !rc; k++)
    {
        rc = _pTreePlan->PerformNodeOperation(nAttrInfos, tAttrInfos, pData);



        if(!rc && pData)
        {
            printer.Print(cout, pData);
            if(pData != NULL)
            {
                delete[] pData;
                pData = NULL;
            }
        }
    }

    if(rc == QL_EOF)
        rc = OK_RC;

    if(_pTreePlan != NULL)
    {
        delete _pTreePlan;
        _pTreePlan = NULL;
    }

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

