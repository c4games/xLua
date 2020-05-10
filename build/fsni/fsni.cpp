#define LUA_LIB

#include "fsni.h"

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include <unordered_map>
#include <thread>
#include <mutex>
#include <algorithm>

// memcpy
#include <string.h>

#include "aes.h"

#include "yasio/cxx17/string_view.hpp"
#include "yasio/detail/object_pool.hpp"

#include "ZipFile.h"

#if defined(__ANDROID__)
#include <android/log.h>
#define FSNI_LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,"FSNI-" FSNI_VER,__VA_ARGS__)
#define FSNI_LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,"FSNI-" FSNI_VER,__VA_ARGS__)
#else
#define FSNI_LOGD(...)
#define FSNI_LOGE(...)
#endif

#if defined(_WIN32)
#define O_READ_FLAGS O_BINARY | O_RDONLY, S_IREAD
#define O_WRITE_FLAGS O_CREAT | O_RDWR | O_BINARY, S_IWRITE | S_IREAD
#define O_APPEND_FLAGS O_APPEND | O_CREAT | O_RDWR | O_BINARY, S_IWRITE | S_IREAD
#define posix_open ::_open
#define posix_close ::_close
#define posix_lseek ::_lseek
#define posix_read ::_read
#define posix_write ::_write
#include <io.h>
#include <fcntl.h>
#include <Windows.h>
#include <direct.h>
#else
// S_IRUSR | S_IRGRP | S_IROTH
#define O_READ_FLAGS O_RDONLY, S_IRUSR
#define O_WRITE_FLAGS O_CREAT | O_RDWR, S_IRWXU
#define O_APPEND_FLAGS O_APPEND | O_CREAT | O_RDWR, S_IRWXU
#define posix_open ::open
#define posix_close ::close
#define posix_lseek ::lseek
#define posix_read ::read
#define posix_write ::write
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#if defined(_WINDLL)
#define FSNI_API __declspec(dllexport)
#else
#define FSNI_API
#endif
#define FSNI_INVALID_FILE_HANDLE -1
#define APK_PREFIX "jar:file://"
#define APK_PREFIX_LEN sizeof(APK_PREFIX)

// path helper methods
template<typename _Elem, typename _Pr, typename _Fn> inline
void _fsni_splitpath(_Elem* s, _Pr _Pred, _Fn func) // will convert '\\' to '/'
{
    _Elem* _Start = s; // the start of every string
    _Elem* _Ptr = s;   // source string iterator
    while (_Pred(_Ptr))
    {
        if ('\\' == *_Ptr || '/' == *_Ptr)
        {
            if (_Ptr != _Start) {
                auto _Ch = *_Ptr;
                *_Ptr = '\0';
                bool should_brk = func(s);
#if defined(_WIN32)
                *_Ptr = '\\';
#else // For unix linux like system.
                * _Ptr = '/';
#endif
                if (should_brk)
                    return;
            }
            _Start = _Ptr + 1;
        }
        ++_Ptr;
    }
    if (_Start < _Ptr) {
        func(s);
    }
}

template<typename _Elem, typename _Fn> inline
void fsni_splitpath(_Elem* s, _Fn func) // will convert '\\' to '/'
{
    _fsni_splitpath(s, [=](_Elem* _Ptr) {return *_Ptr != '\0'; }, func);
}

static void fsni_mkdir(std::string dir)
{
    if (dir.empty()) return;

    fsni_splitpath(&dir.front(), [](const char* subdir) {
        bool should_brk = false;

        if (!fsni_exists(subdir, fsni_chkflags::directory))
        {
#ifdef _WIN32
            should_brk = !(0 == ::mkdir(subdir));
#else
            should_brk = !(0 == ::mkdir(subdir, S_IRWXU | S_IRWXG | S_IRWXO));
#endif
        }

        return should_brk;
        });
}

