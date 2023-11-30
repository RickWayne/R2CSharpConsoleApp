using System;
using System.Collections.Generic;
using Utilities.Static.Const;
using SnapPlus.Models.Erosion.Interfaces;

namespace SnapPlus.Models.Erosion.Entities
{
    using static ConstRusle2;


    /// <summary>
    /// Interface to the RUSLE2 API. Currently runs with a DLL on same system, and a GDB file using the old SQLite 2 format.
    /// Note that you can instantiate as many Rusle2Helpers as you like, but they will all refer to the same singleton
    /// IRusle2 instance under the hood.
    /// </summary>
    public class Rusle2Helper : IRusle2Helper
    {
        #region Constructor, Properties and class objects

        public IRusle2 R2instance;

        private const string ROTATIONAL_RESULTS_HEADER = "--------------------------------- Rotational Results ----------------------------------";

        public string GetDebugLog { get; set; } = "";

        public string GetGetDebugLog()
        { return GetDebugLog; }

        public string GetSetDebugLog()
        { return SetDebugLog; }

        public string SetDebugLog { get; set; } = "";

        public Rusle2Helper()
        {
            R2instance = Rusle2.GetInstance();
        }

        /// <summary>
        /// Return our Rusle2 instance. Not called GetInstance() b/c of possible confusion with IRusle2's
        /// instantiation method of the same name.
        /// </summary>
        /// <returns>The IRusle2 instance held by this instance of Rusle2Helper.</returns>
        public IRusle2 Instance()
        {
            return R2instance;
        }

        public void ClearDebugLog()
        {
            GetDebugLog = "";
            SetDebugLog = "";
        }

        #endregion Constructor, Properties and class objects

        #region File stuff

        /// <summary>
        /// Open management file and return the pointer.
        /// </summary>
        /// <param name="mgtName">Management name, e.g. "managements\Alfalfa; Spring Direct, SVT, z1"</param>
        /// <returns>An IntPtr to the open management, or IntPtr.Zero if open failed.</returns>
        public IntPtr GetManagement(string mgtName)
        {
            return R2instance.FilesOpen(mgtName);
        }

        public bool FileSaveAsXml(IntPtr fileHandle, string filePath)
        {
            return R2instance.FileSaveAsXml(fileHandle, filePath);
        }

        /// <summary>
        /// Close a list of R2 files
        /// </summary>
        /// <param name="filePtrs"></param>
        /// <returns></returns>
        public (bool, int) CloseFiles(List<IntPtr> filePtrs)
        {
            int cnt = 0;
            foreach (IntPtr filePtr in filePtrs)
            {
                if (R2instance.FileClose(filePtr))
                    cnt++;
            }
            return (cnt.Equals(filePtrs.Count), cnt);
        }

        /// <summary>
        /// Close one R2 file
        /// </summary>
        /// <param name="filePtr"></param>
        /// <returns></returns>
        public bool CloseFile(IntPtr filePtr)
        {
            return R2instance.FileClose(filePtr);
        }

        /// <summary>
        /// Close all open R2 files
        /// </summary>
        /// <returns></returns>
        public bool CloseAllFiles()
        {
            return R2instance.FilesCloseAll();
        }

        /// <summary>
        /// Close profile/default, if open, then reopen profile/default
        /// </summary>
        /// <returns></returns>
        public bool FlushProfile()
        {
            return R2instance.ProfileFlush();
        }

        public IntPtr FilesOpen(string file)
        {
            return R2instance.FilesOpen(file);
        }

        /// <summary>
        /// Open profile/default
        /// </summary>
        /// <returns></returns>
        public (IntPtr, string) GetProfile()
        {
            return R2instance.GetProfile();
        }

        public IntPtr GetFileSysHandle()
        {
            return R2instance.GetFileSysHandle();
        }

        #endregion File stuff

        #region Local Get & Set Attr

        #region Local Get Attr

        /// <summary>
        /// Get R2 management attribute item value by index
        /// </summary>
        /// <param name="mgtPtr"></param>
        /// <param name="item"></param>
        /// <param name="index"></param>
        /// <returns></returns>
        public string GetAttrValue(IntPtr mgtPtr, string item, int index)
        {
            string value = "";
            try
            {
                value = R2instance.FileGetAttrValue(mgtPtr, item, index);
                GetDebugLog += CRLF + $"GetAttrValue {mgtPtr.ToInt64}, {item}, {index}: {value}";
            }
            catch (Exception)
            {
                throw;
            }
            return value;
        }

        /// <summary>
        /// Get attribute value in given units
        /// </summary>
        /// <param name="mgtPtr"></param>
        /// <param name="item"></param>
        /// <param name="index"></param>
        /// <param name="units"></param>
        /// <returns></returns>
        public string GetPrecipValue(IntPtr mgtPtr, string item, int index, string units)
        {
            //18Aug21 JWW Get GDB Climate precip in inches
            string value = "";
            try
            {
                value = R2instance.FileGetAttrValueAux(mgtPtr, item, index, units);
            }
            catch (Exception)
            {
                throw;
            }
            return value;
        }

