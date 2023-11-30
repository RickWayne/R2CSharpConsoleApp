//! @file api-rome.cpp
//! @brief Implementation of exported functions for the ROME API.
//!
//! Copyright (C) 2000-2010 University of Tennessee.
//! All rights reserved.
//!
//! This file defines the Rome API implementation.
//! - The Rome API consists entirely of C language functions.
//! - The Rome API is an interface for using Rusle2 calculations from a DLL.
//! - This interface is also called by the OLE interface, as a thin wrapper for it (facade).<br>
//!
//! <b> Module State </b><br>
//! Most Rome API functions need to use the AFX_MANAGE_STATE() macro
//!   at the beginning of the function. This is required for many MFC functions
//!   to work correctly. This is necessary if any resources are used.<br>
//! This is required if the function calls any MFC functions, even indirectly.
//! The argument is AfxGetAppModuleState(), which is different from the one
//!   used when switching module states in a DLL.
//!
//! <b> Extended Error Information </b><br>
//! Most API functions return a value of #RX_FAILURE (-1) for errors.<br>
//! Rome API functions does not throw exceptions, and thus cannot use them to pass error status
//!   information across the API boundary.
//! For most functions calling RomeGetLastError() will yield additional information.


#include "stdafx.h"

#include "api-rome.h"
#include "api-rome-priv.h"
#include "attr.h"
#include "commonFile.h"
#include "core.h"
#include "dbfilesys.h"
#include "global.h"
#include "rxfiles.h"
#include "titles.h"     // UnitTestCanRun()


#ifdef BUILD_MOSES // ROMEDLL_IGNORE
#include "mainfrm.h"
#endif

typedef struct DBFIND RT_DBFIND;    //!< Opaque pointer to find result sets returned by Rome API.


#ifdef _DEBUG
#undef THIS_FILE
LOCAL char THIS_FILE[] = __FILE__;
#endif

// Verify that some API symbols are the same at compile time.
#if (RX_TRUE != TRUE)
    #error "error: compile-time test failed: (RX_TRUE != TRUE)"
#endif
#if (RX_FALSE != FALSE)
    #error "error: compile-time test failed: (RX_FALSE != FALSE)"
#endif
#if (RX_FAILURE != FAILURE)
    #error "error: compile-time test failed: (RX_FAILURE != FAILURE)"
#endif

/*! @file api-rome.cpp

<b> Automation API </b><br>
Most functions in the IRome Automation API have a corresponding function in the DLL API.
The mapping is rather straightforward:
- I[<em>ClassName</em>]::[<em>FnName</em>]() --> Rome[<em>ClassName</em>][<em>FnName</em>]().<br>
  Example: IFile::Save() --> RomeFileSave().

The following are exceptions:
- IRome::Run()           --> RomeEngineRun()
- IRome::SetStatusBar()  --> RomeStatusbarMessage()
- IRome::GetAutorun()    --> RomeEngineGetAutorun()
- IRome::SetAutorun()    --> RomeEngineSetAutorun()
- Idatabase::Find()      --> RomeDatabaseFindOpen()
- Idatabase::FindItem()  --> RomeDatabaseFindInfo()

<b> Find Operations </b><br>
The "Find" functions have been changed to support thread-safe behavior, so the
  way in which they are called has changed. RomeDatabaseFindOpen() is used to
  obtain a search result set pointer, which is then queried by API functions.
  When the search result set is no longer needed, it must be closed with
  RomeDatabaseFindClose().

<b> Statusbar Functions </b><br>
The status bar is now "write-only". There is no function to read its current
  string value, corresponding to the readable IRome::Statusbar property.

Rome API functions generally have the following structure:
<pre>
  begin try block
    AFX_MANAGE_STATE() macro, whether required or not
    ASSERT_OR_SETERROR_AND_RETURN_***() test(s) to validate arguments
    ROME_API_[NO]LOCK() macro
    begin command logging
    do the command action
  end try block
  begin catch handler
    try-catch block for complex error handling behavior which might throw an exception
    ASSERT_OR_SETERROR_AND_RETURN_***() to report the exception
  end catch handler
</pre>
*/

/////////////////////////////////////////////////////////////////////////////
// Global variables

// There should be no global variables use unless absolutely necessary.
// Global variables are incompatible with thread-safe code.


/////////////////////////////////////////////////////////////////////////////
// Global utility functions

#if USE_ROMESHELL_LOGGING

//! Activate a filename for use in the RomeShell log file.
//! This will generate an "Activate" command if the filename
//!   is different from the currently active one.
//! @param pszFile  The filename to activate.
//!                 This may be empty or NULL.
//! @return TRUE on success, FALSE on failure.
//!
BOOL LogShellActivate(LPCSTR pszFile)
{
    CString sOldFile = ::RomeThreadGetNamedString("LogShellActivate");
    CString sNewFile = pszFile;
    BOOL bSuccess = TRUE;
    if (!FullnameEquals(pszFile, sOldFile))
    {
        bSuccess = (LogFilePrintf0(LOG_SHELL, "Activate \"%s\"\n", sNewFile) > 0);
        RomeThreadSetNamedString("LogShellActivate", sNewFile);
    }
    return bSuccess;
}

#endif // USE_ROMESHELL_LOGGING


/////////////////////////////////////////////////////////////////////////////
//! @name Rome session functions
//! @{

//! Get a pointer to the Rome database interface.
//! This pointer is used for all Rome database operations.
//! This pointer does not need to be freed or released when finished using it.
//! @note This will return a non-NULL pointer, even if a database hasn't been opened
//!   using RomeDatabaseOpen(). The pointer represents the abstract interface itself,
//!   not the database connection.
//! @param pApp  The Rome interface pointer obtained from RomeInit().
//! @return the interface pointer, or NULL on failure.
//!
//! @RomeAPI Wrapper for CRomeCore::Files.
//!
ROME_API RT_Database* RomeGetDatabase(RT_App* pApp)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pApp,             "RomeGetDatabase: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,        "RomeGetDatabase: invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,         "RomeGetDatabase: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,     "RomeGetDatabase: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        // Does not require command logging.

        return &pApp->Files;
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_NULL(0,                "RomeGetDatabase: exception.");
    }
}


//! Return a full disk path given a path relative to the Rome root directory.
//! If the directory has been redirected somewhere else,
//!   then that location will be returned instead.
//! @param pApp  The Rome interface pointer obtained from RomeInit().
//! @param pszPath  A path (or file) name relative to the root directory.
//!   The name argument is case-insensitive.
//!   Standard subdirectory names which may be redirected:
//!   - Binaries  For program executables and DLLs.
//!   - Export    For files exported from Rusle2.
//!   - Import    For files to be imported into Rusle2.
//!   - Session   Used to store temporary files.
//!   - Users     Used to store user templates.<br>
//!   Examples:
//!   - "Binaries"
//!   - "Export\"
//!   - "Import\database-name.gdb"
//!   - "Session\Temp"<br>
//!   If the path starts with an unrecognized directory, it is appended as a
//!     subfolder of the root directory.
//! @return The full pathname for the requested path, or NULL on error.
//!
//! @note The root directory is returned on NULL or empty string.
//! @note This function works successfully on files and folders which don't exist.
//!   That is because it works on the path name @em strings and redirection mappings,
//!   not on the actual files and directories themselves.
//! @RomeAPI Wrapper for CCoreSession::GetPath().
//!
ROME_API RT_CSTR RomeGetDirectory(RT_App* pApp, RT_CSTR pszPath /*= NULL*/)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pApp,             "RomeGetDirectory: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,        "RomeGetDirectory: Invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,         "RomeGetDirectory: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,     "RomeGetDirectory: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        // Does not require command logging.
        // Don't log this function - it gets called too many times and floods the log file.
//    	CLogFileElement1(LOGELEM_HIST, "user", "RomeGetDirectory", "path='%s'/>\n", (CString)XMLEncode(pszPath));

        return pApp->User.GetPath(pszPath);
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeGetDirectory: exception.");
    }
}


//! Get a full disk path given a path relative to the Rome root directory.
//! @see RomeGetDirectory() [above] for full documentation.
//!
//! This version is required for use by Intel Fortran, which can't use functions
//!   which return a string pointer. Instead it must have its first 2 arguments
//!   be a pointer to a buffer and its length, to return the string in.
//!   The function must have a return type of void.
//! @param[out] pBuf The pointer to a buffer to return a NUL-terminated string in.
//! @param nBufLen   The length of the buffer to return a NUL-terminated string in.
//! @param pApp      The Rome interface pointer obtained from RomeInit().
//! @param pszPath   A path (or file) name relative to the root directory.
//!
//! @RomeAPI Wrapper for RomeGetDirectory().
//! @FortranAPI This function allows calling RomeGetDirectory() from Fortran.
//!
ROME_API RT_void RomeGetDirectoryF(RT_PCHAR pBuf, RT_UINT nBufLen, RT_App* pApp, RT_CSTR pszPath /*= NULL*/)
{
    try
    {
        // Does not require AFX_MANAGE_STATE() macro.

	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

		// Validate arguments unique to the Fortran function version.
		// The remaining arguments will be validated in the normal Rome API call.
        ASSERT_OR_SETERROR_AND_RETURN(pBuf,           "RomeGetDirectoryF: NULL buffer pointer.");
        ASSERT_OR_SETERROR_AND_RETURN(nBufLen > 0,    "RomeGetDirectoryF: non-positive buffer length.");

        ROME_API_LOCK();

        // Does not require command logging.

        LPCSTR pszDir  = RomeGetDirectory(pApp, pszPath);
        CopyStrF(pBuf, nBufLen, pszDir);
    }
    catch (...)
    {
        try
        {
            if (pBuf && nBufLen > 0)
                *pBuf = 0;
            CString sInfo; sInfo.Format("RomeGetDirectoryF: exception for Buffer = '0x%08X', Length = %d, Name = '%s'.", pBuf, nBufLen, (CString)pszPath);
            ASSERT_OR_SETERROR_AND_RETURN(0,        sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN(0,        "RomeGetDirectoryF: exception in catch block.");
        }
    }
}


//! Get a pointer to the Rome engine interface.
//! This pointer does not need to be freed or released when finished using it.
//! @param pApp  The Rome interface pointer obtained from RomeInit().
//! @return  The interface pointer, or NULL on failure.
//!
//! @RomeAPI Wrapper for CRomeCore::Engine.
//!
ROME_API RT_Engine* RomeGetEngine(RT_App* pApp)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pApp,             "RomeGetEngine: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,        "RomeGetEngine: invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,         "RomeGetEngine: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,     "RomeGetEngine: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Does not require command logging.

	    return &pApp->Engine;
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_NULL(0,                "RomeGetEngine: exception.");
    }
}


//! Get a pointer to the Rome filesystem interface.
//! This pointer does not need to be freed or released when finished using it.
//! @param pApp  The Rome interface pointer obtained from RomeInit().
//! @return  The interface pointer, or NULL on failure.
//!
//! @RomeAPI Wrapper for CRomeCore::Files.
//!
ROME_API RT_Files* RomeGetFiles(RT_App* pApp)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pApp,             "RomeGetFiles: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,        "RomeGetFiles: invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,         "RomeGetFiles: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,     "RomeGetFiles: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Does not require command logging.

	    return &pApp->Files;
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_NULL(0,                "RomeGetFiles: exception.");
    }
}


//! Get string properties of the app.
//! @param pApp  The Rome interface pointer obtained from RomeInit().
//! @param nProp The property to get.
//! - #RX_PROPERTYSTR_APPFULLNAME
//!     Return the full filename of the application executable, including path.
//!     Example: "C:\Program Files\Rusle2\Binaries\Rusle2.exe".
//! - #RX_PROPERTYSTR_APPNAME
//!     Return the English name (title) of the application.
//!     Example: "Rusle2".
//! - #RX_PROPERTYSTR_APPPATH
//!     Return the path to the application executable.
//!     Example: "C:\Program Files\Rusle2\Binaries".
//! - #RX_PROPERTYSTR_DBAUTHOR
//!     Return the global "owner" field of the database.
//! - #RX_PROPERTYSTR_DBCOMMENTS
//!     Return the global "info" field of the database.
//! - #RX_PROPERTYSTR_DBDATE
//!     Return the global "date" field of the database.
//! - #RX_PROPERTYSTR_DBFULLNAME
//!     Get the full filename of the database, including directory.
//! - #RX_PROPERTYSTR_DBNAME
//!     Get the short filename of the database.
//!     Example: "moses.gdb".
//! - #RX_PROPERTYSTR_DBPATH
//!     Get the full path of the database.
//!     Example: "C:\Program Files\Rusle2".
//!
//! @return NULL on failure.
//! @RomeAPI
//!
ROME_API RT_CSTR RomeGetPropertyStr(RT_App* pApp, RT_UINT nProp)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pApp,             "RomeGetPropertyStr: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,        "RomeGetPropertyStr: invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,         "RomeGetPropertyStr: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,     "RomeGetPropertyStr: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement1(LOGELEM_HIST, "user", "RomeGetPropertyStr", "type='%d'/>\n", nProp);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeGetPropertyStr %d\n", nProp);
#endif

	    CString& sResult = RomeThreadGetNamedString("RomeGetPropertyStr");

	    switch (nProp)
	    {
		    // Return the full filename of the application executable, including path.
		    // Example: "C:\Program Files\Rusle2\Binaries\Rusle2.exe".
		    case RX_PROPERTYSTR_APPFULLNAME:
		    {
			    CString sExeName = (LPCSTR)pApp->RomeNotificationSend(RX_NOTIFY_APP_EXENAME, NULL);
			    if (strempty(sExeName))
				    return NULL;

                CString sLocal = "Binaries\\" + sExeName + ".exe";
                sResult = RomeGetDirectory(pApp, sLocal);
			    return sResult;
		    }

		    // Return the English name (title) of the application.
		    // Example: "Rusle2".
		    case RX_PROPERTYSTR_APPNAME:
		    {
			    sResult = (LPCSTR)pApp->RomeNotificationSend(RX_NOTIFY_APP_APPNAME, NULL);
			    if (!strempty(sResult))
				    return sResult;
			    else
				    return NULL;
		    }

		    // Return the path to the application executable.
		    // Example: "C:\Program Files\Rusle2\Binaries".
		    case RX_PROPERTYSTR_APPPATH:
		    {
			    sResult = pApp->User.GetPath("Binaries");
			    return sResult;
		    }

		    case RX_PROPERTYSTR_DBAUTHOR:
		    {
			    sResult = DbSysGetInfo(pApp->Files.GetDatalink(), "owner");
			    return sResult;
		    }

		    case RX_PROPERTYSTR_DBCOMMENTS:
		    {
			    sResult = DbSysGetInfo(pApp->Files.GetDatalink(), "info");
			    return sResult;
		    }

		    case RX_PROPERTYSTR_DBDATE:
		    {
			    sResult = DbSysGetInfo(pApp->Files.GetDatalink(), "date");
			    return sResult;
		    }

		    // Get the full filename of the database, including directory.
		    case RX_PROPERTYSTR_DBFULLNAME:
		    {
			    sResult =  pApp->Files.m_sCurrentDatabase;
                // todo: validate "sResult" as a URL.
//              ASSERT(DiskFilePathValidEx(sResult, RX_PATHValid_PATH | RX_PATHValid_EMPTY));
                sResult.Replace("\\\\", "\\"); // Hack: fix double backslashes.
			    return sResult;
		    }

		    // Get the short filename of the database.
		    // Example: "moses.gdb".
		    case RX_PROPERTYSTR_DBNAME:
		    {
			    sResult = FullnameGetFilename(pApp->Files.m_sCurrentDatabase);
                // todo: validate "sResult" as a URL.
//               ASSERT(DiskFilePathValid(sResult));
                sResult.Replace("\\\\", "\\"); // Hack: fix double backslashes.
			    return sResult;
		    }

		    // Get the full path of the database.
		    // Example: "C:\Program Files\Rusle2".
		    case RX_PROPERTYSTR_DBPATH:
		    {
			    sResult = FullnameGetPath(pApp->Files.m_sCurrentDatabase);
                // todo: validate "sResult" as a URL.
//              ASSERT(DiskFilePathValid(sResult));
                sResult.Replace("\\\\", "\\"); // Hack: fix double backslashes.
			    return sResult;
		    }

		    default:
                ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeGetPropertyStr: unknown property value.");
	    }
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeGetPropertyStr: exception for Property = '%d'.", nProp);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeGetPropertyStr: exception in catch block.");
        }
    }
}


//! Get string properties of the app.
//! See RomeGetPropertyStr() [above] for full documentation.
//!
//! This version is required for use by Intel Fortran, which can't use functions
//!   which return a string pointer. Instead it must have its first 2 arguments
//!   be a pointer to a buffer and its length, to return the string in.
//!   The function must have a return type of void.
//! @param[out] pBuf  The pointer to a buffer to return a NUL-terminated string in.
//! @param nBufLen    The length of the buffer to return a NUL-terminated string in.
//! @param pApp       The Rome interface pointer obtained from RomeInit().
//! @param nProp      The property to get.
//!
//! @RomeAPI Wrapper for RomeGetPropertyStr().
//! @FortranAPI This function allows calling RomeGetPropertyStr() from Fortran.
//!
ROME_API RT_void RomeGetPropertyStrF(RT_PCHAR pBuf, RT_UINT nBufLen, RT_App* pApp, RT_UINT nProp)
{
    try
    {
        // Does not require AFX_MANAGE_STATE() macro.

	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

		// Validate arguments unique to the Fortran function version.
		// The remaining arguments will be validated in the normal Rome API call.
        ASSERT_OR_SETERROR_AND_RETURN(pBuf,           "RomeGetPropertyStrF: NULL buffer pointer.");
        ASSERT_OR_SETERROR_AND_RETURN(nBufLen > 0,    "RomeGetPropertyStrF: non-positive buffer length.");

        ROME_API_LOCK();

        // Does not require command logging.

        LPCSTR pszProp = RomeGetPropertyStr(pApp, nProp);
        CopyStrF(pBuf, nBufLen, pszProp);
    }
    catch (...)
    {
        try
        {
            if (pBuf && nBufLen > 0)
                *pBuf = 0;
            CString sInfo; sInfo.Format("RomeGetPropertyStrF: exception for Buffer = '0x%08X', Length = %d, Property = %d.", pBuf, nBufLen, nProp);
            ASSERT_OR_SETERROR_AND_RETURN(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN(0,    "RomeGetPropertyStrF: exception in catch block.");
        }
    }
}


//! Get the #SCIENCEVERSION of the Rome instance.
//! @copydoc SCIENCEVERSION
//! @param pApp  The Rome interface pointer obtained from RomeInit().
//!
//! @return  The science version, or zero (0) on error.
//! @RomeAPI Wrapper for CRomeCore::GetScienceVersion().
//!
ROME_API RT_UINT RomeGetScienceVersion(RT_App* pApp)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

	    ASSERT_OR_SETERROR_AND_RETURN_ZERO(pApp,             "RomeGetScienceVersion: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(bValidApp,        "RomeGetScienceVersion: invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(!bExited,         "RomeGetScienceVersion: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(!bSameThread,     "RomeGetScienceVersion: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement0(LOGELEM_HIST, "user", "RomeGetScienceVersion", "/>\n");
#if USE_ROMESHELL_LOGGING
        LogFilePrintf0(LOG_SHELL, "RomeGetScienceVersion\n");
#endif

	    return pApp->GetScienceVersion();
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(0,                "RomeGetScienceVersion: exception.");
    }
}


//! Get a pointer to the Rome statusbar interface.
//! This pointer does not need to be freed or released when finished using it.
//! @param pApp  The Rome interface pointer obtained from RomeInit().
//!
//! @return  The interface pointer, or NULL on failure.
//! @RomeAPI Wrapper for CMainFrame::StatusBar.
//!
ROME_API RT_Statusbar* RomeGetStatusbar(RT_App* pApp)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

#ifdef BUILD_MOSES // ROMEDLL_IGNORE
        ASSERT_OR_SETERROR_AND_RETURN_NULL(pApp,             "RomeGetStatusbar: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,        "RomeGetStatusbar: invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,         "RomeGetStatusbar: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,     "RomeGetStatusbar: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Does not require command logging.

	    CMainFrame* pFrame = GetMainFrame();
	    if (!pFrame)
		    return NULL;

	    return &(pFrame->StatusBar);
#else // ROMEDLL_IGNORE
	UNUSED(pApp);
	    return NULL;
#endif // ROMEDLL_IGNORE
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_NULL(0,                "RomeGetStatusbar: exception.");
    }
}


//! Get a title string mapped to a title key.
//! @param pApp   The Rome interface pointer obtained from RomeInit().
//! @param pszKey The key to lookup.
//!   There are special values that can be used:
//! - "#APPVERSION"       - The version of the calling application.
//! - "#APPNAME"          - The name of the calling application.
//! - "#BUILDDATE"        - The date the core [science] code was compiled.
//! - "#BUILDTIME"        - The time the core [science] code was compiled.
//! - "#COMPILER_OPTIONS" - The options used to compile the project.
//! - "#ROMEVERSION"      - The version of the core Rome code (may be EXE or DLL)
//! - "#ROMENAME"         - The name of the core Rome module (may be EXE or DLL)
//! - "#SCIENCEVERSION"   - The version of the core Rome model, in format YYYYMMDD
//! - "#STARTTIME"        - The startup time of the current run.
//! - "#VERSION_TOMCRYPT" - The version of the TOMCRYPT library used (as DLL or compiled in)
//! - "#VERSION_ZLIB"     - The version of the ZLIB library used (as DLL or compiled in)
//! @return  The title string if found, or NULL on failure.
//!
//! @see Document "Rusle2 Translation Titles.rtf" for more information.
//! @note This function is also used as a backdoor to add functions to the Rome API
//!   without having to add new exported functions.
//!   Special keys handled:
//! - "Filename1:Filename2:#XML_FILE_COMPARE"<br>
//!     Do a diff between 2 XML files.
//! - "AttrName:#ATTR_UNITS"<br>
//!     Return the title of the current template unit for this parameter.
//!     The parameter name can be a "remote name" (e.g. "#RD:CLIMATE_PTR:EI_10YEAR").
//! - "UnitTestCanRun:TestName"<br>
//!     Return "1" if UnitTestCanRun(TestName) is TRUE, otherwise NULL.
//! @RomeAPI  Wrapper for TitleGet().
//!
ROME_API RT_CSTR RomeGetTitle(RT_App* pApp, RT_CSTR pszKey)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

	    CLogFileElement1(LOGELEM_HIST, "user", "RomeGetTitle", "key='%s'/>\n", (CString)XMLEncode(pszKey));
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeGetTitle \"%s\"\n", (CString)pszKey);
#endif

		if (pApp)
		{
            BOOL bValidApp = (pApp == &App);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,        "RomeGetTitle: invalid Rome app pointer.");
            BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,         "RomeGetTitle: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
            THREADId nCurThreadId = GetCurrentThreadId();
            BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,     "RomeGetTitle: Rome API function called on different thread from RomeInit().");
#endif
        }

        ////////////////////////////////////////
        // Handle special string arguments first.

		// @todo require a non-NULL Rome interface pointer.
        RT_App& Core = pApp? *pApp: App;

	    int location = 0;
#ifdef USE_XML_ARCHIVES
	    // Run the xml file comparison used for the test suite.  Looking for an argument of the form
	    // "[LPCSTR]xmlFilename1:[LPCSTR]xmlFilename2:#XML_FILE_COMPARE"
	    if (pszKey && (location = ((CString)pszKey).Find(":#XML_FILE_COMPARE")) > -1)
	    {
		    RT_BOOL bDiff = (RT_BOOL)Core.RomeNotificationSend(RX_NOTIFY_XML_FILE_COMPARE, pszKey);
		    return Bool2Str(bDiff);
	    }