static unsigned char s_default_scret[] = { 0xeb,0x1b,0x95,0xf9,0xb1,0x33,0x2a,0x20,0x66,0x26,0x66,0x36,0x57,0x1b,0x50,0xbb, };
static unsigned char s_default_iv[] = { 0x24,0xf2,0xd5,0x3a,0x31,0xdf,0xae,0x8d,0xfb,0xad,0x43,0x8a,0x43,0x5e,0xa0,0xd0, };

static const int s_fsni_flags[][2] = {
    O_READ_FLAGS,
    O_WRITE_FLAGS,
    O_APPEND_FLAGS,
};

struct fsni_stream_secret {
    AES_KEY key;
    unsigned char iv[16];
};

struct fsni_stream {
    union {
        voidp entry;
        int fd;
    };
    int64_t offset;
    bool streaming; // whether in apk or obb file.
    fsni_stream_secret* secret;
};

struct fsni_context {
    std::string streamingPath, persistPath;
    fsni::ZipFile zip;
    std::string key;
    std::string iv;
    yasio::gc::object_pool<fsni_stream, std::recursive_mutex> filesPool;
    yasio::gc::object_pool<fsni_stream_secret, std::recursive_mutex> secretsPool;
};

static void _fsni_setkey(std::string& lhs, const cxx17::string_view& rhs) {
    static const size_t keyLen = 16;
    if (!rhs.empty()) {
        lhs.assign(rhs.data(), (std::min)(rhs.length(), keyLen));
        if (lhs.size() < keyLen)
            lhs.insert(lhs.end(), keyLen - lhs.size(), '\0'); // fill 0, if key insufficient
    }
    else
        lhs.assign(keyLen, '\0');
}

static void _fsni_set_secret(fsni_context* ctx, const cxx17::string_view& key, const cxx17::string_view& iv)
{
    _fsni_setkey(ctx->key, key);
    _fsni_setkey(ctx->iv, iv);
}

static fsni_context* s_fsni_ctx = nullptr;

