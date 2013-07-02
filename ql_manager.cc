//
// ql_manager_stub.cc
//

// Note that for the SM component (HW3) the QL is actually a
// simple stub that will allow everything to compile.  Without
// a QL stub, we would need two parsers.

#include "ql_treeplan.h"
#include "ql_treeplandelete.h"

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
}

//
// QL_Manager::~QL_Manager()
//
// Destructor for the QL Manager
//
QL_Manager::~QL_Manager()
{

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

    QL_TreePlan *pTreePlan = new QL_TreePlan(_pSmm, _pIxm, _pRmm);

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
    if((rc = pTreePlan->BuildFromQuery(vSelAttrs,vRelations,vConditions)))
        return rc;

    // print the query plan
    pTreePlan->Print(' ',0);

    // get the information
    int nAttrInfos = 0;
    DataAttrInfo *tAttrInfos = NULL;
    char *pData = NULL;

    // prepare the printer
    Printer printer(tInfos, vSelAttrs.size());
    printer.PrintHeader(cout);

    while(!rc)
    {
        rc = pTreePlan->GetNext(nAttrInfos, tAttrInfos, pData);

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

    if(pTreePlan != NULL)
    {
        delete pTreePlan;
        pTreePlan = NULL;
    }


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
    RC rc = OK_RC;
    int i;
    int nbTuples = 0;
    RelAttr relAttr;
    RM_FileHandle fh;

    QL_TreePlanDelete *pTreePlanDelete = new QL_TreePlanDelete(_pSmm, _pIxm, _pRmm);

    std::vector<RelAttr> vSelAttrs;
    std::vector<const char*> vRelations;
    std::vector<Condition> vConditions;

    cout << "QL Delete\n";

    relAttr.relName = new char[MAXSTRINGLEN];
    relAttr.attrName = new char[MAXSTRINGLEN];
    strcpy(relAttr.relName, relName);
    strcpy(relAttr.attrName, "*");
    vSelAttrs.push_back(relAttr);

    vRelations.push_back(relName);

    for(int i=0; i<nConditions; i++)
    {
        vConditions.push_back(conditions[i]);
    }

    if((rc = PostParse(vSelAttrs, vRelations, vConditions)))
        return rc;

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
    if((rc = pTreePlanDelete->BuildFromQuery(vSelAttrs,vRelations,vConditions)))
        return rc;

    // print the query plan
    pTreePlanDelete->Print(' ',0);

    // prepare the printer
    Printer printer(tInfos, vSelAttrs.size());
    printer.PrintHeader(cout);



    if (rc = _pRmm->OpenFile(vRelations[0], fh))
       return rc;


    while(!rc)
    {
        RC rcDelete;
        RM_Record rec;
        RID rid;
        char *pData = NULL;

        rc = pTreePlanDelete->GetNext(rec);

        if(!rc)
        {
            if((rcDelete = rec.GetData(pData)))
                return rcDelete;
            if((rcDelete = rec.GetRid(rid)))
                return rcDelete;

            printer.Print(cout, pData);
            nbTuples++;

            // delete
            if((rcDelete = fh.DeleteRec(rid)))
                return rcDelete;

            // -------------------
            // NB: STILL HAVE TO DELETE FROM INDEX
            // TO BE DONE
            // -------------------

        }
    }

    cout << endl << nbTuples << " tuple(s)" << endl << endl;

    if (rc = _pRmm->CloseFile(fh))
       return rc;

    if(rc == QL_EOF)
        rc = OK_RC;

    cout << "delete tInfos" << endl;

    if(tInfos != NULL)
    {
        delete[] tInfos;
        tInfos = NULL;
    }

    cout << "Delete pTree Plan" << endl;

    if(pTreePlanDelete != NULL)
    {
        delete pTreePlanDelete;
        pTreePlanDelete = NULL;
    }


    cout << "END OF QUERY : " << rc << endl;
    return rc;
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
    RC rc = OK_RC;
    int i;
    int nbTuples = 0;
    RelAttr relAttr;
    RM_FileHandle fh;
    IX_IndexHandle *pIxh = NULL;

    QL_TreePlanDelete *pTreePlanDelete = new QL_TreePlanDelete(_pSmm, _pIxm, _pRmm);

    DataAttrInfo updatedAttribute;

    std::vector<RelAttr> vSelAttrs;
    std::vector<const char*> vRelations;
    std::vector<Condition> vConditions;

    cout << "QL Update\n";

    relAttr.relName = new char[MAXSTRINGLEN];
    relAttr.attrName = new char[MAXSTRINGLEN];
    strcpy(relAttr.relName, relName);
    strcpy(relAttr.attrName, "*");
    vSelAttrs.push_back(relAttr);

    vRelations.push_back(relName);

    for(int i=0; i<nConditions; i++)
    {
        vConditions.push_back(conditions[i]);
    }

    if((rc = PostParse(vSelAttrs, vRelations, vConditions)))
        return rc;

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
    if((rc = pTreePlanDelete->BuildFromQuery(vSelAttrs,vRelations,vConditions)))
        return rc;

    // print the query plan
    pTreePlanDelete->Print(' ',0);

    // prepare the printer
    Printer printer(tInfos, vSelAttrs.size());
    printer.PrintHeader(cout);

    // prepare the updatedAttribute
    if((rc = _pSmm->GetAttributeStructure(relName, updAttr.attrName, updatedAttribute)))
        return rc;


    // open RM file handler
    if((rc = _pRmm->OpenFile(vRelations[0], fh)))
       return rc;

    // if needed open index handler
    for(int i=0; i<vSelAttrs.size(); i++)
    {
        if(tInfos[i].indexNo >= 0 && strcmp(tInfos[i].attrName, updatedAttribute.attrName) == 0)
        {
            pIxh = new IX_IndexHandle();
            if((rc = _pIxm->OpenIndex(vRelations[0], tInfos[i].indexNo, *pIxh)))
                return rc;
        }
    }


    while(!rc)
    {
        RC rcDelete;
        RM_Record rec;
        char *pData = NULL;

        rc = pTreePlanDelete->GetNext(rec);

        if(!rc)
        {
            if((rcDelete = rec.GetData(pData)))
                return rcDelete;

            memcpy(pData + updatedAttribute.offset, rhsValue.data, updatedAttribute.attrLength);

            printer.Print(cout, pData);
            nbTuples++;

            // update
            if((rcDelete = fh.UpdateRec(rec)))
                return rcDelete;

            if((pIxh != NULL))
            {

            }

            // -------------------
            // NB: STILL HAVE TO DELETE FROM INDEX
            // TO BE DONE
            // -------------------

        }
    }

    cout << endl << nbTuples << " tuple(s)" << endl << endl;

    if(pIxh != NULL)
    {
        if((rc = _pIxm->CloseIndex(*pIxh)))
            return rc;
    }

    if(pIxh != NULL)
    {
        delete pIxh;
        pIxh = NULL;
    }


    if (rc = _pRmm->CloseFile(fh))
       return rc;

    if(rc == QL_EOF)
        rc = OK_RC;

    cout << "delete tInfos" << endl;

    if(tInfos != NULL)
    {
        delete[] tInfos;
        tInfos = NULL;
    }

    cout << "Delete pTree Plan" << endl;

    if(pTreePlanDelete != NULL)
    {
        delete pTreePlanDelete;
        pTreePlanDelete = NULL;
    }


    cout << "END OF QUERY : " << rc << endl;
    return rc;



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