#endif // USE_XML_ARCHIVES

	    // #ATTR_UNITS: return the title of the unit for this parameter, not the parameter title
	    location = 0;
	    if (pszKey && (location = ((CString)pszKey).Find(":#ATTR_UNITS")) > -1)
	    {
		    CString sAttrName = ((CString)pszKey).Left(location);
#if USE_USER_TEMPLATES
		    LPCNAME pszUnit = Core.pPreferences->GetPrefUnit(sAttrName);
#else
			CListing* pAttrL = Core.AttrCatalog.GetListing(sAttrName);
			LPCNAME pszUnit = pAttrL? pAttrL->GetUnit(): "";
#endif
		    pszKey = pszUnit;
	    }

        if (pszKey && strncmp(pszKey, "UnitTestCanRun:", 15)==0)
        {
#if RUN_UNIT_TESTS
            LPCSTR pszTestName = pszKey + 15;
            BOOL bCanRun = UnitTestCanRun(pszTestName);
            return bCanRun? "1": NULL;
#else
            return NULL;
#endif
        }

        ////////////////////////////////////////

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pApp,             "RomeGetTitle: NULL Rome app pointer.");

        ROME_API_LOCK();

	    return pApp->Titles.FindAux(pszKey, TITLES_AppGetTitle);
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeGetTitle: exception for key = '%s'.", (CString)pszKey);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeGetTitle: exception in catch block.");
        }
    }
}


//! Get a title string mapped to a title key.
//! See RomeGetTitle() [above] for full documentation.
//!
//! This version is required for use by Intel Fortran, which can't use functions
//!   which return a string pointer. Instead it must have its first 2 arguments
//!   be a pointer to a buffer and its length, to return the string in.
//!   The function must have a return type of void.
//! @param[out] pBuf The pointer to a buffer to return a NUL-terminated string in.
//! @param nBufLen   The length of the buffer to return a NUL-terminated string in.
//! @param pApp      The Rome interface pointer obtained from RomeInit().
//! @param pszKey    The key to lookup.
//!
//! @RomeAPI Wrapper for RomeGetTitle().
//! @FortranAPI This function allows calling RomeGetTitle() from Fortran.
//!
ROME_API RT_void RomeGetTitleF(RT_PCHAR pBuf, RT_UINT nBufLen, RT_App* pApp, RT_CSTR pszKey)
{
    try
    {
        // Does not require AFX_MANAGE_STATE() macro.

	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

		// Validate arguments unique to the Fortran function version.
		// The remaining arguments will be validated in the normal Rome API call.
        ASSERT_OR_SETERROR_AND_RETURN(pBuf,           "RomeGetTitleF: NULL buffer pointer.");
        ASSERT_OR_SETERROR_AND_RETURN(nBufLen > 0,    "RomeGetTitleF: non-positive buffer length.");

        ROME_API_LOCK();

        // Does not require command logging.

        LPCSTR pszTitle = RomeGetTitle(pApp, pszKey);
        CopyStrF(pBuf, nBufLen, pszTitle);
    }
    catch (...)
    {
        try
        {
            if (pBuf && nBufLen > 0)
                *pBuf = 0;
            CString sInfo; sInfo.Format("RomeGetPropertyStrF: exception for Buffer = '0x%08X', Length = %d, Key = '%s'.", pBuf, nBufLen, CString(pszKey));
            ASSERT_OR_SETERROR_AND_RETURN(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN(0,    "RomeGetPropertyStrF: exception in catch block.");
        }
    }
}


//! Create a new (key, title) translation pair and add it to the titles map.
//! @param pApp      The Rome interface pointer obtained from RomeInit().
//! @param pszKey    The key string.
//! @param pszTitle  The string to map to the key.
//!   If this is NULL, the title will be removed.
//! @param nFlags    Flags used by titles functions
//!   (not all flag combinations are legal)
//! - #RX_TITLES_USER     Add to user  titles
//! - #RX_TITLES_FIXED    Add to fixed titles
//! - #RX_TITLES_INTERNAL Add to internal titles
//! - #RX_TITLES_NODUP    Don't set a duplicate title
//!
//! This makes use of the order INTERNAL < FIXED < USER.
//! Don't set a title in a level when it duplicates
//!   an existing title with a key at a lower level.
//! Do set the title if a different title is mapped
//!   to the same key at the same or a lower level.
//! @return TRUE on success.
//!
//! @see Document "Rusle2 Translation Titles.rtf" for more information.
//! @RomeAPI  Wrapper for CRomeTitles::TitleSet().
//!
ROME_API RT_BOOL RomeSetTitle(RT_App* pApp, RT_CSTR pszKey, RT_CSTR pszTitle, RT_UINT nFlags)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pApp,             "RomeSetTitle: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,        "RomeSetTitle: invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,         "RomeSetTitle: RomeExit() has already been called.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pszKey,           "RomeSetTitle: NULL key string.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!strempty(pszKey),"RomeSetTitle: empty key string.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,     "RomeSetTitle: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement3(LOGELEM_HIST, "user", "RomeSetTitle", "key='%s' text='%d' flags='%d'/>\n", (CString)XMLEncode(pszKey), pszTitle, nFlags);
#if USE_ROMESHELL_LOGGING
        LPCSTR pszCmdTitle = pszTitle? pszTitle: "#NULL";
        LogFilePrintf3(LOG_SHELL, "RomeSetTitle \"%s\" \"%s\" %d\n", pszKey, pszCmdTitle, nFlags);
#endif

	    return pApp->Titles.TitleSet(pszKey, pszTitle, nFlags);
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeSetTitle: exception for key = '%s' title = '%s' flags='%d'.", (CString)pszKey, (CString)pszTitle, nFlags);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeSetTitle: exception in catch block.");
        }
    }
}


//! Load a user template file.
//! @param pApp   The Rome interface pointer obtained from RomeInit().
//! @param pszFilename The name of the disk template file.
//!   This can be a short filename, in which case the full path to the
//!   "Users" directory will be prepended. If the "Users" directory has been
//!   remapped by configuration setting, that directory will be used.
//! @return RX_TRUE on success, RX_FALSE on failure or error.
//!
//! @see RomeTemplateSave().
//! @RomeAPI Wrapper for CRomeCore::LoadTemplate().
//!
ROME_API RT_BOOL RomeTemplateLoad(RT_App* pApp, RT_CSTR pszFilename)
{
#if USE_USER_TEMPLATES
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pApp,                   "RomeTemplateLoad: NULL Rome app pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!strempty(pszFilename), "RomeTemplateLoad: empty path name.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,              "RomeTemplateLoad: invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,               "RomeTemplateLoad: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,           "RomeTemplateLoad: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement1(LOGELEM_HIST, "user", "RomeTemplateLoad", "file='%s'/>\n", XMLEncode(pszFilename));
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeTemplateLoad \"%s\"\n", pszFilename);
#endif

	    BOOL bLoaded = pApp->LoadTemplate(pszFilename);
	    return bLoaded;
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,                      "RomeTemplateLoad: exception.");
    }
#else
	UNUSED2(pApp, pszFilename);
	return RX_FALSE;
#endif // USE_USER_TEMPLATES
}


//! Save the active user template.
//! @param pApp  The Rome interface pointer obtained from RomeInit().
//! @param pszFilename  The filename to save the template as.
//!   This can be a short filename, in which case the full path to the
//!   "Users" directory will be prepended. If the "Users" directory has been
//!   remapped by configuration setting, that directory will be used.
//!   If this is NULL, the current name will be used.
//! @return  RX_TRUE on success, RX_FALSE on failure or error.
//!
//! @see RomeTemplateLoad().
//! @RomeAPI Wrapper for CRomeCore::SaveTemplate().
//!
ROME_API RT_BOOL RomeTemplateSave(RT_App* pApp, RT_CSTR pszFilename /*= NULL*/)
{
#if USE_USER_TEMPLATES
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pApp,                   "RomeTemplateSave: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,              "RomeTemplateSave: invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,               "RomeTemplateSave: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,           "RomeTemplateSave: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement1(LOGELEM_HIST, "user", "RomeTemplateSave", "file='%s'/>\n", XMLEncode(pszFilename));
#if USE_ROMESHELL_LOGGING
        if (pszFilename)
            LogFilePrintf1(LOG_SHELL, "RomeTemplateSave \"%s\"\n", pszFilename);
        else
            LogFilePrintf0(LOG_SHELL, "RomeTemplateSave\n");
#endif

	    BOOL bSaved = pApp->SaveTemplate(pszFilename);
	    return bSaved;
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,                      "RomeTemplateSave: exception.");
    }
#else
	UNUSED2(pApp, pszFilename);
	return RX_FALSE;
#endif // USE_USER_TEMPLATES
}


//! Get error information set by the API.
//! This function may be called when an API function returns an error value.
//! This may return additional information in text format.
//! The information string is currently thread-local.
//! @param pApp  The Rome interface pointer obtained from RomeInit().
//! @return  An error information string, or NULL on failure.
//!
//! @see RomeSetLastError().
//! @RomeAPI
//!
ROME_API RT_CSTR RomeGetLastError(RT_App* pApp)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

        // Can't use Get/SetLastError macros insIde this function!
        BOOL bValidApp = (pApp == NULL) || (pApp == &App);
        ASSERT_OR_RETURN_NULL(bValidApp);
		BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_RETURN_NULL(!bExited);
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (App.m_nThreadId = nCurThreadId);
        ASSERT_OR_RETURN_VALUE(!bSameThread, "RomeGetLastError: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_NOLOCK();

        // Does not require command logging.

        // Get a thread-local error information string.
        CString& sInfo = RomeThreadGetNamedString("RomeGetLastError");
        return sInfo;
    }
    catch (...)
    {
        ASSERT(FALSE);
        // Note: don't try to use RomeGet/SetLastError() here if this code isn't working!
        return NULL; // failure
    }
}


//! Set additional error information for a Rome API error.
//! This information is retrieved by RomeGetLastError().
//! Note that it may be overwritten if not retrieved directly after it is set.
//! This information string is currently thread-local.
//! @param pApp    The Rome interface pointer obtained from RomeInit().
//! @param pszInfo The text error description.<br>
//!   If this is NULL the error information will be cleared.<br>
//!   If the string starts with a special character, then<br>
//!   '+'  the text will be @em appended  to the current string as a new line.<br>
//!   '-'  the text will be @em prepended to the current string as a new line.<br>
//!   '='  the text will @em replace the current string.<br>
//!   If the string doesn't start with a special character, then whether the string replaces the old
//!     one or is affixed in some way depends on compile-time settings of this function.
//! @return  RX_TRUE on success, RX_FALSE on failure or error.
//!
//! @see RomeGetLastError().
//! @RomeAPI
//!
ROME_API RT_BOOL RomeSetLastError(RT_App* pApp, RT_CSTR pszInfo)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        // Can't use Get/SetLastError macros insIde this function!
		if (pApp)
		{
			BOOL bValidApp = (pApp == &App);
			ASSERT_OR_RETURN_FALSE(bValidApp);
			BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
			ASSERT_OR_RETURN_FALSE(!bExited);
#if USE_ROMEAPI_THREADIdS
            THREADId nCurThreadId = GetCurrentThreadId();
            BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
            ASSERT_OR_RETURN_FALSE(!bSameThread); // RomeSetLastError: Rome API function called on different thread from RomeInit().
#endif
		}

        ROME_API_NOLOCK();

	    // Does not require command logging.

        //! @todo Require an interface pointer to use this function.
//      ASSERT_OR_RETURN_FALSE(pApp);

#if defined(_DEBUG) && !defined(BUILD_MOSES)
        // If we are setting a new error message, we don't want to wipe out an old one.
        // Using the '+' and '-' prefixes allows us to maintain a 'call stack' of error messages.
        // NOTE: Another way to do this would be to just change this function to always
        //   concatenate the new error info.
        RT_CSTR pszOldError = RomeGetLastError(pApp);
        ASSERT(strempty(pszOldError) || strempty(pszInfo) || streq(pszOldError, pszInfo) || *pszInfo == '-' || *pszInfo == '+' || *pszInfo == '=');
#endif

        // Get the thread-local error information string.
        CString sInfo = RomeThreadGetNamedString("RomeGetLastError");

        if (pszInfo == NULL)
            sInfo = "";
        else
        if (*pszInfo == '+')
        {
            // Affix the new string to the end of the current string.
            if (!strempty(sInfo))
                sInfo = (sInfo + "\n") + (pszInfo+1);
            else
                sInfo = (pszInfo+1);
        }
        else
        if (*pszInfo == '-')
        {
            // Prefix the new string to the start of the current string.
            if (!strempty(sInfo))
                sInfo = (pszInfo+1) + ("\n" + sInfo);
            else
                sInfo = (pszInfo+1);
        }
        else
        if (*pszInfo == '=')
        {
            // Replace the new string as the current string.
            sInfo = (pszInfo+1);
        }
        else
        {
#if 0
            // *** Don't use this code for now. It is unsafe because it can
            // ***   lead to unbounded strings when visuals code repeatedly asks for
            // ***   obsolete parameters.
            // Prefix the new string to the start of the current string.
            if (!strempty(sInfo))
                sInfo = pszInfo + ("\n" + sInfo);
            else
                sInfo = pszInfo;
#else
            // Replace the new string as the current string.
            sInfo = pszInfo;
#endif
        }

        RomeThreadSetNamedString("RomeGetLastError", sInfo);

        return RX_TRUE; // success
    }
    catch (...)
    {
        ASSERT(FALSE);
        // Note: don't try to use RomeGet/SetLastError() here if this code isn't working!
        return RX_FALSE; // failure
    }
}


//! Call when finished using the Rome interface.
//!   This frees system resources allocated by the interface.
//!   After this call it is forbIdden call any other Rome API functions 
//!   or to use any data returned by Rome API functions during the session
//!   that is owned by the Rome session.
//! @param pApp  The Rome interface pointer obtained from RomeInit().
//! @return RX_TRUE on succcess, RX_FALSE on failure.
//!
//! @warning After RomeExit() has been called, you may not call RomeInit() to create a new Rome session.
//! @see RomeInit().
//! @RomeAPI Wrapper for CRomeCore::Exit().
//!
ROME_API RT_BOOL RomeExit(RT_App* pApp)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pApp,            "RomeExit: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,       "RomeExit: invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,        "RomeExit: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT(!bSameThread);
#endif

        ROME_API_LOCK();

	    CLogFileElement0(LOGELEM_HIST, "user", "RomeExit", "/>\n");
#if USE_ROMESHELL_LOGGING
        LogFilePrintf0(LOG_SHELL, "RomeExit\n");
#endif

        return pApp->Exit();
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,               "RomeExit: exception.");
    }
}


//! Initialize the Rome API for use.
//! This function returns a Rome interface pointer required by most Rome API functions.
//!   Call RomeExit() on this pointer when finished with it.
//! @param pszArgs  A command-line argument string used to configure the DLL.
//! - Arguments are separated by spaces. In order to contain spaces or
//!     other special characters, arguments should be double-quoted.
//! - This string may be NULL or empty.
//! - The first argument is assumed to be the name of the calling app, and is ignored.
//! - /DirRoot=...        The root directory ... to use for the all files.
//! - /Path:(name)=(path) The file or folder (name) is redirected to the Win32 (path).
//! - /UnitSystem=(name)  The system of units to use initially.
//! - -  "US"               the English/British unit system
//! - -  "SI"               the metric unit system
//! - -                   If the (name) part is empty, it will use SI units.
//! @since 2007-10-08 If no unit system is specified, it will default to SI units.<br>
//!   Note: in the past this was incorrectly documented as using default US units.<br>
//!   An unrecognized unit system name is now ignored.
//! @return  A pointer to the Rome interface, or NULL on failure.
//!
//! @warning After RomeExit() has been called, you may not call RomeInit() to create a new Rome session.
//! @see CRomeCore::Init(), CRomeCore::ParseArgs(LPCSTR, ...), ParseArgs(RMapStringToString&, ...), RomeExit().
//! @RomeAPI Wrapper for CRomeCore::Init().
//!
ROME_API RT_App* RomeInit(RT_CSTR pszArgs)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

	    // Because this function can get called simultaneously from
	    //   separate threads, make sure this doesn't happen.
	    RFX_CRITICAL_SECTION();

        // Get a pointer to the app instance.
        // @todo Dynamically create a new CRomeCore instance, which is then freed by RomeExit().
        CRomeCore* pApp = &App;

		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,                "RomeInit: RomeExit() has already been called.");

        //! @note RomeInit() may be called multiple times safely.
        //! Later calls will return the same Rome app instance as the first call.
        //! The @p pszArgs argument will be ignored on all calls after the first.
        //! Initialization is only done on the first call to RomeInit().
	    if (pApp->HasFlag(DLLSTATE_INITROME))
		    return &App;

        ROME_API_LOCK();

	    CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeInit", "args='%s'>\n", (CString)XMLEncode(pszArgs));
#if USE_ROMESHELL_LOGGING
        if (!strempty(pszArgs))
            LogFilePrintf1(LOG_SHELL, "RomeInit %s\n", pszArgs);
        else
            LogFilePrintf0(LOG_SHELL, "RomeInit\n");
#endif

        // Set the Rome notification callback.
        // Currently this is passed into the CRomeCore constructor.
        // TODO: Generalize to register a window/thread handle or IP address to send messages to.

        // HACK: add a place to stop execution and wait for the user to continue.
	    // This alows using menu item Build > Start Debug. > Attach to Process...
	    //   to attach to the calling executable, and then continue running.
    //	MessageBox(NULL, "Waiting to attach to calling process", "RomeDLL", MB_OK);

	    // Store a copy of the command line this was invoked with.
	    pApp->m_sCommandLine = pszArgs;

        CStringArray aCommandLine;
        BOOL bParsed = CRomeCore::ParseArgs(pszArgs, aCommandLine);
        // Note: the DLLSTATE_INITARGS flag will only be set when the flags are handled in CRomeCore::Init().
    	ASSERT_OR_SETERROR_AND_RETURN_NULL(bParsed, "-RomeInit: failed to parse command line arguments.");

	    const int argc = aCommandLine.GetSize();
	    CArray<LPCSTR, LPCSTR> vCommandLine;
	    for (int i=0; i<argc; i++)
		    vCommandLine.Add(aCommandLine[i]);
	    const char** argv = vCommandLine.GetData();
	    const char** envp = NULL;

#if USE_ROMEAPI_THREADIdS
        //! @note If #USE_ROMEAPI_THREADIdS is set, store the thread Id of this thread.
        //! This will be checked in subsequent Rome API calls to check that it matches.
        pApp->m_nThreadId = GetCurrentThreadId();
#endif

        //////////////////////////////////

	    return pApp->Init(argc, argv, envp);
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeInit: exception for Args = '%s'.", CString(pszArgs));
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeInit: exception in catch block.");
        }
    }
}


//! Manage adding and removing listeners on various Rome objects.
//! @param nAction    The action to perform, and the target type.<br>
//!   Example: #RX_LISTENER_ADD | #RX_LISTENER_TARGET_FILE.
//! @param pTarget    The Rome object that is being observed.
//! @param pObserver  An opaque pointer to or Id of the observer.
//! @param pEventHandler  The event callback function to invoke.
//!
//! @return  Non-zero on success, zero on failure.
//! @RomeAPI
//!
ROME_API RT_BOOL Rome_Listener(RT_UINT nAction, RT_void* pTarget, RT_void* pObserver, RT_EventHandler pEventHandler)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        const UINT nTargetType = nAction & RX_LISTENER_TARGET_MASK;
        ASSERT_OR_RETURN_NULL(nTargetType != RX_LISTENER_TARGET_NONE);
        const UINT nActionType = nAction & RX_LISTENER_ACTION_MASK;

        // Currently all arguments are required to be non-NULL.
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(pTarget,         "Rome_Listener: NULL target argument.");
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(pObserver,       "Rome_Listener: NULL observer argument.");
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(pEventHandler,   "Rome_Listener: NULL event handler.");
        //
		BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(!bExited,        "Rome_Listener: RomeExit() has already been called.");

        ROME_API_LOCK();

        RT_BOOL bRet = RX_FALSE;

        switch (nTargetType)
        {
            case RX_LISTENER_TARGET_FILE:
            {
                // @todo Add USE_ROMEAPI_THREADIdS check.
                bRet = RomeFile_Listener((CFileObj*)pTarget, nActionType, pObserver, pEventHandler);
                break;
            }

            case RX_LISTENER_TARGET_OBJ:
            {
                // @todo Add USE_ROMEAPI_THREADIdS check.
                bRet = RomeObj_Listener((RT_SubObj*)pTarget, nActionType, pObserver, pEventHandler);
                break;
            }
/*
            case RX_LISTENER_TARGET_TASK:
            {
                bRet = ===((CEngineTask*)pTarget, nActionType, pObserver, pEventHandler);
                break;
            }
*/
            default:
                ASSERT(false);
                break;
        }

        return bRet;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("Rome_Listener: exception for Action = %d.", nAction);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "Rome_Listener: exception in catch block.");
        }
    }
}

//! @} // Rome session functions
/////////////////////////////////////////////////////////////////////////////
//! @name Rome Catalog functions
//! @{

//! Get the number of dimensions an attr has.
//! @param pApp    The Rome interface pointer obtained from RomeInit().
//! @param pszAttr  The parameter name used by the catalog (e.g. "CLAY").
//!   This can be a 'long' attr name with a remote prefix (e.g. "#RD:SOIL_PTR:CLAY").
//! @return  The attr's number of dimensions, or #RX_FAILURE (-1) on error.
//!
//! @see RomeFileGetAttrDimSize(), RomeFileGetAttrSizeEx().
//! @RomeAPI
//!
ROME_API RT_INT RomeCatalogGetAttrDimCount(RT_App* pApp, RT_CNAME pszAttr)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pApp,               "RomeCatalogGetAttrDimCount: NULL Rome app pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!strempty(pszAttr), "RomeCatalogGetAttrDimCount: empty attr name.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,          "RomeCatalogGetAttrDimCount: invalid Rome pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,           "RomeCatalogGetAttrDimCount: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeCatalogGetAttrDimCount: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_NOLOCK();

        // Note: There is no need to finish running the stack since this doesn't require access to the file or attr instance.

	    // Does not require command logging.
//#if USE_ROMESHELL_LOGGING
//        LogShellActivate(sFile);
//        LogFilePrintf1(LOG_SHELL, "RomeCatalogGetAttrDimCount \"%s\"\n", pszAttr);
//#endif

        // Find the catalog listing for this parameter.
        CListing* pListing = pApp->AttrCatalog.GetListing(pszAttr);
        TEST_OR_SETERROR_AND_RETURN_FAILURE(pListing,            "RomeCatalogGetAttrDimCount: Parameter not found.");

        // Get the number of dimensions from the catalog listing.
        LPCNAME dim0 = pListing->GetDim(0);
        LPCNAME dim1 = pListing->GetDim(1);
        int nDims = (!strempty(dim0) && !streq(dim0, "1")) +
                    (!strempty(dim1) && !streq(dim1, "1"));
        ASSERT(0 <= nDims && nDims <= CDimensions::MAXDIMNUM);

        return nDims;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFileGetAttrDimCount: exception for Attr = '%s'.", (CString)pszAttr);
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    "RomeFileGetAttrDimCount: exception in catch block.");
        }
    }
}


