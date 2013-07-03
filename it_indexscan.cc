#include "it_indexscan.h"

#include "ql_internal.h"

#include <iostream>

using namespace std;

IT_IndexScan::IT_IndexScan(RM_Manager *rmm, IX_Manager *ixm, SM_Manager *smm,
                           const char* relName, CompOp scanOp, DataAttrInfo &dAttr, void *value):
    _pRmm(rmm), _pIxm(ixm), _pSmm(smm),
    _relName(relName), _scanOp(scanOp), _iAttr(dAttr)
{
    _bIsOpen = false;

    if(value == NULL)
        _value = NULL;
    else
    {
        switch(_iAttr.attrType)
        {
        case INT:
            _value = new int();
            memcpy(_value, value, sizeof(int));
            break;
        case FLOAT:
            _value = new float();
            memcpy(_value, value, sizeof(float));
            break;
        case STRING:
            _value = new char[_iAttr.attrLength];
            strcpy((char*)_value, (char*) value);
            fillString((char*)_value, _iAttr.attrLength);
            break;
        default:
            _value = NULL;
            break;
        }
    }
}

IT_IndexScan::~IT_IndexScan()
{
    switch(_iAttr.attrType)
    {
    case INT:
        delete ((int *) _value);
        break;
    case FLOAT:
        delete ((float *) _value);
        break;
    case STRING:
        delete[] ((char *) _value);
        break;
    default:
        _value = NULL;
        break;
    }
}

RC IT_IndexScan::Open()
{
  if(_bIsOpen)
  {
    return IX_ALREADY_OPEN;
  }

//  cout << "IXSTARTS" << endl;

  RC rc = OK_RC;

  if((rc = _pRmm->OpenFile(_relName, _rmfh)))
    return rc;

  if((rc = _pIxm->OpenIndex(_relName, _iAttr.indexNo, _ixih)))
    return rc;

  if((rc = _ixis.OpenScan(_ixih, _scanOp, _value)))
    return rc;

  if((rc = _pSmm->GetRelationStructure(_relName, _dAttr, _nAttr)))
      return rc;


  _bIsOpen = true;

  return rc;
}

RC IT_IndexScan::GetNext(int &nAttr, DataAttrInfo *&pAttr, char *&pData)
{
  RC rc;

  RM_Record rec;

  int recordSize;
  char *pRecData;

  if(!_bIsOpen)
  {
    return QL_ITERATOR_NOT_OPENED;
  }

  RID rid;

  rc = _ixis.GetNextEntry(rid);
  if (rc != IX_EOF && rc != 0)
    return rc;

  if (rc == IX_EOF)
    return QL_EOF;

  rc = _rmfh.GetRec(rid, rec);
  if (rc != 0 ) return rc;

  if((rc = rec.GetRecordSize(recordSize)))
      return rc;

  if((rc = rec.GetData(pRecData)))
      return rc;

  pData = new char[recordSize];
  memcpy(pData, pRecData, recordSize);

  nAttr = _nAttr;
  pAttr = _dAttr;

  return rc;
}

RC IT_IndexScan::GetNext(RM_Record &oRecord)
{
  RC rc;

  if(!_bIsOpen)
  {
    return QL_ITERATOR_NOT_OPENED;
  }

  RID rid;

  rc = _ixis.GetNextEntry(rid);
  if (rc != IX_EOF && rc != 0)
    return rc;

  if (rc == IX_EOF)
    return QL_EOF;

  rc = _rmfh.GetRec(rid, oRecord);
  if (rc != 0 ) return rc;

  return rc;
}

RC IT_IndexScan::Close()
{
  RC rc = OK_RC;

  if(!_bIsOpen)
  {
    return QL_ITERATOR_NOT_OPENED;
  }

  if((rc = _ixis.CloseScan()))
    return rc;

  if((rc = _pIxm->CloseIndex(_ixih)))
      return rc;

  if((rc = _pRmm->CloseFile(_rmfh)))
    return rc;

  _bIsOpen = false;
  return rc;
}
