using System;

namespace SnapPlus.Models.Erosion.Interfaces
{
    public interface IRusle2
    {
        bool OpenDatabase(string path);
        bool CloseDatabase();
        IntPtr FilesOpen(string fileNameInDatabase, int flags = 0);
        // Close the open R2 filesystem
        bool FilesCloseAll();
        bool FileClose(IntPtr filePtr);
        // Close and reopen the profile
        bool ProfileFlush();
        public IntPtr GetFileSysHandle();
        (IntPtr, string) GetProfile();
        bool EngineRun(); // uses internal engine pointer
        bool EngineGetAutorun(); // uses internal engine pointer
        bool EngineSetAutorun(bool autoRun); // uses internal engine pointer
        string GetTitle(string key);
        IntPtr ProfileGetAttr(string attrName);
        IntPtr FileGetAttr(IntPtr fileHandle, string attrName);
        int FileGetAttrSize(IntPtr fileHandle, string attrName);
        int GetAttrDimSize(string attrName);
        string FileGetAttrValue(IntPtr fileHandle, string attrName, int index);
        //18Aug21 JWW Get GDB Climate precip in inches
        string FileGetAttrValueAux(IntPtr fileHandle, string attrName, int index, string attrUnits);
        int FileSetAttrValue(IntPtr fileHandle, string attrName, string value, int index);
        bool ProfileOpen(string profileName = @"profiles\default");
        int ProfileSetAttrSize(string attrName, int newSize);
        int FileSetAttrSize(IntPtr fileHandle, string attrName, int newSize);
        int ProfileGetAttrSize(string attrName);
        string ProfileGetAttrValue(string attrName, int index);
        int ProfileSetAttrValue(string attrName, string value, int index);
        string GetPropertyStr(int propertyId);
        string GetLastError();
        void ClearLastError();
        int GetScienceVersion();
        bool FileSaveAsXml(IntPtr fileHandle, string filePath);
        int FilesGetCount(IntPtr fileSystemPtr);
        IntPtr FilesGetItem(IntPtr fileSystemPtr, int index);
        string FileGetFullname(IntPtr fileHandle);



        //*  Offered by api-rome.h
        //   ROME_API RT_BOOL		RomeFileClose(RT_FileObj* pFile);
        //   ROME_API RT_Attr*		RomeFileGetAttr(RT_FileObj* pFile, RT_CNAME pszAttrName);
        //   ROME_API RT_SHORT		RomeFileGetAttrSize(RT_FileObj* pFile, RT_CNAME pszAttr);
        //   ROME_API RT_INT         RomeFileGetAttrSizeEx(RT_FileObj* pFile, RT_CNAME pszAttr);
        //   ROME_API RT_CSTR		RomeFileGetAttrValue(RT_FileObj* pFile, RT_CNAME pszAttr, RT_INT nIndex);
        //   ROME_API RT_SHORT		RomeFileSetAttrSize(RT_FileObj* pFile, RT_CNAME pszAttr, RT_INT nNewSize);
        //   ROME_API RT_SHORT		RomeFileSetAttrValue(RT_FileObj* pFile, RT_CNAME pszAttr, RT_CSTR pszValue, RT_INT nIndex);
        //   ROME_API RT_CSTR		RomeGetPropertyStr(RT_App* pApp, RT_UINT nProp);
        //   ROME_API RT_void		RomeFilesCloseAll(RT_Files* pFiles, RT_UINT nFlags = 0);
        //   ROME_API RT_void		RomeFilesClose(RT_Files* pFiles, RT_UINT nFlags = 0);
        //   ROME_API RT_FileObj*	RomeFilesOpen(RT_Files* pFiles, RT_CSTR pszFilename, RT_UINT nFlags);
        //   ROME_API RT_BOOL		RomeEngineGetAutorun(RT_Engine* pEngine);
        //   ROME_API RT_BOOL		RomeEngineRun(RT_Engine* pEngine);
        //   ROME_API RT_void		RomeEngineSetAutorun(RT_Engine* pEngine, RT_BOOL bAutorun);
        //   ROME_API RT_CSTR        RomeGetLastError(RT_App* pApp);
        //   ROME_API RT_BOOL		RomeExit(RT_App* pApp);
        //   ROME_API RT_BOOL		RomeDatabaseClose(RT_Database* pDatabase, RT_CSTR pszDatabase = NULL);