        /// <summary>
        /// Get R2 management attribute OP_DATE value by index
        /// </summary>
        /// <param name="mngPtr"></param>
        /// <param name="index"></param>
        /// <returns></returns>
        public string GetOP_DATE(IntPtr mngPtr, int index)
        {
            string value = "";
            try
            {
                value = R2instance.FileGetAttrValue(mngPtr, ConstRusle2.OP_DATE, index);
            }
            catch (Exception)
            {
                throw;
            }
            return value;
        }

        /// <summary>
        /// Get profile attribute value by index
        /// </summary>
        /// <param name="item"></param>
        /// <param name="index"></param>
        /// <returns></returns>
        public string GetProfileAttrValue(string item, int index)
        {
            string value = "0.0";
            try
            {
                {
                    value = R2instance.ProfileGetAttrValue(item, index);
                    GetDebugLog += CRLF + $"GetProfileAttrValue {item}, {index}: {value}";
                }
            }
            catch (Exception)
            {
                throw;
            }
            return value;
        }

        /// <summary>
        /// Get OP_DATE size from R2Profile address
        /// </summary>
        /// <returns></returns>
        public int GetProfileOP_DATEAttrSize()
        {
            int size = 0;
            try
            {
                size = R2instance.ProfileGetAttrSize(ConstRusle2.OP_DATE);
            }
            catch (Exception)
            {
                throw;
            }
            return size;
        }

        /// <summary>
        /// Get attribute item size from R2Profile address
        /// </summary>
        /// <returns></returns>
        public int GetProfileAttrSize(string item)
        {
            int size = 0;
            try
            {
                size = R2instance.ProfileGetAttrSize(item);
                GetDebugLog += CRLF + $"GetProfileAttrSize {item}: {size}";
            }
            catch (Exception)
            {
                throw;
            }
            return size;
        }

        /// <summary>
        /// Get OP_DATE size from management address
        /// </summary>
        /// <returns></returns>
        public int GetOP_DATESize(IntPtr mngPtr)
        {
            int size = 0;
            try
            {
                size = R2instance.FileGetAttrSize(mngPtr, ConstRusle2.OP_DATE);
            }
            catch (Exception)
            {
                throw;
            }
            return size;
        }

        /// <summary>
        /// Get attribute item size from management address
        /// </summary>
        /// <returns></returns>
        public int GetAttrSize(IntPtr mngPtr, string item)
        {
            int size = 0;
            try
            {
                size = R2instance.FileGetAttrSize(mngPtr, item);
                GetDebugLog += CRLF + $"GetAttrSize {mngPtr.ToInt64}, {item}: {size}";
            }
            catch (Exception)
            {
                throw;
            }
            return size;
        }

        #endregion Local Get Attr

        #region Local Set Attr

        /// <summary>
        /// Set attribute item value for management address at index offset
        /// </summary>
        public int SetMgtAttrValue(IntPtr mgtPtr, string item, string value, int index)
        {
            int status = 0;
            try
            {
                status = R2instance.FileSetAttrValue(mgtPtr, item, value, index);
                SetDebugLog += CRLF + $"SetMgtAttrValue {mgtPtr.ToInt64}, {item}, {index}, {value}: {status}";
            }
            catch (Exception)
            {
                throw;
            }
            return status;
        }

        /// <summary>
        /// Set attribute item value for R2Profile address at index offset
        /// </summary>
        /// <returns>1 if the value was changed, 0 if unchanged, -1 if error</returns>
        public int SetProfileAttrValue(string item, string value, int index)
        {
            int status = 0;
            try
            {
                status = R2instance.ProfileSetAttrValue(item, value, index);
                SetDebugLog += CRLF + $"SetProfileAttrValue {item}, {index}, {value}: {status}";
            }
            catch (Exception)
            {
                throw;
            }
            return status;
        }

        public string GetLastError()
        {
            return R2instance.GetLastError();
        }

        /// <summary>
        /// Set the attribute item size at the r2Profile address
        /// </summary>
        /// <param name="item"></param>
        /// <param name="value"></param>
        /// <returns></returns>
        public int SetAttrSize(string item, int value)
        {
            int status = 0;
            try
            {
                status = R2instance.ProfileSetAttrSize(item, value);
                SetDebugLog += CRLF + $"SetProfileAttrSize {item}, {value}: {status}";
            }
            catch (Exception)
            {
                throw;
            }
            return status;
        }

        #endregion Local Set Attr

        #endregion Local Get & Set Attr

        #region Set methods

        public bool setFieldProperties(string county, string r2Soil)
        {
            bool status = true;
            
            if (SetProfileAttrValue(CLIMATE_PTR, @"climates\USA\Wisconsin\" + county + " County", R2_BASE_INDEX) < 0)
                throw new Exception($"Could not set CLIMATE_PTR to '{county}'");
            if (SetProfileAttrValue(SOIL_PTR, r2Soil, R2_BASE_INDEX) < 0)
                throw new Exception($"Could not set SOIL_PTR to '{r2Soil}'");

            string r2SoilSet = GetProfileAttrValue(SOIL_PTR, R2_BASE_INDEX);
            return status;
        }




 
 
        #endregion Set methods

        #region Get methods


        public int GetAttrDimSize(string item)
        {
            return R2instance.GetAttrDimSize(item);
        }

        #endregion Run and get results
    }
}