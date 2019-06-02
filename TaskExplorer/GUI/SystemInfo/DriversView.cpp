#include "stdafx.h"
#include "DriversView.h"

#include "../API/Windows/ProcessHacker.h"

CDriversView::CDriversView()
{
	NTSTATUS status;

    PRTL_PROCESS_MODULES ModuleInfo;
 
    ModuleInfo=(PRTL_PROCESS_MODULES)VirtualAlloc(NULL,1024*1024,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); // Allocate memory for the module list
 
    if(!ModuleInfo)
    {
		qDebug() << "\nUnable to allocate memory for module list " << GetLastError();
        return;
    }
 
    if(!NT_SUCCESS(status=NtQuerySystemInformation(SystemModuleInformation, ModuleInfo,1024*1024,NULL))) 
    {
        qDebug() << "Error: Unable to query module list " << status;
 
        VirtualFree(ModuleInfo,0,MEM_RELEASE);
        return;
    }
 
    for(ULONG i=0;i<ModuleInfo->NumberOfModules;i++)
    {
		qDebug() << QString::fromLatin1((char*)ModuleInfo->Modules[i].FullPathName);
        /*printf("\n*****************************************************\n");
        printf("\nImage base: %#x\n",ModuleInfo->Modules[i].ImageBase);
        printf("\nImage name: %s\n",ModuleInfo->Modules[i].FullPathName+ModuleInfo->Modules[i].OffsetToFileName);
        printf("\nImage full path: %s\n",ModuleInfo->Modules[i].FullPathName);
        printf("\nImage size: %d\n",ModuleInfo->Modules[i].ImageSize);
        printf("\n*****************************************************\n");*/
    }
 
    VirtualFree(ModuleInfo,0,MEM_RELEASE);
}


CDriversView::~CDriversView()
{

}
