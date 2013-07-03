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

#include "ql_internal.h"



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

    // Tree plan
    QL_TreePlan *pTreePlan = new QL_TreePlan(_pSmm, _pIxm, _pRmm);

    // Fill the vectors
    std::vector<RelAttr> vSelAttrs;
    std::vector<const char*> vRelations;
    std::vector<Condition> vConditions;

    for(int i=0; i<nSelAttrs; i++)
        vSelAttrs.push_back(selAttrs[i]);

    for(int i=0; i<nRelations; i++)
        vRelations.push_back(relations[i]);

    for(int i=0; i<nConditions; i++)
        vConditions.push_back(conditions[i]);


    // Post check the data
    if((rc = PostCheck(vSelAttrs, vRelations, vConditions)))
        return rc;

    // Post parse the data
    if((rc = PostParse(vSelAttrs, vRelations, vConditions)))
        return rc;

    cout << "QL Select\n";

    // Get the header structure
    DataAttrInfo *tInfos = new DataAttrInfo[vSelAttrs.size()];
    int startOffset = 0;
    for (i = 0; i < vSelAttrs.size(); i++)
    {
        if((rc = _pSmm->GetAttributeStructure(vSelAttrs[i].relName, vSelAttrs[i].attrName, tInfos[i])))
            return rc;

        tInfos[i].offset = startOffset;
        startOffset += tInfos[i].attrLength;
    }


    // build the tree query plan
    if((rc = pTreePlan->BuildFromQuery(vSelAttrs,vRelations,vConditions)))
        return rc;

    // print the query plan
    if(bQueryPlans)
        pTreePlan->Print(' ', 0);

    // prepare the printer
    Printer printer(tInfos, vSelAttrs.size());
    printer.PrintHeader(cout);

    // prepare the loop vars
    int nAttrInfos = 0;
    DataAttrInfo *tAttrInfos = NULL;
    char *pData = NULL;

    // loop on the entries returned by the query plan
    while(!rc)
    {
        // get next entry
        rc = pTreePlan->GetNext(nAttrInfos, tAttrInfos, pData);

        if(!rc && pData)
        {
            // print the tuple
            printer.Print(cout, pData);

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

    if(tInfos != NULL)
    {
        delete[] tInfos;
        tInfos = NULL;
    }

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
    IX_IndexHandle *pIxh = NULL;

    // Tree plan
    QL_TreePlanDelete *pTreePlanDelete = new QL_TreePlanDelete(_pSmm, _pIxm, _pRmm);

    // Fill the vectors
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
        vConditions.push_back(conditions[i]);

    // Post parse the data
    if((rc = PostParse(vSelAttrs, vRelations, vConditions)))
        return rc;

    DataAttrInfo *tInfos = new DataAttrInfo[vSelAttrs.size()];
    int startOffset = 0;
    for (i = 0; i < vSelAttrs.size(); i++)
    {
        if((rc = _pSmm->GetAttributeStructure(vSelAttrs[i].relName, vSelAttrs[i].attrName, tInfos[i])))
            return rc;

        tInfos[i].offset = startOffset;
        startOffset += tInfos[i].attrLength;
    }

    // build the tree query plan
    if((rc = pTreePlanDelete->BuildFromQuery(vSelAttrs,vRelations,vConditions)))
        return rc;

    // print the query plan
    pTreePlanDelete->Print(' ',0);

    // prepare the printer
    Printer printer(tInfos, vSelAttrs.size());
    printer.PrintHeader(cout);

    // open the relation file
    if (rc = _pRmm->OpenFile(vRelations[0], fh))
       return rc;

    // if needed open index handler
//    for(int i=0; i<vSelAttrs.size(); i++)
//    {
//        if(tInfos[i].indexNo >= 0 && strcmp(tInfos[i].attrName, updatedAttribute.attrName) == 0)
//        {
//            pIxh = new IX_IndexHandle();
//            if((rc = _pIxm->OpenIndex(vRelations[0], tInfos[i].indexNo, *pIxh)))
//                return rc;
//        }
//    }

    // loop on the entries
    while(!rc)
    {
        RC rcDelete;
        RM_Record rec;
        RID rid;
        char *pData = NULL;

        // get next entry
        rc = pTreePlanDelete->GetNext(rec);

        if(!rc)
        {
            if((rcDelete = rec.GetData(pData)))
                return rcDelete;
            if((rcDelete = rec.GetRid(rid)))
                return rcDelete;

            // print the tuple
            printer.Print(cout, pData);
            nbTuples++;

            // delete from RM
            if((rcDelete = fh.DeleteRec(rid)))
                return rcDelete;

            // delete from IX
            if((pIxh != NULL))
            {

            }
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

    if(tInfos != NULL)
    {
        delete[] tInfos;
        tInfos = NULL;
    }

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
    DataAttrInfo updatedAttribute;

    // Tree query plan
    QL_TreePlanDelete *pTreePlanDelete = new QL_TreePlanDelete(_pSmm, _pIxm, _pRmm);

    // Fill the vectors
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
        vConditions.push_back(conditions[i]);

    // Post parse the data
    if((rc = PostParse(vSelAttrs, vRelations, vConditions)))
        return rc;

    DataAttrInfo *tInfos = new DataAttrInfo[vSelAttrs.size()];
    int startOffset = 0;
    for (i = 0; i < vSelAttrs.size(); i++)
    {
        if((rc = _pSmm->GetAttributeStructure(vSelAttrs[i].relName, vSelAttrs[i].attrName, tInfos[i])))
            return rc;

        tInfos[i].offset = startOffset;
        startOffset += tInfos[i].attrLength;
    }

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

    // loop on the entries
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

            // update the buffer
            memcpy(pData + updatedAttribute.offset, rhsValue.data, updatedAttribute.attrLength);

            // if it's a string, fill with '\0'
            if(updatedAttribute.attrType == STRING)
                fillString(pData + updatedAttribute.offset, updatedAttribute.attrLength);

            // print the tuple
            printer.Print(cout, pData);
            nbTuples++;

            // update from RM
            if((rcDelete = fh.UpdateRec(rec)))
                return rcDelete;

            // update from IX
            if((pIxh != NULL))
            {

            }
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

    if(tInfos != NULL)
    {
        delete[] tInfos;
        tInfos = NULL;
    }

    if(pTreePlanDelete != NULL)
    {
        delete pTreePlanDelete;
        pTreePlanDelete = NULL;
    }


    cout << "END OF QUERY : " << rc << endl;
    return rc;
}


RC QL_Manager::PostCheck(std::vector<RelAttr> &vSelAttrs,
                         std::vector<const char*> &vRelations,
                         std::vector<Condition> &vConditions)
{
    RC rc = OK_RC;

    // double relation
    for(unsigned int i=0; i<vRelations.size(); i++)
    {
        for(unsigned int j=i+1; j<vRelations.size(); j++)
        {
            if(strcmp(vRelations[i], vRelations[j]) == 0)
            {
                return QL_TWICE_RELATION;
            }
        }
    }

    // attribute's relation does appear in relations list?
    for(unsigned int i=0; i<vSelAttrs.size(); i++)
    {
        // not null relation name
        if(vSelAttrs[i].relName != NULL)
        {
            bool inRelationList = false;
            for(unsigned int j=0; j<vRelations.size(); j++)
            {
                if(strcmp(vSelAttrs[i].relName, vRelations[j]) == 0)
                {
                    inRelationList = true;
                    break;
                }
            }

            if(!inRelationList)
                return QL_NO_MATCHING_RELATION;
            else
            {
                // check if the attribute exists in the relation
                int nAttr;
                DataAttrInfo *attr = NULL;
                bool inAttributeList = false;

                if((rc = _pSmm->GetRelationStructure(vSelAttrs[i].relName, attr, nAttr)))
                    return rc;

                int j=0;
                for(j=0; j<nAttr; j++)
                {
                    if(strcmp(attr[j].attrName, vSelAttrs[i].attrName) == 0)
                    {
                        inAttributeList = true;
                        break;
                    }
                }

                if(attr != NULL)
                {
                    delete[] attr;
                    attr = NULL;
                }

                if(!inAttributeList)
                    return QL_NO_MATCHING_RELATION;
            }
        }
        // null relation name
        else
        {
            // should be only one relation in the list
            if(vRelations.size() != 1)
                return QL_UNDEFINED_RELATION;

            // check if the attribute exists in the relation
            int nAttr;
            DataAttrInfo *attr = NULL;
            bool inAttributeList = false;

            if((rc = _pSmm->GetRelationStructure(vRelations[0], attr, nAttr)))
                return rc;

            int j=0;
            for(j=0; j<nAttr; j++)
            {
                if(strcmp(attr[j].attrName, vSelAttrs[i].attrName) == 0)
                {
                    inAttributeList = true;
                    break;
                }
            }

            if(!inAttributeList)
                return QL_NO_MATCHING_RELATION;
            else
            {
//                cout << j << endl;
//                cout << "copy (" << attr[j].attrLength << ")" << endl;
                vSelAttrs[i].relName = new char[attr[j].attrLength];
//                cout << "from: " << vRelations[0] << " / to: " << vSelAttrs[i].relName << endl;
                strcpy(vSelAttrs[i].relName, vRelations[0]);
//                cout << "copied" << endl;
                fillString(vSelAttrs[i].relName, attr[j].attrLength);
//                cout << vSelAttrs[i].relName << endl;
//                cout << "end copy" << endl;
            }

            if(attr != NULL)
            {
                delete[] attr;
                attr = NULL;
            }
        }
    }


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

//            cout << vRelations[i] << endl;

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

