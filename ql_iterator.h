#ifndef QL_ITERATOR
#define QL_ITERATOR

class QL_Iterator {
  public: 
    QL_Iterator():bIsOpen(false){};
    virtual ~QL_Iterator(){};

    virtual RC Open() = 0;
    virtual RC GetNext() = 0;
    virtual RC Close() = 0;
  private:
    boolean bIsOpen;
};

#endif
