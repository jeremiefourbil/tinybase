//
// ql.h
//   Query Language Component Interface
//

// This file only gives the stub for the QL component

#ifndef QL_H
#define QL_H

#include <stdlib.h>
#include <string.h>
#include <vector>

#include "redbase.h"
#include "parser.h"
#include "rm.h"
#include "ix.h"
#include "sm.h"

//
// QL_Manager: query language (DML)
//
class QL_Manager {
public:
    QL_Manager (SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm);
    ~QL_Manager();                       // Destructor

    RC Select  (int nSelAttrs,           // # attrs in select clause
        const RelAttr selAttrs[],        // attrs in select clause
        int   nRelations,                // # relations in from clause
        const char * const relations[],  // relations in from clause
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

    RC Insert  (const char *relName,     // relation to insert into
        int   nValues,                   // # values
        const Value values[]);           // values to insert

    RC Delete  (const char *relName,     // relation to delete from
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

    RC Update  (const char *relName,     // relation to update
        const RelAttr &updAttr,          // attribute to update
        const int bIsValue,              // 1 if RHS is a value, 0 if attribute
        const RelAttr &rhsRelAttr,       // attr on RHS to set LHS equal to
        const Value &rhsValue,           // or value to set attr equal to
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

private:
    RC PostCheck(std::vector<RelAttr> &vSelAttrs,
                             std::vector<const char*> &vRelations,
                             std::vector<Condition> &vConditions);

    RC PostConditionsCheck(std::vector<RelAttr> &vSelAttrs,
                             std::vector<const char*> &vRelations,
                             std::vector<Condition> &vConditions);

    RC PostParse(std::vector<RelAttr> &vSelAttrs,
                          std::vector<const char*> &vRelations,
                          std::vector<Condition> &vConditions);
    SM_Manager *_pSmm;
    IX_Manager *_pIxm;
    RM_Manager *_pRmm;
};

//
// Print-error function
//
void QL_PrintError(RC rc);

#define QL_UNKNOWN_TYPE               (START_QL_ERR - 0)
#define QL_ITERATOR_NOT_OPENED        (START_QL_ERR - 1)
#define QL_NULL_CHILD                 (START_QL_ERR - 2)
#define QL_NO_RELATION                (START_QL_ERR - 3)
#define QL_LASTERROR                  QL_NULL_CHILD

#define QL_TWICE_RELATION             (START_QL_WARN + 0)
#define QL_NO_MATCHING_RELATION       (START_QL_WARN + 1)
#define QL_UNDEFINED_RELATION         (START_QL_WARN + 2)
#define QL_BAD_JOIN                   (START_QL_WARN + 3)
#define QL_NON_TREATED_CASE           (START_QL_WARN + 4)
#define QL_LASTWARN                   QL_NON_TREATED_CASE

#define QL_EOF                        PF_EOF


#endif
