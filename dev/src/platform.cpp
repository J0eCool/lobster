// misc platform specific stuff

#include "stdafx.h"

#ifdef WIN32
    #define VC_EXTRALEAN
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #define FILESEP '\\'
#else
    #include <sys/time.h>
    #define FILESEP '/'
#endif

#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#ifndef __IOS__
#include <Carbon/Carbon.h>
#endif
#endif

#ifdef __ANDROID__
#include <android/log.h>
#include <android/asset_manager.h>
// FIXME:
#include "sdlincludes.h"
#include "sdlinterface.h"
#endif

string datadir;     // main dir to load files relative to, on windows this is where lobster.exe resides, on apple platforms it's the Resource folder in the bundle
string auxdir;      // auxiliary dir to load files from, this is where the bytecode file you're running or the main .lobster file you're compiling reside
string writedir;    // folder to write to, usually the same as auxdir, special folder on mobile platforms

string StripFilePart(const char *filepath)
{
    auto fpos = strrchr(filepath, FILESEP);
    return fpos ? string(filepath, fpos - filepath + 1) : "";
}

string StripDirPart(const char *filepath)
{
    auto fpos = strrchr(filepath, FILESEP);
    if (!fpos) fpos = strrchr(filepath, ':');
    return fpos ? fpos + 1 : filepath;
}

bool SetupDefaultDirs(const char *exefilepath, const char *auxfilepath, bool forcecommandline)
{
    datadir = StripFilePart(exefilepath);
    auxdir = auxfilepath ? StripFilePart(SanitizePath(auxfilepath).c_str()) : datadir;
    writedir = auxdir;

    // FIXME: use SDL_GetBasePath() instead?
    #ifdef __APPLE__
        if (!forcecommandline)
        {
            // default data dir is the Resources folder inside the .app bundle
            CFBundleRef mainBundle = CFBundleGetMainBundle();
            CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
            char path[PATH_MAX];
            auto res = CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX);
            CFRelease(resourcesURL);
            if (!res)
                return false;
            datadir = string(path) + "/";
            #ifdef __IOS__
                writedir = StripFilePart(path) + "Documents/"; // there's probably a better way to do this in CF
            #else
                writedir = datadir; // FIXME: this should probably be ~/Library/Application Support/AppName, but for now this works for non-app store apps
            #endif
        }
    #elif defined(__ANDROID__)
        SDL_Init(0); // FIXME, is this needed? bad dependency.
        auto internalstoragepath = SDL_AndroidGetInternalStoragePath();
        auto externalstoragepath = SDL_AndroidGetExternalStoragePath();
        DebugLog(-1, internalstoragepath);
        DebugLog(-1, externalstoragepath);
        if (internalstoragepath) datadir = internalstoragepath + string("/");
        if (externalstoragepath) writedir = externalstoragepath + string("/");
        // for some reason, the above SDL functionality doesn't actually work, we have to use the relative path only to access APK files:
        datadir = "";
        auxdir = writedir;
    #endif

    return true;
}

string SanitizePath(const char *path)
{
    string r;
    while (*path)
    {
        if (*path == '\\' || *path == '/') r += FILESEP;
        else r += *path;
        path++;
    }
    return r;
}

/*
uchar *LoadFileAndroid(const char *relfilename, size_t *lenret)
{
    // FIXME: a mode other than AASSET_MODE_BUFFER may involve less copying?
    // FIXME: no native activity!
    auto activity = (jobject)SDL_AndroidGetActivity();
    auto file = AAssetManager_open(activity->assetManager, relfilename, AASSET_MODE_BUFFER);
    if (!file) return NULL;
    auto len = AAsset_getLength(file);
    auto buf = (uchar *)malloc(len + 1);
    if (!buf) { AAsset_close(file); return NULL; }
    buf[len] = 0;
    memcpy(buf, AAsset_getBuffer(file), len);
    AAsset_close(file);
    if (lenret) *lenret = len;
    return buf;
}
*/

