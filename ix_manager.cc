#include "ix_internal.h"

#include <sstream>
#include <iostream>

using namespace std;

IX_Manager::IX_Manager(PF_Manager &pfm) : pPfm(&pfm)
{

}

IX_Manager::~IX_Manager()
{

}

// Create a new Index
RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
               AttrType attrType, int attrLength)
{
    RC rc;
    PF_FileHandle pfFileHandle;
    PF_PageHandle pageHandle;
    char* pData;
    IX_FileHdr *fileHdr;

    const char *realFileName = GenerateFileName(fileName, indexNo);

    // Sanity Check: recordSize should not be too large (or small)
    // Note that PF_Manager::CreateFile() will take care of fileName
    if (sizeof(IX_FileHdr) >= PF_PAGE_SIZE)
       // Test: invalid creation
       return (IX_INSUFFISANT_PAGE_SIZE);

    // Call PF_Manager::CreateFile()
    if (rc = pPfm->CreateFile(realFileName))
       // Test: existing realFileName, wrong permission
       goto err_return;

    // Call PF_Manager::OpenFile()
    if (rc = pPfm->OpenFile(realFileName, pfFileHandle))
       // Should not happen
       goto err_destroy;

    // Allocate the header page (pageNum must be 0)
    if (rc = pfFileHandle.AllocatePage(pageHandle))
       // Should not happen
       goto err_close;

    // Get a pointer where header information will be written
    if (rc = pageHandle.GetData(pData))
       // Should not happen
       goto err_unpin;

    // Write the file header (to the buffer pool)
    fileHdr = (IX_FileHdr *) pData;
    fileHdr->rootNum = IX_EMPTY;
    fileHdr->attrLength=attrLength;
    fileHdr->attrType=attrType;
    fileHdr->height = 0;
    fileHdr->firstLeafNum = IX_EMPTY;

    // Mark the header page as dirty
    if (rc = pfFileHandle.MarkDirty(IX_HEADER_PAGE_NUM))
       // Should not happen
       goto err_unpin;

    // Unpin the header page
    if (rc = pfFileHandle.UnpinPage(IX_HEADER_PAGE_NUM))
       // Should not happen
       goto err_close;

    // Call PF_Manager::CloseFile()
    if (rc = pPfm->CloseFile(pfFileHandle))
       // Should not happen
       goto err_destroy;

    // Return ok
    return (0);

    // Recover from inconsistent state due to unexpected error
 err_unpin:
    pfFileHandle.UnpinPage(IX_HEADER_PAGE_NUM);
 err_close:
    pPfm->CloseFile(pfFileHandle);
 err_destroy:
    pPfm->DestroyFile(realFileName);
 err_return:
    // Return error
    return (rc);
}

// Destroy and Index
RC IX_Manager::DestroyIndex(const char *fileName, int indexNo)
{
    // !!!!!!!!!!!!!!!!
    // ToDo: check if needed to recursively destroy nodes...
    // !!!!!!!!!!!!!!!!


    RC rc;
    const char *realFileName = GenerateFileName(fileName, indexNo);

    // Call PF_Manager::DestroyFile()
    if (rc = pPfm->DestroyFile(realFileName))
       // Test: non-existing fileName, wrong permission
       goto err_return;

    // Return ok
    return (0);

 err_return:
    // Return error
    return (rc);
}

// Open an Index
RC IX_Manager::OpenIndex(const char *fileName, int indexNo,
             IX_IndexHandle &indexHandle)
{
    RC rc;
    PF_PageHandle pageHandle;
    char* pData;

    const char *realFileName = GenerateFileName(fileName, indexNo);

    // Call PF_Manager::OpenFile()
    if (rc = pPfm->OpenFile(realFileName, indexHandle.pfFileHandle))
       // Test: non-existing realFileName, opened fileHandle
       goto err_return;

    // Get the header page
    if (rc = indexHandle.pfFileHandle.GetFirstPage(pageHandle))
       // Test: invalid file
       goto err_close;

    // Get a pointer where header information resides
    if (rc = pageHandle.GetData(pData))
       // Should not happen
       goto err_unpin;

    // Read the file header (from the buffer pool to RM_FileHandle)
    memcpy(&indexHandle.fileHdr, pData, sizeof(indexHandle.fileHdr));

    // Unpin the header page
    if (rc = indexHandle.pfFileHandle.UnpinPage(IX_HEADER_PAGE_NUM))
       // Should not happen
       goto err_close;

    // TODO: cannot guarantee the validity of file header at this time

    // Set file header to be not changed
    indexHandle.bHdrChanged = FALSE;

    // Return ok
    return (0);

    // Recover from inconsistent state due to unexpected error
    err_unpin:
    indexHandle.pfFileHandle.UnpinPage(IX_HEADER_PAGE_NUM);
    err_close:
    pPfm->CloseFile(indexHandle.pfFileHandle);
    err_return:
    // Return error
    return (rc);
}

// Close an Index
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle)
{
    RC rc;

    PF_PageHandle pageHandle;
    char* pData;

    // Get the header page
    if (rc = indexHandle.pfFileHandle.GetFirstPage(pageHandle))
        // Test: unopened(closed) indexHandle, invalid file
        goto err_return;

    // Get a pointer where header information will be written
    if (rc = pageHandle.GetData(pData))
        // Should not happen
        goto err_unpin;

    // Write the file header (to the buffer pool)
    memcpy(pData, &indexHandle.fileHdr, sizeof(indexHandle.fileHdr));

    // Mark the header page as dirty
    if (rc = indexHandle.pfFileHandle.MarkDirty(IX_HEADER_PAGE_NUM))
        // Should not happen
        goto err_unpin;

    // Unpin the header page
    if (rc = indexHandle.pfFileHandle.UnpinPage(IX_HEADER_PAGE_NUM))
        // Should not happen
        goto err_return;

    // Set file header to be not changed
    indexHandle.bHdrChanged = FALSE;

    indexHandle.ForcePages();

    // Call PF_Manager::CloseFile()
    if (rc = pPfm->CloseFile(indexHandle.pfFileHandle))
        goto err_return;


    // Return ok
    return (0);

    // Recover from inconsistent state due to unexpected error
err_unpin:
    indexHandle.pfFileHandle.UnpinPage(IX_HEADER_PAGE_NUM);
err_return:
    // Return error
    return (rc);
}


const char* IX_Manager::GenerateFileName(const char *fileName, int indexNo)
{
    std::string outputFileName(fileName);
    outputFileName.append(".");

    std::ostringstream oss;
    oss << indexNo;

    outputFileName.append(oss.str());

    return outputFileName.c_str();
}