extern "C" {
    FSNI_API void fsni_startup(const char* streamingPath/*internal path*/, const char* persistPath/*hot update path*/)
    {
        if (s_fsni_ctx) {
            FSNI_LOGD("fsni_startup ---> fsni context already initialized!");
            return;
        }
        s_fsni_ctx = new fsni_context();
        s_fsni_ctx->streamingPath = streamingPath;
        s_fsni_ctx->persistPath = persistPath;

        _fsni_set_secret(s_fsni_ctx, cxx17::string_view((const char*)s_default_scret, sizeof(s_default_scret)), cxx17::string_view((const char*)s_default_iv, sizeof(s_default_iv)));

        FSNI_LOGD("fsni_startup ---> streamingPath:%s, persistPath:%s", streamingPath, persistPath);

        if (cxx20::starts_with(cxx17::string_view(s_fsni_ctx->streamingPath), APK_PREFIX))
        { // Android streamingPath format: jar:file://${APK_PATH}!/assets/
            // [FSNI] Init, streamingAssetsPath: jar:file:///data/app/com.c4games.redalert3d-TBAXBO37ccSyzWzUsJwcHQ==/base.apk!/assets
            // because filter always full relative to zip file, so should remove prefix
            auto endpos = s_fsni_ctx->streamingPath.rfind("!/");
            if (endpos != std::string::npos) {
                std::string apkPath = s_fsni_ctx->streamingPath.substr(APK_PREFIX_LEN - 1, endpos - APK_PREFIX_LEN + 1);
                std::string strFilter = s_fsni_ctx->streamingPath.substr(endpos + 2);
                s_fsni_ctx->zip.open(apkPath, strFilter); // try open
            }
        }
    }
    FSNI_API void fsni_cleanup()
    {
        if (s_fsni_ctx) {
            delete s_fsni_ctx;
            s_fsni_ctx = nullptr;
        }
    }

    FSNI_API voidp fsni_open(const char* fileName, int mode)
    {
        union {
            voidp entry;
            int fd;
        };

        int internalError = 0, error = 0;

        bool absolute = (fileName[0] == '/' || (isalpha(fileName[0]) && fileName[1] == ':'));

        // try open from hot update path disk
        std::string fullPath;
        if (!absolute)
            fullPath = s_fsni_ctx->persistPath + fileName;
        else
            fullPath = fileName;
        auto flags = s_fsni_flags[mode & 0xff];

        bool readonly = flags[0] == s_fsni_flags[fsni_mode::read][0];
        if (!readonly) { // try make file's parent directory
            auto slash = fullPath.find_last_of(R"(/\)");
            if (slash != std::string::npos) {
                auto chTmp = fullPath[slash]; // store
                fullPath[slash] = '\0';
                if (!fsni_exists(fullPath.c_str(), fsni_chkflags::directory))
                    fsni_mkdir(fullPath.substr(0, slash));
                fullPath[slash] = chTmp; // restore
            }
        }

        fd = posix_open(fullPath.c_str(), flags[0], flags[1]);
        bool streaming = false;
        if (fd == FSNI_INVALID_FILE_HANDLE) {
            internalError = errno;
            if (readonly && !absolute) { // only readonly and not absolute path, we can try to read from app internal path
                // try open from internal path
                if (s_fsni_ctx->zip) { // android, from apk
                    auto pEntry = s_fsni_ctx->zip.vopen(fileName);
                    if (pEntry) {
                        entry = pEntry;
                        streaming = true;
                    }
                    else error = ENOENT;
                }
                else { // ios, from disk
                    fullPath = s_fsni_ctx->streamingPath + fileName;
                    fd = posix_open(fullPath.c_str(), O_READ_FLAGS);
                    if (fd == -1)
                        internalError = errno;
                }
            }
        }

        if ((!streaming && fd != FSNI_INVALID_FILE_HANDLE) || (streaming && entry != nullptr)) {
            fsni_stream* f = (fsni_stream*)s_fsni_ctx->filesPool.construct();
            if (f != nullptr) {
                f->entry = entry;
                f->offset = 0;
                f->streaming = streaming;

                if (!(mode >> 16)) { // high word to mark whether open file as secret.
                    f->secret = nullptr;
                }
                else {
                    auto secret = (fsni_stream_secret*)s_fsni_ctx->secretsPool.allocate();
                    ossl_aes_set_encrypt_key((const unsigned char*)s_fsni_ctx->key.c_str(), 128, &secret->key);
                    memcpy(secret->iv, s_fsni_ctx->iv.c_str(), (std::min)(sizeof(secret->iv), s_fsni_ctx->iv.size()));
                    f->secret = secret;
                }
                return f;
            }
            else error = ENOMEM;
        }

        FSNI_LOGE("fsni_open ----> %s failed, internalError:%d(%s), error:%d(%s)!", fullPath.c_str(),
            internalError, strerror(internalError),
            error, strerror(error));
        return nullptr;
    }

    FSNI_API int fsni_read(voidp fp, voidp buf, int size)
    {
        int n = 0;
        fsni_stream* nfs = (fsni_stream*)fp;
        if (nfs != nullptr) {
            if (!nfs->streaming) {
                if (nfs->fd != FSNI_INVALID_FILE_HANDLE)
                    n = posix_read(nfs->fd, buf, size);
            }
            else {
                n = s_fsni_ctx->zip.read((fsni::ZipEntryInfo*)nfs->entry, nfs->offset, buf, size);
                nfs->offset += n;
            }

            if (nfs->secret && n > 0) {
                int ignored_num = 0;
                ossl_aes_cfb128_encrypt((unsigned char*)buf, (unsigned char*)buf, size, &nfs->secret->key, nfs->secret->iv, &ignored_num, AES_DECRYPT);
            }
        }
        return n;
    }

    FSNI_API int fsni_write(voidp fp, const voidp buf, int size) {
        fsni_stream* nfs = (fsni_stream*)fp;
        if (nfs != nullptr && !nfs->streaming && nfs->fd != FSNI_INVALID_FILE_HANDLE)
        { // for write mode, must always writeable path
            if (nfs->secret && size > 0) {
                int ignored_num = 0;
                ossl_aes_cfb128_encrypt((unsigned char*)buf, (unsigned char*)buf, size, &nfs->secret->key, nfs->secret->iv, &ignored_num, AES_ENCRYPT);
            }
            return posix_write(nfs->fd, buf, size);
        }
        return 0;
    }

    FSNI_API int fsni_seek(voidp fp, int offset, int origin)
    {
        fsni_stream* nfs = (fsni_stream*)fp;
        if (nfs != nullptr) {
            if (!nfs->streaming) {
                if (nfs->fd != FSNI_INVALID_FILE_HANDLE)
                    return posix_lseek(nfs->fd, offset, origin);
            }
            else {
                long result = -1;
                auto entry = (fsni::ZipEntryInfo*)nfs->entry;
                if (entry != nullptr) {

                    switch (origin) {
                    case SEEK_SET:
                        result = offset;
                        break;
                    case SEEK_CUR:
                        result = nfs->offset + offset;
                        break;
                    case SEEK_END:
                        result = (long)fsni::ZipFile::size(entry) + offset;
                        break;
                    default:;
                    }

                    if (result >= 0) {
                        nfs->offset = result;
                    }
                    else
                        result = -1;
                }

                return result;
            }
        }

        return -1;
    }

    FSNI_API void fsni_close(voidp fp)
    {
        fsni_stream* nfs = (fsni_stream*)fp;
        if (nfs != nullptr) {
            if (!nfs->streaming) {
                if (nfs->fd != FSNI_INVALID_FILE_HANDLE)
                    posix_close(nfs->fd);
            }
            if (nfs->secret)
                s_fsni_ctx->secretsPool.deallocate(nfs->secret);
            s_fsni_ctx->filesPool.deallocate(nfs);
        }
    }

    FSNI_API int fsni_getsize(voidp fp)
    {
        int size = fsni_seek(fp, 0, SEEK_END);
        fsni_seek(fp, 0, SEEK_SET);
        return size;
    }

    int fsni_remove(const char* path)
    {
        return ::remove(path);
    }
    int fsni_rename(const char* oldName, const char* newName)
    {
        return ::rename(oldName, newName);
    }
    bool fsni_exists(const char* path, int flags)
    {
        bool found = false;
#if defined(_WIN32)
        DWORD attr = GetFileAttributesA(path);
        if (attr != INVALID_FILE_ATTRIBUTES) {
            if (flags & 1)
                found = !(attr & FILE_ATTRIBUTE_DIRECTORY);
            if (flags & 2)
                found = found || (attr & FILE_ATTRIBUTE_DIRECTORY);
        }
#else
        struct stat st;
        if (::stat(path, &st) == 0)
        {
            if (flags & 1)
                found = S_ISREG(st.st_mode);
            if (flags & 2)
                found = found || S_ISDIR(st.st_mode);;
        }
#endif
        // check android, from apk
        if (!found && s_fsni_ctx->zip) {
            found = s_fsni_ctx->zip.exists(path);
        }

        return found;
    }

    FSNI_API voidp fsni_alloc(int size) {
        return malloc(size);
    }

    FSNI_API void fsni_free(voidp ptr) {
        if (ptr)
            free(ptr);
    }

    FSNI_API voidp fsni_strdup(const char* s, int* len)
    {
        auto n = strlen(s);
        auto dup = (char*)fsni_alloc(n + 1);
        if (dup) {
            memcpy(dup, s, n);
            dup[n] = '\0';
        }
        if (len)
            *len = n;
        return dup;
    }
    FSNI_API int fsni_strlen(const char* s)
    {
        return strlen(s);
    }
    FSNI_API voidp fsni_strndup(const char* s, int len)
    {
        auto dup = (char*)fsni_alloc(len + 1);
        if (dup) {
            memcpy(dup, s, len);
            dup[len] = '\0';
        }
        return dup;
    }
    FSNI_API voidp fsni_memdup(const voidp p, int size)
    {
        voidp dup = fsni_alloc(size);
        if (dup)
            memcpy(dup, p, size);
        return dup;
    }
}
