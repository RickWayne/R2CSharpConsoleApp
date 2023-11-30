using SnapPlus.Models.Erosion.Interfaces;
using System;
using System.IO;
using System.Runtime.InteropServices;

namespace SnapPlus.Models.Erosion
{
    /// <summary>
    /// Interface to the RUSLE2 API. Currently runs with a DLL on same system, and a GDB file using the old SQLite 2 format.
    /// </summary>
    public class Rusle2 : IRusle2, IDisposable
    {
        private const string PROFILE_DEFAULT = @"profiles\default";
        private static Rusle2 instance = null;

        private IntPtr handle = IntPtr.Zero; // Each instance of the class has its own R2 handle. Hopefully re-entrant?
        private IntPtr database = IntPtr.Zero;
        private string openDatabasePath = null;
        private IntPtr fileSystemPtr = IntPtr.Zero;
        private IntPtr engine = IntPtr.Zero;
        private IntPtr profile = IntPtr.Zero;
        private string profileName = PROFILE_DEFAULT;
        private UnManStringHeap heap = new UnManStringHeap();
        private bool disposedValue;

        // Rome interface return values
        public const int RX_TRUE = 1;

        public const int RX_FALSE = 0;
        public const int RX_FAILURE = -1;

        #region Lifecycle Methods

        /*
         * I am still not happy with how this works -- the caller still has to refer to the concrete Rusle2 class to get
         * an IRusle2-implementing object. Thinking that instantiation should go into a separate Rusle2Factory class which
         * can be interfaced, and thus created via dependency injection. The _factory_ class will have a concrete reference
         * and make the call to the static GetInstance method in this class, but that's hidden from the caller. If we were to
         * create a different IRusle2-implementing class that, say, spawned processes for parallel calls, none of the calling code would change.
         * Likewise we could create a mock instance of R2 for testing helper methods or something. For now I'm leaving as is.
         * Joe, I suggest you go ahead and just write to the interface as is, if I have some big brainstorm I will undertake to
         * refactor.
         */