//! Get the tag (type string) for a parameter.
//! @param pApp    The Rome interface pointer obtained from RomeInit().
//! @param pszAttr  The parameter name used by the catalog (e.g. "CLAY").
//!   This can be a 'long' attr name with a remote prefix.
//! @return The type string on success, or empty on failure or error.
//!
//! @see RomeCatalogGetAttrType(), GetParamTag().
//! @RomeAPI Wrapper for CListing::GetTag().
//!
ROME_API RT_CSTR RomeCatalogGetAttrTag(RT_App* pApp, RT_CNAME pszAttr)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pApp,                  "RomeCatalogGetAttrType: NULL Rome app pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!strempty(pszAttr),    "RomeCatalogGetAttrType: empty attr name.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,             "RomeCatalogGetAttrType: invalid Rome pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,              "RomeCatalogGetAttrType: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,          "RomeFileGetAttrDimCount: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_NOLOCK();

        ParamType nType = (ParamType)RomeCatalogGetAttrType(pApp, pszAttr);

        RT_CSTR pszTag = GetParamTag(nType);

        return pszTag; // success
    }
    catch (...)
    {
        ASSERT(FALSE);
        return ""; // failure
    }
}

//! Get the integer type of a parameter.
//! These are values of type enum #ParamType, which are exposed in "imoses.h".
//! @param pApp    The Rome interface pointer obtained from RomeInit().
//! @param pszAttr  The parameter name used by the catalog (e.g. "CLAY").
//!   This can be a 'long' attr name with a remote prefix.
//! @return The parameter integer type on success, or 0 (#RX_ATTR_NUL) on failure or error.
//!
//! @see RomeCatalogGetAttrTag(), enum ParamType.
//! @RomeAPI Wrapper for CListing::GetType().
//!
ROME_API RT_UINT RomeCatalogGetAttrType(RT_App* pApp, RT_CNAME pszAttr)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_ZERO(pApp,              "RomeCatalogGetAttrType: NULL Rome app pointer.");
        BOOL bValidApp = (pApp == &App);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(bValidApp,         "RomeCatalogGetAttrType: Invalid Rome app pointer.");
		BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(!bExited,          "RomeCatalogGetAttrType: RomeExit() has already been called.");
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(pszAttr,           "RomeCatalogGetAttrType: NULL attr name.");
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(!strempty(pszAttr),"RomeCatalogGetAttrType: Empty attr name.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pApp->m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(!bSameThread,      "RomeCatalogGetAttrType: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_NOLOCK();

	    // Does not require command logging.

        // Find the catalog listing for this parameter.
        CListing* pListing = pApp->AttrCatalog.GetListing(pszAttr);
        TEST_OR_SETERROR_AND_RETURN_ZERO(pListing,            "RomeCatalogGetAttrType: Parameter not found.");

        // Get the type from the catalog listing.
        ParamType nType = pListing->GetType();

        // Handle case of a float which is really an integer.
        if (nType == RX_ATTR_FLT && pListing->GetFlag(ACF_INTEGRAL))
            nType = ATTR_INT;

        return nType; // success
    }
    catch (...)
    {
        ASSERT(FALSE);
        return RX_ATTR_NUL; // failure
    }
}


//! @} // Rome Catalog functions
/////////////////////////////////////////////////////////////////////////////
//! @name Rome Database functions
//! @{

// Verify that RX_ and DBFIND_ flags match each other.
#if (RX_DBFILEINFO_QUERY   != DBFIND_INFO_QUERY)
  #error "error: compile-time test failed: (RX_DBFILEINFO_QUERY   != DBFIND_INFO_QUERY)"
#endif
#if (RX_DBFILEINFO_NAME    != DBFIND_INFO_NAME )
  #error "error: compile-time test failed: (RX_DBFILEINFO_NAME    != DBFIND_INFO_NAME )"
#endif
#if (RX_DBFILEINFO_PATH    != DBFIND_INFO_PATH )
  #error "error: compile-time test failed: (RX_DBFILEINFO_PATH    != DBFIND_INFO_PATH )"
#endif
#if (RX_DBFILEINFO_RIGHT   != DBFIND_INFO_RIGHT)
  #error "error: compile-time test failed: (RX_DBFILEINFO_RIGHT   != DBFIND_INFO_RIGHT)"
#endif
#if (RX_DBFILEINFO_TABLE   != DBFIND_INFO_TABLE)
  #error "error: compile-time test failed: (RX_DBFILEINFO_TABLE   != DBFIND_INFO_TABLE)"
#endif
#if (RX_DBFILEINFO_OUTER   != DBFIND_INFO_OUTER)
  #error "error: compile-time test failed: (RX_DBFILEINFO_OUTER   != DBFIND_INFO_OUTER)"
#endif
#if (RX_DBFILEINFO_LEFT    != DBFIND_INFO_LEFT )
  #error "error: compile-time test failed: (RX_DBFILEINFO_LEFT    != DBFIND_INFO_LEFT )"
#endif
#if (RX_DBFILEINFO_FULL    != DBFIND_INFO_FULL )
  #error "error: compile-time test failed: (RX_DBFILEINFO_FULL    != DBFIND_INFO_FULL )"
#endif
#if (RX_DBFILEINFO_OWNER   != DBFIND_INFO_OWNER)
  #error "error: compile-time test failed: (RX_DBFILEINFO_OWNER   != DBFIND_INFO_OWNER)"
#endif
#if (RX_DBFILEINFO_GROUP   != DBFIND_INFO_GROUP)
  #error "error: compile-time test failed: (RX_DBFILEINFO_GROUP   != DBFIND_INFO_GROUP)"
#endif
#if (RX_DBFILEINFO_PERMS   != DBFIND_INFO_PERMS)
  #error "error: compile-time test failed: (RX_DBFILEINFO_PERMS   != DBFIND_INFO_PERMS)"
#endif
#if (RX_DBFILEINFO_DATE    != DBFIND_INFO_DATE )
  #error "error: compile-time test failed: (RX_DBFILEINFO_DATE    != DBFIND_INFO_DATE )"
#endif
#if (RX_DBFILEINFO_DATA    != DBFIND_INFO_DATA )
  #error "error: compile-time test failed: (RX_DBFILEINFO_DATA    != DBFIND_INFO_DATA )"
#endif
#if (RX_DBFILEINFO_FOLDER   != DBFIND_INFO_FOLDER)
  #error "error: compile-time test failed: (RX_DBFILEINFO_FOLDERS   != DBFIND_INFO_FOLDERS)"
#endif


//! Close a named database.
//! @param pDatabase  The Rome database interface pointer obtained from RomeGetDatabase().
//! @param pszDatabase  The name of the database file to close.
//!   Currently this argument is ignored. It will be used when the Rome filesystem has the ability
//!   to open multiple database simultaneously.
//! @return  RX_TRUE on success, RX_FALSE on failure.
//!
//! @warning This will fail if there are files open that need to be closed first.
//! @see RomeDatabaseOpen().
//! @RomeAPI Wrapper for CFileSys::CloseDatabase().
//!
ROME_API RT_BOOL RomeDatabaseClose(RT_Database* pDatabase, RT_CSTR pszDatabase /*= NULL*/)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pDatabase,      "RomeDatabaseClose: null database pointer.");
        BOOL bValidApp = (&pDatabase->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,      "RomeDatabaseClose: invalid database pointer.");
		BOOL bExited = pDatabase->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,       "RomeDatabaseClose: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pDatabase->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,   "RomeDatabaseClose: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement1(LOGELEM_HIST, "user", "RomeDatabaseClose", "file='%s'/>\n", (CString)XMLEncode(pszDatabase));
#if USE_ROMESHELL_LOGGING
        if (!strempty(pszDatabase))
            LogFilePrintf1(LOG_SHELL, "RomeDatabaseClose \"%s\"\n", pszDatabase);
        else
            LogFilePrintf0(LOG_SHELL, "RomeDatabaseClose\n");
#endif

        pDatabase->CloseFiles(CVF_CloseTempFiles | CVF_CloseComboFiles |CVF_CloseLazyFiles);
	    if (pDatabase->FilesToClose(false))
		    return RX_FALSE; // handle this error

	    // Close the current database.
	    RT_BOOL bClosed = pDatabase->CloseDatabase();
	    return bClosed;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeDatabaseClose: exception for Database = '%s'.", CString(pszDatabase));
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,   sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,   "RomeDatabaseClose: exception in catch block.");
        }
    }
}


//! Delete a file record from the database.
//! @param pDatabase  The Rome database interface pointer obtained from RomeGetDatabase().
//! @param pszPathname  The name of the file to delete. (e.g. "soils\default")
//! @param nFlags  Flags which affect its behavior.
//!   This argument is currently ignored (unimplemented).
//!
//! @return RX_TRUE on success, RX_FALSE on failure.
//! @RomeAPI Wrapper for CFileSys::RemoveRecordFromDatabase().
//!
ROME_API RT_BOOL RomeDatabaseFileDelete(RT_Database* pDatabase, RT_CSTR pszPathname, RT_UINT nFlags)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        UNUSED(nFlags);

        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pDatabase,              "RomeDatabaseFileDelete: NULL database pointer.");
        BOOL bValidApp = (&pDatabase->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,				"RomeDatabaseFileDelete: invalid database pointer.");
		BOOL bExited = pDatabase->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,				"RomeDatabaseFileDelete: RomeExit() has already been called.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!strempty(pszPathname), "RomeDatabaseFileDelete: empty path name.");
		ASSERT(nFlags == 0);	// This argument is unused
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pDatabase->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,           "RomeDatabaseFileDelete: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement2(LOGELEM_HIST, "user", "RomeDatabaseFileDelete", "file='%s' flags='%d'/>\n", (CString)XMLEncode(pszPathname), nFlags);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf2(LOG_SHELL, "RomeDatabaseFileDelete \"%s\" %d\n", pszPathname, nFlags);
#endif

	    // Delete the record.
	    RT_BOOL bDeleted = pDatabase->RemoveRecordFromDatabase(pszPathname);
	    return bDeleted;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeDatabaseFileDelete: exception for Pathname = '%s', Flags = 0x%X.", CString(pszPathname), nFlags);
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,   sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,   "RomeDatabaseFileDelete: exception in catch block.");
        }
    }
}


//! Retrieve information about a file in the database.
//! @param pDatabase    The Rome database interface pointer obtained from RomeGetDatabase().
//! @param pszFilename  The name of the file. (e.g. "soils\default")
//! @param nInfoType    The type of information to retrieve:
//! - #RX_DBFILEINFO_QUERY	 0 // info: (string result of query)
//! - #RX_DBFILEINFO_NAME	 1 // info:                name
//! - #RX_DBFILEINFO_PATH	 2 // info:         path
//! - #RX_DBFILEINFO_RIGHT	 3 // info:         path \ name
//! - #RX_DBFILEINFO_TABLE	 4 // info: table
//! - #RX_DBFILEINFO_OUTER	 5 // info: table \        name
//! - #RX_DBFILEINFO_LEFT	 6 // info: table \ path
//! - #RX_DBFILEINFO_FULL	 7 // info: table \ path \ name
//! - #RX_DBFILEINFO_OWNER	 8 // info: owner field
//! - #RX_DBFILEINFO_GROUP	 9 // info: group field
//! - #RX_DBFILEINFO_PERMS	10 // info: perms field
//! - #RX_DBFILEINFO_DATE	11 // info: date  field
//! - #RX_DBFILEINFO_DATA	12 // info: data  field
//! - #RX_DBFILEINFO_FOLDER	13 // info: is this a folder? return "0" or "1".
//!
//! @return  The information as a string, or NULL on failure.
//! @RomeAPI Wrapper for DbFindInfo().
//!
ROME_API RT_CSTR RomeDatabaseFileInfo(RT_Database* pDatabase, RT_CSTR pszFilename, RT_UINT nInfoType)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pDatabase,              "RomeDatabaseFileInfo: NULL database pointer.");
        BOOL bValidApp = (&pDatabase->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,              "RomeDatabaseFileInfo: invalid database pointer.");
		BOOL bExited = pDatabase->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,               "RomeDatabaseFileInfo: RomeExit() has already been called.");
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!strempty(pszFilename), "RomeDatabaseFileInfo: empty file name.");
        ASSERT_OR_SETERROR_AND_RETURN_NULL(pDatabase->IsOpen(),    "RomeDatabaseFileInfo: database not open.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pDatabase->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,           "RomeDatabaseFileInfo: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement2(LOGELEM_HIST, "user", "RomeDatabaseFileInfo", "file='%s' type='%d'/>\n", (CString)XMLEncode(pszFilename), nInfoType);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf2(LOG_SHELL, "RomeDatabaseFileInfo \"%s\" %d\n", pszFilename, nInfoType);
#endif

	    DBFIND* pFind = DbFindOpen(pDatabase->GetDatalink(), pszFilename, DBSYS_FIND_BOTH | DBSYS_FIND_EXACT);
	    ASSERT_OR_RETURN_NULL(pFind);

	    LPCSTR pszInfo = DbFindInfo(pFind, nInfoType);
	    DbFindClose(pFind);

	    return pszInfo;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeDatabaseFileInfo: exception for Filename = '%s', Type = %d.", CString(pszFilename), nInfoType);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeDatabaseFileInfo: exception in catch block.");
        }
    }
}


//! Retrieve information about a file in the database.
//! See RomeDatabaseFileInfo() [above] for full documentation.
//!
//! This version is required for use by Intel Fortran, which can't use functions
//!   which return a string pointer. Instead it must have its first 2 arguments
//!   be a pointer to a buffer and its length, to return the string in.
//!   The function must have a return type of void.
//! @param pBuf       The pointer to a buffer to return a NUL-terminated string in.
//! @param nBufLen    The length of the buffer to return a NUL-terminated string in.
//! @param pDatabase  The Rome database interface pointer obtained from RomeGetDatabase().
//! @param pszFilename  The name of the file. (e.g. "soils\default")
//! @param nInfoType    The type of information to retrieve.
//!
//! @RomeAPI Wrapper for RomeDatabaseFileInfo().
//! @FortranAPI This function allows calling RomeDatabaseFileInfo() from Fortran.
//!
ROME_API RT_void RomeDatabaseFileInfoF(RT_PCHAR pBuf, RT_UINT nBufLen, RT_Database* pDatabase, RT_CSTR pszFilename, RT_UINT nInfoType)
{
    try
    {
        // Does not require AFX_MANAGE_STATE() macro.

	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

		// Validate arguments unique to the Fortran function version.
		// The remaining arguments will be validated in the normal Rome API call.
        ASSERT_OR_SETERROR_AND_RETURN(pBuf,           "RomeDatabaseFileInfoF: NULL buffer pointer.");
        ASSERT_OR_SETERROR_AND_RETURN(nBufLen > 0,    "RomeDatabaseFileInfoF: non-positive buffer length.");

        ROME_API_LOCK();

        // Does not require command logging.

        LPCSTR pszInfo = RomeDatabaseFileInfo(pDatabase, pszFilename, nInfoType);
        CopyStrF(pBuf, nBufLen, pszInfo);
    }
    catch (...)
    {
        try
        {
            if (pBuf && nBufLen > 0)
                *pBuf = 0;
            CString sInfo; sInfo.Format("RomeDatabaseFileInfoF: exception for Buffer = '0x%08X', Length = %d, Filename = '%s', Type = %d.", pBuf, nBufLen, CString(pszFilename), nInfoType);
            ASSERT_OR_SETERROR_AND_RETURN(0,         sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN(0,         "RomeGetPropertyStr: exception in catch block.");
        }
    }
}


//! Get a pointer to the Rome interface from the database interface.
//! @param pDatabase  The Rome database interface pointer obtained from RomeGetDatabase().
//! @return the interface pointer, or NULL on failure.
//!
ROME_API RT_App* RomeDatabaseGetApp(RT_Database* pDatabase)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pDatabase,              "RomeDatabaseGetApp: NULL database pointer.");
        BOOL bValidApp = (&pDatabase->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,              "RomeDatabaseGetApp: invalid database pointer.");
		BOOL bExited = pDatabase->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,               "RomeDatabaseGetApp: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pDatabase->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,           "RomeDatabaseGetApp: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        // Does not require command logging.

        CRomeCore* pRomeCore = &pDatabase->Core;
        return pRomeCore;
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_NULL(0,        "RomeDatabaseGetApp: exception.");
    }
}


//! Get the read-only state of the database.
//! A database may be read-only for many reasons:
//! - The database file is read-only.
//! - The database file is on a read-only filesystem.
//! - The user only has read access to the database.
//! - The connection has been set to read-only programmatically.
//!
//! @param pDatabase  The Rome database interface pointer obtained from RomeGetDatabase().
//!
//! @return RX_TRUE if read-only, RX_FALSE if writeable, and #RX_FAILURE on error.
//!   Returns RX_TRUE for a null @p pDatabase argument.
//! @RomeAPI Wrapper for CFileSys::IsReadOnly().
//!
ROME_API RT_BOOL RomeDatabaseGetReadOnly(RT_Database* pDatabase)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_VALUE(pDatabase,				"RomeDatabaseGetReadOnly: NULL database pointer.", RX_TRUE);
        BOOL bValidApp = (&pDatabase->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,            "RomeDatabaseGetReadOnly: invalid database pointer.");
		BOOL bExited = pDatabase->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,             "RomeDatabaseGetReadOnly: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pDatabase->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,         "RomeDatabaseGetReadOnly: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement0(LOGELEM_HIST, "user", "RomeDatabaseGetReadOnly", "/>\n");
#if USE_ROMESHELL_LOGGING
        LogFilePrintf0(LOG_SHELL, "RomeDatabaseGetReadOnly\n");
#endif

	    RT_BOOL bReadOnly = pDatabase->IsReadOnly();
	    return bReadOnly;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeDatabaseGetReadOnly: exception for Database = 0x08%X.", pDatabase);
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, "RomeDatabaseGetReadOnly: exception in catch block.");
        }
    }
}


//! Open a new database for use by Rome.
//! @param pDatabase    The Rome database interface pointer obtained from RomeGetDatabase().
//! @param pszDatabase  The fullname of the database file to open on disk.
//!   The argument "#DefaultDatabase" (case-insensitive)
//!     will open the default database (usually "moses.gdb").
//! @return RX_TRUE on success, RX_FALSE / #RX_FAILURE on failure.
//!
//! @warning  This will fail if there are files open that need to be closed first.
//! @see RomeDatabaseClose().
//! @RomeAPI Wrapper for CFileSys::Open().
//!
ROME_API RT_BOOL RomeDatabaseOpen(RT_Database* pDatabase, RT_CSTR pszDatabase)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pDatabase,            "RomeDatabaseOpen: NULL database pointer.");
        BOOL bValidApp = (&pDatabase->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,            "RomeDatabaseOpen: invalid database pointer.");
		BOOL bExited = pDatabase->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,             "RomeDatabaseOpen: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pDatabase->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,         "RomeDatabaseOpen: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeDatabaseOpen", "file='%s'>\n", (CString)XMLEncode(pszDatabase));
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeDatabaseOpen \"%s\"\n", (CString)pszDatabase);
#endif

	    pDatabase->CloseFiles(CVF_CloseTempFiles | CVF_CloseComboFiles | CVF_CloseLazyFiles);
        BOOL bOpenFiles = pDatabase->FilesToClose(false);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bOpenFiles,            "RomeDatabaseOpen: files still remaining open.");

	    // Close the current database.
        BOOL bClosed = pDatabase->CloseDatabase();
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bClosed,                "RomeDatabaseOpen: failed to close database.");

	    RT_BOOL bOpened = pDatabase->Open(pszDatabase);
	    return bOpened;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeDatabaseOpen: exception for Database = '%s'.", CString(pszDatabase));
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, "RomeDatabaseOpen: exception in catch block.");
        }
    }
}

//! @} // Rome Database functions
/////////////////////////////////////////////////////////////////////////////
//! @name Rome Database search functions
//! @{

// Verify that RX_ and DBSYS_ flags match each other.
#if (RX_DBFIND_FILES   != DBSYS_FIND_FILES)
  #error "error: compile-time test failed: (RX_DBFIND_FILES   != DBSYS_FIND_FILES)"
#endif
#if (RX_DBFIND_FOLDERS != DBSYS_FIND_FOLDERS)
  #error "error: compile-time test failed: (RX_DBFIND_FOLDERS != DBSYS_FIND_FOLDERS)"
#endif
#if (RX_DBFIND_RECURSE != DBSYS_FIND_RECURSE)
  #error "error: compile-time test failed: (RX_DBFIND_RECURSE != DBSYS_FIND_RECURSE)"
#endif
#if (RX_DBFIND_ADDROOT != DBSYS_FIND_ADDROOT)
  #error "error: compile-time test failed: (RX_DBFIND_ADDROOT != DBSYS_FIND_ADDROOT)"
#endif
#if (RX_DBFIND_EXACT != DBSYS_FIND_EXACT)
  #error "error: compile-time test failed: (RX_DBFIND_EXACT != DBSYS_FIND_EXACT)"
#endif
#if (RX_DBFIND_TABLES  != DBSYS_FIND_TABLES)
  #error "error: compile-time test failed: (RX_DBFIND_TABLES != DBSYS_FIND_TABLES)"
#endif
#if (RX_DBFIND_QUERY   != DBSYS_FIND_QUERY)
  #error "error: compile-time test failed: (RX_DBFIND_QUERY != DBSYS_FIND_QUERY)"
#endif
#if (RX_DBFIND_FLAGBITS   != DBSYS_FIND_FLAGBITS)
  #error "error: compile-time test failed: (RX_DBFIND_FLAGBITS   != DBSYS_FIND_FLAGBITS)"
#endif
#if (RX_DBFIND_FLAGMASK   != DBSYS_FIND_FLAGMASK)
  #error "error: compile-time test failed: (RX_DBFIND_FLAGMASK   != DBSYS_FIND_FLAGMASK)"
#endif

#if (RX_DBFIND_QUERY   != DBSYS_FIND_QUERY)
  #error "error: compile-time test failed: (RX_DBFIND_QUERY != DBSYS_FIND_QUERY)"
#endif


//! Start a new search and return a pointer to the find result set.
//! The results are accessed using RomeDatabaseFindInfo().
//! The result set must be closed using RomeDatabaseFindClose() when
//!   you are finished with it.
//! @param pDatabase  The Rome database interface pointer obtained from RomeGetDatabase().
//! @param pszPattern is the pattern to search with. Its meaning varies
//!   depending on @a nFindFlags. It generally is the table or folder
//!   to search in. If it is NULL or empty, it means all tables.
//! @param nFindFlags are flags that control the search type.
//! - #RX_DBFIND_FILES		1<<0	// Match file names
//! - #RX_DBFIND_FOLDERS	1<<1	// Match folder names
//! - #RX_DBFIND_RECURSE	1<<2	// Recurse into subdirectories.
//! - #RX_DBFIND_ADDROOT	1<<3	// Add the root to the search results
//! - #RX_DBFIND_EXACT		1<<4	// Find single record matching argument string
//! - #RX_DBFIND_TABLES		1<<10	// Find all tables in the database
//! - #RX_DBFIND_QUERY		2<<10	// The pattern string is a SQL query.
//!
//! @return a result set pointer, or NULL on failure.
//!
//! Example to find the number of soils files:
//! @code
//!     RT_DBFIND* pDbFind = RomeDatabaseFindOpen(pDatabase, "soils", RX_DBFIND_FILES | RX_DBFIND_RECURSE);
//!     int nSoils = RomeDatabaseFindCount(pDbFind);
//!     RomeDatabaseFindClose(pDbFind);
//! @endcode
//!
//! @warning Failing to close the find result may leak memory or other program resources.
//! @see RomeDatabaseFindClose(), RomeDatabaseFindCount(), RomeDatabaseFindInfo().
//! @RomeAPI Wrapper for DbFindOpen().
//!
ROME_API RT_DBFIND* RomeDatabaseFindOpen(RT_Database* pDatabase, RT_CSTR pszPattern, RT_UINT nFindFlags)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pDatabase,              "RomeDatabaseFindOpen: NULL database pointer.");
        BOOL bValidApp = (&pDatabase->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,              "RomeDatabaseFindOpen: invalid database pointer.");
		BOOL bExited = pDatabase->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,               "RomeDatabaseFindOpen: RomeExit() has already been called.");
        ASSERT_OR_SETERROR_AND_RETURN_NULL(pDatabase->IsOpen(),    "RomeDatabaseFindOpen: database not open.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pDatabase->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,           "RomeDatabaseFindOpen: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // pszPattern is allowed to be NULL or empty.
        RX_DBFIND_ASSERT_LEGAL_FLAGS(nFindFlags)

#if USE_LOG_FILES
	    CLogFileElement log(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeDatabaseFindOpen", "args='%s' flags='%d'>\n", (CString)XMLEncode(pszPattern), nFindFlags);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf2(LOG_SHELL, "RomeDatabaseFindOpen \"%s\" %d\n", (CString)pszPattern, nFindFlags);
#endif
#endif // USE_LOG_FILES

	    DBFIND* pFind = DbFindOpen(pDatabase->GetDatalink(), pszPattern, nFindFlags);
#if USE_LOG_FILES
	    if (log.Logged()) LogFilePrintf1(LOG_HIST, "<output find='0x%08X'/>\n", (UINT)pFind);
#endif
        ASSERT_OR_SETERROR_AND_RETURN_NULL(pFind,                  "RomeDatabaseFindOpen: NULL find context pointer.");
	    return pFind;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeDatabaseFindOpen: exception for Pattern = '%s', nFlags = 0x%X.", CString(pszPattern), nFindFlags);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeDatabaseFindOpen: exception in catch block.");
        }
    }
}


