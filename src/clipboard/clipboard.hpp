#pragma once

#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdio>
#endif

namespace terminadventure::clipboard
{

inline std::string Read()
{
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return "";
    std::string result;

    if (HANDLE h = GetClipboardData(CF_UNICODETEXT))
    {
        if (const wchar_t* w = static_cast<const wchar_t*>(GlobalLock(h)))
        {
            const int size = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
            if (size > 1)
            {
                result.resize(static_cast<std::size_t>(size) - 1);
                WideCharToMultiByte(CP_UTF8, 0, w, -1, &result[0], size, nullptr, nullptr);
            }
            GlobalUnlock(h);
        }
    }

    if (result.empty())
    {
        if (HANDLE h = GetClipboardData(CF_TEXT))
        {
            if (const char* a = static_cast<const char*>(GlobalLock(h)))
            {
                const int wide = MultiByteToWideChar(CP_ACP, 0, a, -1, nullptr, 0);
                if (wide > 1)
                {
                    std::wstring wbuf(static_cast<std::size_t>(wide), 0);
                    MultiByteToWideChar(CP_ACP, 0, a, -1, &wbuf[0], wide);
                    const int utf8 = WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (utf8 > 1)
                    {
                        result.resize(static_cast<std::size_t>(utf8) - 1);
                        WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), -1, &result[0], utf8, nullptr, nullptr);
                    }
                }
                GlobalUnlock(h);
            }
        }
    }

    CloseClipboard();
    return result;
#else
    const char* commands[] = {
        "wl-paste --no-newline 2>/dev/null",
        "xclip -o -selection clipboard 2>/dev/null",
        "xsel -o --clipboard 2>/dev/null",
    };
    for (const char* cmd : commands)
    {
        FILE* pipe = popen(cmd, "r");
        if (pipe == nullptr) continue;
        std::string data;
        char buf[512];
        std::size_t n;
        while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0)
        {
            data.append(buf, n);
        }
        const int rc = pclose(pipe);
        if (rc == 0 && !data.empty()) return data;
    }
    return "";
#endif
}

inline bool Write(const std::string& text)
{
#ifdef _WIN32
    const int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wlen <= 1) return false;
    std::wstring wide(static_cast<std::size_t>(wlen), 0);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide[0], wlen);

    if (!OpenClipboard(nullptr)) return false;
    if (!EmptyClipboard())
    {
        CloseClipboard();
        return false;
    }

    const std::size_t bytes = static_cast<std::size_t>(wlen) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h == nullptr)
    {
        CloseClipboard();
        return false;
    }
    void* lock = GlobalLock(h);
    if (lock == nullptr)
    {
        GlobalFree(h);
        CloseClipboard();
        return false;
    }
    memcpy(lock, wide.c_str(), bytes);
    GlobalUnlock(h);

    if (!SetClipboardData(CF_UNICODETEXT, h))
    {
        GlobalFree(h);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
#else
    const char* commands[] = {
        "wl-copy 2>/dev/null",
        "xclip -i -selection clipboard 2>/dev/null",
        "xsel --input --clipboard 2>/dev/null",
    };
    for (const char* cmd : commands)
    {
        FILE* pipe = popen(cmd, "w");
        if (pipe == nullptr) continue;
        std::fwrite(text.data(), 1, text.size(), pipe);
        const int rc = pclose(pipe);
        if (rc == 0) return true;
    }
    return false;
#endif
}

}  // namespace terminadventure::clipboard