        //* In api-rome.h but not used by SP2
        //   ROME_API RT_BOOL		RomeFileGetFloatArray(RT_FileObj* pFile, RT_CNAME pszAttr, RT_REAL* pArray, RT_INT* pSize, RT_UINT nVariant = RX_VARIANT_CATALOG, RT_CNAME pszUnit = "");
        //   ROME_API RT_BOOL		RomeFileDelete(RT_FileObj* pFile);
        //   ROME_API RT_INT         RomeFileGetAttrDimSize(RT_FileObj* pFile, RT_CNAME pszAttr, RT_INT nDim);
        //   ROME_API RT_CSTR		RomeFileGetAttrValueAux(RT_FileObj* pFile, RT_CNAME pszAttr, RT_INT nIndex, RT_UINT nVariant, RT_CNAME pszUnit);
        //   ROME_API RT_void		RomeFileGetAttrValueF(RT_PCHAR pBuf, RT_UINT nBufLen, RT_FileObj* pFile, RT_CNAME pszAttr, RT_SHORT nIndex);
        //   ROME_API RT_CSTR		RomeFileGetFullname(RT_FileObj* pFile);
        //   ROME_API RT_void		RomeFileGetFullnameF(RT_PCHAR pBuf, RT_UINT nBufLen, RT_FileObj* pFile);
        //   ROME_API RT_INT			RomeFileSave(RT_FileObj* pFile);
        //   ROME_API RT_BOOL		RomeFileSaveAs(RT_FileObj* pFile, RT_CSTR pszNewName);
        //   ROME_API RT_BOOL		RomeFileSaveAsEx(RT_FileObj* pFile, RT_CSTR pszNewName, RT_UINT nFlags);
        //   ROME_API RT_FileObj*	RomeFilesAdd(RT_Files* pFiles, RT_CNAME pszObjType, RT_CSTR pszFullname);
        //   ROME_API RT_SHORT		RomeFileSetAttrValueAux(RT_FileObj* pFile, RT_CNAME pszAttr, RT_CSTR pszValue, RT_INT nIndex, RT_UINT nVariant, RT_CNAME pszUnit);
        //   ROME_API RT_INT			RomeFilesGetCount(RT_Files* pFiles);
        //   ROME_API RT_BOOL		RomeFilesGetDependencies(RT_Files* pFiles, RT_CSTR pszFilename, RT_CSTRA& depsArray, RT_INT& depsSize);
        //   ROME_API RT_FileObj*	RomeFilesGetItem(RT_Files* pFiles, RT_INT nItem);
        //   ROME_API RT_INT			RomeFilesPragma(RT_Files* pFiles, RT_UINT nPragma, RT_void* pExtra);
        //   ROME_API RT_INT			RomeEngineLockUpdate(RT_Engine* pEngine);
        //   ROME_API RT_BOOL		RomeEngineIsLocked(RT_Engine* pEngine);
        //   ROME_API RT_INT			RomeEngineUnlockUpdate(RT_Engine* pEngine);
        //   ROME_API RT_BOOL        RomeSetLastError(RT_App* pApp, RT_CSTR pszInfo);