//! Close a find result set returned by RomeDatabaseFindOpen().
//! Result sets are dynamic objects that must be closed when you are finished using them.
//! @param pDbFind is the result set returned by RomeDatabaseFindOpen().
//!
//! @warning Failing to close the find result may leak memory or other program resources.
//! @see RomeDatabaseFindOpen().
//! @RomeAPI Wrapper for DbFindClose().
//!
ROME_API RT_void RomeDatabaseFindClose(RT_DBFIND* pDbFind)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN(pDbFind,              "RomeDatabaseFindClose: NULL find pointer.");
//		BOOL bValidApp = (&pApp == &App);
//      ASSERT_OR_SETERROR_AND_RETURN(bValidApp,            "RomeDatabaseFindClose: invalid database pointer.");
		BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN(!bExited,             "RomeDatabaseFindClose: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (App.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN(!bSameThread,         "RomeDatabaseFindClose: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement1(LOGELEM_HIST, "user", "RomeDatabaseFindClose", "find='0x%08X'/>\n", (UINT)pDbFind);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeDatabaseFindClose %d\n", (UINT)pDbFind);
#endif

	    DbFindClose(pDbFind);
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeDatabaseFindClose: exception for Find = 0x%X.", pDbFind);
            ASSERT_OR_SETERROR_AND_RETURN(0,        sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN(0,        "RomeDatabaseFindClose: exception in catch block.");
        }
    }
}


//! Return the number of items in the find result set.
//! @param pDbFind  The find result set returned by RomeDatabaseFindOpen().
//! @return  0 on error or if none found.
//!
//! @see RomeDatabaseFindOpen(), RomeDatabaseFindClose().
//! @RomeAPI Wrapper for DbFindCount().
//!
ROME_API RT_INT RomeDatabaseFindCount(RT_DBFIND* pDbFind)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_ZERO(pDbFind,         "RomeDatabaseFindCount: NULL find pointer.");
//		BOOL bValidApp = (&pApp == &App);
//      ASSERT_OR_SETERROR_AND_RETURN_ZERO(bValidApp,       "RomeDatabaseFindCount: invalid database pointer.");
		BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(!bExited,        "RomeDatabaseFindCount: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (App.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(!bSameThread,    "RomeDatabaseFindCount: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement1(LOGELEM_HIST, "user", "RomeDatabaseFindCount", "find='0x%08X'/>\n", (UINT)pDbFind);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeDatabaseFindCount %d\n", (UINT)pDbFind);
#endif

	    int nCount = DbFindCount(pDbFind);
	    return nCount;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeDatabaseFindCount: exception for Find = 0x%X.", pDbFind);
            ASSERT_OR_SETERROR_AND_RETURN_ZERO(0, sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_ZERO(0, "RomeDatabaseFindCount: exception in catch block.");
        }
    }
}


//! Access indivIdual results in a find result set.
//! @param pDbFind    The find result set returned by RomeDatabaseFindOpen().
//! @param nIndex     The index of the result to access.
//! @param nInfoType  The type of information to get from the result.
//! - #RX_DBFILEINFO_QUERY	 0 // info: String result of the SQL query.
//!                            // (For internal / testing use only.)
//! - #RX_DBFILEINFO_NAME	 1 // info:                name
//! - #RX_DBFILEINFO_PATH	 2 // info:         path
//! - #RX_DBFILEINFO_RIGHT	 3 // info:         path \ name
//! - #RX_DBFILEINFO_TABLE	 4 // info: table
//! - #RX_DBFILEINFO_OUTER	 5 // info: table \        name
//! - #RX_DBFILEINFO_LEFT	 6 // info: table \ path
//! - #RX_DBFILEINFO_FULL	 7 // info: table \ path \ name
//! - #RX_DBFILEINFO_OWNER	 8 // info: owner field
//! - #RX_DBFILEINFO_GROUP	 9 // info: group field
//! - #RX_DBFILEINFO_PERMS	10 // info: perms field
//! - #RX_DBFILEINFO_DATE	11 // info: date  field
//! - #RX_DBFILEINFO_DATA	12 // info: data  field
//! - #RX_DBFILEINFO_FOLDER	13 // info: is this a folder? return "0" or "1".
//!
//! @return  A value which depends on the type of information asked for.
//!   Returns NULL on error.
//!
//! @see RomeDatabaseFindOpen(), RomeDatabaseFindClose().
//! @RomeAPI Wrapper for DbFindInfo().
//!
ROME_API RT_CSTR RomeDatabaseFindInfo(RT_DBFIND* pDbFind, RT_INT nIndex, RT_UINT nInfoType)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pDbFind,            "RomeDatabaseFindInfo: NULL find pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_NULL(nIndex >= 0,        "RomeDatabaseFindInfo: negative index.");
//		BOOL bValidApp = (&pApp == &App);
//      ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,          "RomeDatabaseFindInfo: invalid database pointer.");
		BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,           "RomeDatabaseFindInfo: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (App.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,       "RomeDatabaseFindInfo: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement3(LOGELEM_HIST, "user", "RomeDatabaseFindInfo", "find='0x%08X' index='%d' type='%d'/>\n", (UINT)pDbFind, nIndex, nInfoType);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf2(LOG_SHELL, "RomeDatabaseFindInfo %d %d %d\n", (UINT)pDbFind, nIndex, nInfoType);
#endif

	    long nItem = DbFindSeek(pDbFind, nIndex);
	    if (nItem < 0)
		    return NULL;

	    LPCSTR pszInfo = DbFindInfo(pDbFind, nInfoType);
	    return pszInfo;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeDatabaseFindInfo: exception for Find = 0x%X, Index = %d, Type = %d.", pDbFind, nIndex, nInfoType);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0, sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0, "RomeDatabaseFindInfo: exception in catch block.");
        }
    }
}


//! Access indivIdual results in a find result set.
//! See RomeDatabaseFindInfo() [above] for full documentation.
//!
//! This version is required for use by Intel Fortran, which can't use functions
//!   which return a string pointer. Instead it must have its first 2 arguments
//!   be a pointer to a buffer and its length, to return the string in.
//!   The function must have a return type of void.
//! @param[out] pBuf The pointer to a buffer to return a NUL-terminated string in.
//! @param nBufLen   The length of the buffer to return a NUL-terminated string in.
//! @param pDbFind   The find result set returned by RomeDatabaseFindOpen().
//! @param nIndex    The index of the result to access.
//! @param nInfoType The type of information to get from the result.
//!
//! @RomeAPI Wrapper for RomeDatabaseFindInfo().
//! @FortranAPI This function allows calling RomeDatabaseFindInfo() from Fortran.
//!
ROME_API RT_void RomeDatabaseFindInfoF(RT_PCHAR pBuf, RT_UINT nBufLen, RT_DBFIND* pDbFind, RT_INT nIndex, RT_UINT nInfoType)
{
    try
    {
    	// Does not require AFX_MANAGE_STATE() macro.

	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

		// Validate arguments unique to the Fortran function version.
		// The remaining arguments will be validated in the normal Rome API call.
        ASSERT_OR_SETERROR_AND_RETURN(pBuf,           "RomeDatabaseFindInfoF: NULL buffer pointer.");
        ASSERT_OR_SETERROR_AND_RETURN(nBufLen > 0,    "RomeDatabaseFindInfoF: non-positive buffer length.");

        ROME_API_LOCK();

        // Does not require command logging.

        LPCSTR pszInfo = RomeDatabaseFindInfo(pDbFind, nIndex, nInfoType);
        CopyStrF(pBuf, nBufLen, pszInfo);
    }
    catch (...)
    {
        try
        {
            if (pBuf && nBufLen > 0)
                *pBuf = 0;
            CString sInfo; sInfo.Format("RomeDatabaseFindInfoF: exception for Buffer = '0x%08X', Length = %d, Index = %d, Type = %d.", pBuf, nBufLen, nIndex, nInfoType);
            ASSERT_OR_SETERROR_AND_RETURN(0,     sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN(0,     "RomeDatabaseFindInfoF: exception in catch block.");
        }
    }
}


//! @} // Rome Database search functions
/////////////////////////////////////////////////////////////////////////////
//! @name Rome Engine functions
//! @{

//! Finish running the update stack until it is empty.
//! This should occur regardless of whether AutoUpdate is on,
//!   and should leave the AutoUpdate state unchanged.
//! This is especially important when reloading files.
//! @param pEngine  The Rome engine interface pointer obtained from RomeGetEngine().
//! @return  RX_TRUE on success, RX_FALSE on failure.
//!
//! @see RomeEngineRun(), RomeEngineLockUpdate(), RomeEngineSetAutorun().
//! @RomeAPI Wrapper for CEngineBase::FinishUpdates().
//!
ROME_API RT_BOOL RomeEngineFinishUpdates(RT_Engine* pEngine)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pEngine,            "RomeEngineFinishUpdates: NULL engine pointer.");
        BOOL bValidEngine = (&pEngine->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidEngine,       "RomeEngineFinishUpdates: invalid Rome engine pointer.");
		BOOL bExited = pEngine->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,           "RomeEngineFinishUpdates: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pEngine->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,       "RomeEngineFinishUpdates: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement0(LOGELEM_HIST, "user", "RomeEngineFinishUpdates", "/>\n");
#if USE_ROMESHELL_LOGGING
        LogFilePrintf0(LOG_SHELL, "RomeEngineFinishUpdates\n");
#endif

	    pEngine->FinishUpdates();

	    return RX_TRUE;
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(0, "RomeEngineFinishUpdates: exception.");
    }
}


//! Get the Autorun state for the Rome engine.
//! When the autorun flag is set, after each value changes the engine will recalculate
//!   the outputs, leading to much slower performance.
//! Note that you can also avoid autoupdating in some cases by changing
//!   the values in an "auxiliary" file like a SOIL, and then changing
//!   the main PROFILE's ptr to use that SOIL file after all changes have
//!   been made, which only then causes the updating to happen.
//! @param pEngine  The Rome engine interface pointer obtained from RomeGetEngine().
//! @return  The autorun state (RX_TRUE or RX_FALSE), or #RX_FAILURE on error.
//!
//! @see  RomeEngineFinishUpdates(), RomeEngineRun(), RomeEngineSetAutorun().
//! @RomeAPI Wrapper for CEngineBase::IsUpdating().
//!
ROME_API RT_BOOL RomeEngineGetAutorun(RT_Engine* pEngine)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pEngine,            "RomeEngineGetAutorun: NULL engine pointer.");
        BOOL bValidEngine = (&pEngine->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidEngine,       "RomeEngineGetAutorun: invalid Rome engine pointer.");
		BOOL bExited = pEngine->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,           "RomeEngineGetAutorun: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pEngine->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeEngineGetAutorun: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        CLogFileElement0(LOGELEM_HIST, "user", "RomeEngineGetAutorun", "/>\n");
#if USE_ROMESHELL_LOGGING
        LogFilePrintf0(LOG_SHELL, "RomeEngineGetAutorun\n");
#endif

        return pEngine->IsUpdating();
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, "RomeEngineGetAutorun: exception.");
    }
}


//! Is the engine locked?
//! @param pEngine  The Rome engine interface pointer obtained from RomeGetEngine().
//! @return  #RX_FAILURE on error.
//!
//! @see RomeEngineLockUpdate(), RomeEngineUnlockUpdate().
//! @RomeAPI Wrapper for CEngineBase::IsLocked().
//!
ROME_API RT_BOOL RomeEngineIsLocked(RT_Engine* pEngine)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pEngine,            "RomeEngineIsLocked: NULL engine pointer.");
        BOOL bValidEngine = (&pEngine->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidEngine,       "RomeEngineIsLocked: invalid Rome engine pointer.");
		BOOL bExited = pEngine->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,           "RomeEngineIsLocked: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pEngine->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeEngineIsLocked: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        // Don't log this function - it gets called too many times and floods the log file.
//    	CLogFileElement0(LOGELEM_HIST, "user", "RomeEngineIsLocked", "/>\n");

	    return pEngine->IsLocked();
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, "RomeEngineIsLocked: exception.");
    }
}


//! Lock the engine from running.
//! This increments a lock count.
//! The new lock count is returned.
//! @param pEngine  The Rome engine interface pointer obtained from RomeGetEngine().
//! @return #RX_FAILURE on error.
//!
//! @see RomeEngineIsLocked(), RomeEngineUnlockUpdate().
//! @RomeAPI Wrapper for CEngineBase::LockUpdate().
//!
ROME_API RT_INT RomeEngineLockUpdate(RT_Engine* pEngine)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pEngine,            "RomeEngineLockUpdate: NULL engine pointer.");
        BOOL bValidEngine = (&pEngine->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidEngine,       "RomeEngineLockUpdate: invalid Rome engine pointer.");
		BOOL bExited = pEngine->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,           "RomeEngineLockUpdate: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pEngine->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeEngineLockUpdate: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        // Don't log this function - it gets called too many times and floods the log file.
//      CLogFileElement0(LOGELEM_HIST, "user", "RomeEngineLockUpdate", "/>\n");

	    return pEngine->LockUpdate();
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, "RomeEngineLockUpdate: exception.");
    }
}


//! Unlock the engine to allow running.
//! This decrements the lock count. The engine will not run if more locks remain.
//! @param pEngine  The Rome engine interface pointer obtained from RomeGetEngine().
//! @return  The new lock count, or #RX_FAILURE (-1) on error.
//!
//! @see RomeEngineIsLocked(), RomeEngineLockUpdate().
//! @RomeAPI Wrapper for CEngineBase::UnlockUpdate().
//!
ROME_API RT_INT RomeEngineUnlockUpdate(RT_Engine* pEngine)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pEngine,            "RomeEngineUnlockUpdate: NULL engine pointer.");
        BOOL bValidEngine = (&pEngine->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidEngine,       "RomeEngineUnlockUpdate: invalid Rome engine pointer.");
		BOOL bExited = pEngine->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,           "RomeEngineUnlockUpdate: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pEngine->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeEngineUnlockUpdate: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        // Don't log this function - it gets called too many times and floods the log file.
//    	CLogFileElement0(LOGELEM_HIST, "user", "RomeEngineUnlockUpdate", "/>\n");

	    return pEngine->UnlockUpdate();
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, "RomeEngineUnlockUpdate: exception.");
    }
}


//! Run the engine until done.
//! This may not produce any change if autocalc is already on.
//! @param pEngine  The Rome engine interface pointer obtained from RomeGetEngine().
//! @return #RX_FAILURE (-1) on error.
//!
//! @see RomeEngineLockUpdate(), RomeEngineSetAutorun().
//! @RomeAPI Wrapper for CEngineBase::Run().
//!
ROME_API RT_BOOL RomeEngineRun(RT_Engine* pEngine)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pEngine,            "RomeEngineRun: NULL engine pointer.");
        BOOL bValidEngine = (&pEngine->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidEngine,       "RomeEngineRun: invalid Rome engine pointer.");
		BOOL bExited = pEngine->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,           "RomeEngineRun: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pEngine->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeEngineRun: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement0(LOGELEM_HIST, "user", "RomeEngineRun", "/>\n");
#if USE_ROMESHELL_LOGGING
        LogFilePrintf0(LOG_SHELL, "RomeEngineRun\n");
#endif

	    RT_BOOL bRun = pEngine->Run();
        return bRun;
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, "RomeEngineRun: exception.");
    }
}


//! Set the Autorun flag in Rusle2.
//! When the autorun flag is set, after each value changes it will recalculate
//!   the outputs, leading to much slower performance.
//! @param pEngine  The Rome engine interface pointer obtained from RomeGetEngine().
//! @param bAutorun The new autorun state to set.
//!
//! @see RomeEngineGetAutorun(), RomeEngineRun().
//! @RomeAPI Wrapper for CEngineBase::SetUpdating().
//!
ROME_API RT_void RomeEngineSetAutorun(RT_Engine* pEngine, RT_BOOL bAutorun)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN(pEngine,                    "RomeEngineSetAutorun: NULL engine pointer.");
        BOOL bValidEngine = (&pEngine->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN(bValidEngine,               "RomeEngineSetAutorun: invalid Rome engine pointer.");
		BOOL bExited = pEngine->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN(!bExited,                   "RomeEngineSetAutorun: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pEngine->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN(!bSameThread,               "RomeEngineSetAutorun: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement1(LOGELEM_HIST, "user", "RomeEngineSetAutorun", "flags='%d'/>\n", bAutorun);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeEngineSetAutorun %d\n", bAutorun);
#endif

	    const UINT nFlags = (bAutorun? UPDATE_ON: UPDATE_OFF) | UPDATE_SHOW | UPDATE_USER;
	    pEngine->SetUpdating(nFlags);
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN(0, "RomeEngineSetAutorun: exception.");
    }
}


//! Set whether the computational engine should display the calc function
//!   names in the status bar.
//! @note Messages are now locked using a locked count, so unlocking
//!   must be done at least as many times as locking in order to show messages.
//! This function may still be called as if the lock state was a simple
//!   boolean, and should still work the same if calls aren't nested.
//! @note Changed to lock messages with a FALSE argument, which matches
//!   the old behavior in the import DLLs.
//! @param pEngine  The Rome engine interface pointer obtained from RomeGetEngine().
//! @param bShowMessages Boolean value for whether to lock showing messages (FALSE)
//!    or unlock showing messages (TRUE).
//!
//! @return  The old locked state, or #RX_FAILURE on error.
//! @RomeAPI Wrapper for CEngineBase::ShowStatus().
//!
ROME_API RT_BOOL RomeEngineShowStatus(RT_Engine* pEngine, RT_BOOL bShowMessages)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pEngine,            "RomeEngineShowStatus: NULL engine pointer.");
        BOOL bValidEngine = (&pEngine->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidEngine,       "RomeEngineShowStatus: invalid Rome engine pointer.");
		BOOL bExited = pEngine->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,           "RomeEngineShowStatus: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pEngine->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeEngineShowStatus: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    CLogFileElement1(LOGELEM_HIST, "user", "RomeEngineShowStatus", "flags='%d'/>\n", bShowMessages);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "//RomeEngineShowStatus %d\n", bShowMessages);
#endif

	    return pEngine->ShowStatus(bShowMessages);
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, "RomeEngineShowStatus: exception.");
    }
}


//! @} // Rome Engine functions
/////////////////////////////////////////////////////////////////////////////
//! @name Rome File functions
//! @{

//! Close an open file in the Rome filesystem.
//! This also deletes a top-level file itself unless it is of type #OBJT_NOCLOSE or #OBJT_NOCLOSE_LAZY,
//!   or it is a temporary file.
//! @param pFile pointer to a Rome file.
//! @return  RX_TRUE if the file was closed/reloaded, otherwise RX_FALSE.
//!   Returns #RX_FAILURE on error.
//!
//! @see RomeFilesGetItem(), RomeFilesOpen(), RomeFilesCloseAll().
//! @RomeAPI Wrapper for CFileObj::CloseView().
//!
ROME_API RT_BOOL RomeFileClose(CFileObj* pFile)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pFile,            "RomeFileClose: NULL file pointer.");
        CRomeCore& Core = pFile->Core;
        BOOL bValidApp = (&Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,        "RomeFileClose: invalid file system pointer.");
		BOOL bExited = Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,         "RomeFileClose: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidFile,       "RomeFileClose: invalid file pointer.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,     "RomeFileClose: Rome API function called on different thread from RomeInit().");
#endif

#ifdef USE_ROMEAPI_REFCOUNT
        //! @note This decrements the reference count of times this pointer is returned by the Rome API.
        //! The file will not actually be closed until this count drops to 0.
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidRefs,       "RomeFileClose: invalid file reference count.");
        VERIFY(pFile->m_nRomeRefs-- >= 1);
#endif

        ROME_API_LOCK();

        CString sFile = pFile->GetFileName();
	    CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileClose", "file='%s'>\n", XMLEncode(sFile));
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeFileClose \"%s\"\n", sFile);
#endif

		// Verify that the engine is finished before we alter the filesystem.
		ASSERT(Core.Engine.IsFinished());

        // Close the file without saving, if it has no open references..
	    return pFile->CloseView(CVF_NOSAVE);
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0, "RomeFileClose: exception.");
    }
}


//! @deprecated This function should not be used.
//!   Use RomeFileClose() instead.
//!
//! Delete a Rome file from memory.
//! @param pFile pointer to a Rome file.
//! @return #RX_FAILURE on error.
//!
//! @RomeAPI Wrapper for CFileSys::DeleteFile().
//!
ROME_API RT_BOOL RomeFileDelete(CFileObj* pFile)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pFile,            "RomeFileDelete: NULL file pointer.");
        BOOL bValidApp = (&pFile->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,        "RomeFileDelete: invalid file system pointer.");
		BOOL bExited = pFile->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,         "RomeFileDelete: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidFile,       "RomeFileDelete: invalid file pointer.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,     "RomeFileDelete: Rome API function called on different thread from RomeInit().");
#endif

#ifdef USE_ROMEAPI_REFCOUNT
        //! @note This bypasses the reference count of times this pointer is returned by the Rome API.
        //! Normally the file would not be closed until this count drops to 0. However, this function forces the file to close.
        VERIFY(pFile->m_nRomeRefs-- >= 1);
#endif

        ROME_API_LOCK();

        CString sFile = pFile->GetFileName();
	    CLogFileElement1(LOGELEM_HIST, "user", "RomeFileDelete", "file='%s'/>\n", XMLEncode(sFile));
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeFileDelete \"%s\"\n", sFile);
#endif

	    pFile->Core.Files.DeleteFile(pFile);
	    return RX_TRUE;
    }
    catch (...)
    {
        try
        {
            CString sFile = pFile? pFile->GetFileName(): "NULL";
            CString sInfo; sInfo.Format("RomeFileDelete: exception for File = '%s'.", sFile);
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,     sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,     "RomeFileDelete: exception in catch block.");
        }
    }
}