        /// <summary>
        /// Returns the Singleton instance of IRusle2. Calls ctor if necessary, but only once.
        /// </summary>
        /// <returns>An IRusle2 object</returns>
        public static IRusle2 GetInstance(bool openDefaultDatabase = true)
        {
            string path = "YOU HAVE TO SET UP A DEFAULT PATH HERE SOMEHOW";
            string file = "moses.gdb";

            if (instance == null) // r2Path is ignored if the Singleton has already been initialized
            {
                instance = new Rusle2(path);
            }
            if (openDefaultDatabase && path != null)
            {
                // Open a GDB too.
                string fullGdbPath;
                    fullGdbPath = Path.Combine(path, file);

                // Is no database open? openDatabasePath always gets set if so
                if (!instance.isDatabaseOpen())
                {
                    // Shouldn't this examine the return value?
                    instance.OpenDatabase(fullGdbPath);
                }
                instance.ProfileOpen();
            }
            return instance;
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="Rusle2"/> class.
        /// </summary>
        private Rusle2()
        { }

        private Rusle2(string r2Path)
        {
            if (!Init(r2Path)) throw new Exception($"Could not initialize RUSLE2 with path {r2Path}");
            if (!GetDatabase()) throw new Exception($"Could not initialize RUSLE2 database at {r2Path}");
            if (!GetEngine()) throw new Exception($"Could not initialize RUSLE2 engine with path {r2Path}");
            if (!EngineSetAutorun(false)) throw new Exception($"Could not set RUSLE2 engine to autorun=false, path {r2Path}");
            if (!GetFiles()) throw new Exception($"Could not set up for RUSLE2 'files' at {r2Path}");
        }

        /// <summary>
        /// Do we have an R2 database open? Only way we can tell is to check openDatabasePath.
        /// </summary>
        /// <returns></returns>
        private bool isDatabaseOpen()
        {
            return handle != IntPtr.Zero && database != IntPtr.Zero && !string.IsNullOrEmpty(openDatabasePath);
        }

        /// <summary>
        /// Initialize the RUSLE2 DLL.
        /// </summary>
        /// <param name="r2DataPath">Path to data files, e.g. "TestData" or @"C:\ProgramData\UWSoils\SnapPlus3\RUSLE2".</param>
        /// <returns>True if successful.</returns>
        private bool Init(string r2DataPath)
        {
            // This also works if you just pass it the path, but this is what the docs say
            IntPtr ptr = heap.StringToPtr(@"SnapPlus3 /DirRoot={r2DataPath}");
            handle = Rusle2.RomeInit(ptr);
            return (handle != IntPtr.Zero);
        }

        /// <summary>
        /// Open a profile. Only one ever used to my knowledge is the default one.
        /// </summary>
        /// <param name="profileName">Name of profile to enter, defaults to PROFILE_DEFAULT</param>
        /// <returns>True if successful.</returns>
        public bool ProfileOpen(string profileName = PROFILE_DEFAULT)
        {
            if ((profile = FilesOpen(profileName)) == IntPtr.Zero)
            {
                string errMesg = GetLastError(); // Right now, this is just for debugging, but we could also log it
                return false;
            }
            else
            {
                this.profileName = profileName;
                return true;
            }
        }

        /// <summary>
        /// SnapPlus 2 had a method to close and re-open the profile. This method, if successful, ends with the profile (re)opened.
        /// </summary>
        /// <returns>True if successful.</returns>
        public bool ProfileFlush()
        {
            if (profile != IntPtr.Zero) // not open
            {
                FileClose(profile);
            }

            profile = IntPtr.Zero;
            profile = FilesOpen(profileName);
            if (profile == IntPtr.Zero)
            {
                string errMesg = GetLastError(); // Right now, this is just for debugging, but we could also log it
                return false;
            }
            else
                return true;
        }

        public (IntPtr, string) GetProfile()
        {
            return (profile, profileName); // IntPtr.Zero - not open
        }

        public IntPtr GetFileSysHandle()
        {
            return fileSystemPtr; // IntPtr.Zero - not open
        }

        /// <summary>
        /// Set all of our instance variables to null or zero values to
        /// prevent inadvertent use of an exited/disposed R2.
        /// </summary>
        private void zeroOutInternalState()
        {
            RomeExit(handle); // Should close database, any open files too?
            handle = IntPtr.Zero;
            FilesCloseAll();
            database = IntPtr.Zero;
            fileSystemPtr = IntPtr.Zero;
            engine = IntPtr.Zero;
            profile = IntPtr.Zero;
            if (heap != null)
            {
                heap.Dispose();
                heap = null;
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    // Dispose managed state (managed objects)
                    openDatabasePath = null;
                }

                // Free unmanaged resources (unmanaged objects) and override finalizer
                zeroOutInternalState();
                disposedValue = true;
            }
        }

        // Override finalizer only if 'Dispose(bool disposing)' has code to free unmanaged resources
        ~Rusle2()
        {
            // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
            Dispose(disposing: false);
        }