uchar *LoadFilePlatform(const char *absfilename, size_t *lenret)
{
    #ifdef __ANDROID__
        DebugLog(-1, absfilename);
        return SDLLoadFile(absfilename, lenret);

        // FIXME: apk loading not working, just stick a temp src in here for now
        auto src = strstr(absfilename, ".lobster") ?
            "print(gl_window(\"hypocycloid\", 1024, 768))\n"
            "while(gl_frame()):\n"
            "    if(gl_wentdown(\"escape\")): return\n"
            "    gl_clear([0, 0, 0, 0])\n"
            "    gl_translate(gl_windowsize() / 2.0)\n"
            "    gl_scale(gl_windowsize()[1] / 4.0)\n"
            "    scalechange := sin(gl_time() * 50) * 0.2\n"
            "    pts := map(360 * 4 + 1) a:\n"
            "        p := [ 0, 0 ]\n"
            "        for(5) i:\n"
            "            p += sincos(a / 4.0 * pow(3, i)) * pow(0.4 + scalechange, i)\n"
            "        p\n"
            "    gl_linemode(1):\n"
            "        gl_polygon(pts)\n"
            :
            "SHADER color\n"
            "    VERTEX\n"
            "        INPUTS apos:4\n"
            "        UNIFORMS mvp\n"
            "        gl_Position = mvp * apos;\n"
            "    PIXEL\n"
            "        UNIFORMS col\n"
            "        gl_FragColor = col;\n";
        if (lenret) *lenret = strlen(src);
        return (uchar *)strdup(src);
    #else
        return loadfile(absfilename, lenret);
    #endif
}

uchar *LoadFile(const char *relfilename, size_t *lenret)
{
    auto srfn = SanitizePath(relfilename);
    auto f = LoadFilePlatform((datadir + srfn).c_str(), lenret);
    if (f) return f;
    f = LoadFilePlatform((auxdir + srfn).c_str(), lenret);
    if (f) return f;
    return LoadFilePlatform((writedir + srfn).c_str(), lenret);
}

FILE *OpenForWriting(const char *relfilename, bool binary)
{
    return fopen((writedir + SanitizePath(relfilename)).c_str(), binary ? "wb" : "w");
}

void DebugLog(int lev, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    #ifdef __ANDROID__
        __android_log_vprint(lev < 0 ? ANDROID_LOG_INFO : (lev > 0 ? ANDROID_LOG_ERROR : ANDROID_LOG_WARN), "lobster", msg, args);
    #elif defined(WIN32)
        char buf[256];
        vsnprintf(buf, sizeof(buf), msg, args);
        buf[sizeof(buf) - 1] = 0;
        OutputDebugStringA("LOG: ");
        OutputDebugStringA(buf);
        OutputDebugStringA("\n");
    #elif defined(__IOS__)
        extern void IOSLog(const char *msg);
        //IOSLog(msg);
    #else
        if (lev >= MINLOGLEVEL) vprintf(msg, args);
    #endif
    va_end(args);
}

void ProgramOutput(const char *msg)
{
    #ifdef __ANDROID__
        DebugLog(0, msg);
    #else
        printf("%s\n", msg);
    #endif
}

void MsgBox(const char *err)
{
    #if defined(__APPLE__) && !defined(__IOS__)
        // FIXME: this code should never be run when running from command line
        DialogRef alertDialog;
        CFStringRef sr = //CFStringCreateWithCharacters(NULL, err, strlen(err));
        CFStringCreateWithCString(kCFAllocatorDefault, err, ::GetApplicationTextEncoding());
        CreateStandardAlert(kAlertStopAlert, CFSTR("Error:"), sr, NULL, &alertDialog);
        RunStandardAlert (alertDialog, NULL, NULL);
    #endif
}

#ifndef WIN32   // emulate QPC on *nix, thanks Lee
    struct LARGE_INTEGER
    {
        long long int QuadPart;
    };

    void QueryPerformanceCounter(LARGE_INTEGER *dst)
    {
        struct timeval t;
        gettimeofday (& t, NULL);
        dst->QuadPart = t.tv_sec * 1000000LL + t.tv_usec;
    }

    void QueryPerformanceFrequency(LARGE_INTEGER *dst)
    {
        dst->QuadPart = 1000000LL;
    }
#endif

LARGE_INTEGER freq, start;

void InitTime()
{
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
}

double SecondsSinceStart()
{
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);
    return double(end.QuadPart - start.QuadPart) / double(freq.QuadPart);
}