//! Return the [unique] instance of a named parameter in a given object.
//! This will return an existing instance if there already is one.
//! @param pFile    The Rome file to get the attr in.
//! @param pszAttr  The parameter name used by the catalog (e.g. "CLAY").
//!   This can be a 'long' attr name with a remote prefix.
//!
//! @return  A pointer to the attr in the given file, or NULL on failure.
//! @RomeAPI Wrapper for FindOrCreate().
//!
ROME_API RT_Attr* RomeFileGetAttr(CFileObj* pFile, RT_CNAME pszAttr)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pFile,             "RomeFileGetAttr: NULL file pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!strempty(pszAttr),"RomeFileGetAttr: empty attr name.");
        BOOL bValidApp = (&pFile->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,         "RomeFileGetAttr: invalid file pointer.");
		BOOL bExited = pFile->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,          "RomeFileGetAttr: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidFile,        "RomeFileGetAttr: invalid file pointer.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidRefs,        "RomeFileGetAttr: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,      "RomeFileGetAttr: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        CString sFile = pFile->GetFileName();
	    CLogFileElement2(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileGetAttr", "file='%s' attr='%s'>\n", XMLEncode(sFile), pszAttr);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf2(LOG_SHELL, "//RomeFileGetAttr \"%s\" \"%s\"\n", sFile, pszAttr);
#endif

        FILEOBJ_READLOCK(pFile);
	    return FindOrCreate(pszAttr, pFile);
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFileGetAttr: exception for File = '0x%08X', Attr = '%s'.", pFile, (CString)pszAttr);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,     sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,     "RomeFileGetAttr: exception in catch block.");
        }
    }
}


//! Get the size of an attribute's dimension.
//! This will create an attr that doesn't exist yet.
//! The attr must be requested in the correct file type.
//! @param pFile    The Rome file to get the attr in.
//! @param pszAttr  The parameter name used by the catalog (e.g. "CLAY").
//! @param nDim     The 0-based index of the dimension (in [0, #MAXDIMNUM]).
//! @return  The attr's dimension size, or #RX_FAILURE (-1) on error.
//!
//! @note This will accept a long attr name (with remote prefix - e.g. "#RD:SOIL_PTR:CLAY")
//! @see RomeCatalogGetAttrDimCount(), RomeFileGetAttrSizeEx().
//! @RomeAPI Wrapper for CDimensions::GetSize().
//!
ROME_API RT_INT RomeFileGetAttrDimSize(CFileObj* pFile, RT_CNAME pszAttr, RT_INT nDim)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pFile,              "RomeFileGetAttrDimSize: NULL file pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!strempty(pszAttr), "RomeFileGetAttrDimSize: empty attr name.");
        CRomeCore& Core = pFile->Core;
        BOOL bValidApp = (&Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,             "RomeFileGetAttrDimSize: invalid file pointer.");
		BOOL bExited = Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,              "RomeFileGetAttrDimSize: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidFile,         "RomeFileGetAttrDimSize: invalid file pointer.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidRefs,         "RomeFileGetAttrDimSize: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeFileGetAttrDimSize: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Wait for the stack to finish.
        // This makes sure that a size retrieved below won't get changed by functions on the stack.
	    Core.Engine.FinishUpdates();

	    FILEOBJ_READLOCK(pFile);

        CString sFile = pFile->GetFileName();
	    CLogFileElement2(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileGetAttrDimSize", "file='%s' attr='%s'>\n", XMLEncode(sFile), pszAttr);
#if USE_ROMESHELL_LOGGING
        LogShellActivate(sFile);
        LogFilePrintf1(LOG_SHELL, "RomeFileGetAttrDimSize \"%s\"\n", pszAttr);
#endif

	    // Find the attribute in the file.
	    CAttr* pAttr = FindOrCreate(pszAttr, pFile);
	    ATTR_READLOCK(pAttr);

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pAttr,              "+RomeFileGetAttrDimSize: failed to create attr.");

		// Verify that the engine is finished before we get information back from the model.
	    Core.Engine.FinishUpdates();

        int nSize = pAttr->dimensions.GetSize(nDim);

        return nSize;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFileGetAttrDimSize: exception for File = '0x%08X', Attr = '%s'.", pFile, (CString)pszAttr);
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    "RomeFileGetAttrDimSize: exception in catch block.");
        }
    }
}


//! @deprecated Use RomeFileGetAttrSizeEx() instead.
//!
//! Get the size of an attribute, returned as a @b short integer.
//! This will create an attr that doesn't exist yet.
//! The attr must be requested in the correct file type.
//! @param pFile    The Rome file to get the attr in.
//! @param pszAttr  The parameter name used by the catalog (e.g. "CLAY").
//! @return  The attr size, or #RX_FAILURE (-1) on error.
//!
//! @warning this function cannot return a size greater than 32767.
//!   For attrs which might exceed that size, use RomeFileGetAttrSizeEx().
//!   If the size is greater than 32767, this function will return #RX_FAILURE
//!   instead and set an error message which can be retrieved by RomeGetLastError().
//! @see RomeFileGetAttrSizeEx(), RomeFileSetAttrSize().
//! @RomeAPI Wrapper for CAttr::GetSize().
//!
ROME_API RT_SHORT RomeFileGetAttrSize(CFileObj* pFile, RT_CNAME pszAttr)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pFile,              "RomeFileGetAttrSize: NULL file pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!strempty(pszAttr), "RomeFileGetAttrSize: empty attr name.");
        CRomeCore& Core = pFile->Core;
        BOOL bValidApp = (&Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,             "RomeFileGetAttrSize: invalid file pointer.");
		BOOL bExited = Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,              "RomeFileGetAttrSize: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidFile,         "RomeFileGetAttrSize: invalid file pointer.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidRefs,         "RomeFileGetAttrSize: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeFileGetAttrSize: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Wait for the stack to finish.
        // This makes sure that a size retrieved below won't get changed by functions on the stack.
	    Core.Engine.FinishUpdates();

	    FILEOBJ_READLOCK(pFile);

        CString sFile = pFile->GetFileName();
	    CLogFileElement2(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileGetAttrSize", "file='%s' attr='%s'>\n", XMLEncode(sFile), pszAttr);
#if USE_ROMESHELL_LOGGING
        LogShellActivate(sFile);
        LogFilePrintf1(LOG_SHELL, "RomeFileGetAttrSize \"%s\"\n", pszAttr);
#endif

	    // Find the attribute in the file.
	    CAttr* pAttr = FindOrCreate(pszAttr, pFile);
#if USE_ROMEAPI_ZEROATTRSIZE
        //! @note If symbol #USE_ROMEAPI_ZEROATTRSIZE is defined, and the attr is not found,
        //!   return 0 if this is a legal parameter name.
        //! This allows gracefully handling asking for a parameter in a "polymorphic" object type,
        //!   when that object type is empty (e.g. "OP_PROCESS_NO_EFFECT").
        //! The size of an attr that exists will never be 0.
        if (pAttr == NULL && pFile->IsEmpty() && Core.AttrCatalog.GetListing(pszAttr))
            return 0;
#endif
	    ATTR_READLOCK(pAttr);

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pAttr,              "+RomeFileGetAttrSize: failed to create attr.");

		// Verify that the engine is finished before we get information back from the model.
	    Core.Engine.FinishUpdates();

        int nSize = pAttr->GetSize();

        RT_SHORT xSize = (RT_SHORT)nSize;
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(nSize == (int)xSize, "RomeFileGetAttrSize: size is too large to cast to short.");

        return (RT_SHORT)nSize;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFileGetAttrSize: exception for File = '0x%08X', Attr = '%s'.", pFile, (CString)pszAttr);
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    "RomeFileGetAttrSize: exception in catch block.");
        }
    }
}


//! Get the size of an attribute, returned as a @b long integer.
//! This will create an attr that doesn't exist yet.
//! The attr must be requested in the correct file type.
//! @param pFile    The Rome file to get the attr in.
//! @param pszAttr  The parameter name used by the catalog (e.g. "CLAY").
//! @return  The attr size, or #RX_FAILURE (-1) on error.
//!
//! @warning this function is required when the size will exceed 32767.
//! @see RomeFileGetAttrSize(), RomeFileSetAttrSize()
//! @RomeAPI Wrapper for CAttr::GetSize().
//!
ROME_API RT_INT RomeFileGetAttrSizeEx(CFileObj* pFile, RT_CNAME pszAttr)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pFile,              "RomeFileGetAttrSizeEx: NULL file pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!strempty(pszAttr), "RomeFileGetAttrSizeEx: empty attr name.");
        CRomeCore& Core = pFile->Core;
        BOOL bValidApp = (&Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,          "RomeFileGetAttrSizeEx: invalid file pointer.");
		BOOL bExited = Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,           "RomeFileGetAttrSizeEx: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidFile,         "RomeFileGetAttrSizeEx: invalid file pointer.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidRefs,         "RomeFileGetAttrSizeEx: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeFileGetAttrSizeEx: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Wait for the stack to finish.
        // This makes sure that a size retrieved below won't get changed by functions on the stack.
	    Core.Engine.FinishUpdates();

	    FILEOBJ_READLOCK(pFile);

        CString sFile = pFile->GetFileName();
#if USE_LOG_FILES
	    CLogFileElement log(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileGetAttrSizeEx", "file='%s' attr='%s'>\n", XMLEncode(sFile), pszAttr);
#if USE_ROMESHELL_LOGGING
        LogShellActivate(sFile);
        LogFilePrintf1(LOG_SHELL, "RomeFileGetAttrSizeEx \"%s\"\n", pszAttr);
#endif
#endif // USE_LOG_FILES

	    // Find the attribute in the file.
	    CAttr* pAttr = FindOrCreate(pszAttr, pFile);
#if USE_ROMEAPI_ZEROATTRSIZE
        //! @note If symbol #USE_ROMEAPI_ZEROATTRSIZE is defined, and the attr is not found,
        //!   return 0 if this is a legal parameter name.
        //! This allows gracefully handling asking for a parameter in a "polymorphic" object type,
        //!   when that object type is empty (e.g. "OP_PROCESS_NO_EFFECT").
        //! The size of an attr that exists will never be 0.
        if (pAttr == NULL && pFile->IsEmpty() && Core.AttrCatalog.GetListing(pszAttr))
            return 0;
#endif
	    ATTR_READLOCK(pAttr);

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pAttr,              "RomeFileGetAttrSizeEx: failed to create attr.");

		// Verify that the engine is finished before we get information back from the model.
	    Core.Engine.FinishUpdates();

        int nSize = pAttr->GetSize();

#if USE_LOG_FILES
        // Log this size if it's not the default.
        if (nSize != 1 && log.Logged())
	        LogFilePrintf1(LOG_HIST, "<value s='%d'/>", nSize);
#endif

        return nSize;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFileGetAttrSizeEx: exception for File = '0x%08X', Attr = '%s'.", pFile, (CString)pszAttr);
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    "RomeFileGetAttrSizeEx: exception in catch block.");
        }
    }
}


//! Get the "value" string for an attribute, not the "display" string.
//!   Note: this string should not exceed #MAX_SETSTR_SIZE in length.
//! @note This will create an attr that doesn't exist yet.
//! @note The attr must be requested in the correct file type.
//! @note This function will use the unit and variant from the current template.
//! @param pFile   The Rome file to get the attr in.
//! @param pszAttr The parameter name used by the catalog (e.g. "CLAY").
//! @param nIndex  The "flat" index (use 0 for a 1x1 attr).
//! @return  A string pointer for the value at the given index.
//!   Returns NULL on error, including out-of-range index.
//!
//! @warning This function cannot handle an index greater than 32767.
//! @since 2007-08-10  -1 returns the "current" index of the parameter.
//! @see RomeFileSetAttrValue().
//! @RomeAPI Wrapper for AttrGetStr().
//!
ROME_API RT_CSTR RomeFileGetAttrValue(CFileObj* pFile, RT_CNAME pszAttr, RT_INT nIndex)
{
	RT_CSTR retVal = RomeFileGetAttrValueAux(pFile, pszAttr, nIndex, RX_VARIANT_CATALOG, "#U_TEMPLATE");
	return retVal;
}


ROME_API RT_CSTR RomeFileGetAttrValueAux(CFileObj* pFile, RT_CNAME pszAttr, RT_INT nIndex, RT_UINT nVariant, RT_CNAME pszUnit)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pFile,              "RomeFileGetAttrValue: NULL file pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!strempty(pszAttr), "RomeFileGetAttrValue: empty attr name.");
        BOOL bValidIndex = ((nIndex >= 0) || (nIndex == -1));
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidIndex,        "RomeFileGetAttrValue: invalid index.");
        CRomeCore& Core = pFile->Core;
        BOOL bValidApp = (&Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,          "RomeFileGetAttrValue: invalid file pointer.");
		BOOL bExited = Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,           "RomeFileGetAttrValue: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidFile,         "RomeFileGetAttrValue: invalid file pointer.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidRefs,         "RomeFileGetAttrValue: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,       "RomeFileGetAttrValue: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Wait for the stack to finish.
        // This makes sure that a value retrieved below won't get changed by functions on the stack.
	    Core.Engine.FinishUpdates();

        CString sFile = pFile->GetFileName();
	    CLogFileElement3(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileGetAttrValue", "file='%s' attr='%s' index='%d'>\n", XMLEncode(sFile), pszAttr, nIndex);
#if USE_ROMESHELL_LOGGING
        LogShellActivate(sFile);
        LogFilePrintf4(LOG_SHELL, "RomeFileGetAttrValue \"%s\" %d\n", pszAttr, nIndex);
#endif

        // Find the attribute in the file.
	    CAttr* pAttr = FindOrCreate(pszAttr, pFile);

	    if (!pAttr)
        {
            // The attr name must be listed in the catalog.
            CListing* pAttrListing = Core.AttrCatalog.GetListing(pszAttr);
            BOOL bValidAttrName = (pAttrListing != NULL);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidAttrName,     "RomeFileGetAttrValue: no Rusle2 parameter of that name.");

            // The attr must be asked for in the correct object type.
            LPCSTR pszObjName = pFile->GetObjType()->GetName();
            BOOL bValidObjType = pAttrListing->IsValidObject(pszObjName);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidObjType,      "RomeFileGetAttrValue: Rusle2 parameter asked for in wrong object type.");

            // If not handled above, give a generic error message.
            ASSERT_OR_SETERROR_AND_RETURN_NULL(pAttr,              "RomeFileGetAttrValue: failed to create attr.");
        }

        // Set the active object for debugging purposes.
        Core.SetActiveObj(pAttr->GetObj());

        // Get the current index for the attribute.
        if (nIndex == -1)
        {
            int      nCurrent = AttrGetIndex(pAttr, 0);
            CString& sCurrent = RomeThreadGetNamedString("RomeFileGetAttrValue");
            sCurrent = Int2Str(nCurrent);
            return sCurrent;
        }

		// Verify that the engine is finished before we get information back from the model.
	    Core.Engine.FinishUpdates();

		ASSERT_OR_RETURN_FALSE(pAttr->IsValidUnits(pszUnit));
		if (strlen(pszUnit) == 0) // need to use the default
			pszUnit = "#U_TEMPLATE";

	    // Get the "value" string from the attribute.
	    LPCSTR pszValue = AttrGetStr(pAttr, nIndex, nVariant, pszUnit);
//		LPCSTR pszValue = AttrGetStr(pAttr, nIndex, RX_VARIANT_TEMPLATE, "#U_TEMPLATE");
		if (pszValue == NULL)
            pszValue = "NULL";

        // Log this for debugging purposes.
        CString sIndex;
        if (nIndex > 0) sIndex.Format(" index='%d'", nIndex);
        CString sUnit;
#if USE_USER_TEMPLATES
        LPCSTR pszPrefUnit = pAttr->GetPrefUnit();
        LPCSTR pszDefUnit  = pAttr->GetDefUnit();
        if (!streq(pszPrefUnit, pszDefUnit))
            sUnit.Format(" unit='%s'", pszPrefUnit);
#endif
        Core.SetActiveObj(pAttr->GetObj());
        CLogFileElement4(LOGELEM_HIST, "user", "AttrGetStr", "attr='%s'%s><value s='%s'%s/></user>\n",
                            pAttr->GetName(), sIndex, pszValue, sUnit);

        ASSERT(MAX_SETSTR_SIZE < 0 || strlen(pszValue) <= MAX_SETSTR_SIZE);
        return pszValue;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFileGetAttrValue: exception for File = '0x%08X', Attr = '%s', Index = %d.", pFile, (CString)pszAttr, (int)nIndex);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeFileGetAttrValue: exception in catch block.");
        }
    }
}


//! Get the "value" string for an attribute, not display text.
//! See RomeFileGetAttrValue() [above] for full documentation.
//!
//! This version is required for use by Intel Fortran, which can't use functions
//!   which return a string pointer. Instead it must have its first 2 arguments
//!   be a pointer to a buffer and its length, to return the string in.
//!   The function must have a return type of void.
//! @param[out] pBuf  The pointer to a buffer to return a NUL-terminated string in.
//! @param nBufLen    The length of the buffer to return a NUL-terminated string in.
//! @param pFile      The pointer to a Rome file.
//! @param pszAttr    The name of the parameter to get the value for.
//! @param nIndex     The "flat" index (use 0 for a 1x1 attr).
//!
//! @warning This function cannot handle an index greater than 32767.
//! @since 2007-08-10  -1 returns the "current" index of the parameter.
//! @RomeAPI Wrapper for RomeFileGetAttrValue().
//! @FortranAPI This function allows calling RomeFileGetAttrValue() from Fortran.
//!
ROME_API RT_void RomeFileGetAttrValueF(RT_PCHAR pBuf, RT_UINT nBufLen, RT_FileObj* pFile, RT_CNAME pszAttr, RT_SHORT nIndex)
{
    try
    {
    	// Does not require AFX_MANAGE_STATE() macro.

	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

		// Validate arguments unique to the Fortran function version.
		// The remaining arguments will be validated in the normal Rome API call.
        ASSERT_OR_SETERROR_AND_RETURN(pBuf,           "RomeFileGetAttrValueF: NULL buffer pointer.");
        ASSERT_OR_SETERROR_AND_RETURN(nBufLen > 0,    "RomeFileGetAttrValueF: non-positive buffer length.");

        ROME_API_LOCK();

        // Does not require command logging.

        LPCSTR pszValue = RomeFileGetAttrValue(pFile, pszAttr, nIndex);
        CopyStrF(pBuf, nBufLen, pszValue);
    }
    catch (...)
    {
        try
        {
            if (pBuf && nBufLen > 0)
                *pBuf = 0;
            CString sInfo; sInfo.Format("RomeFileGetAttrValueF: exception for Buffer = '0x%08X', Length = %d, Attr = '%s', Index = %d.", pBuf, nBufLen, (CString)pszAttr, (int)nIndex);
            ASSERT_OR_SETERROR_AND_RETURN(0,         sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN(0,         "RomeFileGetAttrValueF: exception in catch block.");
        }
    }
}


//! Get an array of floating point values.
//! @param pFile          The pointer to a Rome file.
//! @param pszAttr        The name of the parameter to get the values for.
//! @param[out] pArray    The array to place the values in. This must be at least as large as the size in @p pSize.
//! @param[in,out] pSize  (in)The size of the array to hold the values.
//!                       If this size is too small, the size required will be set and this will return FALSE.
//!                       (out) The number of data values returned in @p pArray.
//! @param nVariant       The variant to get the values in.
//! - #RX_VARIANT_INTERVAL      Represents the value stored at an index.
//!                             This is the default variant (#RX_VARIANT_DEFAULT) for all parameters.
//! - #RX_VARIANT_CUMULATIVE    Represents the sum of all values up to and including that index.
//!                             This is currently the only other supported variant.
//! - #RX_VARIANT_TEMPLATE (-1) Used in API functions to mean the variant specified
//!                             by the current user template.
//! - #RX_VARIANT_CATALOG  (-2) Used in API functions to mean the variant specified
//!                             by the catalog. Currently this is always #RX_VARIANT_INTERVAL.
//! @param pszUnit        The unit to retrieve the values in. Allowable values depend on the actual parameter.
//!                       An empty string will use the catalog unit.
//! @return RX_TRUE on success, RX_FALSE on failure.
//!
//! @note This will create an attr that doesn't exist yet.
//! @note The attr must be requested in the correct file type.
//! @see RomeFileGetAttrValue(), RomeFileGetAttrSizeEx(), RomeFileSetFloatArray().
//! @RomeAPI Wrapper for AttrGetFloatArray().
//!
ROME_API RT_BOOL RomeFileGetFloatArray(RT_FileObj* pFile, RT_CNAME pszAttr, RT_REAL* pArray, RT_INT* pSize, RT_UINT nVariant /*= RX_VARIANT_CATALOG*/, RT_CNAME pszUnit /*= ""*/)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pFile,              "RomeFileGetFloatArray: NULL file pointer.");
        CRomeCore& Core = pFile->Core;
        BOOL bValidApp = (&Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,          "RomeFileGetFloatArray: invalid file pointer.");
		BOOL bExited = Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,           "RomeFileGetFloatArray: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidFile,         "RomeFileGetFloatArray: invalid file pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!strempty(pszAttr), "RomeFileGetFloatArray: empty attr name.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pArray,             "RomeFileGetFloatArray: NULL array pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pSize,              "RomeFileGetFloatArray: NULL size pointer.");
        const int nArraySize = *pSize;
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(nArraySize,         "RomeFileGetFloatArray: non-positive size.");
        BOOL bValidVariant = VariantIsValid(nVariant, TRUE);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidVariant,      "RomeFileGetFloatArray: invalid variant.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidRefs,         "RomeFileGetFloatArray: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,       "RomeFileGetFloatArray: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Wait for the stack to finish.
        // This makes sure that a value retrieved below won't get changed by functions on the stack.
	    Core.Engine.FinishUpdates();

        CString sFile = pFile->GetFileName();
	    CLogFileElement3(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileGetFloatArray", "file='%s' attr='%s' size='%d'>\n", XMLEncode(sFile), pszAttr, *pSize);
#if USE_ROMESHELL_LOGGING
        LogShellActivate(sFile);
        LogFilePrintf4(LOG_SHELL, "//RomeFileGetFloatArray \"%s\" %d %d \"%s\"\n", pszAttr, *pSize, nVariant, (CString)pszUnit);
#endif

        // Find the attribute in the file.
	    CAttr* pAttr = FindOrCreate(pszAttr, pFile);
	    if (!pAttr)
		    return RX_FALSE;

        // Set the active object for debugging purposes.
        Core.SetActiveObj(pAttr->GetObj());

		// Verify that the engine is finished before we get information back from the model.
		ASSERT(Core.Engine.IsFinished());

        RT_BOOL bSet = AttrGetFloatArray(pAttr, pArray, pSize, nVariant, pszUnit);
        return bSet;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFileGetFloatArray: exception for File = '0x%08X', Attr = '%s', Size = %d.", pFile, (CString)pszAttr, (int)*pSize);
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    "RomeFileGetFloatArray: exception in catch block.");
        }
    }
}


//! Get the full filename of a file object.
//! This will include the table prefix.
//!   Example: "climates\default".
//! @param pFile  A pointer to a Rome file.
//! @return A pointer to the filename, or NULL on failure.
//!
//! @RomeAPI Wrapper for CFileObj::GetFileName().
//!
ROME_API RT_CSTR RomeFileGetFullname(CFileObj* pFile)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pFile,              "RomeFileGetFullname: NULL file pointer.");
        BOOL bValidApp = (&pFile->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,          "RomeFileGetFullname: invalid file pointer.");
		BOOL bExited = pFile->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,           "RomeFileGetFullname: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidFile,         "RomeFileGetFullname: invalid file pointer.");
        LPCSTR pszFile = pFile->GetFileName();
        ASSERT_OR_SETERROR_AND_RETURN_NULL(pszFile,            "RomeFileGetFullname: NULL filename pointer.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidRefs,         "RomeFileGetFullname: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,       "RomeFileGetFullname: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        CLogFileElement1(LOGELEM_HIST, "user", "RomeFileGetFullname", "file='%s'/>\n", XMLEncode(pszFile));
#if USE_ROMESHELL_LOGGING
        LogShellActivate(pszFile);
        LogFilePrintf1(LOG_SHELL, "//RomeFileGetFullname \"%s\"\n", pszFile);
#endif

        return pszFile;
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeFileGetFullname: exception.");
    }
}


