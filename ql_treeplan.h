#ifndef QL_TREEPLAN_H
#define QL_TREEPLAN_H

class QL_TreePlan
{
public:
    enum NodeOperation{
        UNION,
        COMPARISON,
        PROJECTION,
        JOIN
    };

    QL_TreePlan();
    ~QL_TreePlan();

private:
    QL_TreePlan *_pLc;
    QL_TreePlan *_pRc;

};

#endif // QL_TREEPLAN_H
