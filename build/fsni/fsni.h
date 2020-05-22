/****************************************************************************
Copyright (c) 2020 c4games.com

https://github.com/c4games

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/
#ifndef _FSNI_H_
#define _FSNI_H_

#define FSNI_VER "1.1.6"

#if defined(_WINDLL)
#  if defined(LUA_LIB)
#    define FSNI_API __declspec(dllexport)
#  else
#    define FSNI_API __declspec(dllimport)
#  endif
#else
#  define FSNI_API
#endif

#define voidp void*

/*
The File Stream Native Interface
The DllImports for C#

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern int fsni_startup(string streamingAssetsPath, string persistentDataPath);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern IntPtr fsni_open(string fileName, int mode);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern int fsni_read(IntPtr fp, byte[] buf, int len);
[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern int fsni_read(IntPtr fp, IntPtr buf, int len);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern int fsni_write(IntPtr fp, byte[] buf, int len);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern int fsni_seek(IntPtr fp, int offset, int origin);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern void fsni_close(IntPtr fp);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern int fsni_getsize(IntPtr fp);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern int fsni_remove(string path);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern int fsni_rename(string oldName, string newName);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern bool fsni_exists(string path, int flags);
[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern IntPtr fsni_alloc(int size);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern void fsni_free(IntPtr buf);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern IntPtr fsni_strdup(string s, out int len);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern int fsni_strlen(string s);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern IntPtr fsni_strndup(string s, int len);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern IntPtr fsni_memdup(IntPtr s, int len);

[DllImport(LIBNAME, CallingConvention = CallingConvention.Cdecl)]
public static extern void fsni_cleanup();
*/
namespace fsni_chkflags {
    enum {
        file = 1,
        directory = 1 << 1,
        relative = 1 << 2,
    };
}
namespace fsni_mode {
    enum {
        read,
        write,
        append,
        security = 1 << 16,
    };
};

extern "C" {
    FSNI_API void fsni_startup(const char* pszStreamingPath/*internal path*/, const char* pszPersistPath/*hot update path*/);
    /*
    @params:
      path: path of file
      mode: 0: read, 1: write, 2: append
    */
    FSNI_API voidp fsni_open(const char* path, int mode);
    FSNI_API int fsni_read(voidp fp, voidp buf, int size); 
    FSNI_API int fsni_write(voidp fp, const voidp buf, int size);
    FSNI_API int fsni_seek(voidp fp, int offset, int origin);
    FSNI_API void fsni_close(voidp fp);
    FSNI_API int fsni_getsize(voidp fp);
    FSNI_API int fsni_remove(const char* path);
    FSNI_API int fsni_rename(const char* oldName, const char* newName);

    /* ------------ native memory operations ------------- */
    FSNI_API voidp fsni_alloc(int size);
    FSNI_API void fsni_free(voidp);

    // pitfall: C# string.Length not real utf8 string bytes.
    FSNI_API voidp fsni_strdup(const char* s, int* len);
    FSNI_API int fsni_strlen(const char* s);
    FSNI_API voidp fsni_strndup(const char* s, int len);
    FSNI_API voidp fsni_memdup(const voidp p, int size);

    /*
    @flags: see fsni_chkflags
    @see: fsni_chkflags
    */
    FSNI_API bool fsni_exists(const char* path, int flags);
    FSNI_API void fsni_cleanup();
}

#endif // _FSNI_H_