//! Return the full filename of a file object.
//! See RomeFileGetFullname() [above] for full documentation.
//!
//! This version is required for use by Intel Fortran, which can't use functions
//!   which return a string pointer. Instead it must have its first 2 arguments
//!   be a pointer to a buffer and its length, to return the string in.
//!   The function must have a return type of void.
//! @param[out] pBuf is the pointer to a buffer to return a NUL-terminated string in.
//! @param nBufLen is the length of the buffer to return a NUL-terminated string in.
//! @param pFile pointer to a Rome file.
//!
//! @RomeAPI Wrapper for RomeFileGetFullname().
//! @FortranAPI This function allows calling RomeFileGetFullname() from Fortran.
//!
ROME_API RT_void RomeFileGetFullnameF(RT_PCHAR pBuf, RT_UINT nBufLen, RT_FileObj* pFile)
{
    try
    {
        // Does not require AFX_MANAGE_STATE() macro.

        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

		// Validate arguments unique to the Fortran function version.
		// The remaining arguments will be validated in the normal Rome API call.
        ASSERT_OR_SETERROR_AND_RETURN(pBuf,           "RomeFileGetFullnameF: NULL buffer pointer.");
        ASSERT_OR_SETERROR_AND_RETURN(nBufLen > 0,    "RomeFileGetFullnameF: non-positive buffer length.");

        ROME_API_LOCK();

        // Does not require command logging.

        LPCSTR pszName = RomeFileGetFullname(pFile);
        CString sName;
        sName = pszName;
        CopyStrF(pBuf, nBufLen, pszName);
    }
    catch (...)
    {
        try
        {
            if (pBuf && nBufLen > 0)
                *pBuf = 0;
            CString sInfo; sInfo.Format("RomeFileGetFullnameF: exception for Buffer = '0x%08X', Length = %d, File = '0x%08X'.", pBuf, nBufLen, pFile);
            ASSERT_OR_SETERROR_AND_RETURN(0,        sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN(0,        "RomeFileGetFullnameF: exception in catch block.");
        }
    }
}


//! Perform an action using the Rome listener interface.
//! @param pFile         A pointer to a Rome file.
//! @param nActionType   The action to perform (e.g. #RX_LISTENER_ADD).
//! @param pObserver     The observer that will listen for the events.
//! @param pEventHandler The function to invoke for the events being listened for.
//!
//! @warning this is currently for internal use only.
//! @return  RX_TRUE (success) or RX_FALSE (failure).
//! @RomeAPI
//!
ROME_API RT_BOOL RomeFile_Listener(RT_FileObj* pFile, RT_UINT nActionType, RT_void* pObserver, RT_EventHandler pEventHandler)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_RETURN_FALSE(pFile);
        BOOL bValidApp = (&pFile->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,        "RomeFile_Listener: invalid file pointer.");
		BOOL bExited = pFile->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,         "RomeFile_Listener: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidFile,       "RomeFile_Listener: invalid file pointer.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidRefs,       "RomeFile_Listener: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,     "RomeFile_Listener: Rome API function called on different thread from RomeInit().");
#endif

        ASSERT((nActionType & RX_LISTENER_ACTION_MASK) == nActionType);
        // TODO: test that this is the correct object type.

        ROME_API_LOCK();

        BOOL bRet = RX_FALSE;

        switch (nActionType)
        {
            UNUSED2(pObserver, pEventHandler);
/*
            case RX_LISTENER_ADD:
                bRet = pFile->RomeFilesSet.Add(pObserver, pEventHandler);
                break;
            case RX_LISTENER_REMOVE:
                bRet = pFile->RomeFilesSet.Remove(pObserver);
		        // Call OnFinalRelease to check if the file can be deleted.
                pFile->OnFinalRelease();
                break;
            case RX_LISTENER_REMOVEALL:
                bRet = pFile->RomeFilesSet.RemoveAll(pObserver);
		        // Call OnFinalRelease to check if the file can be deleted.
                pFile->OnFinalRelease();
                break;
*/
            case RX_LISTENER_ADD:
            case RX_LISTENER_REMOVE:
            case RX_LISTENER_REMOVEALL:
            default:
                ASSERT(FALSE);
                break;
        }
        return bRet;
    }
    catch (...)
    {
        try
        {
            CString sFile = pFile? pFile->GetFileName(): "NULL";
            CString sInfo; sInfo.Format("RomeFile_Listener: exception for File = '%s', Action = %d", sFile, nActionType);
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,   sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,   "RomeFile_Listener: exception in catch block.");
        }
    }
}


//! Save a file object to its current location.
//! @param pFile  Apointer to a Rome file.
//! @return  RX_TRUE on success, #RX_FAILURE (-1) on error.
//!
//! @see RomeFilesOpen(), RomeFileSaveAs(), RomeFileSaveAsEx().
//! @RomeAPI Wrapper for RomeFileSaveAs().
//!
ROME_API RT_INT RomeFileSave(CFileObj* pFile)
{
    try
    {
        // Note: this function doesn't require resource locking.

	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pFile,          "RomeFileSave: NULL file pointer.");
        BOOL bValidApp = (&pFile->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,      "RomeFileSave: invalid file pointer.");
		BOOL bExited = pFile->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,       "RomeFileSave: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidFile,     "RomeFileSave: invalid file pointer.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidRefs,     "RomeFileSave: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,   "RomeFileSave: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        CString sFile = pFile->GetFileName();
	    CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileSave", "file='%s'>\n", XMLEncode(sFile));
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeFileSave \"%s\"\n", sFile);
#endif

        return RomeFileSaveAs(pFile, pFile->GetFileName());
    }
    catch (...)
    {
        try
        {
            CString sFile = pFile? pFile->GetFileName(): "NULL";
            CString sInfo; sInfo.Format("RomeFileSave: exception for File = '%s'", sFile);
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    "RomeFileSave: exception in catch block.");
        }
    }
}


//! Save this file to the database under a specific name.
//! Mark the file as clean after saving to the database.
//! @param pFile  A pointer to a Rome file.
//! @param pszNewName  The fullname of the file to save as.
//! - Example: "profiles\working\farm1".
//! - This can be an external file if prefix "#XML:" is used.<br>
//!     Example: "#XML:C:\Rusle2\Export\profile1.pro.xml".
//! - This can be an external file if prefix "#SKEL:" is used.<br>
//!     Example: "#SKEL:C:\Rusle2\Export\management1.man.skel".
//! @return  RX_TRUE on success, RX_FALSE on failure, #RX_FAILURE on error.
//!
//! @see RomeFilesOpen(), RomeFileSave(), RomeFileSaveAsEx().
//! @RomeAPI Wrapper for RomeFileSaveAsEx().
//!
ROME_API RT_BOOL RomeFileSaveAs(CFileObj* pFile, RT_CSTR pszNewName)
{
    // Note: this function doesn't require the AFX_MANAGE_STATE macro.
    // Note: this function doesn't require exception handling.
    // Note: this function doesn't require resource locking.
    // Note: this function doesn't require command logging.

	return RomeFileSaveAsEx(pFile, pszNewName, 0);
}


//! Save this file to the database under a specific name.
//! Mark the file as clean after saving to the database.
//! @param pFile  A pointer to a Rome file.
//! @param pszNewName  The fullname of the file to save as.
//! - Example: "profiles\working\farm1".
//! - This can be an external file if prefix "#XML:" is used.<br>
//!     Example: "#XML:C:\Rusle2\Export\profile1.pro.xml".
//! - This can be an external file if prefix "#SKEL:" is used.<br>
//!     Example: "#SKEL:C:\Rusle2\Export\management1.man.skel".
//! - This can be an external fileset if prefix "#FILESET:" is used.<br>
//!     Example: "#FILESET:C:\Rusle2\Export\profile1.fileset.xml".
//! @param nFlags  Flags which modify saving behavior.
//! - #RX_FILE_SAVEASEX_CALC   Save calculated data in \<Calc> tags
//!
//! @return  RX_TRUE on success, RX_FALSE on failure, #RX_FAILURE on error.
//!
//! @see RomeFilesOpen(), RomeFileSave(), RomeFileSaveAs().
//! @RomeAPI Wrapper for CFileSys::Save().
//!
ROME_API RT_BOOL RomeFileSaveAsEx(CFileObj* pFile, RT_CSTR pszNewName, RT_UINT nFlags)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pFile,                 "RomeFileSaveAsEx: NULL file pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!strempty(pszNewName), "RomeFileSaveAsEx: empty attr name.");
        CRomeCore& Core = pFile->Core;
        BOOL bValidApp = (&Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,             "RomeFileSaveAsEx: invalid file pointer.");
		BOOL bExited = Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,              "RomeFileSaveAsEx: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidFile,            "RomeFileSaveAsEx: invalid file pointer.");
#if TARGET_ROMEDLL && defined(USE_ROMEAPI_REFCOUNT)
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidRefs,            "RomeFileSaveAsEx: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,          "RomeFileSaveAsEx: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        // Wait for the stack to finish.
        // This makes sure that changes that would be made by functions on the stack
        //   are done before the file is saved.
	    Core.Engine.FinishUpdates();

	    FILEOBJ_WRITELOCK(pFile);

        CString sFile = pFile->GetFileName();
        CLogFileElement2(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileSaveAsEx", "file='%s' new='%s'>\n", XMLEncode(sFile), XMLEncode(pszNewName));
#if USE_ROMESHELL_LOGGING
        LogFilePrintf3(LOG_SHELL, "RomeFileSaveAsEx \"%s\" \"%s\" %d\n", sFile, pszNewName, nFlags);
#endif

	    // The external filename, with any magic prefix stripped.
	    LPCSTR pszDiskName = "";
	    UINT   nExpFlags = 0;

	    // Check for magic prefixes and strip them off:
	    // "#XML:"      export as Rusle2 XML format.
	    // "#SKEL:"     export as NRCS management skeleton format.
        // "#FILESET:"  export as Rusle2 full fileset XML format.
        if (strncmp(pszNewName, "#XML:", 5) == 0)
	    {
		    // Get the external filename following the magic prefix.
		    pszDiskName = pszNewName + 5;

		    nExpFlags = EXPORTOBJECT_FORMAT_XML_NEW | EXPORTOBJECT_QUIET | ((nFlags & RX_FILE_SAVEASEX_CALC)? EXPORTOBJECT_CALC: 0);
	    }
#if USE_SKELETONS
	    else
	    if (strncmp(pszNewName, "#SKEL:", 6) == 0)
	    {
		    // Get the external filename following the magic prefix.
		    pszDiskName = pszNewName + 6;

		    nExpFlags = EXPORTOBJECT_FORMAT_MAN_CFF | EXPORTOBJECT_QUIET;
	    }
#endif // USE_SKELETONS
#if USE_FILESETS
	    else
	    if (strncmp(pszNewName, "#FILELIST:", 10) == 0)
	    {
		    // Get the external filename following the magic prefix.
		    pszDiskName = pszNewName + 10;

            // Do the export.
            UINT nFilesetFlags  = 0/*EXPORTFILESET_COUNTS*/  // Output counts (unsupported)
                                | EXPORTFILESET_META_SET    // Output metadata for the fileset
                                | EXPORTFILESET_META_FILE   // Output metadata for each file
                                | EXPORTFILESET_LIST_DEPS   // Output dependent files
                                ;
            CString sBaseFile = pFile->GetFileName();
            CString sFilesetArgs = "purpose=\"dependents\" basefile=\"" + sBaseFile + "\"";

            RT_BOOL nRet = FilesetExport(Core, pszDiskName, nFilesetFlags, sFilesetArgs);
            return nRet;
	    }
	    else
	    if (strncmp(pszNewName, "#FILESET:", 9) == 0)
	    {
		    // Get the external filename following the magic prefix.
		    pszDiskName = pszNewName + 9;

            // Do the export.
            UINT nFilesetFlags  = 0/*EXPORTFILESET_COUNTS*/  // Output counts (unsupported)
                                | EXPORTFILESET_META_SET    // Output metadata for the fileset
                                | EXPORTFILESET_META_FILE   // Output metadata for each file
                                | EXPORTFILESET_LIST_DEPS   // Output dependent files
                                | EXPORTFILESET_DATA_OPEN   // Output XML data for open files
                                | EXPORTFILESET_DATA_DB     // Output XML data for database files
                                ;
            if (nFlags & RX_FILE_SAVEASEX_CALC)
                nFilesetFlags |= EXPORTFILESET_CALC_OPEN;
            CString sBaseFile = pFile->GetFileName();
            CString sFilesetArgs = "purpose=\"dependents\" basefile=\"" + sBaseFile + "\"";

            RT_BOOL nRet = FilesetExport(Core, pszDiskName, nFilesetFlags, sFilesetArgs);
            return nRet;
	    }
#endif // USE_FILESETS
	    else
	    {
		    // Use a flag to signify saving INTO the database.
		    nExpFlags = EXPORTOBJECT_FORMAT_XML_OLD;
	    }

	    RT_BOOL nRet = RX_FALSE;
        UINT nFormat = (nExpFlags & EXPORTOBJECT_FORMAT_MASK);
	    switch (nFormat)
	    {
		    case EXPORTOBJECT_FORMAT_XML_NEW:
			    nRet = ExportObject(Core, pFile, pszDiskName, nExpFlags);
			    break;
#if USE_SKELETONS
		    case EXPORTOBJECT_FORMAT_MAN_CFF:
			    nRet = ExportObject(Core, pFile, pszDiskName, nExpFlags);
			    break;
#endif // USE_SKELETONS
		    case EXPORTOBJECT_FORMAT_XML_OLD:
		    {
			    LPCSTR pszOldName = pFile->GetFileName();
                Core.SetActiveObj(pFile);
			    CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "FileSaveAs", "file='%s'>\n", XMLEncode(pszOldName));
//			    CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "SaveAs", "file='%s'>\n",   XMLEncode(pszNewName));

			    const UINT nSaveFlags = MoveFlag(~nFlags, RX_FILE_SAVEASEX_PRIVATE, FSF_MARKCLEAN | FSF_SAVE)
                                      | MoveFlag( nFlags, RX_FILE_SAVEASEX_PRIVATE, FSF_PRIVATE)
                                      | MoveFlag( nFlags, RX_FILE_SAVEASEX_CALC,    FSF_CALC)
                                      ;
			    nRet = Core.Files.Save(pFile, pszNewName, nSaveFlags);

			    break;
		    }
		    default:
                ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    "RomeFileSaveAsEx: unknown flags argument.");
			    break;
	    }
	    ASSERT(nRet);

	    return nRet;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFileSaveAsEx: exception for File = '0x%08X', NewName = '%s', Flags = %d.", pFile, (CString)pszNewName, nFlags);
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    "RomeFileSaveAsEx: exception in catch block.");
        }
    }
}


//! Set the root size of an attribute.
//! This will create an attr that doesn't exist yet.
//! The attr must be requested in the correct file type.
//!
//! @param pFile        pointer to a Rome file.
//! @param pszAttr		The internal attr name (e.g. "CLAY").
//! @param nNewSize		The new size (must be > 0).
//!
//! @return  RX_TRUE (1) if the size changed, RX_FALSE (0) if unchanged, #RX_FAILURE (-1) on error.
//!
//! @warning This function cannot handle a size of greater than 32767.
//! @note  This uses return type #RT_SHORT instead of #RT_BOOL to return a signed value.
//! @see RomeFileGetAttrSizeEx().
//! @RomeAPI Wrapper for UserCmdResizeDim().
//!
ROME_API RT_SHORT RomeFileSetAttrSize(CFileObj* pFile, RT_CNAME pszAttr, RT_INT nNewSize)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pFile,                  "RomeFileSetAttrSize: NULL file pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!strempty(pszAttr),     "RomeFileSetAttrSize: empty attr name.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(nNewSize >=0,           "RomeFileSetAttrSize: negative size.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(nNewSize !=0,           "RomeFileSetAttrSize: zero size.");
        CRomeCore& Core = pFile->Core;
        BOOL bValidApp = (&Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,              "RomeFileSetAttrSize: invalid file pointer.");
		BOOL bExited = Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,               "RomeFileSetAttrSize: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidFile,             "RomeFileSetAttrSize: invalid file pointer.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidRefs,             "RomeFileSetAttrSize: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,           "RomeFileSetAttrSize: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Wait for the stack to finish.
        // This makes sure that a change below won't get overwritten by functions on the stack.
	    Core.Engine.FinishUpdates();

	    FILEOBJ_READLOCK(pFile);

        // Set the active object.
        pFile->Core.SetActiveObj(pFile);

        CString sFile = pFile->GetFileName();
        CLogFileElement3(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileSetAttrSize", "file='%s' attr='%s' size='%d'>\n", (CString)XMLEncode(sFile), XMLEncode(pszAttr), nNewSize);
#if USE_ROMESHELL_LOGGING
        LogShellActivate(sFile);
        LogFilePrintf2(LOG_SHELL, "RomeFileSetAttrSize \"%s\" %d\n", pszAttr, nNewSize);
#endif

	    // Find the attribute in the file.
	    CAttr* pAttr = FindOrCreate(pszAttr, pFile);
	    ATTR_WRITELOCK(pAttr);

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pAttr,                  "RomeFileSetAttrSize: failed to create attr.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pAttr->IsDimension(),   "RomeFileSetAttrSize: cannot resize a non-dimension attr.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pAttr->CanUserResize(), "RomeFileSetAttrSize: the attr cannot be resized.");

		// Verify that the engine is finished before we get information back from the model.
        Core.Engine.FinishUpdates();

	    const int nOldSize   = pAttr->GetSize();
        const int nDeltaSize = nNewSize - nOldSize;
#if 1
        const BOOL bDelete   = (nDeltaSize < 0);
        const int nNumRows   = abs(nDeltaSize);
		//! @note If we are deleting, resize by repeatedly deleting the last index.
		//! If we are inserting, resize by repeatedly inserting after the last index.
		int nIndex = bDelete? nOldSize - 1: nOldSize;
		for (int i = nNumRows; i > 0; i--)
        {
            UserCmdResizeDim(pAttr, "", nIndex, bDelete);
            bDelete? nIndex--: nIndex++;
        }
#else
	    pAttr->SetRootSize(nNewSize);
#endif

        ASSERT(Core.GetActiveObj() == pAttr->GetObj());
	    CLogFileElement3(LOGELEM_HIST, "user", "AttrSetSize", "attr='%s'><new s='%d'/><old s='%d'/></user>\n",
                                pAttr->GetName(), nNewSize, nOldSize);

	    return (nOldSize != nNewSize);
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFileSetAttrSize: exception for File = '0x%08X', Attr = '%s', Size = %d.", pFile, (CString)pszAttr, (int)nNewSize);
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    "RomeFileSetAttrSize: exception in catch block.");
        }
    }
}


//! Set the value string for an attribute.
//! The attr must be requested in the correct file type.
//! This function will use the unit and variant from the current template.
//! This will create an attr that doesn't exist yet.
//!
//! @param pFile    A pointer to a Rome file.
//! @param pszAttr  Internal attr name (e.g. "CLAY")
//! @param pszValue	The "value" string, @em not the "display" string.<br>
//!                 Note: this string cannot exceed #MAX_SETSTR_SIZE in length.
//!                 This can take on special values:
//! -               "#INSERT" - insert at [before] the index
//! -               "#DELETE" - remove at the index
//!
//! @note The allowed values and formats depends on the type of the parameter begin set:
//! - #ATTR_BOL  Boolean parameters (class CBoolean) take the values "YES", "NO", "0", "1", and "NaN".
//!   These strings are case-insensitive. The numeric versions must match exactly.
//!   A string like "0.0" or ".0" will not be recognized.
//! - #ATTR_DTE  Date parameters (class CDate) take values values are Rusle2 simulation dates, not real calendar dates.
//!   These values are in the current (template) variant and unit.
//!   "NaN" is accepted for a missing value.
//! - #ATTR_FLT  Floating point parameters (class CFloat) take values in standard floating point format.
//!   Examples: 2, -5, 800.3. Scientific notation is also accepted. Examples: 2E5, -2.8E-8.
//!   "NaN" is accepted for a missing value.
//!   These values are in the current (template) variant and unit.
//! - #ATTR_INT  Integer parameters (stored in class CFloat) take signed integer values.
//!   Examples: 2, -5. Scientific notation is also accepted as long as they represent an integer. Examples: 2E5, but NOT -2.8E-8.
//!   "NaN" is accepted for a missing value.
//!   Integer values generally do not use variant and unit.
//! - #ATTR_LST  List parameters (class CListAttr) accept a finite set of string values specified in the catalog.
//!   "NaN" is accepted for a missing value.
//! - #ATTR_PTR  Pointer paramters (class CPtrAttr) accept internal Rusle2 filenames.
//!   An empty string ("") is accepted for a missing (#NaN) value.
//!   These filenames have their root folder (e.g. "profiles", "climates") stripped off.
//!   That information is specified for the parameter by the catalog.
//! - - "#ENTRY_CUSTOM" signifies that that data element has been modified by the user from
//!      the choice they previously made, but it is distinct from "#ENTRY_NONE".
//! - - "#ENTRY_DEFAULT" signifies to use the default file stored in the template instead.
//! - - "#ENTRY_MODEL" signifies to use the hard-coded default file generate by the model.
//! - - "#ENTRY_NONE" is used to specify an "empty" value set by the user (not the same as NaN).
//! - - "#ENTRY_NULL" is used to signify that that data element is unset (not the same as NaN).
//! - #ATTR_STR  String parameters (class CStrAttr) accept any string.
//!   An empty string ("") is accepted for a missing (#NaN) value.
//!
//! @param nIndex	the "flat" index (use 0 for a 1x1 attr).
//!   @warning This function cannot handle an index of greater than 32767.
//!
//! @return  RX_TRUE (1) if the value changed, RX_FALSE (0) if unchanged, #RX_FAILURE (-1) on error.
//!
//! @note This uses return type #RT_SHORT instead of #RT_BOOL to return a signed value.
//! @see RomeFileGetAttrValue(), RomeFileSetAttrValue().
//! @see #ENTRY_CUSTOM, #ENTRY_DEFAULT, #ENTRY_INTERNAL, #ENTRY_NONE, #ENTRY_MIXED, #ENTRY_MODEL, #ENTRY_NULL.
//! @RomeAPI  Wrapper for DoCmdSetStr(), UserCmdResizeDim().
//!
ROME_API RT_SHORT RomeFileSetAttrValue(CFileObj* pFile, RT_CNAME pszAttr, RT_CSTR pszValue, RT_INT nIndex)
{
	RT_SHORT retVal = RomeFileSetAttrValueAux(pFile, pszAttr, pszValue, nIndex, RX_VARIANT_CATALOG, "#U_TEMPLATE");
	return retVal;
}


ROME_API RT_SHORT RomeFileSetAttrValueAux(CFileObj* pFile, RT_CNAME pszAttr, RT_CSTR pszValue, RT_INT nIndex, RT_UINT nVariant, RT_CNAME pszUnit)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pFile,              "RomeFileSetAttrValue: NULL file pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!strempty(pszAttr), "RomeFileSetAttrValue: empty attr name.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pszValue,           "RomeFileSetAttrValue: NULL value pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(nIndex >= 0,        "RomeFileSetAttrValue: negative index.");
        BOOL bValidSize = (MAX_SETSTR_SIZE <= 0) || (strlen(pszValue) <= MAX_SETSTR_SIZE);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidSize,         "RomeFileSetAttrValue: value string exceeds MAX_SETSTR_SIZE.");
        CRomeCore& Core = pFile->Core;
        BOOL bValidApp = (&Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,          "RomeFileSetAttrValue: invalid file pointer.");
		BOOL bExited = Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,           "RomeFileSetAttrValue: RomeExit() has already been called.");
        BOOL bValidFile = CFileObj::IsValid(pFile);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidFile,         "RomeFileSetAttrValue: invalid file pointer.");