        // * Used in SP2
        //   RomeInit             = (romeInitFunc)RomeDLL.GetUnmanagedFunction<romeInitFunc>("RomeInit");
        //   RomeExit             = (romeExitFunc)RomeDLL.GetUnmanagedFunction<romeExitFunc>("RomeExit");
        //   RomeGetDatabase      = (romeGetDatabaseFunc)RomeDLL.GetUnmanagedFunction<romeGetDatabaseFunc>("RomeGetDatabase");
        //   RomeDatabaseOpen     = (romeDatabaseOpenFunc)RomeDLL.GetUnmanagedFunction<romeDatabaseOpenFunc>("RomeDatabaseOpen");
        //   RomeDatabaseClose    = (romeDatabaseCloseFunc)RomeDLL.GetUnmanagedFunction<romeDatabaseCloseFunc>("RomeDatabaseClose");
        //   RomeGetEngine        = (romeGetEngineFunc)RomeDLL.GetUnmanagedFunction<romeGetEngineFunc>("RomeGetEngine");
        //   RomeEngineRun        = (romeEngineRunFunc)RomeDLL.GetUnmanagedFunction<romeEngineRunFunc>("RomeEngineRun");
        //   RomeEngineGetAutorun = (romeEngineGetAutorunFunc)RomeDLL.GetUnmanagedFunction<romeEngineGetAutorunFunc>("RomeEngineGetAutorun");
        //   RomeEngineSetAutorun = (romeEngineSetAutorunFunc)RomeDLL.GetUnmanagedFunction<romeEngineSetAutorunFunc>("RomeEngineSetAutorun");
        //   RomeGetFiles         = (romeGetFilesFunc)RomeDLL.GetUnmanagedFunction<romeGetFilesFunc>("RomeGetFiles");
        //   RomeFilesOpen        = (romeFilesOpenFunc)RomeDLL.GetUnmanagedFunction<romeFilesOpenFunc>("RomeFilesOpen");
        //   RomeFileClose        = (romeFileCloseFunc)RomeDLL.GetUnmanagedFunction<romeFileCloseFunc>("RomeFileClose");
        //   RomeGetTitle         = (romeGetTitleFunc)RomeDLL.GetUnmanagedFunction<romeGetTitleFunc>("RomeGetTitle");
        //   RomeGetPropertyStr   = (romeGetPropertyStrFunc)RomeDLL.GetUnmanagedFunction<romeGetPropertyStrFunc>("RomeGetPropertyStr");
        //   RomeFileSetAttrValue = (romeFileSetAttrValueFunc)RomeDLL.GetUnmanagedFunction<romeFileSetAttrValueFunc>("RomeFileSetAttrValue");
        //   RomeFileGetAttrValue = (romeFileGetAttrValueFunc)RomeDLL.GetUnmanagedFunction<romeFileGetAttrValueFunc>("RomeFileGetAttrValue");
        //   RomeFileSetAttrSize  = (romeFileSetAttrSizeFunc)RomeDLL.GetUnmanagedFunction<romeFileSetAttrSizeFunc>("RomeFileSetAttrSize");
        //   RomeFileGetAttrSizeEx= (romeFileGetAttrSizeExFunc)RomeDLL.GetUnmanagedFunction<romeFileGetAttrSizeExFunc>("RomeFileGetAttrSizeEx");
        //   //28Mar2015 JWW Open ???
        //   //RomeFilesCloseAll  = (romeFilesCloseAllFunc)RomeDLL.GetUnmanagedFunction<romeFilesCloseAllFunc>("RomeDatabaseOpen");
        //   RomeFilesCloseAll    = (romeFilesCloseAllFunc)RomeDLL.GetUnmanagedFunction<romeFilesCloseAllFunc>("RomeFilesCloseAll");
        //   //imoses.h :: AttrGetFlatIndex = (romeAttrGetFlatIndex)RomeDLL.GetUnmanagedFunction<romeAttrGetFlatIndex>("AttrGetFlatIndex");
        //   //28Mar2015 JWW added
        //   RomeGetLastError = (romeGetLastErrorFunc)RomeDLL.GetUnmanagedFunction<romeGetLastErrorFunc>("RomeGetLastError");

    }
}
