using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace SnapPlus.Models.Erosion
{
    /// <summary>
    /// Manager for allocating unmanaged strings for use by R2. Ensure that they get deallocated per
    /// https://www.deleaker.com/blog/2021/03/19/unmanaged-memory-leaks-in-dotnet/.
    /// </summary>
    public class UnManStringHeap : IDisposable
    {
        private List<IntPtr> unManStrings = new List<IntPtr>();
        private bool _disposed;

        /// <summary>
        /// Convert a .NET string to a pointer that a C++ DLL like RomeAPI can use.
        /// </summary>
        /// <param name="str">String to be converted.</param>
        /// <returns>The pointer as an IntPtr.</returns>
        public IntPtr StringToPtr(string str)
        {
            // allocates space in unmanaged heap
            IntPtr ret = Marshal.StringToHGlobalAnsi(str);
            // so keep track of it so that we can return it later
            unManStrings.Add(ret);
            return ret;
        }

        /// <summary>
        /// Just the opposite: Given a char * - style pointer to an ANSI string, return a .NET string. Does not affect the char *.
        /// </summary>
        /// <param name="stringPtr">IntPtr to address of first character of null-terminated string.</param>
        /// <returns>A newly-allocated .NET string object.</returns>
        public string PtrToString(IntPtr stringPtr)
        {
            return Marshal.PtrToStringAnsi(stringPtr);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                for (int ii=0; ii < unManStrings.Count; ii++)
                {
                    
                    if (unManStrings[ii] != IntPtr.Zero)
                    {
                        Marshal.Release(unManStrings[ii]);
                        unManStrings[ii] = IntPtr.Zero;
                    }
                }
                _disposed = true;
            }
        }
        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="UnManStringHeap"/> class. Goes through the list of allocated strings and deallocates them.
        /// </summary>
        ~UnManStringHeap()
        {
            Dispose(disposing: false);
        }
    }
}