#ifdef USE_ROMEAPI_REFCOUNT
        BOOL bValidRefs = (pFile->m_nRomeRefs >= 1);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidRefs,         "RomeFileSetAttrValue: invalid file reference count.");
#endif
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,       "RomeFileSetAttrValue: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Wait for the stack to finish.
        // This makes sure that a value changed below won't get overwritten by functions on the stack.
	    Core.Engine.FinishUpdates();

	    RT_SHORT nRet = 0;

        {
            CString sFile = pFile->GetFileName();
            CLogFileElement4(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFileSetAttrValue", "file='%s' attr='%s' value='%s' index='%d'>\n", (CString)XMLEncode(sFile), pszAttr, pszValue, nIndex);
#if USE_ROMESHELL_LOGGING
            LogShellActivate(sFile);
            LogFilePrintf3(LOG_SHELL, "RomeFileSetAttrValue \"%s\" \"%s\" %d\n", pszAttr, pszValue, nIndex);
#endif

			CAttr* pAttr = NULL;
			{
				// @note Use CUpdateLock to lock the engine while we are creating the parameter.
                // We don't want the engine running during web building done during creation of a new parameter.
                // Parameter creation will run calc functions directly and throw others on the stack.
                // @warning We must restrict the scope of this lock to just the parameter creation.
                //   There is a FinishUpdates() call below which requires an unlocked state to run correctly.
				CUpdateLock lock;

				// Find the attribute in the file and create it if it doesn't exist.
				pAttr = FindOrCreate(pszAttr, pFile);
			}

            if (!pAttr)
            {
                // The attr name must be listed in the catalog.
                CListing* pAttrListing = Core.AttrCatalog.GetListing(pszAttr);
                BOOL bValidAttrName = (pAttrListing != NULL);
                ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidAttrName,     "RomeFileSetAttrValue: no Rusle2 parameter of that name.");

                // The attr must be asked for in the correct object type.
                LPCSTR pszObjName = pFile->GetObjType()->GetName();
                BOOL bValidObjType = pAttrListing->IsValidObject(pszObjName);
                ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidObjType,      "RomeFileSetAttrValue: Rusle2 parameter asked for in wrong object type.");

                // If not handled above, give a generic error message.
                ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pAttr,              "RomeFileSetAttrValue: failed to create attr.");
            }

            // Set the active object for debugging purposes.
            Core.SetActiveObj(pAttr->GetObj());

			// Verify that the engine is finished before we alter the model.
            Core.Engine.FinishUpdates();

			ASSERT_OR_RETURN_FALSE(pAttr->IsValidUnits(pszUnit));
			if (strlen(pszUnit) == 0) // need to use the default
				pszUnit = "#U_TEMPLATE";

	        // Handle special values for INSERT and DELETE first.
		    if (strieq(pszValue, "#INSERT"))
		    {
//			    nRet = (RT_SHORT)AttrDoInsert(pAttr, nIndex, SIF_UNDOINFO | SIF_EXTERNAL | SIF_QUIET);
	            // Insert in the dimension -- this will cause all dependent attrs to resize.
	            // Insertion is now handled by the command class CCmdResizeDim.
                CAttr* pDim = pAttr->dimensions.GetDimPtr(0);
	            nRet = (RT_SHORT)UserCmdResizeDim(pDim, "", nIndex, FALSE /*INSERT*/);
		    }
		    else
		    if (strieq(pszValue, "#DELETE"))
		    {
//			    nRet = (RT_SHORT)AttrDoRemove(pAttr, nIndex, SIF_UNDOINFO | SIF_EXTERNAL | SIF_QUIET);
	            // Delete in the dimension -- this will cause all dependent attrs to resize.
	            // Deletion is now handled by the command class CCmdResizeDim.
                CAttr* pDim = pAttr->dimensions.GetDimPtr(0);
	            nRet = (RT_SHORT)UserCmdResizeDim(pDim, "", nIndex, TRUE /*DELETE*/);
		    }
            else
            {
    	        nRet = (RT_SHORT)::DoCmdSetStr(pAttr, pszValue, nIndex, SIF_UNDOINFO | SIF_EXTERNAL | SIF_QUIET, nVariant, pszUnit);
//				nRet = (RT_SHORT)::DoCmdSetStr(pAttr, pszValue, nIndex, SIF_UNDOINFO | SIF_EXTERNAL | SIF_QUIET);
			}
        }

	    return nRet;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFileSetAttrValue: exception for File = '0x%08X', Attr = '%s', Value = '%s', Index = %d.", pFile, (CString)pszAttr, (CString)pszValue, (int)nIndex);
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,    "RomeFileSetAttrValue: exception in catch block.");
        }
    }
}


//! Perform an action using the Rome listener interface.
//! @warning this is currently for internal use only.
//! @param pObj           A pointer to a Rome [sub]object.
//! @param nActionType    The action to perform (e.g. #RX_LISTENER_ADD).
//! @param pObserver      The observer that will listen for the events.
//! @param pEventHandler  The function to invoke for the events being listened for.
//!
//! @return RX_TRUE (success) or RX_FALSE (failure).
//! @RomeAPI
//!
ROME_API RT_BOOL RomeObj_Listener(RT_SubObj* pObj, RT_UINT nActionType, RT_void* pObserver, RT_EventHandler pEventHandler)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pObj,                   "RomeObj_Listener: NULL object pointer.");
        ASSERT((nActionType & RX_LISTENER_ACTION_MASK) == nActionType);
        // TODO: test that this is the correct object type.
        BOOL bValidApp = (&pObj->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,              "RomeObj_Listener: invalid object pointer.");
		BOOL bExited = pObj->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,               "RomeObj_Listener: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pObj->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,           "RomeObj_Listener: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        BOOL bRet = RX_FALSE;

        switch (nActionType)
        {
            UNUSED2(pObserver, pEventHandler);
/*
            case RX_LISTENER_ADD:
                bRet = pObj->ListenerSet.Add(pObserver, pEventHandler);
                break;
            case RX_LISTENER_REMOVE:
                bRet = pObj->ListenerSet.Remove(pObserver);
                break;
            case RX_LISTENER_REMOVEALL:
                bRet = pObj->ListenerSet.RemoveAll(pObserver);
                break;
*/
            case RX_LISTENER_ADD:
            case RX_LISTENER_REMOVE:
            case RX_LISTENER_REMOVEALL:
            default:
                ASSERT(FALSE);
                break;
        }

        return bRet;
    }
    catch (...)
    {
        try
        {
            CString sObj = pObj? pObj->GetDisplayTitle(): "NULL";
            CString sInfo; sInfo.Format("RomeObj_Listener: exception for Obj = '%s', Action = %d.", sObj, nActionType);
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    "RomeObj_Listener: exception in catch block.");
        }
    }
}


//! @} // Rome File functions
/////////////////////////////////////////////////////////////////////////////
//! @name Rome Filesystem functions
//! @{

//! Create and open a new Rome file.
//! This creates a new file not stored in the database.
//! @param pFiles       A pointer to the Rome filesystem interface returned by RomeGetFiles().
//! @param pszObjType   The internal name of the object type.<br>
//!    Example: "CLIMATE".
//! @param pszFullname  The full pathname of the file, including object table name.<br>
//!    Example: "climates\Tennessee\Knoxville".
//!
//! @see RomeFilesOpen() for opening a file from the database.
//! @return pointer to the new Rome file, or NULL on failure.
//! @RomeAPI Wrapper for CRomeFiles::NewFileObj().
//!
ROME_API RT_FileObj* RomeFilesAdd(RT_Files* pFiles, RT_CNAME pszObjType, RT_CSTR pszFullname)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pFiles,             "RomeFilesAdd: NULL file system pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_NULL(pszObjType,         "RomeFilesAdd: NULL object name.");
        ASSERT_OR_SETERROR_AND_RETURN_NULL(pszFullname,        "RomeFilesAdd: NULL file name.");
        BOOL bValidApp = (&pFiles->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,          "RomeFilesAdd: invalid file pointer.");
		BOOL bExited = pFiles->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,           "RomeFilesAdd: RomeExit() has already been called.");
        BOOL bValidFiles = CFileSys::IsValid(pFiles);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidFiles,        "RomeFilesAdd: invalid file system pointer.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFiles->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,       "RomeFilesAdd: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

	    // Make local CString copies to be able to pass references to CFileObj constructor.
	    CString sObjType = pszObjType;
	    CString sFullname = pszFullname;

	    FILESYS_WRITELOCK();

        CLogFileElement2(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFilesAdd", "file='%s' type='%s'>\n", (CString)XMLEncode(sFullname), sObjType);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf2(LOG_SHELL, "RomeFilesAdd \"%s\" \"%s\"\n", sFullname, sObjType);
#endif

	    CFileObj* pNewFile = pFiles->NewFileObj(sObjType, sFullname);

	    //! This sets the #SCIENCEVERSION to the app's science version.
	    // TODO: move this to CFileObj constructor or Save?
	    if (pNewFile)
		    pNewFile->SetScienceVersion( pFiles->Core.GetScienceVersion() );

        // ===

	    return pNewFile;
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeFilesAdd: exception for Type = '%s', Filename = '%s'.", (CString)pszObjType, (CString)pszFullname);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeFilesAdd: exception in catch block.");
        }
    }
}


//! Close all open files (and thus all views).
//! @note There can be open files with no view,
//!   but when all views are closed, no files should be left open
//!   unless they were opened by automation.
//! @note The "COEFFICIENTS" file is an exception. It will not be
//!   closed by this function unless flag #RX_CLOSEALL_NOCLOSE is used.
//! @note The global subobject "CONSTANTS" will not be closed - it isn't a file.
//! Close top level files first. If you close a lower-level file first
//!   it can get reloaded by its referencing CPtrAttr!
//! @param pFiles  A pointer to the Rome filesystem interface returned by RomeGetFiles().
//! @param nFlags  Flags controlling behavior. (These correspond to internal flags of type enum #CloseViewFlags).
//! - #RX_CLOSEALL_SAVE         Allow saving modified [and temp] files.
//!                             If this flag isn't set, flag #RX_CLOSEALL_CANCEL will have no effect.
//! - #RX_CLOSEALL_CANCEL       Allow canceling this operation.
//!                             The [Cancel] button won't be shown unless this flag is set.
//!                             Note: This only works in applications which handle dialog notifications.
//! - #RX_CLOSEALL_TEMP         Close temporary files.
//!                             Normally temporary files remain open until the app closes.
//! - #RX_CLOSEALL_USED         Close files still being used.
//! - #RX_CLOSEALL_NOCLOSE      Close internal #OBJT_NOCLOSE  and #OBJT_NOCLOSE_LAZYfiles.
//!                             This is normally only done on app shutdown.
//! - #RX_CLOSEALL_NOMODIFIED	Don't close modified files.
//! - #RX_CLOSEALL_NOUPDATE     Don't allow engine to finish before closing files
//! - #RX_CLOSEALL_NOUNUSED     Don't close unused top-level files (independent of #RX_CLOSEALL_NOUSED)
//!
//! @see RomeFilesOpen(), RomeFileClose().
//! @RomeAPI Wrapper for CFileSys::CloseAllFiles().
//!
ROME_API RT_void RomeFilesCloseAll(RT_Files* pFiles, RT_UINT nFlags /*=0*/)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN(pFiles,                  "RomeFilesCloseAll: NULL file system pointer.");
        BOOL bValidApp = (&pFiles->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN(bValidApp,               "RomeFilesCloseAll: invalid file system pointer.");
		BOOL bExited = pFiles->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN(!bExited,			       "RomeFilesCloseAll: RomeExit() has already been called.");
        BOOL bValidFiles = CFileSys::IsValid(pFiles);
        ASSERT_OR_SETERROR_AND_RETURN(bValidFiles,             "RomeFilesCloseAll: invalid file system pointer.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFiles->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN(!bSameThread,            "RomeFilesCloseAll: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        //! @since 2009-05-06 If the default @p nFlags argument of 0 is used, this function will instead
        //!   use flags combination #RX_CLOSEALL_DeleteAllFiles.
        if (nFlags == 0)
            nFlags = RX_CLOSEALL_DeleteAllFiles;

        CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFilesCloseAll", "flags='%d'>\n", nFlags);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeFilesCloseAll %d\n", nFlags);
#endif

        pFiles->CloseAllFiles(nFlags);
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN(0,                       "RomeFilesCloseAll: exception.");
    }
}

ROME_API RT_void RomeFilesClose(RT_Files* pFiles, RT_UINT nFlags /*=0*/)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN(pFiles,                  "RomeFilesClose: NULL file system pointer.");
        BOOL bValidApp = (&pFiles->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN(bValidApp,               "RomeFilesClose: invalid file system pointer.");
		BOOL bExited = pFiles->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN(!bExited,			       "RomeFilesClose: RomeExit() has already been called.");
        BOOL bValidFiles = CFileSys::IsValid(pFiles);
        ASSERT_OR_SETERROR_AND_RETURN(bValidFiles,             "RomeFilesClose: invalid file system pointer.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFiles->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN(!bSameThread,            "RomeFilesClose: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFilesClose", "flags='%d'>\n", nFlags);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeFilesClose %d\n", nFlags);
#endif

        pFiles->CloseFiles(nFlags);
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN(0,                       "RomeFilesClose: exception.");
    }
}

//! Return the number of open files in the Rome filesystem.
//! This includes files opened as a result of opening other files.
//! @note This only counts files visible in the current access level.
//! @param pFiles  A pointer to the Rome filesystem interface.
//!
//! @return  The number of open files, or #RX_FAILURE (-1) on error.
//! @see  RomeGetFiles(), RomeFilesGetItem().
//! @RomeAPI Wrapper for CFileSys::GetFileCount().
//!
ROME_API RT_INT RomeFilesGetCount(RT_Files* pFiles)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pFiles,          "RomeFilesGetCount: NULL file system pointer.");
        BOOL bValidApp = (&pFiles->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp,       "RomeFilesGetCount: invalid file system pointer.");
		BOOL bExited = pFiles->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited,        "RomeFilesGetCount: RomeExit() has already been called.");
        BOOL bValidFiles = CFileSys::IsValid(pFiles);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidFiles,     "RomeFilesGetCount: invalid file system pointer.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFiles->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread,    "RomeFilesGetCount: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        CLogFileElement0(LOGELEM_HIST, "user", "RomeFilesGetCount", "/>\n");
#if USE_ROMESHELL_LOGGING
        LogFilePrintf0(LOG_SHELL, "RomeFilesGetCount\n");
#endif

	    return pFiles->GetFileCount();
    }
    catch (...)
    {
        ASSERT_OR_SETERROR_AND_RETURN_FAILURE(0,  "RomeFilesGetCount: exception.");
    }
}


//! Return all object dependencies for file passed by pszFilename. This was implemented from CAttrView::BuildArrayAllReferencedFiles
//! @note The depsArray arguement will dynamically create a 2d character array and must be deleted after use to avoid memory leaks.
//! @param pFiles  A pointer to the Rome filesystem interface.
//! @param pszFilename A valid filename within the open ROME database to find all dependencies
//! @param depsArray A pointer to a 2d character array. This will be initialized during the function call and return any dependencies.
//! @param depsSize An integer reference to store the size accompanied with depsArray
//!
//! @return  True on success, False on failure.
//!
ROME_API RT_BOOL RomeFilesGetDependencies(RT_Files* pFiles, RT_CSTR pszFilename, RT_CSTRA& depsArray, RT_INT& depsSize)
{
	try
	{
		// Switch to the app's MFC module state while in this scope.
		// This is required for many MFC functions to work correctly.
		AFX_MANAGE_STATE(AfxGetAppModuleState());

		ASSERT_OR_SETERROR_AND_RETURN_FALSE(pFiles, "RomeFilesGetDependecies: NULL file system pointer.");
		ASSERT_OR_SETERROR_AND_RETURN_NULL(pszFilename, "RomeFilesGetDependecies: NULL filename pointer.");
		BOOL bValidApp = (&pFiles->Core == &App);
		ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp, "RomeFilesGetDependecies: invalid file system pointer.");
		BOOL bExited = pFiles->Core.HasFlag(DLLSTATE_CLOSED);
		ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited, "RomeFilesGetDependecies: RomeExit() has already been called.");
		BOOL bValidFiles = CFileSys::IsValid(pFiles);
		ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidFiles, "RomeFilesGetDependecies: invalid file system pointer.")
#if USE_ROMEAPI_THREADIdS
			THREADId nCurThreadId = GetCurrentThreadId();
		BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
		ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread, "RomeFileGetFloatArray: Rome API function called on different thread from RomeInit().");
#endif

		ROME_API_LOCK();

		// Wait for the stack to finish.
		// This makes sure that a value retrieved below won't get changed by functions on the stack.
		pFiles->Core.Engine.FinishUpdates();

		CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFilesGetDependecies", "file='%s'>\n", (CString)XMLEncode(pszFilename));
#if USE_ROMESHELL_LOGGING
		LogFilePrintf2(LOG_SHELL, "RomeFilesGetDependecies \"%s\"\n", pszFilename);
#endif

		// Create CStringArray to store filesys.GetFileDependencies results
		CStringArray csaDeps;
		CList<CString> stack;
		RT_FileObj* pFile;
		POSITION pos;
		CAttr* pAttr;

		// if (ArrayFind(csaDeps, pszFilename) == -1) {
		// 	csaDeps.Add(pszFilename);
		//}

		stack.AddHead(LPCSTR(pszFilename));

		// While stack isn't empty
		while (stack.GetSize() > 0)
		{
			// Open file at top of stack
			pFile = RomeFilesOpen(pFiles, stack.RemoveHead(), 0);

			// Begin parameter search for open file object
			pos = pFile->m_params.GetStartPosition();

			// Loop through all attributes within file object
			while (pos)
			{
				pAttr = pFile->m_params.GetNextValue(pos);
				CListing* pListing = pAttr->GetListing();
				if (pListing)
				{
					ParamType attrType = pListing->GetType();
					// looks like it could at least point to a true file object
					if (attrType == ATTR_PTR || attrType == ATTR_SUB)
					{
						int numPtrs = pAttr->GetSize();
						for (int i = 0; i < numPtrs; i++)
						{
							// If is ATTR_PTR, make sure that it exists in DB, otherwise will mess up the process.  
							// Would be caught by consistency check, but there is no guarantee that has been run
							if (attrType == ATTR_PTR)
							{
								LPCSTR fileName = pAttr->GetStr(i);
								if (!App.Files.FileExists(fileName))
								{
									continue;
								}
							}
							CSubObj* pCheckSubObj = pAttr->GetPtr(i);
							if (pCheckSubObj)
							{
								if (pCheckSubObj->IsFile())
								{
									LPCSTR fileName = pCheckSubObj->GetFileName();
									// If this file hasn't been added to list yet then do so
									if (ArrayFind(csaDeps, fileName) == -1)
									{
										stack.AddHead(fileName);
										csaDeps.Add(fileName);
									}
								}
							}
						}
					}
				}
			}

			RomeFileClose(pFile);
		}

		//	RT_BOOL bDepsOK = pFiles->GetFileDependencies(pszFilename, false, csaDeps);

		//	// If filesys.GetFileDependencies returned true
		//	if (bDepsOK)
		//	{
		// Set size return argument to size of csaDeps
		depsSize = csaDeps.GetCount() - 1;

		// Initialize RT_CSTRA return argument
		depsArray = new char*[depsSize];

		// Manually copy contents of csaDeps to return argument depsArray as csaDeps will deallocate when leaving scope
		for (int i = 0; i < depsSize; i++)
		{
			// Allocate length of csaDeps to depsArray for each element i
			depsArray[i] = new char[csaDeps.GetAt(i).GetLength()];

			// For each character in element i of csaDeps make a copy to depsArray
			for (int j = 0; j < csaDeps.GetAt(i).GetLength(); j++)
			{
				depsArray[i][j] = csaDeps.GetAt(i).GetAt(j);
			}

			// Add null terminating character as CString GetAt method does not include this in length count
			depsArray[i][csaDeps.GetAt(i).GetLength()] = '\0';
		}
		//	}
		//	else
		//	{
		//		// Null return arguments on failure and set size to 0
		//		depsArray = 0;
		//		depsSize = 0;
		//	}

		//	return bDepsOK;
		return true;
	}
	catch (...)
	{
		try
		{
			CString sInfo; sInfo.Format("RomeFilesGetDependecies: exception for File = '%s'.", (CString)pszFilename);
			ASSERT_OR_SETERROR_AND_RETURN_FALSE(0, sInfo);
		}
		catch (...)
		{
			ASSERT_OR_SETERROR_AND_RETURN_FALSE(0, "RomeFilesGetDependecies: exception in catch block.");
		}
	}
}


//! Get a file in the collection of open Rome files.
//! Increment the reference count on the file returned.
//! @note This only returns files visible in the current access level.
//! @note This file must be released by calling RomeFileClose().
//! @note This is the primary method of iterating over open files.
//! @param pFiles  The filesystem interface returned by RomeGetFiles().
//! @param nItem   An integer index used to iterate over items in the filesystem.
//!   The item indexes are 0-based
//!   The total number is obtained from RomeFilesGetCount().
//! @return the file pointer to the requested file if successful.
//!   Returns NULL if the index is out of range.
//!   Returns NULL on failure.
//!
//! @see RomeFilesOpen(), RomeFileClose(), RomeFileGetFullname().
//! @RomeAPI Wrapper for CFileSys::GetFile().
//!
ROME_API RT_FileObj* RomeFilesGetItem(RT_Files* pFiles, RT_INT nItem)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pFiles,             "RomeFilesGetItem: NULL file system pointer.");
        BOOL bValidApp = (&pFiles->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,          "RomeFilesGetItem: invalid file system pointer.");
		BOOL bExited = pFiles->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,           "RomeFilesGetItem: RomeExit() has already been called.");
        BOOL bValidFiles = CFileSys::IsValid(pFiles);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidFiles,        "RomeFilesGetItem: invalid file system pointer.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFiles->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,       "RomeFilesGetItem: Rome API function called on different thread from RomeInit().");
#endif
        BOOL bValidItem = (nItem >= 0);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidItem,         "RomeFilesGetItem: invalid (negative) item index.");

        ROME_API_LOCK();

        CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFilesGetItem", "index='%d'>\n", nItem);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "RomeFilesGetItem %d\n", nItem);
#endif

        RT_FileObj* pFile = pFiles->GetFile(nItem);

#ifdef USE_ROMEAPI_REFCOUNT
        //! @since 2009-03-08 This increments the reference count of times this pointer is returned by the Rome API.
        //! The file will be closed when this count drops to 0. Call RomeFilesClose() to release this reference to this file.
        if (pFile)
            VERIFY(pFile->m_nRomeRefs++ >= 0);
#endif

	    return pFile;
    }
    catch (...)
    {
        try
        {
            RT_App* pApp = &pFiles->Core;
            CString sInfo; sInfo.Format("RomeFilesGetItem: exception for Item = %d.", nItem);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeFilesGetItem: exception in catch block.");
        }
    }
}


