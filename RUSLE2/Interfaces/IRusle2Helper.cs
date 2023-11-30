using System;
using System.Collections.Generic;
using System.Text;

using static Utilities.Static.Const.ConstRusle2;

using SnapPlus.Models.Erosion.Interfaces;
using System.Linq;
using System.Threading.Tasks;
using SnapPlus.Models.Erosion.Entities;

namespace SnapPlus.Models.Erosion.Interfaces
{
    public interface IRusle2Helper
    {
        IRusle2 Instance();

        #region Get methods

        IntPtr GetManagement(string mgtName);

        #endregion Get methods

        bool FileSaveAsXml(IntPtr mgtPtr, string fileFullName);

        bool FlushProfile();

        IntPtr FilesOpen(string file);

        (IntPtr, string) GetProfile();

        public IntPtr GetFileSysHandle();

        bool CloseAllFiles();

        bool CloseFile(IntPtr filePtr);

        (bool, int) CloseFiles(List<IntPtr> filePtrs);

        int GetOP_DATESize(IntPtr mgtPtr);

        int GetAttrSize(IntPtr mgtPtr, string item);

        int GetAttrDimSize(string item);

        int GetProfileAttrSize(string item);

        string GetLastError();

        string GetOP_DATE(IntPtr mgtPtr, int index);

        string GetAttrValue(IntPtr mgtPtr, string item, int index);

        string GetProfileAttrValue(string item, int index);

        string GetPrecipValue(IntPtr mgtPtr, string item, int index, string units);

        string GetGetDebugLog();

        string GetSetDebugLog();

        int SetMgtAttrValue(IntPtr mgtPtr, string item, string value, int index);

        int SetProfileAttrValue(string item, string value, int index);

        int SetAttrSize(string item, int value);

        bool setFieldProperties(string county, string r2Soil);
    }
}