        public void Dispose()
        {
            // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        #endregion Lifecycle Methods

        /// <summary>
        /// GetDatabase -- initializes the database, but doesn't open it. TODO: Probably unneeded, just call from OpenDatabase.
        /// </summary>
        /// <returns>True if successful.</returns>
        private bool GetDatabase()
        {
            if (handle == IntPtr.Zero)
                return false;
            database = RomeGetDatabase(handle);
            if (database == IntPtr.Zero)
            {
                string errMesg = GetLastError(); // Right now, this is just for debugging, but we could also log it
                return false;
            }
            return true;
        }

        /// <summary>
        /// Actually opens the GDB. Record the name of the currently-open database internally so subsequent opens are harmless no-ops.
        /// </summary>
        /// <param name="path">Full path to the GDB file.</param>
        /// <returns>True if successful.</returns>
        public bool OpenDatabase(string path)
        {
            //03Sep21 JWW Why not just trim the backslash?
            path = path.TrimEnd('\\');

            if (string.IsNullOrEmpty(path))
                return false;

            if (handle == IntPtr.Zero)
                throw new Exception($"Tried to open GDB {path} when Rusle2 library was not initialized.");
            // Check if we already have it open
            if (isDatabaseOpen() && openDatabasePath == path)
                return true;
            bool success = false;

            /* DEBUG ONLY
            string cwd = Directory.GetCurrentDirectory();
            string[] folders = Directory.GetDirectories(cwd);
            string[] files = Directory.GetFiles(".");
            // string[] tdFiles = Directory.GetFiles("TestData");
            DEBUG ONLY */

            if (File.Exists(path))
            {
                IntPtr pathPtr = heap.StringToPtr(path);
                success = RomeDatabaseOpen(database, pathPtr);
                if (success)
                    openDatabasePath = path;
                else
                {
                    string errMesg = GetLastError(); // Right now, this is just for debugging, but we could also log it
                                                     //Is GlobalProtect connected?
                    openDatabasePath = null;
                }
            }
            else
            {
                success = false;
                openDatabasePath = null;
            }
            return success;
        }

        /// <summary>
        /// Close the R2 database, but only if it's open.
        /// </summary>
        /// <returns></returns>
        public bool CloseDatabase()
        {
            bool ret = true;
            openDatabasePath = null;
            if (database != IntPtr.Zero)
            {
                FilesCloseAll(); // Added RW July 2022 -- Rome close doesn't work if files are open
                ret = RomeDatabaseClose(database, IntPtr.Zero);
            }
            // Allow closing an already-closed database, it's fine
            return ret;
        }

        /// <summary>
        /// Initialize the GDB for "file" access. TODO: This can probably be hidden, and just be
        /// called from FilesOpen.
        /// </summary>
        /// <returns>True if successful.</returns>
        private bool GetFiles()
        {
            fileSystemPtr = RomeGetFiles(handle);
            if (fileSystemPtr == IntPtr.Zero)
            {
                string errMesg = GetLastError(); // Right now, this is just for debugging, but we could also log it

                return false;
            }
            else
                return true;
        }

        /// <summary>
        /// Open a file in the GDB. Depending on how R2 is set up, might actually be one or more
        /// physical files of XML, otherwise get 'em out of the database.
        /// </summary>
        /// <param name="r2FakePath">A string identifying the data, which looks like a file path.</param>
        /// <returns>An IntPtr to the data, to be passed to other R2 functions.</returns>
        public IntPtr FilesOpen(string r2FakePath, int flags = 0)
        {
            if (fileSystemPtr == IntPtr.Zero)
            {
                if (!GetFiles())
                    throw new Exception(@"Could not open R2 'filesystem'");
            }
            IntPtr r2FakePathPtr = heap.StringToPtr(r2FakePath);
            IntPtr fileObj = RomeFilesOpen(fileSystemPtr, r2FakePathPtr, flags);
            string errMesg = null;
            if (fileObj == IntPtr.Zero)
                errMesg = GetLastError(); // Right now, this is just for debugging, but we could also log it
            return fileObj;
        }

        /// <summary>
        /// Initialize the R2 "engine".
        /// </summary>
        /// <returns>true if successful</returns>
        private bool GetEngine()
        {
            engine = RomeGetEngine(handle);
            if (engine == IntPtr.Zero)
                return false;
            else
                return true;
        }

        // Uses an RT_FILEOBJ. Apparently undocumented. Removing it from the interface, at least as a trial.
        public bool FileClose(IntPtr fileHandle)
        {
            RomeFileClose(fileHandle);
            return true;
        }

        public bool FilesCloseAll()
        {
            RomeFilesCloseAll(fileSystemPtr, 0); // Secood argument ignored;
                                                 // 10May21 when called with V2 - RX_CLOSEALL_TEMP = 8, it hangs
            return true;
        }

        public bool EngineRun()
        {
            return RomeEngineRun(engine);
        }

        public bool EngineGetAutorun()
        {
            return RomeEngineGetAutorun(engine);
        }

        public bool EngineSetAutorun(bool autoRun)
        {
            RomeEngineSetAutorun(engine, autoRun);
            return true;
        }

        #region Value getters and setters of various sorts -- Attrs, ScienceVersion, Titles, error messages

        public string GetTitle(string key)
        {
            IntPtr keyPtr = heap.StringToPtr(key);
            return heap.PtrToString(RomeGetTitle(handle, keyPtr));
        }

        public IntPtr FileGetAttr(IntPtr fileHandle, string attrName)
        {
            IntPtr attrNamePtr = heap.StringToPtr(attrName); // TODO: Mem leak, figure out how to dispose
            return RomeFileGetAttr(fileHandle, attrNamePtr);
        }

        public int FileGetAttrSize(IntPtr fileHandle, string attrName)
        {
            IntPtr attrNamePtr = heap.StringToPtr(attrName); // TODO: Mem leak, figure out how to dispose
            return RomeFileGetAttrSize(fileHandle, attrNamePtr);
        }

        public int GetAttrDimSize(string attrName)
        {
            IntPtr attrNamePtr = heap.StringToPtr(attrName);
            return RomeCatalogGetAttrDimCount(handle, attrNamePtr);
        }

        public string FileGetAttrValue(IntPtr fileHandle, string attrName, int index)
        {
            IntPtr attrNamePtr = heap.StringToPtr(attrName); // TODO: Mem leak, figure out how to dispose
            return heap.PtrToString(RomeFileGetAttrValue(fileHandle, attrNamePtr, index));
        }

        public string FileGetAttrValueAux(IntPtr fileHandle, string attrName, int index, string attrUnits)
        {
            //18Aug21 JWW Get GDB Climate precip in inches
            //#define RX_VARIANT_CATALOG      ((RT_UINT)-2)
            IntPtr RX_VARIANT_CATALOG = IntPtr.Zero - 2;

            //ROME_API RT_CSTR RomeFileGetAttrValueAux(RT_FileObj * pFile, RT_CNAME pszAttr, RT_INT nIndex, RT_UINT nVariant, RT_CNAME pszUnit);
            IntPtr attrNamePtr = heap.StringToPtr(attrName); // TODO: Mem leak, figure out how to dispose
            IntPtr attrUnitsPtr = heap.StringToPtr(attrUnits); // TODO: Mem leak, figure out how to dispose
            return heap.PtrToString(RomeFileGetAttrValueAux(fileHandle, attrNamePtr, index, RX_VARIANT_CATALOG, attrUnitsPtr));
        }

        public int FileSetAttrValue(IntPtr fileHandle, string attrName, string value, int index)
        {
            IntPtr attrNamePtr = heap.StringToPtr(attrName); // TODO: Mem leak, figure out how to dispose
            IntPtr attrValuePtr = heap.StringToPtr(value);   // Ditto
            return RomeFileSetAttrValue(fileHandle, attrNamePtr, attrValuePtr, index);
        }

        /// <summary>
        /// Set the root size of an attribute in a file. Create the attr if it doesn't exist (I think.)
        /// </summary>
        /// <param name="fileHandle">Rome file name (e.g. "profiles\default".</param>
        /// <param name="attrName">Duh.</param>
        /// <param name="newSize">Also duh.</param>
        /// <returns>1 if successful, 0 if unsuccessful, -1 if error.</returns>
        public int FileSetAttrSize(IntPtr fileHandle, string attrName, int newSize)
        {
            IntPtr attrNamePtr = heap.StringToPtr(attrName); // TODO: Mem leak, figure out how to dispose
            return RomeFileSetAttrSize(fileHandle, attrNamePtr, newSize);
        }

        public IntPtr ProfileGetAttr(string attrName)
        {
            return FileGetAttr(profile, attrName);
        }

        public string ProfileGetAttrValue(string attrName, int index)
        {
            return FileGetAttrValue(profile, attrName, index);
        }

        public int ProfileSetAttrValue(string attrName, string value, int index)
        {
            return FileSetAttrValue(profile, attrName, value, index);
        }

        public int ProfileSetAttrSize(string attrName, int newSize)
        {
            return FileSetAttrSize(profile, attrName, newSize);
        }

        public int ProfileGetAttrSize(string attrName)
        {
            return FileGetAttrSize(profile, attrName);
        }

        public string GetPropertyStr(int propertyId)
        {
            IntPtr propStringPtr = RomeGetPropertyStr(handle, propertyId);
            return heap.PtrToString(propStringPtr);
        }

        public string GetLastError()
        {
            IntPtr errStringPtr = RomeGetLastError(handle);
            return heap.PtrToString(errStringPtr);
        }

        public void ClearLastError()
        {
            if (handle != IntPtr.Zero)
                RomeSetLastError(handle, IntPtr.Zero);
        }

        /// <summary>
        /// RUSLE science version.
        /// </summary>
        /// <returns>The science version as an integer.</returns>
        public int GetScienceVersion()
        {
            if (handle == IntPtr.Zero)
                return -1;
            else
                return RomeGetScienceVersion(handle);
        }

        /// <summary>
        /// Save an open R2 "file" as a real XML file on disk. Creates folders as necessary
        /// </summary>
        /// <param name="fileHandle">An open R2 file handle.</param>
        /// <param name="filePath">Path to the XML file. If folders in the path don't exist, creates them.</param>
        /// <returns>True if successful, false if not.</returns>
        public bool FileSaveAsXml(IntPtr fileHandle, string filePath)
        {
            IntPtr filePathPtr = heap.StringToPtr($"#XML:{filePath}");
            switch (RomeFileSaveAs(fileHandle, filePathPtr))
            {
                case RX_TRUE: return true;
                case RX_FAILURE: return false;
                case RX_FALSE: return false;
                default: return false;
            }
        }

        /// <summary>
        /// Return the number of files open in the Rome file system
        /// </summary>
        /// <param name="fileSystemHandle">Rome file system pointer</param>
        /// <returns>number of open files</returns>
        public int FilesGetCount(IntPtr fileSystemHandle)
        {
            return RomeFilesGetCount(fileSystemHandle);
        }

        /// <summary>
        /// Return index'th open file handle for the file system
        /// </summary>
        /// <param name="fileSystemPtr">The open file system</param>
        /// <param name="index">Zero-based index into the open files</param>
        /// <returns>The file handle for the particular file</returns>
        public IntPtr FilesGetItem(IntPtr fileSystemPtr, int index)
        {
            return RomeFilesGetItem(fileSystemPtr, index);
        }

        /// <summary>
        /// Return the full name for the (open) file
        /// </summary>
        /// <param name="fileHandle">File handle</param>
        /// <returns>The full name (i.e. R2 path)</returns>
        public string FileGetFullname(IntPtr fileHandle)
        {
            return heap.PtrToString(RomeFileGetFullname(fileHandle));
        }

        #endregion Value getters and setters of various sorts -- Attrs, ScienceVersion, Titles, error messages

        /* EXTERN REFERENCES TO ROMEAPI.DLL */
        // see https://docs.microsoft.com/en-us/dotnet/standard/native-interop/pinvoke

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern unsafe IntPtr RomeInit(IntPtr args);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern unsafe int RomeGetScienceVersion(IntPtr args);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern unsafe IntPtr RomeGetDatabase(IntPtr handle);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern unsafe bool RomeDatabaseOpen(IntPtr handle, IntPtr path);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern unsafe bool RomeDatabaseClose(IntPtr handle, IntPtr dbNameIsIgnoredByR2);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern unsafe IntPtr RomeGetFiles(IntPtr handle);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern unsafe IntPtr RomeFilesOpen(IntPtr handle, IntPtr r2FakePathPtr, int flags);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern unsafe IntPtr RomeGetEngine(IntPtr handle);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RomeFileClose(IntPtr fileHandle);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RomeFilesClose(IntPtr fileHandle, int flags = 0);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RomeFilesCloseAll(IntPtr filesHandle, int flags = 0);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RomeEngineRun(IntPtr engineHandle); // uses internal engine pointer

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RomeEngineGetAutorun(IntPtr engineHandle); // uses internal engine pointer

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RomeEngineSetAutorun(IntPtr engineHandle, bool autoRun); // uses internal engine pointer

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RomeGetTitle(IntPtr handle, IntPtr key);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RomeFileGetAttr(IntPtr fileHandle, IntPtr attrName);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RomeFileGetAttrValue(IntPtr fileHandle, IntPtr attrName, int index);

        //18Aug21 JWW Get GDB Climate precip in inches
        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RomeFileGetAttrValueAux(IntPtr fileHandle, IntPtr attrName, int index, IntPtr variant, IntPtr attrUnits);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RomeFileSetAttrValue(IntPtr fileHandle, IntPtr attrName, IntPtr value, int index);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RomeFileSetAttrSize(IntPtr fileHandle, IntPtr attrName, int newSize);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RomeFileGetAttrSize(IntPtr fileHandle, IntPtr attrName);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RomeCatalogGetAttrDimCount(IntPtr handle, IntPtr attrName);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RomeGetPropertyStr(IntPtr handle, int propertyId);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RomeGetLastError(IntPtr handle);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RomeSetLastError(IntPtr handle, IntPtr message);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RomeExit(IntPtr handle);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RomeFileSaveAs(IntPtr handle, IntPtr filePath);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RomeFilesGetCount(IntPtr fileSystemHandle);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RomeFilesGetItem(IntPtr fileSystemPtr, int index);

        [DllImport(@"RomeDLL.dll", CharSet = CharSet.Unicode, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RomeFileGetFullname(IntPtr fileHandle);
    }
}