//! Open a named file in the Rome filesystem.
//! This can be a file in the database or one generated dynamically.
//! This can return a file with a different name than the one asked for
//!   when dealing with cloned files, load failures, etc.
//! @param pFiles       The filesystem interface returned by RomeGetFiles().
//! @param pszFullname  The full name of the file, including table path.<br>
//!   Filenames in Rusle2 are case-insensitive.<br>
//!   The filename part can take on special values:
//!   - #ENTRY_CUSTOM     Signals user-entered custom data (set internally only). Requires catalog flag #ACF_ALLOW_CUSTOM.
//!   - #ENTRY_DEFAULT    Substitute the corresponding default file for this object type.
//!   - #ENTRY_MODEL      Open an empty file of this object type.
//!   - #ENTRY_NONE       No entry (can be set by user). Requires catalog flag #ACF_ALLOW_NONE.
//!   - #ENTRY_NULL       No entry (can be set by user). Requires catalog flag #ACF_ALLOW_NULL.
//!
//!   These special values must be passed in with an object prefix.<br>
//!   If the object type is OBJT_UNIQUE, only the type (table) needs to be specified. Any
//!     path past the object name is ignored. Ex: "no path coeff\bogus name".<br>
//!   If only an object type is given, a file open dialog will be shown.<br>
//!   Ex: "climates\Tennessee\Knoxville"  Opens a file from the database.<br>
//!   Ex: "soils\#ENTRY_MODEL"            Special values require an object prefix.<br>
//!   Ex: "no path coeff"                 Opens the unique "COEFFICIENTS" object.<br>
//!   Ex: "climates"                      Opens file open dialog if only table specified.<br>
//!   A default name like "soils\default" will always succeed.<br>
//!   A name like "soils\#ENTRY_MODEL" will create the hard-coded file.<br>
//!   This can be an external file if prefix "#XML:" is used.<br>
//!     Example: "#XML:C:\Rusle2\Export\profile1.pro.xml".<br>
//!   This can be an explicit XML file string instead of its filename. It must begin with "<?xml".<br>
//!     Example: "<?xml version='1.0'?><Obj><Type>PROFILE</Type>...</Obj>".<br>
//!   This can be an external file if prefix "#SKEL:" is used.<br>
//!     Example: "#SKEL:C:\Rusle2\Export\management1.man.skel".
//!   This can be an external fileset if prefix "#FILESET:" is used.<br>
//!     Example: "#FILESET:C:\Rusle2\Export\profile1.fileset.xml".
//! @param nFlags  Flags corresponding to internal flags of type enum #OpenModeFlags.<br>
//!   Currently the user should pass in 0 for this argument.<br>
//!   The following flags are added internally:
//! - #RX_FILESOPEN_USE_OPEN    Return an already open modified file.
//! - #RX_FILESOPEN_NO_CREATE   Don't create a file it can't find in the database, and returns NULL.
//! - #RX_FILESOPEN_LOG_HIST    Log this action to the history log (commands.xml).
//! - #RX_FILESOPEN_CMD_USER    Caused by a user action (for logging purposes).
//!
//! @return  A pointer to the opened file, or NULL on failure.
//!
//! @note This file must be released by calling RomeFileClose().
//! @see RomeFileClose(), RomeFilesCloseAll().
//! @RomeAPI Wrapper for CFileSys::OpenOrCreateFile().
//!
ROME_API RT_FileObj* RomeFilesOpen(RT_Files* pFiles, RT_CSTR pszFullname, RT_UINT nFlags)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_NULL(pFiles,                          "RomeFilesOpen: NULL file system pointer.");
        ASSERT_OR_SETERROR_AND_RETURN_NULL(pszFullname,                     "RomeFilesOpen: NULL filename pointer.");
        TEST_OR_SETERROR_AND_RETURN_NULL(!strieq(pszFullname, ENTRY_CUSTOM), "RomeFilesOpen: attempt to open file '#ENTRY_CUSTOM'.");
        TEST_OR_SETERROR_AND_RETURN_NULL(!strieq(pszFullname, ENTRY_NONE),   "RomeFilesOpen: attempt to open file '#ENTRY_NONE'.");
        TEST_OR_SETERROR_AND_RETURN_NULL(!strieq(pszFullname, ENTRY_NULL),   "RomeFilesOpen: attempt to open file '#ENTRY_NULL'.");
        TEST_OR_SETERROR_AND_RETURN_NULL(!strempty(pszFullname),            "RomeFilesOpen: attempt to open empty filename.");
        BOOL bValidApp = (&pFiles->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidApp,                       "RomeFilesOpen: invalid file system pointer.");
		BOOL bExited = pFiles->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bExited,                        "RomeFilesOpen: RomeExit() has already been called.");
        BOOL bValidFiles = CFileSys::IsValid(pFiles);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bValidFiles,                     "RomeFilesOpen: invalid file system pointer.");
        BOOL bOpen = pFiles->IsOpen() || ::HasFlag(nFlags, RX_FILESOPEN_PRIVATE);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(bOpen,                           "RomeFilesOpen: no database open.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFiles->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_NULL(!bSameThread,                    "RomeFilesOpen: Rome API function called on different thread from RomeInit().");
#endif

	    // TODO: validate filename argument.

        ROME_API_LOCK();

        CLogFileElement2(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFilesOpen", "file='%s' flags='%d'>\n", (CString)XMLEncode(pszFullname), nFlags);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf2(LOG_SHELL, "RomeFilesOpen \"%s\" %d\n", pszFullname, nFlags);
#endif

	    //  Validate flags argument.
	    //! @note For a long time the Rome and IRome interfaces required passing in 0.
	    //! Additional flags weren't documented or used.
	    //! So we interpret 0 to mean the default flags #RX_FILESOPEN_USE_OPEN | #RX_FILESOPEN_NO_CREATE.
	    if (nFlags == 0)
		    nFlags = OMF_USE_OPEN | OMF_NO_CREATE;

	    // Log this as a user command.
	    nFlags |= (OMF_LOG_HIST | OMF_CMD_USER);

	    // TODO: add flag OMF_AUTOMATION for files opened by automation,
	    //   and handle in all file open functions.

	    FILESYS_WRITELOCK();

	    CFileObj* pFO = NULL;

        CRomeCore& Core = pFiles->Core;

	    // Check for magic prefixes and strip them off:
	    // "#XML:"  import from Rusle2 XML file on disk.
	    // "<?xml"   import from Rusle2 XML string in memory.
	    if (strncmp(pszFullname, "#XML:", 5) == 0)
	    {
		    // Get the external filename following the magic prefix.
		    LPCSTR pszDiskName = pszFullname + 5;

		    nFlags = (nFlags & ~(OMF_FORMAT_CFF | OMF_USE_OPEN)) | OMF_FORMAT_XML;
		    pFO = ImportObject(Core, pszDiskName, nFlags);
	    }
	    else
	    if (strncmp(pszFullname, "<?xml", 5) == 0)
	    {
            // Load the XML data from the filename string argument.
		    CString buffer = pszFullname;

		    nFlags = (nFlags & ~(OMF_FORMAT_CFF | OMF_USE_OPEN)) | OMF_FORMAT_XML;
		    pFO = ImportObjectXML(Core, buffer, NULL /* fetch filename from <Filename> element */, nFlags);
	    }
#ifdef BUILD_MOSES // ROMEDLL_IGNORE
#if USE_SKELETONS
	    else
	    if (strncmp(pszFullname, "#SKEL:", 6) == 0)
	    {
		    // Get the external filename following the magic prefix.
		    LPCSTR pszDiskName = pszFullname + 6;

		    nFlags = (nFlags & ~OMF_FORMAT_XML) | OMF_FORMAT_CFF;
		    pFO = ImportObject(Core, pszDiskName, nFlags);
	    }
#endif // USE_SKELETONS
#endif // ROMEDLL_IGNORE
#if USE_FILESETS
	    else
	    if (strncmp(pszFullname, "#FILESET:", 9) == 0)
	    {
		    // Get the external filename following the magic prefix.
		    LPCSTR pszDiskName = pszFullname + 9;

		    nFlags = (nFlags & ~OMF_FORMAT_XML) | OMF_FORMAT_CFF;
		    BOOL bOpened = FilesetOpen(Core, pszDiskName, nFlags);
			if (!bOpened)
				return NULL;

			//! @todo open the base file and return its pointer.
			return NULL;
	    }
#endif // USE_FILESETS
	    else
	    {
		    pFO = pFiles->OpenOrCreateFile(pszFullname, nFlags);
	    }

#ifdef USE_ROMEAPI_REFCOUNT
        //! @note This increments the reference count of times this pointer is returned by the Rome API.
        //! The file will be closed when this count drops to 0.
        if (pFO)
            VERIFY(pFO->m_nRomeRefs++ >= 0);
#endif

	    return pFO;
    }
    catch (...)
    {
        try
        {
            RT_App* pApp = &pFiles->Core;
            CString sInfo; sInfo.Format("RomeFilesOpen: exception for Filename = '%s', Flags = 0x%X.", (CString)pszFullname, nFlags);
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_NULL(0,    "RomeFilesOpen: exception in catch block.");
        }
    }
}


//! Invoke a 'pragma' function.
//! This is a general "backdoor" through which unsupported operations may be done.
//! @param pFiles the filesystem interface returned by RomeGetFiles().
//! @param nPragma an integer pragma value (e.g. #RX_PRAGMA_DB_CLEAR_CACHE).
//! @param pExtra may be used to pass in extra data.
//!
//! @return a value which may depend on the action,
//!   but often indicates success (RX_TRUE) or failure (RX_FALSE).
//!
//! @warning This is an internal function.
//! @RomeAPI Wrapper for CFileSys::Pragma().
//!
ROME_API RT_INT RomeFilesPragma(RT_Files* pFiles, RT_UINT nPragma, void* pExtra)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

        ASSERT_OR_SETERROR_AND_RETURN_ZERO(pFiles,             "RomeFilesPragma: NULL file system pointer.");
        BOOL bValidApp = (&pFiles->Core == &App);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(bValidApp,          "RomeFilesPragma: invalid file system pointer.");
		BOOL bExited = pFiles->Core.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(!bExited,           "RomeFilesPragma: RomeExit() has already been called.");
        BOOL bValidFiles = CFileSys::IsValid(pFiles);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(bValidFiles,        "RomeFilesPragma: invalid file system pointer.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (pFiles->Core.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_ZERO(!bSameThread,       "RomeFilesPragma: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        // TODO: do more intelligent logging of the "pExtra" info. This can be logged according to type.
        CLogFileElement2(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeFilesPragma", "pragma='%d' args='%0x08X'>\n", nPragma, (UINT)pExtra);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf2(LOG_SHELL, "//RomeFilesPragma %d %d\n", nPragma, (UINT)pExtra);
#endif

        return pFiles->Pragma(nPragma, pExtra);
    }
    catch (...)
    {
        try
        {
            RT_App* pApp = pFiles? &pFiles->Core: NULL;
            CString sInfo; sInfo.Format("RomeFilesPragma: exception for Pragma = %d.", nPragma);
            ASSERT_OR_SETERROR_AND_RETURN_ZERO(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_ZERO(0,    "RomeFilesPragma: exception in catch block.");
        }
    }
}


//! @} Rome Filesystem functions
/////////////////////////////////////////////////////////////////////////////
//! @name Rome Statusbar functions
//! @{

//! Create and display a progress bar on the statusbar.
//! @param pStatus Statusbar interface obtained from RomeGetStatusbar().
//!   This can be NULL, in which case it will be fetched.
//! @param nLower  The lower index for the progress bar
//! @param nUpper  The upper index for the progress bar
//! @param nStep   The step to advance the progress bar
//! @return success (RX_TRUE) or failure (RX_FALSE).
//!
//! @note The progress bar should be destroyed by RomeProgressDestroy() when finished.
//! @RomeAPI Wrapper for CMainStatusbar::ProgressCreate().
//!
ROME_API RT_BOOL RomeProgressCreate(RT_Statusbar* pStatus, RT_INT nLower /*= 0*/, RT_INT nUpper /*= 100*/, RT_INT nStep /*= 1*/)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

		// @todo require non-NULL statusbar pointer.
        if (!pStatus)
            pStatus = RomeGetStatusbar(&App);

        TEST_OR_SETERROR_AND_RETURN_FALSE(pStatus,           "RomeProgressCreate: NULL statusbar pointer.");
//      BOOL bValidApp = (&pStatus->pApp == &App);
//      ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,       "RomeProgressCreate: invalid Rome app pointer.");
		BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,        "RomeProgressCreate: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (App.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,    "RomeProgressCreate: Rome API function called on different thread from RomeInit().");
#endif
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(nLower>=0,       "RomeProgressCreate: invalid (negative) lower index.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(nUpper>=0,       "RomeProgressCreate: invalid (negative) upper index.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(nStep>=1,        "RomeProgressCreate: invalid step value (must be > 0).");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(nUpper>nLower,   "RomeProgressCreate: invalid upper index (less than lower index).");

        ROME_API_LOCK();

        CLogFileElement3(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeProgressCreate", "lower='%d' upper='%d' step='%d'>\n", nLower, nUpper, nStep);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf3(LOG_SHELL, "//RomeProgressCreate %d %d %d\n", nLower, nUpper, nStep);
#endif

#ifdef BUILD_MOSES // ROMEDLL_IGNORE
	    return pStatus->ProgressCreate(nLower, nUpper, nStep);
#else
        return RX_FALSE;
#endif
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeProgressCreate: exception for Status = %0x08X, Lower=%d, Upper=%d, Step=%d.", pStatus, nLower, nUpper, nStep);
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    "RomeProgressCreate: exception in catch block.");
        }
    }
}

//! Set the min and max values for a progress bar on the statusbar.
//! @param pStatus  Statusbar interface obtained from RomeGetStatusbar().
//!   This can be NULL, in which case it will be fetched.
//! @param nLower  The lower index for the progress bar.
//! @param nUpper  The upper index for the progress bar.
//! @return  Success (RX_TRUE) or failure (RX_FALSE).
//!
//! @note The progress bar should have been created with RomeProgressCreate().
//! @RomeAPI Wrapper for CMainStatusbar::ProgressSetRange().
//!
ROME_API RT_BOOL RomeProgressSetRange(RT_Statusbar* pStatus, RT_INT nLower /*= 0*/, RT_INT nUpper /*= 100*/)
{
    try
    {
        // Switch to the app's MFC module state while in this scope.
        // This is required for many MFC functions to work correctly.
        AFX_MANAGE_STATE(AfxGetAppModuleState());

	    if (!pStatus)
		    pStatus = RomeGetStatusbar(&App);

        TEST_OR_SETERROR_AND_RETURN_FALSE(pStatus,			"RomeProgressSetRange: NULL statusbar pointer.");
//      BOOL bValidApp = (&pStatus->Core == &App);
//      ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,      "RomeProgressSetRange: invalid statusbar pointer.");
		BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,       "RomeProgressSetRange: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (App.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,   "RomeProgressSetRange: Rome API function called on different thread from RomeInit().");
#endif
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(nLower>=0,       "RomeProgressCreate: invalid (negative) lower index.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(nUpper>=0,       "RomeProgressCreate: invalid (negative) upper index.");
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(nUpper>nLower,   "RomeProgressCreate: invalid upper index (less than lower index).");

        ROME_API_LOCK();

        CLogFileElement2(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeProgressSetRange", "lower='%d' upper='%d'>\n", nLower, nUpper);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf2(LOG_SHELL, "//RomeProgressSetRange %d %d\n", nLower, nUpper);
#endif

#ifdef BUILD_MOSES // ROMEDLL_IGNORE
	    return pStatus->ProgressSetRange(nLower, nUpper);
#else
	    return RX_FALSE;
#endif
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeProgressSetRange: exception for Status = %0x08X, Lower=%d, Upper=%d.", pStatus, nLower, nUpper);
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    "RomeProgressSetRange: exception in catch block.");
        }
    }
}


//! Set the number of steps for a progress bar on the statusbar.
//! @param pStatus Statusbar interface obtained from RomeGetStatusbar().
//!   This can be NULL, in which case it will be fetched.
//! @param nStep   The step to advance the progress bar
//! @return  Success (RX_TRUE) or failure (RX_FALSE).
//!
//! @note The progress bar should have been created with RomeProgressCreate().
//! @RomeAPI Wrapper for CMainStatusbar::ProgressSetStep().
//!
ROME_API RT_BOOL RomeProgressSetStep(RT_Statusbar* pStatus, RT_INT nStep /*= 1*/)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

	    if (!pStatus)
		    pStatus = RomeGetStatusbar(&App);

        TEST_OR_SETERROR_AND_RETURN_FALSE(pStatus,			"RomeProgressSetStep: NULL statusbar pointer.");
//      BOOL bValidApp = (&pStatus->Core == &App);
//      ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,      "RomeProgressSetStep: invalid statusbar pointer.");
		BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,       "RomeProgressSetStep: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (App.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,   "RomeProgressSetStep: Rome API function called on different thread from RomeInit().");
#endif
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(nStep>=1,        "RomeProgressCreate: invalid step value (must be > 0).");

        ROME_API_LOCK();

        CLogFileElement1(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeProgressSetStep", "step='%d'>\n", nStep);
#if USE_ROMESHELL_LOGGING
        LogFilePrintf1(LOG_SHELL, "//RomeProgressStep %d\n", nStep);
#endif

#ifdef BUILD_MOSES // ROMEDLL_IGNORE
	    return pStatus->ProgressSetStep(nStep);
#else
        return RX_FALSE;
#endif
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeProgressSetStep: exception for Status = %0x08X, Step=%d.", pStatus, nStep);
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    "RomeProgressSetStep: exception in catch block.");
        }
    }
}


//! Step (advance) a progress bar on the statusbar.
//! @note The progress bar should have been created with RomeProgressCreate().
//! @param pStatus  Statusbar interface obtained from RomeGetStatusbar().
//!   This can be NULL, in which case it will be fetched.
//! @return  Success (RX_TRUE) or failure (RX_FALSE).
//!
//! @RomeAPI Wrapper for CMainStatusbar::ProgressStepIt().
//!
ROME_API RT_BOOL RomeProgressStepIt(RT_Statusbar* pStatus)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

	    if (!pStatus)
		    pStatus = RomeGetStatusbar(&App);

        TEST_OR_SETERROR_AND_RETURN_FALSE(pStatus,			"RomeProgressStepIt: NULL statusbar pointer.");
//      BOOL bValidApp = (&pStatus->Core == &App);
//      ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,      "RomeProgressStepIt: invalid object pointer.");
		BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,       "RomeProgressStepIt: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (App.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,   "RomeProgressStepIt: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        CLogFileElement0(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeProgressStepIt", ">\n");
#if USE_ROMESHELL_LOGGING
        LogFilePrintf0(LOG_SHELL, "//RomeProgressStepIt\n");
#endif

#ifdef BUILD_MOSES // ROMEDLL_IGNORE
	    return pStatus->ProgressStepIt();
#else
	    return RX_FALSE;
#endif
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeProgressStepIt: exception for Status = %0x08X.", pStatus);
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    "RomeProgressStepIt: exception in catch block.");
        }
    }
}


//! Destroy a progress bar on the statusbar.
//! @param pStatus  Statusbar interface obtained from RomeGetStatusbar().
//!   This can be NULL, in which case it will be fetched.
//! @return  Success (RX_TRUE) or failure (RX_FALSE).
//!
//! @note The progress bar should have been created with RomeProgressCreate().
//! @RomeAPI Wrapper for CMainStatusbar::ProgressDestroy().
//!
ROME_API RT_BOOL RomeProgressDestroy(RT_Statusbar* pStatus)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

	    if (!pStatus)
		    pStatus = RomeGetStatusbar(&App);

        ASSERT_OR_SETERROR_AND_RETURN_FALSE(pStatus,        "RomeProgressDestroy: NULL statusbar pointer.");
//      BOOL bValidApp = (&pStatus->Core == &App);
//      ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,      "RomeProgressDestroy: invalid statusbar pointer.");
		BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,       "RomeProgressDestroy: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (App.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,   "RomeProgressDestroy: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        CLogFileElement0(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeProgressDestroy", ">\n");
#if USE_ROMESHELL_LOGGING
        LogFilePrintf0(LOG_SHELL, "//RomeProgressDestroy\n");
#endif

#ifdef BUILD_MOSES // ROMEDLL_IGNORE
	    return pStatus->ProgressDestroy();
#else
	    return RX_FALSE;
#endif
    }
    catch (...)
    {
        try
        {
            CString sInfo; sInfo.Format("RomeProgressDestroy: exception for Status = %0x08X.");
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    "RomeProgressDestroy: exception in catch block.");
        }
    }
}


//! Show a message in the status bar's first pane.
//! @param pStatus  Statusbar interface obtained from RomeGetStatusbar().
//!   This can be NULL, in which case it will be fetched.
//! @param lpszNewText  The new message text to set.
//!   If this is a title key, it will automatically be translated.<br>
//!   The message text can take on special values:
//!   - "#LOCK_ENGINE_MESSAGES"    increment lock count for engine messages
//!   - "#UNLOCK_ENGINE_MESSAGES"  decrement lock count for engine messages
//! @param bUpdate  Should the statusbar be updated (repainted) after this change?
//! @return  RX_TRUE on success (This does not indicate if the displayed text changed).
//!   Returns RX_FALSE on failure.
//!
//! @RomeAPI Wrapper for CEngine::StatusbarMessage().
//!
ROME_API RT_BOOL RomeStatusbarMessage(RT_Statusbar* pStatus, RT_CSTR lpszNewText, RT_BOOL bUpdate /*= RX_TRUE*/)
{
    try
    {
	    // Switch to the app's MFC module state while in this scope.
	    // This is required for many MFC functions to work correctly.
	    AFX_MANAGE_STATE(AfxGetAppModuleState());

        // @todo require a non-NULL statusbar pointer.
	    if (pStatus)
        {
//          BOOL bValidApp = (&pStatus->Core == &App);
//          ASSERT_OR_SETERROR_AND_RETURN_FALSE(bValidApp,      "RomeStatusbarMessage: invalid statusbar pointer.");
        }
	    BOOL bExited = App.HasFlag(DLLSTATE_CLOSED);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bExited,       "RomeStatusbarMessage: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
        THREADId nCurThreadId = GetCurrentThreadId();
        BOOL bSameThread = (App.m_nThreadId == nCurThreadId);
        ASSERT_OR_SETERROR_AND_RETURN_FALSE(!bSameThread,   "RomeStatusbarMessage: Rome API function called on different thread from RomeInit().");
#endif

        ROME_API_LOCK();

        // Don't log this function - it gets called too many times and floods the log file.
//      CLogFileElement2(LOGELEM_HIST | LOGELEM_ENDTAG, "user", "RomeStatusbarMessage", "text='%s' flags='%d'>\n", (CString)XMLEncode(lpszNewText), bUpdate);

	    return App.Engine.StatusbarMessage(lpszNewText, bUpdate);
    }
    catch (...)
    {
        try
        {
            CString sText = lpszNewText;
            CString sInfo; sInfo.Format("RomeStatusbarMessage: exception for Status = %0x08X, Text = '%s'.", pStatus, sText);
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    sInfo);
        }
        catch (...)
        {
            ASSERT_OR_SETERROR_AND_RETURN_FALSE(0,    "RomeStatusbarMessage: exception in catch block.");
        }
    }
}


//! @} // Rome Statusbar functions
/////////////////////////////////////////////////////////////////////////////


ROME_API RT_BOOL   RomeSetMessageCallback(RT_App* pApp, int(*call_back)(LPCSTR aMsg, LPCSTR sub1, LPCSTR sub2, UINT nFlags, UINT nType, LPCSTR pszCaption))
{
	AFX_MANAGE_STATE(AfxGetAppModuleState());

	ASSERT_OR_SETERROR_AND_RETURN_FAILURE(pApp, "RomeSetMessageCallback: NULL Rome app pointer.");
	BOOL bValidApp = (pApp == &App);
	ASSERT_OR_SETERROR_AND_RETURN_FAILURE(bValidApp, "RomeSetMessageCallback: invalid Rome pointer.");
	BOOL bExited = pApp->HasFlag(DLLSTATE_CLOSED);
	ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bExited, "RomeSetMessageCallback: RomeExit() has already been called.");
#if USE_ROMEAPI_THREADIdS
	THREADId nCurThreadId = GetCurrentThreadId();
	BOOL bSameThread = (pFile->Core.m_nThreadId == nCurThreadId);
	ASSERT_OR_SETERROR_AND_RETURN_FAILURE(!bSameThread, "RomeSetMessageCallback: Rome API function called on different thread from RomeInit().");
#endif

	ROME_API_NOLOCK();
	return pApp->SetOnMessage(call_back);
}