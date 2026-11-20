// minihtml.cpp — single-file HTML/CSS/JavaScript browser engine for Windows
// Compile (Visual Studio): cl /std:c++17 /EHsc /utf-8 minihtml.cpp /link user32.lib gdi32.lib comctl32.lib winhttp.lib shlwapi.lib gdiplus.lib

#if defined(_WIN32)
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <sstream>
#include <cwctype>
#include <cwchar>
#include <regex>
#include <memory>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <cstring>
#include <limits>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>


// UTF-8 conversion helpers shared by URL/path handling on Windows.
// These are intentionally named identically to the Linux backend helpers.
static std::string WToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value.data(), (int)value.size(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result((size_t)size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value.data(), (int)value.size(), &result[0], size, nullptr, nullptr);
    return result;
}
static std::wstring Utf8ToW(const std::string& value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), (int)value.size(), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result((size_t)size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), (int)value.size(), &result[0], size);
    return result;
}

static int FillRoundedRect(HDC dc, const RECT* r, HBRUSH b, int radius) {
    if (!dc || !r || !b)return 0;
    int rad = std::max(0, std::min(radius, (int)std::min(r->right - r->left, r->bottom - r->top) / 2));
    if (rad <= 0) { FillRect(dc, r, b); return 1; }
    HGDIOBJ old = SelectObject(dc, b);
    BOOL ok = RoundRect(dc, (int)r->left, (int)r->top, (int)r->right, (int)r->bottom, rad * 2, rad * 2);
    if (old)SelectObject(dc, old);
    return ok ? 1 : 0;
}

#else
// Native Linux backend: X11 windowing/drawing and libcurl networking.
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <sstream>
#include <cwctype>
#include <cwchar>
#include <regex>
#include <memory>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <stdexcept>
#include <cstring>
#include <limits>
#include <climits>
#include <numeric>
#include <utility>
#include <ctime>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <png.h>
#include <jpeglib.h>
#include <webp/decode.h>
#include <setjmp.h>
#undef Bool
#include <curl/curl.h>
#include <cstdio>
#include <cstdlib>
#include <clocale>
#include <codecvt>
#include <locale>
#include <thread>
#include <mutex>
#include <atomic>
#include <ucontext.h>
#include <deque>
#include <condition_variable>
#include <deque>
#include <condition_variable>
#include <fstream>
#include <filesystem>
#include <thread>
#include <optional>
#include <map>

using BYTE = unsigned char;
using DWORD = unsigned long;
using UINT = unsigned int;
using UINT_PTR = uintptr_t;
using COLORREF = unsigned long;
using BOOL = int;
using WPARAM = unsigned long;
using LPARAM = long;
using LRESULT = long;
using HWND = void*;
using HINSTANCE = void*;
using HGDIOBJ = struct LinuxGdiObject*;
using HFONT = HGDIOBJ;
using HBRUSH = HGDIOBJ;
using HPEN = HGDIOBJ;
using HDC = struct LinuxDC*;
struct RECT { long left = 0, top = 0, right = 0, bottom = 0; };
struct POINT { long x = 0, y = 0; };
struct SIZE { long cx = 0, cy = 0; };
#define TRUE 1
#define TA_LEFT 0
#define TA_RIGHT 2
#define TA_CENTER 6
#define PS_SOLID 0
#define FALSE 0
#define CLR_INVALID 0xFFFFFFFFul
#define FW_NORMAL 400
#define FW_BOLD 700
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define CLEARTYPE_QUALITY 0
#define DEFAULT_PITCH 0
#define FF_DONTCARE 0
#define TRANSPARENT 1
#define NULL_BRUSH 5
static inline int _wtoi(const wchar_t* s) { return s ? std::wcstol(s, nullptr, 10) : 0; }
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define MB_OK 0
#define MB_ICONINFORMATION 0
#define MB_YESNO 0
#define MB_ICONQUESTION 0
#define IDYES 1
#define RGB(r,g,b) ((COLORREF)(((unsigned long)(r)&255) | (((unsigned long)(g)&255)<<8) | (((unsigned long)(b)&255)<<16)))
#ifdef None
#undef None
#endif

static Display* g_xDisplay = nullptr;
static int g_xScreen = 0;
static int g_xScreenW = 1100, g_xScreenH = 800;
static Window g_xWindow = 0;
static GC g_xGC = nullptr;

struct LinuxGdiObject {
    enum Kind { Font, Brush, Pen, NullBrush } kind = Brush;
    COLORREF color = RGB(0, 0, 0);
    int width = 1;
    int pixelSize = 16;
    bool bold = false, italic = false, underline = false;
};
struct LinuxDC {
    Display* display = nullptr;
    Drawable drawable = 0;
    GC gc = nullptr;
    LinuxGdiObject* font = nullptr;
    LinuxGdiObject* brush = nullptr;
    LinuxGdiObject* pen = nullptr;
    POINT lastPoint{};
    int originY = 0;
    COLORREF textColor = RGB(0, 0, 0);
};
static unsigned long XPixel(COLORREF c) {
    return ((c & 255ul) << 16) | (c & 0x0000FF00ul) | ((c >> 16) & 255ul);
}
static XFontStruct* LinuxGetXFont(LinuxGdiObject* font) {
    static std::map<std::pair<int, bool>, XFontStruct*> cache;
    int px = font ? std::max(6, font->pixelSize) : 16;
    bool bold = font && font->bold;
    auto key = std::make_pair(px, bold);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    char pattern[160];
    std::snprintf(pattern, sizeof(pattern), "-*-helvetica-%s-r-normal--%d-*-*-*-*-*-iso8859-1", bold ? "bold" : "medium", px);
    XFontStruct* loaded = g_xDisplay ? XLoadQueryFont(g_xDisplay, pattern) : nullptr;
    if (!loaded && g_xDisplay) loaded = XLoadQueryFont(g_xDisplay, "fixed");
    cache[key] = loaded;
    return loaded;
}
static std::string WToUtf8(const std::wstring& s) {
    try { return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(s); }
    catch (...) { std::string o; for (wchar_t c : s) o += (c >= 32 && c < 127) ? (char)c : '?'; return o; }
}
static std::wstring Utf8ToW(const std::string& s) {
    try { return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(s); }
    catch (...) { std::wstring o; for (unsigned char c : s) o += c; return o; }
}
static HDC LinuxMakeDC(Drawable d) {
    if (!g_xDisplay) return nullptr;
    LinuxDC* dc = new LinuxDC(); dc->display = g_xDisplay; dc->drawable = d;
    dc->gc = XCreateGC(g_xDisplay, d, 0, nullptr);
    return dc;
}
static HDC GetDC(HWND) {
    if (!g_xDisplay) { g_xDisplay = XOpenDisplay(nullptr); if (!g_xDisplay) return nullptr; g_xScreen = DefaultScreen(g_xDisplay); g_xScreenW = DisplayWidth(g_xDisplay, g_xScreen); g_xScreenH = DisplayHeight(g_xDisplay, g_xScreen); }
    return LinuxMakeDC(RootWindow(g_xDisplay, g_xScreen));
}
static int ReleaseDC(HWND, HDC dc) { if (dc) { if (dc->gc)XFreeGC(dc->display, dc->gc); delete dc; } return 1; }
static HFONT CreateFontW(int height, int, int, int, int weight, BOOL italic, BOOL, BOOL underline, UINT, UINT, UINT, UINT, UINT, const wchar_t*) {
    auto* f = new LinuxGdiObject(); f->kind = LinuxGdiObject::Font; f->pixelSize = std::max(6, std::abs(height)); f->bold = weight >= FW_BOLD; f->italic = italic != 0; f->underline = underline != 0; return f;
}
static HBRUSH CreateSolidBrush(COLORREF c) { auto* o = new LinuxGdiObject(); o->kind = LinuxGdiObject::Brush; o->color = c; return o; }
static HPEN CreatePen(int, int width, COLORREF c) { auto* o = new LinuxGdiObject(); o->kind = LinuxGdiObject::Pen; o->color = c; o->width = std::max(1, width); return o; }
static HGDIOBJ GetStockObject(int which) { static LinuxGdiObject nullBrush{ LinuxGdiObject::NullBrush }; return which == NULL_BRUSH ? &nullBrush : nullptr; }
static HGDIOBJ SelectObject(HDC dc, HGDIOBJ obj) {
    if (!dc || !obj)return nullptr; HGDIOBJ old = nullptr;
    if (obj->kind == LinuxGdiObject::Font) { old = dc->font; dc->font = obj; }
    else if (obj->kind == LinuxGdiObject::Brush || obj->kind == LinuxGdiObject::NullBrush) { old = dc->brush; dc->brush = obj; }
    else { old = dc->pen; dc->pen = obj; }
    return old;
}
static BOOL DeleteObject(HGDIOBJ o) { if (o && o->kind != LinuxGdiObject::NullBrush) delete o; return TRUE; }
static int FillRect(HDC dc, const RECT* r, HBRUSH b) {
    if (!dc || !r || !b)return 0; XSetForeground(dc->display, dc->gc, XPixel(b->color));
    XFillRectangle(dc->display, dc->drawable, dc->gc, (int)r->left, (int)(r->top + dc->originY), (unsigned)std::max(0L, r->right - r->left), (unsigned)std::max(0L, r->bottom - r->top)); return 1;
}
static int FillRoundedRect(HDC dc, const RECT* r, HBRUSH b, int radius) {
    if (!dc || !r || !b)return 0;
    const int x = (int)r->left, y = (int)r->top, w = (int)(r->right - r->left), h = (int)(r->bottom - r->top);
    const int rad = std::max(0, std::min(radius, std::min(w, h) / 2));
    if (rad <= 0)return FillRect(dc, r, b);
    XSetForeground(dc->display, dc->gc, XPixel(b->color));
    const int yy = y + dc->originY;
    XFillRectangle(dc->display, dc->drawable, dc->gc, x + rad, yy, (unsigned)std::max(0, w - 2 * rad), (unsigned)h);
    XFillRectangle(dc->display, dc->drawable, dc->gc, x, yy + rad, (unsigned)w, (unsigned)std::max(0, h - 2 * rad));
    XFillArc(dc->display, dc->drawable, dc->gc, x, yy, 2 * rad, 2 * rad, 90 * 64, 90 * 64);
    XFillArc(dc->display, dc->drawable, dc->gc, x + w - 2 * rad, yy, 2 * rad, 2 * rad, 0, 90 * 64);
    XFillArc(dc->display, dc->drawable, dc->gc, x + w - 2 * rad, yy + h - 2 * rad, 2 * rad, 2 * rad, 270 * 64, 90 * 64);
    XFillArc(dc->display, dc->drawable, dc->gc, x, yy + h - 2 * rad, 2 * rad, 2 * rad, 180 * 64, 90 * 64);
    return 1;
}
static BOOL IntersectRect(RECT* out, const RECT* a, const RECT* b) {
    if (!out || !a || !b)return FALSE; out->left = std::max(a->left, b->left); out->top = std::max(a->top, b->top); out->right = std::min(a->right, b->right); out->bottom = std::min(a->bottom, b->bottom); return out->right > out->left && out->bottom > out->top;
}
static BOOL PtInRect(const RECT* r, POINT p) { return r && p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom; }
static int Rectangle(HDC dc, int x1, int y1, int x2, int y2) { if (!dc)return 0; auto* p = dc->pen; if (p) { XSetForeground(dc->display, dc->gc, XPixel(p->color)); XSetLineAttributes(dc->display, dc->gc, p->width, LineSolid, CapButt, JoinMiter); }XDrawRectangle(dc->display, dc->drawable, dc->gc, x1, y1 + dc->originY, std::max(0, x2 - x1 - 1), std::max(0, y2 - y1 - 1)); return 1; }
static BOOL MoveToEx(HDC dc, int x, int y, POINT*) { if (!dc)return FALSE; dc->lastPoint = { x,y }; return TRUE; }
static BOOL LineTo(HDC dc, int x, int y) { if (!dc)return FALSE; auto& p = dc->lastPoint; if (dc->pen)XSetForeground(dc->display, dc->gc, XPixel(dc->pen->color)); XDrawLine(dc->display, dc->drawable, dc->gc, p.x, p.y + dc->originY, x, y + dc->originY); p = { x,y }; return TRUE; }
static COLORREF SetTextColor(HDC dc, COLORREF c) { if (!dc)return 0; auto old = dc->textColor; dc->textColor = c; return old; }
static int SetBkMode(HDC, int) { return TRANSPARENT; }
static BOOL GetTextExtentPoint32W(HDC dc, const wchar_t* text, int len, SIZE* size) {
    if (!size)return FALSE;
    int px = dc && dc->font ? dc->font->pixelSize : 16;
    std::wstring ws = text ? std::wstring(text, text + std::max(0, len)) : std::wstring();
    std::string utf8 = WToUtf8(ws); XFontStruct* f = LinuxGetXFont(dc ? dc->font : nullptr);
    size->cx = f ? (long)XTextWidth(f, utf8.c_str(), (int)utf8.size()) : (long)std::ceil(utf8.size() * px * 0.56);
    size->cy = f ? f->ascent + f->descent : px + 4; return TRUE;
}
static BOOL TextOutW(HDC dc, int x, int y, const wchar_t* text, int len) {
    if (!dc || !text)return FALSE;
    std::wstring s(text, text + std::max(0, len)); std::string u = WToUtf8(s);
    XSetForeground(dc->display, dc->gc, XPixel(dc->textColor));
    XFontStruct* f = LinuxGetXFont(dc->font);
    if (f) { XSetFont(dc->display, dc->gc, f->fid); XDrawString(dc->display, dc->drawable, dc->gc, x, y + dc->originY + f->ascent, u.c_str(), (int)u.size()); }
    else XDrawString(dc->display, dc->drawable, dc->gc, x, y + dc->originY + 14, u.c_str(), (int)u.size());
    return TRUE;
}
static void OutputDebugStringW(const wchar_t* s) { if (s) { std::string utf8 = WToUtf8(s); std::fwrite(utf8.data(), 1, utf8.size(), stderr); std::fflush(stderr); } }
static void OutputDebugStringA(const char* s) { if (s)std::fputs(s, stderr); }
static int GetSystemMetrics(int which) { return which == SM_CXSCREEN ? g_xScreenW : g_xScreenH; }
static int MessageBoxW(HWND, const wchar_t* text, const wchar_t* title, unsigned) { std::fwprintf(stderr, L"[%ls] %ls\n", title ? title : L"Message", text ? text : L""); return IDYES; }

namespace Gdiplus {
    constexpr int Ok = 0; constexpr int GenericError = 1; enum SmoothingMode { SmoothingModeAntiAlias = 4 };
class Bitmap {
public:
    int status = GenericError;
    unsigned width = 0, height = 0;
    std::vector<unsigned char> rgba;
    unsigned GetWidth() const { return width; }
    unsigned GetHeight() const { return height; }
    int GetLastStatus() const { return status; }
    static Bitmap* FromBytes(const BYTE* data, size_t size) {
        if (!data || !size) return nullptr;
        auto* b = new Bitmap();
        // PNG (libpng simplified API)
        if (size >= 8 && png_sig_cmp(data, 0, 8) == 0) {
            png_image image{}; image.version = PNG_IMAGE_VERSION;
            if (png_image_begin_read_from_memory(&image, data, size)) {
                image.format = PNG_FORMAT_RGBA;
                b->width = image.width; b->height = image.height;
                if (b->width && b->height && (uint64_t)b->width*b->height <= 64000000ULL) {
                    b->rgba.resize(PNG_IMAGE_SIZE(image));
                    if (png_image_finish_read(&image, nullptr, b->rgba.data(), 0, nullptr)) b->status = Ok;
                }
                png_image_free(&image);
            }
        }
        // WebP
        if (b->status != Ok && size >= 12) {
            int w = 0, h = 0;
            if (WebPGetInfo(data, size, &w, &h) && w > 0 && h > 0 && (uint64_t)w*h <= 64000000ULL) {
                uint8_t* decoded = WebPDecodeRGBA(data, size, &w, &h);
                if (decoded) { b->width=w; b->height=h; b->rgba.assign(decoded, decoded+(size_t)w*h*4); WebPFree(decoded); b->status=Ok; }
            }
        }
        // JPEG
        if (b->status != Ok && size > 3 && data[0] == 0xFF && data[1] == 0xD8) {
            struct JpegError { jpeg_error_mgr pub; jmp_buf jump; };
            jpeg_decompress_struct c{}; JpegError err{};
            c.err = jpeg_std_error(&err.pub);
            err.pub.error_exit = [](j_common_ptr info) { auto* e=(JpegError*)info->err; longjmp(e->jump,1); };
            if (setjmp(err.jump) == 0) {
                jpeg_create_decompress(&c); jpeg_mem_src(&c, data, (unsigned long)size);
                if (jpeg_read_header(&c, TRUE) == JPEG_HEADER_OK) {
                    c.out_color_space = JCS_RGB; jpeg_start_decompress(&c);
                    unsigned w=c.output_width, h=c.output_height, ch=c.output_components;
                    if (w && h && (uint64_t)w*h <= 64000000ULL && (ch==3 || ch==1)) {
                        b->width=w; b->height=h; b->rgba.resize((size_t)w*h*4);
                        std::vector<JSAMPLE> row((size_t)w*ch);
                        while (c.output_scanline < c.output_height) {
                            JSAMPROW rp=row.data(); jpeg_read_scanlines(&c,&rp,1);
                            size_t y=c.output_scanline-1;
                            for(unsigned x=0;x<w;x++){ size_t q=(y*w+x)*4; unsigned char v=row[x*ch]; b->rgba[q]=v; b->rgba[q+1]=ch==3?row[x*ch+1]:v; b->rgba[q+2]=ch==3?row[x*ch+2]:v; b->rgba[q+3]=255; }
                        }
                        b->status=Ok;
                    }
                    jpeg_finish_decompress(&c);
                }
                jpeg_destroy_decompress(&c);
            } else { jpeg_destroy_decompress(&c); }
        }
        // Uncompressed 24/32-bit BMP.
        if (b->status != Ok && size >= 54 && data[0]=='B' && data[1]=='M') {
            auto rd32=[&](size_t o)->uint32_t{return (uint32_t)data[o]|((uint32_t)data[o+1]<<8)|((uint32_t)data[o+2]<<16)|((uint32_t)data[o+3]<<24);};
            auto rd16=[&](size_t o)->uint16_t{return (uint16_t)data[o]|((uint16_t)data[o+1]<<8);};
            uint32_t off=rd32(10), dib=rd32(14); int32_t wi=(int32_t)rd32(18), he=(int32_t)rd32(22);
            uint16_t planes=rd16(26), bits=rd16(28); uint32_t compression=rd32(30);
            if(dib>=40&&wi>0&&he!=0&&planes==1&&(bits==24||bits==32)&&compression==0){
                uint32_t hh=(uint32_t)(he<0?-((int64_t)he):he), ww=(uint32_t)wi;
                uint64_t stride=(((uint64_t)ww*bits+31)/32)*4;
                if(ww&&hh&&(uint64_t)ww*hh<=64000000ULL&&off+stride*hh<=size){
                    b->width=ww;b->height=hh;b->rgba.resize((size_t)ww*hh*4);
                    for(uint32_t y=0;y<hh;y++){uint32_t sy=he<0?y:hh-1-y;for(uint32_t x=0;x<ww;x++){
                        size_t s=off+(size_t)sy*stride+(size_t)x*(bits/8),d=((size_t)y*ww+x)*4;
                        b->rgba[d]=data[s+2];b->rgba[d+1]=data[s+1];b->rgba[d+2]=data[s];b->rgba[d+3]=bits==32&&data[s+3]?data[s+3]:255;
                    }}b->status=Ok;
                }
            }
        }
        // SVG: small built-in rasterizer for common inline/vector-image primitives.
        // Supports root dimensions/viewBox and rect, circle, ellipse; complex paths,
        // filters, text, gradients and external resources are intentionally not claimed.
        if (b->status != Ok && size >= 4) {
            std::string svg((const char*)data, size);
            auto numberAttr = [](const std::string& tag, const char* name, double fallback) {
                std::regex re(std::string("(?:^|\\s)") + name + R"(\s*=\s*["']\s*([-+]?[0-9]*\.?[0-9]+))", std::regex::icase);
                std::smatch m; return std::regex_search(tag,m,re) ? std::stod(m[1].str()) : fallback;
            };
            if (svg.find("<svg") != std::string::npos) {
                std::smatch vm; std::regex vr(R"(viewBox\s*=\s*["']\s*[-+0-9.eE]+\s+[-+0-9.eE]+\s+([-+0-9.eE]+)\s+([-+0-9.eE]+))",std::regex::icase);
                double vw=0,vh=0; if(std::regex_search(svg,vm,vr)){vw=std::stod(vm[1]);vh=std::stod(vm[2]);}
                size_t rootEnd=svg.find('>',svg.find("<svg"));
                std::string root=svg.substr(svg.find("<svg"),rootEnd==std::string::npos?svg.size():rootEnd-svg.find("<svg"));
                double dw=numberAttr(root,"width",vw>0?vw:300), dh=numberAttr(root,"height",vh>0?vh:150);
                if(dw>0&&dh>0&&dw<=4096&&dh<=4096){
                    b->width=(unsigned)std::max(1.0,std::round(dw)); b->height=(unsigned)std::max(1.0,std::round(dh));
                    b->rgba.assign((size_t)b->width*b->height*4,0);
                    auto color = [](const std::string& s, unsigned char* out)->bool {
                        if(s=="none"||s.empty())return false;
                        unsigned r=0,g=0,bl=0;
                        if(s[0]=='#' && (s.size()==7||s.size()==4)){
                            if(s.size()==7){r=std::stoul(s.substr(1,2),nullptr,16);g=std::stoul(s.substr(3,2),nullptr,16);bl=std::stoul(s.substr(5,2),nullptr,16);}
                            else {r=std::stoul(s.substr(1,1),nullptr,16)*17;g=std::stoul(s.substr(2,1),nullptr,16)*17;bl=std::stoul(s.substr(3,1),nullptr,16)*17;}
                        } else if(s=="red"){r=255;} else if(s=="green"){g=128;} else if(s=="blue"){bl=255;} else if(s=="black"){} else if(s=="white"){r=g=bl=255;} else if(s=="yellow"){r=g=255;} else if(s=="purple"){r=128;bl=128;} else return false;
                        out[0]=r;out[1]=g;out[2]=bl;out[3]=255;return true;
                    };
                    auto paintRect=[&](double x,double y,double w,double h,const unsigned char* c){
                        int x0=std::max(0,(int)std::floor(x)),y0=std::max(0,(int)std::floor(y));
                        int x1=std::min((int)b->width,(int)std::ceil(x+w)),y1=std::min((int)b->height,(int)std::ceil(y+h));
                        for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++){size_t q=((size_t)yy*b->width+xx)*4;memcpy(&b->rgba[q],c,4);}
                    };
                    std::regex shapes(R"(<(rect|circle|ellipse)\b([^>]*)/?>)",std::regex::icase);
                    for(std::sregex_iterator it(svg.begin(),svg.end(),shapes),end;it!=end;++it){
                        std::string tag=(*it)[0], kind=(*it)[1], attrs=(*it)[2];
                        std::smatch fm; std::regex fr(R"(fill\s*=\s*["']([^"']+)["'])",std::regex::icase);
                        unsigned char c[4]={0,0,0,255}; if(std::regex_search(attrs,fm,fr)&&!color(fm[1],c))continue;
                        if(kind=="rect"){
                            paintRect(numberAttr(attrs,"x",0),numberAttr(attrs,"y",0),numberAttr(attrs,"width",0),numberAttr(attrs,"height",0),c);
                        } else {
                            double cx=numberAttr(attrs,"cx",0),cy=numberAttr(attrs,"cy",0),rx=kind=="circle"?numberAttr(attrs,"r",0):numberAttr(attrs,"rx",0),ry=kind=="circle"?rx:numberAttr(attrs,"ry",0);
                            if(rx<=0||ry<=0)continue;
                            int x0=std::max(0,(int)std::floor(cx-rx)),x1=std::min((int)b->width,(int)std::ceil(cx+rx));
                            int y0=std::max(0,(int)std::floor(cy-ry)),y1=std::min((int)b->height,(int)std::ceil(cy+ry));
                            for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++){double dx=(xx+.5-cx)/rx,dy=(yy+.5-cy)/ry;if(dx*dx+dy*dy<=1){size_t q=((size_t)yy*b->width+xx)*4;memcpy(&b->rgba[q],c,4);}}
                        }
                    }
                    b->status=Ok;
                }
            }
        }
        if (b->status != Ok) { delete b; return nullptr; }
        return b;
    }
};
class Graphics {
    HDC dc_;
public:
    explicit Graphics(HDC d) : dc_(d) {}
    void SetSmoothingMode(SmoothingMode) {}
    void DrawImage(Bitmap* bmp, int x, int y, int w, int h) {
        if (!dc_ || !bmp || bmp->status != Ok || w <= 0 || h <= 0 || !dc_->display) return;
        XImage* image = XCreateImage(dc_->display, DefaultVisual(dc_->display, DefaultScreen(dc_->display)),
            (unsigned)DefaultDepth(dc_->display, DefaultScreen(dc_->display)), ZPixmap, 0, nullptr,
            (unsigned)w, (unsigned)h, 32, 0);
        if (!image) return;
        image->data = (char*)calloc((size_t)image->bytes_per_line, (size_t)h);
        if (!image->data) { image->data=nullptr; XDestroyImage(image); return; }
        for (int dy=0; dy<h; ++dy) {
            unsigned sy=(unsigned)((uint64_t)dy*bmp->height/h);
            for (int dx=0; dx<w; ++dx) {
                unsigned sx=(unsigned)((uint64_t)dx*bmp->width/w);
                size_t p=((size_t)sy*bmp->width+sx)*4;
                unsigned a=bmp->rgba[p+3];
                unsigned r=(bmp->rgba[p]*a+255*(255-a))/255;
                unsigned g=(bmp->rgba[p+1]*a+255*(255-a))/255;
                unsigned b=(bmp->rgba[p+2]*a+255*(255-a))/255;
                XPutPixel(image,dx,dy,((r&255)<<16)|((g&255)<<8)|(b&255));
            }
        }
        XPutImage(dc_->display,dc_->drawable,dc_->gc,image,0,0,x,y+dc_->originY,(unsigned)w,(unsigned)h);
        XDestroyImage(image);
    }
};
}
using namespace Gdiplus;

static size_t CurlWrite(char* ptr, size_t size, size_t nmemb, void* userdata) { auto* v = (std::vector<BYTE>*)userdata; size_t n = size * nmemb; v->insert(v->end(), (BYTE*)ptr, (BYTE*)ptr + n); return n; }
#endif

// ============================================================
// EMBEDDED JAVASCRIPT RUNTIME (single-file implementation)
// ============================================================
// minihtml JavaScript runtime (from-scratch interpreter subset)
// C++17 only; no external JavaScript engine.
// This is an embeddable ES-style subset, not a complete ECMAScript implementation.


using namespace std;

namespace minihtml_js {
    using std::wstring;
    using std::vector;
    using std::unordered_map;
    using std::shared_ptr;
    using std::make_shared;

    class Runtime;
    struct Object;
    struct WorkerState;
    struct Function;
    struct Environment;

    struct Value {
        enum Type { Undefined, Null, Boolean, Number, String, ObjectValue, FunctionValue } type = Undefined;
        bool boolean = false;
        double number = 0.0;
        wstring string;
        shared_ptr<Object> object;
        shared_ptr<Function> function;

        static Value Undef() { return Value(); }
        static Value NullValue() { Value v; v.type = Null; return v; }
        static Value Bool(bool x) { Value v; v.type = Boolean; v.boolean = x; return v; }
        static Value Num(double x) { Value v; v.type = Number; v.number = x; return v; }
        static Value Str(const wstring& x) { Value v; v.type = String; v.string = x; return v; }
        static Value Obj(const shared_ptr<Object>& x) { Value v; v.type = ObjectValue; v.object = x; return v; }
        static Value Fn(const shared_ptr<Function>& x) { Value v; v.type = FunctionValue; v.function = x; return v; }
    };

    struct JSException : std::runtime_error {
        Value thrown;
        explicit JSException(const Value& v, const std::string& message) : std::runtime_error(message), thrown(v) {}
    };

    struct Object {
        unordered_map<wstring, Value> properties;
        vector<Value> elements;
        shared_ptr<Object> prototype; // ordinary [[Prototype]] chain
        bool array = false;
        void* native = nullptr;
        std::function<Value(Runtime&, const wstring&)> getter;
        std::function<void(Runtime&, const wstring&, const Value&)> setter;
        std::function<bool(Runtime&, const wstring&)> has;
        std::shared_ptr<WorkerState> workerState;
    };

    struct WorkerState {
        std::mutex mutex;
        std::condition_variable condition;
        std::deque<std::string> incoming;
        std::atomic<bool> terminated{ false };
        std::thread thread;
        std::weak_ptr<Object> owner;
        ~WorkerState() {
            terminated.store(true);
            condition.notify_all();
            if (thread.joinable()) {
                if (thread.get_id() == std::this_thread::get_id()) thread.detach();
                else thread.join();
            }
        }
    };

    struct Environment {
        unordered_map<wstring, Value> values;
        shared_ptr<Environment> parent;
        explicit Environment(shared_ptr<Environment> p = nullptr) : parent(std::move(p)) {}
        bool HasLocal(const wstring& name) const { return values.find(name) != values.end(); }
        bool Has(const wstring& name) const { return HasLocal(name) || (parent && parent->Has(name)); }
        Value Get(const wstring& name) const {
            auto it = values.find(name);
            if (it != values.end()) return it->second;
            return parent ? parent->Get(name) : Value::Undef();
        }
        void Define(const wstring& name, const Value& value) { values[name] = value; }
        void Set(const wstring& name, const Value& value) {
            auto it = values.find(name);
            if (it != values.end()) { it->second = value; return; }
            if (parent && parent->Has(name)) { parent->Set(name, value); return; }
            values[name] = value;
        }
    };

    struct Function {
        vector<wstring> parameters;
        vector<wstring> defaultExpressions;
        wstring body;
        bool isAsync = false;
        shared_ptr<Environment> closure;
        unordered_map<wstring, Value> properties; // functions are objects in ECMAScript
        std::function<Value(Runtime&, const vector<Value>&, const Value&)> native;
    };

    inline wstring ToString(const Value& v) {
        switch (v.type) {
        case Value::Undefined: return L"undefined";
        case Value::Null: return L"null";
        case Value::Boolean: return v.boolean ? L"true" : L"false";
        case Value::Number: {
            if (std::isnan(v.number)) return L"NaN";
            if (std::isinf(v.number)) return v.number < 0 ? L"-Infinity" : L"Infinity";
            std::wostringstream ss; ss.precision(15); ss << v.number; return ss.str();
        }
        case Value::String: return v.string;
        case Value::ObjectValue: { if (v.object && v.object->array) { wstring out; for (size_t i = 0; i < v.object->elements.size(); ++i) { if (i)out += L","; if (v.object->elements[i].type != Value::Null && v.object->elements[i].type != Value::Undefined)out += ToString(v.object->elements[i]); }return out; }return L"[object Object]"; }
        case Value::FunctionValue: return L"function () { [native code] }";
        }
        return L"undefined";
    }
    inline double ToNumber(const Value& v) {
        switch (v.type) {
        case Value::Number: return v.number;
        case Value::Boolean: return v.boolean ? 1.0 : 0.0;
        case Value::Null: return 0.0;
        case Value::Undefined: return std::nan("");
        case Value::String: {
            wstring s = v.string;
            s.erase(0, s.find_first_not_of(L" \t\r\n"));
            auto p = s.find_last_not_of(L" \t\r\n"); if (p == wstring::npos) return 0.0; s.resize(p + 1);
            if (s.empty()) return 0.0;
            try { return std::stod(s); }
            catch (...) { return std::nan(""); }
        }
        default: return ToNumber(Value::Str(ToString(v)));
        }
    }
    inline bool Truthy(const Value& v) {
        switch (v.type) {
        case Value::Undefined: case Value::Null: return false;
        case Value::Boolean: return v.boolean;
        case Value::Number: return v.number != 0.0 && !std::isnan(v.number);
        case Value::String: return !v.string.empty();
        default: return true;
        }
    }

    struct Token { wstring text; int kind = 0; size_t begin = 0, end = 0; wstring regexFlags; };
    // kind: 0=end, 1=identifier, 2=number, 3=string, 4=operator/punctuation

    class Runtime {
        struct Reference {
            shared_ptr<Environment> environment;
            shared_ptr<Object> object;
            wstring key;
            Value baseValue;
            bool valid = false;
        };
        struct Expr { Value value; Reference ref; };
        struct ReturnSignal { Value value; };

        vector<Token> tokens;
        size_t pos = 0;
        wstring source;
        shared_ptr<Environment> environment;
        shared_ptr<Environment> globalEnvironment;
        Value currentThis = Value::Undef();
        bool suppress = false;
        bool breakRequested = false;
        bool continueRequested = false;
        wstring lastError;
        Value lastThrownValue = Value::Undef();
        static thread_local vector<std::function<void(Runtime&)>> microtaskQueue;
        static thread_local bool drainingMicrotasks;
        static thread_local int executionDepth;

        // Stackful async continuations. A suspended async function keeps its C++
        // interpreter stack, lexical environments and parser position intact.
        struct AsyncTask {
            std::function<void()> entry;
            Value promise;
            Value awaitValue = Value::Undef();
            bool awaitRejected = false;
            bool awaitReady = false;
            bool running = false;
            bool completed = false;
            int callerExecutionDepth = 0;
            int savedExecutionDepth = 0;
            AsyncTask* callerTask = nullptr;
            size_t stackSize = 512 * 1024;
#if defined(_WIN32)
            void* fiber = nullptr;
            void* callerFiber = nullptr;
            ~AsyncTask() { if (fiber) DeleteFiber(fiber); }
#else
            ucontext_t context{};
            ucontext_t* callerContext = nullptr;
            void* stack = nullptr;
            ~AsyncTask() { std::free(stack); }
#endif
        };
        static thread_local AsyncTask* currentAsyncTask;
        static thread_local vector<shared_ptr<AsyncTask>> activeAsyncTasks;
        struct WorkerEvent {
            std::weak_ptr<Object> owner;
            std::string json;
            std::string error;
            bool failed = false;
        };
        static std::mutex workerEventMutex;
        static std::deque<WorkerEvent> workerEvents;
        static std::mutex externalTaskMutex;
        static std::deque<std::function<void(Runtime&)>> externalTasks;
        static void QueueExternalTask(std::function<void(Runtime&)> task) {
            std::lock_guard<std::mutex> lock(externalTaskMutex);
            externalTasks.push_back(std::move(task));
        }
        static void QueueWorkerEvent(const std::weak_ptr<Object>& owner, const std::string& json, bool failed = false, const std::string& error = std::string()) {
            std::lock_guard<std::mutex> lock(workerEventMutex);
            workerEvents.push_back({ owner, json, error, failed });
        }
#if defined(_WIN32)
        static thread_local void* rootFiber;
#else
        static void AsyncFiberEntry() {
            AsyncTask* task = currentAsyncTask;
            if (!task) return;
            try { if (task->entry) task->entry(); }
            catch (...) { }
            task->entry = {};
            task->completed = true;
            task->running = false;
            YieldAsyncTask();
        }
#endif

        static void YieldAsyncTask() {
            AsyncTask* task = currentAsyncTask;
            if (!task) return;
            task->running = false;
            task->savedExecutionDepth = executionDepth;
            executionDepth = task->callerExecutionDepth;
            currentAsyncTask = task->callerTask;
#if defined(_WIN32)
            SwitchToFiber(task->callerFiber);
#else
            if (task->callerContext) swapcontext(&task->context, task->callerContext);
#endif
            executionDepth = task->savedExecutionDepth;
            currentAsyncTask = task;
            task->running = true;
        }

        static void ResumeAsyncTask(const shared_ptr<AsyncTask>& task) {
            if (!task || task->completed || task->running) return;
            AsyncTask* previous = currentAsyncTask;
            task->callerTask = previous;
            task->callerExecutionDepth = executionDepth;
#if defined(_WIN32)
            if (!rootFiber) {
                rootFiber = IsThreadAFiber() ? GetCurrentFiber() : ConvertThreadToFiber(nullptr);
            }
            if (!task->fiber) {
                auto entry = [](void* raw) {
                    auto* t = static_cast<AsyncTask*>(raw);
                    currentAsyncTask = t;
                    try { if (t->entry) t->entry(); }
                    catch (...) { }
                    t->entry = {};
                    t->completed = true;
                    t->running = false;
                    YieldAsyncTask();
                };
                task->fiber = CreateFiberEx(task->stackSize, task->stackSize, 0, entry, task.get());
                if (!task->fiber) throw std::runtime_error("CreateFiberEx failed for async function");
            }
            task->callerFiber = previous ? previous->fiber : rootFiber;
            task->running = true;
            currentAsyncTask = task.get();
            SwitchToFiber(task->fiber);
#else
            if (!task->stack) {
                task->stack = std::malloc(task->stackSize);
                if (!task->stack) throw std::bad_alloc();
                if (getcontext(&task->context) != 0) throw std::runtime_error("getcontext failed for async function");
                task->context.uc_stack.ss_sp = task->stack;
                task->context.uc_stack.ss_size = task->stackSize;
                task->context.uc_stack.ss_flags = 0;
                task->context.uc_link = nullptr;
                makecontext(&task->context, &Runtime::AsyncFiberEntry, 0);
            }
            ucontext_t callerContext{};
            task->callerContext = &callerContext;
            task->running = true;
            currentAsyncTask = task.get();
            if (swapcontext(&callerContext, &task->context) != 0) {
                currentAsyncTask = previous;
                throw std::runtime_error("swapcontext failed for async function");
            }
            task->callerContext = nullptr;
#endif
            currentAsyncTask = previous;
            if (task->completed) {
                auto it = std::find(activeAsyncTasks.begin(), activeAsyncTasks.end(), task);
                if (it != activeAsyncTasks.end()) activeAsyncTasks.erase(it);
            }
        }

        static bool IsIdentStart(wchar_t c) { return iswalpha(c) || c == L'_' || c == L'$'; }
        static bool IsIdentPart(wchar_t c) { return iswalnum(c) || c == L'_' || c == L'$'; }

        void Lex(const wstring& code) {
            source = code; tokens.clear(); pos = 0;
            size_t i = 0;
            const wstring ops[] = { L">>>=", L"===", L"!==", L">>>", L"**=", L"<<=", L">>=", L"==", L"!=", L"<=", L">=", L"&&", L"||", L"??", L"...", L"++", L"--", L"+=", L"-=", L"*=", L"/=", L"%=", L"|=", L"&=", L"^=", L"=>", L"**", L"<<", L">>", L"?." };
            while (i < code.size()) {
                wchar_t c = code[i];
                if (iswspace(c)) { ++i; continue; }
                if (c == L'/' && i + 1 < code.size() && code[i + 1] == L'/') { i += 2; while (i < code.size() && code[i] != L'\n') ++i; continue; }
                if (c == L'/' && i + 1 < code.size() && code[i + 1] == L'*') { i += 2; while (i + 1 < code.size() && !(code[i] == L'*' && code[i + 1] == L'/')) ++i; i = std::min(code.size(), i + 2); continue; }
                Token t; t.begin = i;
                // JavaScript regular-expression literals. A slash starts a regex
                // when the preceding token expects an expression operand.
                bool regexAllowed = tokens.empty();
                if (!tokens.empty()) {
                    const Token& prev = tokens.back();
                    if (prev.kind == 4) {
                        const wstring& op = prev.text;
                        regexAllowed = (op == L"(" || op == L"[" || op == L"{" || op == L"," || op == L";" || op == L":" || op == L"=" || op == L"!" || op == L"?" || op == L"&&" || op == L"||" || op == L"+" || op == L"-" || op == L"*" || op == L"%" || op == L"&" || op == L"|" || op == L"^" || op == L"~" || op == L"==" || op == L"===" || op == L"!=" || op == L"!==" || op == L"<" || op == L">" || op == L"<=" || op == L">=" || op == L"=>");
                    } else if (prev.kind == 1) {
                        const wstring& kw = prev.text;
                        regexAllowed = (kw == L"return" || kw == L"throw" || kw == L"case" || kw == L"delete" || kw == L"void" || kw == L"typeof" || kw == L"instanceof" || kw == L"in" || kw == L"yield" || kw == L"await");
                    }
                }
                if (c == L'/' && regexAllowed && i + 1 < code.size()) {
                    size_t patternBegin = i + 1, j = patternBegin;
                    bool escaped = false, inClass = false, closed = false;
                    for (; j < code.size(); ++j) {
                        wchar_t ch = code[j];
                        if (ch == L'\n' || ch == L'\r') break;
                        if (escaped) { escaped = false; continue; }
                        if (ch == L'\\') { escaped = true; continue; }
                        if (ch == L'[') { inClass = true; continue; }
                        if (ch == L']' && inClass) { inClass = false; continue; }
                        if (ch == L'/' && !inClass) { closed = true; break; }
                    }
                    if (closed) {
                        t.kind = 5; t.text = code.substr(patternBegin, j - patternBegin);
                        i = j + 1;
                        while (i < code.size() && iswalpha(code[i])) t.regexFlags += code[i++];
                    }
                }
                if (t.kind == 5) { /* already consumed a regular-expression literal */ }
                else if (IsIdentStart(c)) { size_t b = i++; while (i < code.size() && IsIdentPart(code[i])) ++i; t.text = code.substr(b, i - b); t.kind = 1; }
                else if (iswdigit(c) || (c == L'.' && i + 1 < code.size() && iswdigit(code[i + 1]))) {
                    size_t b = i;
                    if (c == L'0' && i + 1 < code.size() && (code[i + 1] == L'x' || code[i + 1] == L'X' || code[i + 1] == L'b' || code[i + 1] == L'B' || code[i + 1] == L'o' || code[i + 1] == L'O')) {
                        wchar_t basePrefix = (wchar_t)towlower(code[i + 1]); i += 2;
                        while (i < code.size()) { wchar_t d = code[i]; bool valid = basePrefix == L'x' ? iswxdigit(d) != 0 : basePrefix == L'b' ? (d == L'0' || d == L'1') : (d >= L'0' && d <= L'7'); if (!valid) break; ++i; }
                    } else {
                        ++i; while (i < code.size() && (iswdigit(code[i]) || code[i] == L'.' || code[i] == L'e' || code[i] == L'E' || code[i] == L'+' || code[i] == L'-')) {
                            if ((code[i] == L'+' || code[i] == L'-') && code[i - 1] != L'e' && code[i - 1] != L'E') break;
                            ++i;
                        }
                    }
                    t.text = code.substr(b, i - b); t.kind = 2;
                }
                else if (c == L'`') {
                    ++i; size_t b = i; int interpolationDepth = 0; wchar_t quote = 0; bool escaped = false;
                    while (i < code.size()) {
                        wchar_t ch = code[i];
                        if (escaped) { escaped = false; ++i; continue; }
                        if (ch == L'\\') { escaped = true; ++i; continue; }
                        if (interpolationDepth == 0) { if (ch == L'`') break; if (ch == L'$' && i + 1 < code.size() && code[i + 1] == L'{') { interpolationDepth = 1; i += 2; continue; } ++i; continue; }
                        if (quote) { if (ch == quote) quote = 0; ++i; continue; }
                        if (ch == L'\'' || ch == L'"' || ch == L'`') { quote = ch; ++i; continue; }
                        if (ch == L'{') ++interpolationDepth; else if (ch == L'}') --interpolationDepth;
                        ++i;
                    }
                    t.text = code.substr(b, i - b); t.kind = 6; if (i < code.size()) ++i;
                }
                else if (c == L'\'' || c == L'"') {
                    wchar_t q = c; ++i; wstring s;
                    while (i < code.size() && code[i] != q) {
                        wchar_t ch = code[i++];
                        if (ch == L'\\' && i < code.size()) {
                            wchar_t e = code[i++];
                            if (e == L'n') s += L'\n'; else if (e == L'r') s += L'\r'; else if (e == L't') s += L'\t'; else if (e == L'0') s += L'\0'; else s += e;
                        }
                        else s += ch;
                    }
                    if (i < code.size()) ++i;
                    t.text = s; t.kind = 3;
                }
                else {
                    bool found = false;
                    for (const auto& op : ops) if (code.compare(i, op.size(), op) == 0) { t.text = op; i += op.size(); found = true; break; }
                    if (!found) { t.text.assign(1, c); ++i; }
                    t.kind = 4;
                }
                t.end = i; tokens.push_back(std::move(t));
            }
            tokens.push_back({ L"<eof>", 0, code.size(), code.size() });
        }
        const Token& Peek(size_t ahead = 0) const { return tokens[std::min(pos + ahead, tokens.size() - 1)]; }
        bool Is(const wstring& s) const {
            if (Peek().text != s) return false;
            if (s.empty()) return false;
            if (IsIdentStart(s[0])) return Peek().kind == 1;
            return Peek().kind == 4;
        }
        bool Match(const wstring& s) { if (Is(s)) { ++pos; return true; } return false; }
        Token Take() { Token t = Peek(); if (pos < tokens.size() - 1) ++pos; return t; }
        void Expect(const wstring& s) {
            if (!Match(s)) {
#if !defined(_WIN32)
                std::fprintf(stderr, "[minihtml JS parse] expected=%s got=%s char=%zu line=%zu before=%.100s\n", WToUtf8(s).c_str(), WToUtf8(Peek().text).c_str(), Peek().begin, 1 + std::count(source.begin(), source.begin() + std::min(Peek().begin, source.size()), L'\n'), WToUtf8(source.substr(Peek().begin > 70 ? Peek().begin - 70 : 0, 70)).c_str());
#endif
                throw std::runtime_error("Expected token: " + std::string(s.begin(), s.end()));
            }
        }
        void OptionalSemi() { Match(L";"); }
        wstring CaptureParen() {
            Expect(L"("); size_t b = pos; int depth = 1;
            while (Peek().kind != 0) { if (Is(L"(")) ++depth; else if (Is(L")") && --depth == 0) break; ++pos; }
            size_t e = pos; Expect(L")");
            if (b >= tokens.size() || e >= tokens.size()) return L"";
            return source.substr(tokens[b].begin, tokens[e].begin - tokens[b].begin);
        }
        wstring CaptureBlock() {
            Expect(L"{"); size_t b = pos; int depth = 1;
            while (Peek().kind != 0) { if (Is(L"{")) ++depth; else if (Is(L"}") && --depth == 0) break; ++pos; }
            size_t e = pos; if (!Is(L"}")) {
#if !defined(_WIN32)
                std::fprintf(stderr, "[minihtml JS capture-block EOF] start=%zu line=%zu source=%.100s\n", b < tokens.size() ? tokens[b].begin : 0, b < tokens.size() ? 1 + std::count(source.begin(), source.begin() + std::min(tokens[b].begin, source.size()), L'\n') : 0, b < tokens.size() ? WToUtf8(source.substr(tokens[b].begin, 100)).c_str() : "");
#endif
            }
            Expect(L"}");
            if (b >= tokens.size() || e >= tokens.size()) return L"";
            return source.substr(tokens[b].begin, tokens[e].begin - tokens[b].begin);
        }
        void ParseParameters(vector<wstring>& params, vector<wstring>& defaults) {
            while (!Is(L")") && Peek().kind != 0) {
                Token name = Take();
                if (name.kind != 1) throw runtime_error("Expected parameter name");
                params.push_back(name.text);
                wstring defaultExpr;
                if (Match(L"=")) {
                    size_t first = pos;
                    int paren = 0, bracket = 0, brace = 0;
                    while (Peek().kind != 0) {
                        const wstring& op = Peek().text;
                        if (paren == 0 && bracket == 0 && brace == 0 && (op == L"," || op == L")")) break;
                        if (op == L"(") ++paren; else if (op == L")") --paren;
                        else if (op == L"[") ++bracket; else if (op == L"]") --bracket;
                        else if (op == L"{") ++brace; else if (op == L"}") --brace;
                        ++pos;
                    }
                    if (pos > first) defaultExpr = source.substr(tokens[first].begin, tokens[pos - 1].end - tokens[first].begin);
                }
                defaults.push_back(defaultExpr);
                if (!Match(L",")) break;
            }
        }
        wstring CaptureStatement() {
            if (Is(L"{")) return CaptureBlock();
            size_t b = pos; int par = 0, br = 0, sq = 0;
            while (Peek().kind != 0) {
                if (Is(L"else") && par == 0 && br == 0 && sq == 0) break;
                if (Is(L";") && par == 0 && br == 0 && sq == 0) { ++pos; break; }
                if (Is(L"(")) ++par; else if (Is(L")")) --par;
                else if (Is(L"{")) ++br; else if (Is(L"}")) { if (br == 0) break; --br; }
                else if (Is(L"[")) ++sq; else if (Is(L"]")) --sq;
                ++pos;
                if (par < 0 || br < 0 || sq < 0) break;
            }
            if (b >= tokens.size()) return L"";
            size_t e = pos; if (e > b && tokens[e - 1].text == L";") --e;
            return source.substr(tokens[b].begin, tokens[e - 1].end - tokens[b].begin);
        }
        Value GetProperty(const Value& base, const wstring& key) {
            if (base.type == Value::FunctionValue && base.function) {
                auto f = base.function;
                auto it = f->properties.find(key);
                if (it != f->properties.end()) return it->second;
                if (key == L"call") return MakeNative([](Runtime& r, const vector<Value>& a, const Value& self) {
                    if (self.type != Value::FunctionValue) return Value::Undef();
                    Value thisArg = a.empty() ? Value::Undef() : a[0];
                    vector<Value> args; if (a.size() > 1) args.assign(a.begin() + 1, a.end());
                    return r.Invoke(self, args, thisArg);
                    });
                if (key == L"apply") return MakeNative([](Runtime& r, const vector<Value>& a, const Value& self) {
                    if (self.type != Value::FunctionValue) return Value::Undef();
                    Value thisArg = a.empty() ? Value::Undef() : a[0]; vector<Value> args;
                    if (a.size() > 1 && a[1].type == Value::ObjectValue && a[1].object) {
                        auto arr = a[1].object; if (arr->array) args = arr->elements;
                        else { auto it = arr->properties.find(L"length"); size_t n = it == arr->properties.end() ? 0 : (size_t)max(0.0, ToNumber(it->second)); for (size_t i = 0; i < n && i < 10000; ++i) { auto q = arr->properties.find(std::to_wstring(i)); args.push_back(q == arr->properties.end() ? Value::Undef() : q->second); } }
                    }
                    return r.Invoke(self, args, thisArg);
                    });
                if (key == L"bind") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {
                    if (self.type != Value::FunctionValue || !self.function) return Value::Undef();
                    auto original = self.function; Value thisArg = a.empty() ? Value::Undef() : a[0]; vector<Value> bound;
                    if (a.size() > 1)bound.assign(a.begin() + 1, a.end());
                    auto f = make_shared<Function>(); f->closure = original->closure; f->properties = original->properties;
                    f->native = [original, thisArg, bound](Runtime& r, const vector<Value>& more, const Value&) {vector<Value> all = bound; all.insert(all.end(), more.begin(), more.end()); return r.Invoke(Value::Fn(original), all, thisArg); };
                    return Value::Fn(f);
                    });
            }
            if (base.type == Value::Number) {
                if(key==L"toFixed")return MakeNative([](Runtime&,const vector<Value>& a,const Value& self){int digits=a.empty()?0:max(0,min(20,(int)ToNumber(a[0])));std::wostringstream ss;ss.setf(std::ios::fixed);ss.precision(digits);ss<<self.number;return Value::Str(ss.str());});
                if(key==L"toString")return MakeNative([](Runtime&,const vector<Value>& a,const Value& self){return Value::Str(ToString(self));});
            }
            if (base.type == Value::String) {
                if (key == L"length") return Value::Num((double)base.string.size());
                if (key == L"toString") return MakeNative([](Runtime&, const vector<Value>& a, const Value&) { return Value::Str(a.empty() ? L"" : ToString(a[0])); });
                if (key == L"indexOf") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) { wstring s = ToString(self), q = a.empty() ? L"undefined" : ToString(a[0]); size_t p = s.find(q); return Value::Num(p == wstring::npos ? -1 : (double)p); });
                if (key == L"substring" || key == L"slice") return MakeNative([key](Runtime&, const vector<Value>& a, const Value& self) { wstring s = ToString(self); int b = a.empty() ? 0 : (int)ToNumber(a[0]); int e = a.size() < 2 ? (int)s.size() : (int)ToNumber(a[1]); b = std::max(0, std::min((int)s.size(), b)); e = std::max(0, std::min((int)s.size(), e)); if (key == L"substring" && e < b) std::swap(b, e); if (key == L"slice" && e < 0) e = (int)s.size() + e; return Value::Str(s.substr((size_t)b, (size_t)std::max(0, e - b))); });
                if (key == L"startsWith" || key == L"endsWith") return MakeNative([key](Runtime&,const vector<Value>& a,const Value& self){wstring s=ToString(self),q=a.empty()?L"":ToString(a[0]);return Value::Bool(key==L"startsWith"?s.rfind(q,0)==0:(s.size()>=q.size()&&s.compare(s.size()-q.size(),q.size(),q)==0));});
                if (key == L"padStart" || key == L"padEnd") return MakeNative([key](Runtime&,const vector<Value>& a,const Value& self){wstring s=ToString(self);size_t n=a.empty()?0:(size_t)max(0.0,ToNumber(a[0]));wstring pad=a.size()>1?ToString(a[1]):L" ";if(pad.empty())return Value::Str(s);while(s.size()<n){if(key==L"padStart")s.insert(0,pad.substr(0,min(pad.size(),n-s.size())));else s+=pad.substr(0,min(pad.size(),n-s.size()));}return Value::Str(s);});
                if (key == L"match") return MakeNative([](Runtime&,const vector<Value>& a,const Value& self){if(a.empty()||a[0].type!=Value::ObjectValue||!a[0].object)return Value::NullValue();auto ri=a[0].object->properties.find(L"__minihtml_regex");wstring pattern=ri==a[0].object->properties.end()?ToString(a[0]):ToString(ri->second),flags=ToString(a[0].object->properties[L"__minihtml_flags"]),s=ToString(self);try{auto opt=std::regex_constants::ECMAScript;if(flags.find(L'i')!=wstring::npos)opt|=std::regex_constants::icase;std::wregex re(pattern,opt);std::wsmatch m;if(!std::regex_search(s,m,re))return Value::NullValue();auto out=make_shared<Object>();out->array=true;for(const auto& x:m)out->elements.push_back(Value::Str(x.str()));return Value::Obj(out);}catch(...){return Value::NullValue();}});
                if (key == L"includes") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {return Value::Bool(!a.empty() && ToString(self).find(ToString(a[0])) != wstring::npos); });
                if (key == L"charAt") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {wstring s = ToString(self); int i = a.empty() ? 0 : (int)ToNumber(a[0]); return Value::Str(i >= 0 && (size_t)i < s.size() ? wstring(1, s[(size_t)i]) : L""); });
                if (key == L"split") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {auto arr = make_shared<Object>(); arr->array = true; wstring s = ToString(self), sep = a.empty() ? L"" : ToString(a[0]); if (sep.empty()) { for (wchar_t c : s)arr->elements.push_back(Value::Str(wstring(1, c))); } else { size_t p = 0; for (;;) { size_t e = s.find(sep, p); if (e == wstring::npos) { arr->elements.push_back(Value::Str(s.substr(p))); break; }arr->elements.push_back(Value::Str(s.substr(p, e - p))); p = e + sep.size(); } }return Value::Obj(arr); });
                if (key == L"replace") return MakeNative([](Runtime& runtime, const vector<Value>& a, const Value& self) {
                    wstring s = ToString(self); if (a.size() < 2) return Value::Str(s);
                    wstring to = ToString(a[1]);
                    if (a[0].type == Value::ObjectValue && a[0].object) {
                        auto ri = a[0].object->properties.find(L"__minihtml_regex");
                        if (ri != a[0].object->properties.end()) {
                            wstring pattern = ToString(ri->second), flags;
                            auto fi = a[0].object->properties.find(L"__minihtml_flags"); if (fi != a[0].object->properties.end()) flags = ToString(fi->second);
                            try {
                                auto opt = std::regex_constants::ECMAScript;
                                if (flags.find(L'i') != wstring::npos) opt |= std::regex_constants::icase;
                                std::wregex re(pattern, opt);
                                if (a[1].type == Value::FunctionValue) { wstring out; auto begin=s.cbegin(); std::wsregex_iterator it(s.cbegin(),s.cend(),re),last; bool global=flags.find(L'g')!=wstring::npos; for(;it!=last;++it){const auto& m=*it;out.append(begin,m[0].first);out+=ToString(runtime.Invoke(a[1],{Value::Str(m.str())}));begin=m[0].second;if(!global)break;}out.append(begin,s.cend());s.swap(out); }
                                else if (flags.find(L'g') != wstring::npos) s = std::regex_replace(s, re, to);
                                else { std::wsmatch m; if (std::regex_search(s, m, re)) { wstring out = m.prefix().str() + m.format(to) + m.suffix().str(); s.swap(out); } }
                            } catch (...) { }
                            return Value::Str(s);
                        }
                    }
                    wstring from = ToString(a[0]); size_t p = s.find(from); if (p != wstring::npos) { wstring replacement=a[1].type==Value::FunctionValue?ToString(runtime.Invoke(a[1],{Value::Str(from)})):to; s.replace(p, from.size(), replacement); } return Value::Str(s);
                });
                if (key == L"toLowerCase" || key == L"toUpperCase" || key == L"trim") return MakeNative([key](Runtime&, const vector<Value>&, const Value& self) { wstring s = ToString(self); if (key == L"trim") { size_t b = s.find_first_not_of(L" \t\r\n"), e = s.find_last_not_of(L" \t\r\n"); return Value::Str(b == wstring::npos ? L"" : s.substr(b, e - b + 1)); } for (auto& c : s) c = key == L"toLowerCase" ? towlower(c) : towupper(c); return Value::Str(s); });
            }
            if (base.type == Value::ObjectValue && base.object) {
                auto o = base.object;
                if (o->properties.count(L"__minihtml_regex")) {
                    if (key == L"source") return o->properties[L"__minihtml_regex"];
                    if (key == L"global") return Value::Bool(ToString(o->properties[L"__minihtml_flags"]).find(L'g') != wstring::npos);
                    if (key == L"ignoreCase") return Value::Bool(ToString(o->properties[L"__minihtml_flags"]).find(L'i') != wstring::npos);
                    if (key == L"test" || key == L"exec") return MakeNative([key](Runtime&, const vector<Value>& a, const Value& self) {
                        if (!self.object || a.empty()) return key == L"test" ? Value::Bool(false) : Value::NullValue();
                        auto pi = self.object->properties.find(L"__minihtml_regex"); if (pi == self.object->properties.end()) return Value::NullValue();
                        wstring pattern = ToString(pi->second), flags = ToString(self.object->properties[L"__minihtml_flags"]);
                        try { auto opt = std::regex_constants::ECMAScript; if (flags.find(L'i') != wstring::npos) opt |= std::regex_constants::icase; std::wregex re(pattern, opt); std::wstring input = ToString(a[0]); std::wsmatch m; if (!std::regex_search(input, m, re)) return key == L"test" ? Value::Bool(false) : Value::NullValue(); if (key == L"test") return Value::Bool(true); auto arr = make_shared<Object>(); arr->array = true; for (const auto& part : m) arr->elements.push_back(Value::Str(part.str())); arr->properties[L"index"] = Value::Num((double)m.position()); arr->properties[L"input"] = Value::Str(input); return Value::Obj(arr); } catch (...) { return key == L"test" ? Value::Bool(false) : Value::NullValue(); }
                    });
                }
                if (o->array) {
                    if (key == L"length") return Value::Num((double)o->elements.size());
                    if (key == L"item") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) { int i = a.empty() ? 0 : (int)ToNumber(a[0]); return self.object && i >= 0 && (size_t)i < self.object->elements.size() ? self.object->elements[(size_t)i] : Value::NullValue(); });
                    if (key == L"push") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) { if (self.object) for (const auto& v : a) self.object->elements.push_back(v); return Value::Num(self.object ? (double)self.object->elements.size() : 0); });
                    if (key == L"pop") return MakeNative([](Runtime&, const vector<Value>&, const Value& self) { if (!self.object || self.object->elements.empty()) return Value::Undef(); Value v = self.object->elements.back(); self.object->elements.pop_back(); return v; });
                    if (key == L"indexOf") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {if (!self.object || a.empty())return Value::Num(-1); for (size_t i = 0; i < self.object->elements.size(); ++i)if (StrictEqual(self.object->elements[i], a[0]))return Value::Num((double)i); return Value::Num(-1); });
                    if (key == L"forEach") return MakeNative([](Runtime& r, const vector<Value>& a, const Value& self) {if (self.object && !a.empty())for (size_t i = 0; i < self.object->elements.size(); ++i)r.Invoke(a[0], { self.object->elements[i],Value::Num((double)i),self }); return Value::Undef(); });
                    if (key == L"map") return MakeNative([](Runtime& r, const vector<Value>& a, const Value& self) {auto out = make_shared<Object>(); out->array = true; if (self.object && !a.empty())for (size_t i = 0; i < self.object->elements.size(); ++i)out->elements.push_back(r.Invoke(a[0], { self.object->elements[i],Value::Num((double)i),self })); return Value::Obj(out); });
                    if (key == L"filter" || key == L"some" || key == L"every" || key == L"find" || key == L"findIndex") return MakeNative([key](Runtime& r,const vector<Value>& a,const Value& self){auto out=make_shared<Object>();out->array=true;if(!self.object||a.empty())return key==L"filter"?Value::Obj(out):(key==L"some"||key==L"every"?Value::Bool(key==L"every"):key==L"findIndex"?Value::Num(-1):Value::Undef());for(size_t i=0;i<self.object->elements.size();++i){Value v=self.object->elements[i];bool yes=Truthy(r.Invoke(a[0],{v,Value::Num((double)i),self}));if(key==L"filter"&&yes)out->elements.push_back(v);if(key==L"some"&&yes)return Value::Bool(true);if(key==L"every"&&!yes)return Value::Bool(false);if((key==L"find"||key==L"findIndex")&&yes)return key==L"find"?v:Value::Num((double)i);}if(key==L"filter")return Value::Obj(out);if(key==L"some")return Value::Bool(false);if(key==L"every")return Value::Bool(true);return key==L"findIndex"?Value::Num(-1):Value::Undef();});
                    if (key == L"reduce") return MakeNative([](Runtime& r,const vector<Value>& a,const Value& self){if(!self.object||self.object->elements.empty())return a.size()>1?a[1]:Value::Undef();if(a.empty())return Value::Undef();size_t i=a.size()>1?0:1;Value acc=a.size()>1?a[1]:self.object->elements[0];for(;i<self.object->elements.size();++i)acc=r.Invoke(a[0],{acc,self.object->elements[i],Value::Num((double)i),self});return acc;});
                    if (key == L"includes") return MakeNative([](Runtime&,const vector<Value>& a,const Value& self){if(!self.object||a.empty())return Value::Bool(false);for(const auto& v:self.object->elements)if(StrictEqual(v,a[0]))return Value::Bool(true);return Value::Bool(false);});
                    if (key == L"splice") return MakeNative([](Runtime&,const vector<Value>& a,const Value& self){auto out=make_shared<Object>();out->array=true;if(!self.object||!self.object->array)return Value::Obj(out);int n=(int)self.object->elements.size(),start=a.empty()?0:(int)ToNumber(a[0]);if(start<0)start=max(0,n+start);start=min(n,start);int count=a.size()<2?n-start:max(0,min(n-start,(int)ToNumber(a[1])));for(int i=0;i<count;++i)out->elements.push_back(self.object->elements[(size_t)(start+i)]);self.object->elements.erase(self.object->elements.begin()+start,self.object->elements.begin()+start+count);for(size_t i=2;i<a.size();++i)self.object->elements.insert(self.object->elements.begin()+start+(int)i-2,a[i]);return Value::Obj(out);});
                    if (key == L"sort") return MakeNative([](Runtime& r,const vector<Value>& a,const Value& self){if(!self.object)return self;if(!a.empty()&&a[0].type==Value::FunctionValue)std::stable_sort(self.object->elements.begin(),self.object->elements.end(),[&](const Value& x,const Value& y){return ToNumber(r.Invoke(a[0],{x,y}))<0;});else std::stable_sort(self.object->elements.begin(),self.object->elements.end(),[](const Value& x,const Value& y){return ToString(x)<ToString(y);});return self;});
                    if (key == L"join") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) { wstring sep = a.empty() ? L"," : ToString(a[0]), out; if (self.object) for (size_t i = 0; i < self.object->elements.size(); ++i) { if (i)out += sep; out += ToString(self.object->elements[i]); } return Value::Str(out); });
                    wchar_t* end = nullptr; long idx = wcstol(key.c_str(), &end, 10); if (end && *end == 0 && idx >= 0 && (size_t)idx < o->elements.size()) return o->elements[(size_t)idx];
                }
                if (o->getter) { Value v = o->getter(*this, key); if (v.type != Value::Undefined) return v; }
                auto it = o->properties.find(key); if (it != o->properties.end()) return it->second;
                for (auto proto = o->prototype; proto; proto = proto->prototype) { auto pi = proto->properties.find(key); if (pi != proto->properties.end()) return pi->second; }
                if (key == L"hasOwnProperty") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {if (a.empty() || self.type != Value::ObjectValue || !self.object)return Value::Bool(false); return Value::Bool(self.object->properties.count(ToString(a[0])) != 0); });
                if (key == L"toString") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {
                    int radix = a.empty() ? 10 : (int)ToNumber(a[0]);
                    if (self.type == Value::Number && radix >= 2 && radix <= 36 && std::isfinite(self.number)) {
                        static const wchar_t digits[] = L"0123456789abcdefghijklmnopqrstuvwxyz";
                        bool neg = self.number < 0; double absValue = std::fabs(self.number); unsigned long long whole = (unsigned long long)absValue;
                        wstring out; do { out.insert(out.begin(), digits[whole % (unsigned)radix]); whole /= (unsigned)radix; } while (whole);
                        double fraction = absValue - std::floor(absValue);
                        if (fraction > 0.0000000001) { out += L'.'; for (int i = 0; i < 12 && fraction > 0.0000000001; ++i) { fraction *= radix; int digit = (int)fraction; out += digits[max(0, min(radix - 1, digit))]; fraction -= digit; } }
                        if (neg) out.insert(out.begin(), L'-'); return Value::Str(out);
                    }
                    return Value::Str(ToString(self));
                });
                return Value::Undef();
            }
            if (key == L"toString") return MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {
                    int radix = a.empty() ? 10 : (int)ToNumber(a[0]);
                    if (self.type == Value::Number && radix >= 2 && radix <= 36 && std::isfinite(self.number)) {
                        static const wchar_t digits[] = L"0123456789abcdefghijklmnopqrstuvwxyz";
                        bool neg = self.number < 0; double absValue = std::fabs(self.number); unsigned long long whole = (unsigned long long)absValue;
                        wstring out; do { out.insert(out.begin(), digits[whole % (unsigned)radix]); whole /= (unsigned)radix; } while (whole);
                        double fraction = absValue - std::floor(absValue);
                        if (fraction > 0.0000000001) { out += L'.'; for (int i = 0; i < 12 && fraction > 0.0000000001; ++i) { fraction *= radix; int digit = (int)fraction; out += digits[max(0, min(radix - 1, digit))]; fraction -= digit; } }
                        if (neg) out.insert(out.begin(), L'-'); return Value::Str(out);
                    }
                    return Value::Str(ToString(self));
                });
            return Value::Undef();
        }
        void SetProperty(const Value& base, const wstring& key, const Value& v) {
            if (base.type == Value::FunctionValue && base.function) { base.function->properties[key] = v; return; }
            if (base.type != Value::ObjectValue || !base.object) return;
            auto o = base.object;
            if (o->array) {
                if (key == L"length") { size_t n = (size_t)std::max(0.0, ToNumber(v)); o->elements.resize(n); return; }
                wchar_t* end = nullptr; long idx = wcstol(key.c_str(), &end, 10);
                if (end && *end == 0 && idx >= 0 && idx < 100000) { if ((size_t)idx >= o->elements.size())o->elements.resize((size_t)idx + 1); o->elements[(size_t)idx] = v; return; }
            }
            if (o->setter) o->setter(*this, key, v);
            o->properties[key] = v;
        }
        Value Call(const Value& fn, const vector<Value>& args, const Value& thisValue) {
            if (suppress) return Value::Undef();
            if (fn.type != Value::FunctionValue || !fn.function) return Value::Undef();
            if (fn.function->native) return fn.function->native(*this, args, thisValue);
            if (fn.function->isAsync) {
                Value promise = MakePromise(*this);
                auto task = make_shared<AsyncTask>();
                task->promise = promise;
                auto functionObject = fn.function;
                auto asyncGlobalEnvironment = globalEnvironment;
                task->entry = [asyncGlobalEnvironment, task, functionObject, args, thisValue]() {
                    auto callEnv = make_shared<Environment>(functionObject->closure ? functionObject->closure : asyncGlobalEnvironment);
                    for (size_t i = 0; i < functionObject->parameters.size(); ++i) {
                        Value value = i < args.size() ? args[i] : Value::Undef();
                        if (value.type == Value::Undefined && i < functionObject->defaultExpressions.size() && !functionObject->defaultExpressions[i].empty()) {
                            Runtime defaultRuntime(callEnv);
                            value = defaultRuntime.Evaluate(functionObject->defaultExpressions[i]);
                        }
                        callEnv->Define(functionObject->parameters[i], value);
                    }
                    auto argumentsObject = make_shared<Object>();
                    argumentsObject->array = true;
                    argumentsObject->elements = args;
                    callEnv->Define(L"arguments", Value::Obj(argumentsObject));
                    Runtime nested(callEnv);
                    nested.currentThis = thisValue;
                    try {
                        nested.Lex(functionObject->body);
                        while (nested.Peek().kind != 0) nested.Statement();
                        SettlePromise(nested, task->promise, false, Value::Undef());
                    } catch (const ReturnSignal& returned) {
                        SettlePromise(nested, task->promise, false, returned.value);
                    } catch (const JSException& error) {
                        SettlePromise(nested, task->promise, true, error.thrown);
                    } catch (const std::exception& error) {
                        SettlePromise(nested, task->promise, true, Value::Str(wstring(error.what(), error.what() + strlen(error.what()))));
                    } catch (...) {
                        SettlePromise(nested, task->promise, true, Value::Str(L"Unknown async function exception"));
                    }
                    task->completed = true;
                };
                activeAsyncTasks.push_back(task);
                ResumeAsyncTask(task);
                return promise;
            }
            auto callEnv = make_shared<Environment>(fn.function->closure ? fn.function->closure : globalEnvironment);
            for (size_t i = 0; i < fn.function->parameters.size(); ++i) {
                Value value = i < args.size() ? args[i] : Value::Undef();
                if (value.type == Value::Undefined && i < fn.function->defaultExpressions.size() && !fn.function->defaultExpressions[i].empty()) {
                    Runtime defaultRuntime(callEnv);
                    value = defaultRuntime.Evaluate(fn.function->defaultExpressions[i]);
                }
                callEnv->Define(fn.function->parameters[i], value);
            }
            auto argumentsObject = make_shared<Object>(); argumentsObject->array = true; argumentsObject->elements = args;
            Value arrayCtor = globalEnvironment->Get(L"Array"); if (arrayCtor.type == Value::FunctionValue) { auto p = arrayCtor.function->properties.find(L"prototype"); if (p != arrayCtor.function->properties.end() && p->second.type == Value::ObjectValue)argumentsObject->prototype = p->second.object; }
            callEnv->Define(L"arguments", Value::Obj(argumentsObject));
            Runtime nested(callEnv);
            nested.currentThis = thisValue;
            auto asyncWrap = [&](const Value& result, bool rejected=false) -> Value {
                if (!fn.function->isAsync) return result;
                Value promise = MakePromise(nested);
                if (IsPromiseValue(result)) {
                    auto stateIt=result.object->properties.find(L"__promise_state");
                    if(stateIt!=result.object->properties.end()&&ToString(stateIt->second)!=L"pending") {
                        bool innerRejected=ToString(stateIt->second)==L"rejected";
                        SettlePromise(nested,promise,innerRejected,result.object->properties[L"__promise_value"]);
                    } else {
                        Value ok=nested.MakeNative([promise](Runtime& rr,const vector<Value>& a,const Value&){SettlePromise(rr,promise,false,a.empty()?Value::Undef():a[0]);return Value::Undef();});
                        Value bad=nested.MakeNative([promise](Runtime& rr,const vector<Value>& a,const Value&){SettlePromise(rr,promise,true,a.empty()?Value::Undef():a[0]);return Value::Undef();});
                        nested.Invoke(result.object->properties[L"then"],{ok,bad},result);
                    }
                } else SettlePromise(nested,promise,rejected,result);
                return promise;
            };
            nested.Lex(fn.function->body);
            try {
                while (nested.Peek().kind != 0) nested.Statement();
            }
            catch (const ReturnSignal& r) {
                return asyncWrap(r.value);
            }
            catch (const JSException& e) {
                if(fn.function->isAsync) return asyncWrap(e.thrown,true);
                throw;
            }
            catch (const std::exception& e) {
                if(fn.function->isAsync) return asyncWrap(Value::Str(wstring(e.what(),e.what()+strlen(e.what()))),true);
                throw;
            }
            return asyncWrap(Value::Undef());
        }
        static bool StrictEqual(const Value& a, const Value& b) {
            if (a.type != b.type) return false;
            if (a.type == Value::Number) return a.number == b.number;
            if (a.type == Value::String) return a.string == b.string;
            if (a.type == Value::Boolean) return a.boolean == b.boolean;
            if (a.type == Value::Null || a.type == Value::Undefined) return true;
            if (a.type == Value::ObjectValue) return a.object == b.object;
            return a.function == b.function;
        }
        Expr Binary(Expr a, const wstring& op, Expr b) {
            Value x = a.value, y = b.value;
            if (op == L"+") { if (x.type == Value::String || y.type == Value::String) return { Value::Str(ToString(x) + ToString(y)),{} }; return { Value::Num(ToNumber(x) + ToNumber(y)),{} }; }
            if (op == L"-") return { Value::Num(ToNumber(x) - ToNumber(y)),{} };
            if (op == L"*") return { Value::Num(ToNumber(x) * ToNumber(y)),{} };
            if (op == L"/") return { Value::Num(ToNumber(x) / ToNumber(y)),{} };
            if (op == L"%") return { Value::Num(std::fmod(ToNumber(x),ToNumber(y))),{} };
            if (op == L"==" || op == L"===") return { Value::Bool(op == L"==" ? (StrictEqual(x,y) || ToString(x) == ToString(y) || ToNumber(x) == ToNumber(y)) : StrictEqual(x,y)),{} };
            if (op == L"!=" || op == L"!==") return { Value::Bool(op == L"!=" ? !(StrictEqual(x,y) || ToString(x) == ToString(y) || ToNumber(x) == ToNumber(y)) : !StrictEqual(x,y)),{} };
            if (op == L"<") return { Value::Bool(x.type == Value::String && y.type == Value::String ? x.string < y.string : ToNumber(x) < ToNumber(y)),{} };
            if (op == L">") return { Value::Bool(x.type == Value::String && y.type == Value::String ? x.string > y.string : ToNumber(x) > ToNumber(y)),{} };
            if (op == L"<=") return { Value::Bool(ToNumber(x) <= ToNumber(y)),{} };
            if (op == L">=") return { Value::Bool(ToNumber(x) >= ToNumber(y)),{} };
            if (op == L"&&") return { Truthy(x) ? y : x,{} };
            if (op == L"||") return { Truthy(x) ? x : y,{} };
            if (op == L"|") return { Value::Num((int32_t)ToNumber(x) | (int32_t)ToNumber(y)),{} };
            if (op == L"&") return { Value::Num((int32_t)ToNumber(x) & (int32_t)ToNumber(y)),{} };
            if (op == L"^") return { Value::Num((int32_t)ToNumber(x) ^ (int32_t)ToNumber(y)),{} };
            if (op == L"<<") return { Value::Num((int32_t)ToNumber(x) << ((int32_t)ToNumber(y) & 31)),{} };
            if (op == L">>") return { Value::Num((int32_t)ToNumber(x) >> ((int32_t)ToNumber(y) & 31)),{} };
            if (op == L">>>") return { Value::Num((uint32_t)(int32_t)ToNumber(x) >> ((int32_t)ToNumber(y) & 31)),{} };
            if (op == L"in") { if (y.type == Value::ObjectValue && y.object) { if (y.object->has && y.object->has(*this, ToString(x)))return { Value::Bool(true),{} }; if (y.object->properties.count(ToString(x)))return { Value::Bool(true),{} }; if (y.object->array) { wstring k = ToString(x); return { Value::Bool(k == L"length" || (k.size() && iswdigit(k[0]) && (size_t)wcstol(k.c_str(),nullptr,10) < y.object->elements.size())),{} }; } }return { Value::Bool(false),{} }; }
            if (op == L"instanceof") {
                if (y.type != Value::FunctionValue || !y.function) return { Value::Bool(false),{} };
                auto pi = y.function->properties.find(L"prototype");
                if (pi == y.function->properties.end() || pi->second.type != Value::ObjectValue || !pi->second.object) return { Value::Bool(false),{} };
                shared_ptr<Object> proto;
                if (x.type == Value::ObjectValue && x.object) proto = x.object->prototype;
                else if (x.type == Value::FunctionValue && x.function) { auto fp = x.function->properties.find(L"__prototype"); if (fp != x.function->properties.end() && fp->second.type == Value::ObjectValue) proto = fp->second.object; }
                for (; proto; proto = proto->prototype) if (proto == pi->second.object) return { Value::Bool(true),{} };
                return { Value::Bool(false),{} };
            }
            return { Value::Undef(),{} };
        }
        void Assign(const Reference& r, const Value& v) {
            if (!r.valid || suppress) return;
            if (r.environment) r.environment->Set(r.key, v); else if (r.valid) SetProperty(r.baseValue, r.key, v);
        }
        wstring EvaluateTemplate(const wstring& raw) {
            wstring out;
            for (size_t i = 0; i < raw.size();) {
                if (raw[i] == L'\\' && i + 1 < raw.size()) { wchar_t e=raw[i+1]; if(e==L'n')out+=L'\n'; else if(e==L'r')out+=L'\r'; else if(e==L't')out+=L'\t'; else out+=e; i+=2; continue; }
                if (raw[i] == L'$' && i + 1 < raw.size() && raw[i+1] == L'{') {
                    size_t b=i+2, j=b; int depth=1; wchar_t quote=0; bool esc=false;
                    for (; j<raw.size() && depth>0; ++j) { wchar_t c=raw[j]; if(esc){esc=false;continue;} if(c==L'\\'){esc=true;continue;} if(quote){if(c==quote)quote=0;continue;} if(c==L'\''||c==L'"'||c==L'`'){quote=c;continue;} if(c==L'{')++depth; else if(c==L'}')--depth; }
                    if(depth==0){ wstring expr=raw.substr(b,j-b-1); Value v=Evaluate(expr); out+=ToString(v); i=j; continue; }
                }
                out+=raw[i++];
            }
            return out;
        }
        Expr Primary() {
            if (Match(L"async")) { Expr e=Primary(); if(e.value.type==Value::FunctionValue&&e.value.function)e.value.function->isAsync=true; return e; }
            // Arrow functions are common in DOM event listeners (for example,
            // button.addEventListener("click", () => { ... })). Support both
            // parenthesized and single-parameter arrows, with block or expression bodies.
            auto makeArrow = [&](const vector<wstring>& params) -> Expr {
                Expect(L"=>");
                wstring body;
                if (Is(L"{")) {
                    body = CaptureBlock();
                } else {
                    size_t first = pos;
                    int paren = 0, bracket = 0, brace = 0;
                    while (Peek().kind != 0) {
                        const wstring& op = Peek().text;
                        if (paren == 0 && bracket == 0 && brace == 0 &&
                            (op == L"," || op == L";" || op == L")" || op == L"]" || op == L"}")) break;
                        if (op == L"(") ++paren; else if (op == L")") --paren;
                        else if (op == L"[") ++bracket; else if (op == L"]") --bracket;
                        else if (op == L"{") ++brace; else if (op == L"}") --brace;
                        ++pos;
                    }
                    if (pos == first) body = L"return undefined;";
                    else body = L"return " + source.substr(tokens[first].begin, tokens[pos - 1].end - tokens[first].begin) + L";";
                }
                auto f = make_shared<Function>();
                f->parameters = params; f->body = body; f->closure = environment;
                return { Value::Fn(f),{} };
            };
            if (Is(L"(")) {
                size_t close = pos + 1; int depth = 1;
                while (close < tokens.size() && depth > 0) {
                    if (tokens[close].text == L"(") ++depth;
                    else if (tokens[close].text == L")") --depth;
                    if (depth > 0) ++close;
                }
                if (depth == 0 && close + 1 < tokens.size() && tokens[close + 1].text == L"=>") {
                    vector<wstring> params, defaults;
                    size_t savedPos = pos;
                    pos = pos + 1;
                    ParseParameters(params, defaults);
                    if (pos != close) throw runtime_error("Unsupported arrow parameter list");
                    pos = close + 1;
                    Expr arrow = makeArrow(params);
                    if (arrow.value.function) arrow.value.function->defaultExpressions = defaults;
                    return arrow;
                }
                Match(L"("); Expr e = Expression(); Expect(L")"); return e;
            }
            if (Peek().kind == 1 && Peek(1).text == L"=>") {
                wstring param = Take().text;
                return makeArrow({ param });
            }
            if (Match(L"[")) {
                auto o = make_shared<Object>(); o->array = true;
                while (!Is(L"]") && Peek().kind != 0) {
                    if (Match(L"...")) { Value spread = Assignment().value; if (spread.type == Value::ObjectValue && spread.object) { if (spread.object->array) o->elements.insert(o->elements.end(), spread.object->elements.begin(), spread.object->elements.end()); else { auto it = spread.object->properties.find(L"length"); size_t n = it == spread.object->properties.end() ? 0 : (size_t)max(0.0, ToNumber(it->second)); for (size_t i=0;i<n && i<10000;++i) { auto v=spread.object->properties.find(to_wstring(i)); o->elements.push_back(v==spread.object->properties.end()?Value::Undef():v->second); } } } }
                    else o->elements.push_back(Assignment().value);
                    if (!Match(L","))break;
                }
                Expect(L"]"); return { Value::Obj(o),{} };
            }
            if (Match(L"{")) {
                auto o = make_shared<Object>();
                while (!Is(L"}") && Peek().kind != 0) {
                    if (Match(L"...")) { Value spread = Assignment().value; if (spread.type == Value::ObjectValue && spread.object) { for (const auto& kv : spread.object->properties) o->properties[kv.first] = kv.second; if (spread.object->array) for (size_t i=0;i<spread.object->elements.size();++i) o->properties[to_wstring(i)] = spread.object->elements[i]; } }
                    else { Token k = Take(); wstring name = k.text; if (k.kind == 3 || k.kind == 2) name = k.text; if (Match(L":")) o->properties[name] = Assignment().value; else if (k.kind == 1) o->properties[name] = environment->Get(name); else throw runtime_error("Expected object property colon"); }
                    if (!Match(L","))break;
                }
                Expect(L"}"); return { Value::Obj(o),{} };
            }
            if (Is(L"async") && (Peek(1).text == L"function" || Peek(1).text == L"(" || (Peek(1).kind == 1 && Peek(2).text == L"=>"))) Take();
            if (Is(L"function")) {
                Take(); if (Peek().kind == 1 && Peek(1).text == L"(") Take(); Expect(L"("); vector<wstring> ps, defaults;
                ParseParameters(ps, defaults); Expect(L")"); wstring body = CaptureBlock();
                auto f = make_shared<Function>(); f->parameters = ps; f->defaultExpressions = defaults; f->body = body; f->closure = environment; f->properties[L"prototype"] = Value::Obj(make_shared<Object>()); return { Value::Fn(f),{} };
            }
            Token t = Take();
            if (t.kind == 2) {
                try {
                    if (t.text.size() > 2 && t.text[0] == L'0' && (t.text[1] == L'x' || t.text[1] == L'X')) return { Value::Num((double)std::stoull(t.text.substr(2), nullptr, 16)),{} };
                    if (t.text.size() > 2 && t.text[0] == L'0' && (t.text[1] == L'b' || t.text[1] == L'B')) return { Value::Num((double)std::stoull(t.text.substr(2), nullptr, 2)),{} };
                    if (t.text.size() > 2 && t.text[0] == L'0' && (t.text[1] == L'o' || t.text[1] == L'O')) return { Value::Num((double)std::stoull(t.text.substr(2), nullptr, 8)),{} };
                    return { Value::Num(std::stod(t.text)),{} };
                } catch (...) { return { Value::Num(0),{} }; }
            }
            if (t.kind == 3)return { Value::Str(t.text),{} };
            if (t.kind == 6)return { Value::Str(EvaluateTemplate(t.text)),{} };
            if (t.kind == 5) { auto re = make_shared<Object>(); re->properties[L"__minihtml_regex"] = Value::Str(t.text); re->properties[L"__minihtml_flags"] = Value::Str(t.regexFlags); return { Value::Obj(re),{} }; }
            if (t.kind == 1) {
                if (t.text == L"true")return { Value::Bool(true),{} };
                if (t.text == L"false")return { Value::Bool(false),{} };
                if (t.text == L"null")return { Value::NullValue(),{} };
                if (t.text == L"undefined")return { Value::Undef(),{} };
                if (t.text == L"this")return { currentThis,{} };
                Reference r; r.environment = environment; r.key = t.text; r.valid = true; return { environment->Get(t.text),r };
            }
            return { Value::Undef(),{} };
        }
        Expr Postfix() {
            Expr e = Primary();
            for (;;) {
                if (Match(L".") || Match(L"?.")) { wstring key = Take().text; Value base = e.value; Reference r; r.object = base.object; r.baseValue = base; r.key = key; r.valid = base.type == Value::ObjectValue || base.type == Value::FunctionValue || base.type == Value::String || base.type == Value::Number; e = { GetProperty(base,key),r }; }
                else if (Match(L"[")) { Value base = e.value; wstring key = ToString(Assignment().value); Expect(L"]"); Reference r; r.object = base.object; r.baseValue = base; r.key = key; r.valid = base.type == Value::ObjectValue || base.type == Value::FunctionValue || base.type == Value::String || base.type == Value::Number; e = { GetProperty(base,key),r }; }
                else if (Match(L"(")) { vector<Value>a; while (!Is(L")") && Peek().kind != 0) { if (Match(L"...")) { Value spread = Assignment().value; if (spread.type == Value::ObjectValue && spread.object && spread.object->array) a.insert(a.end(), spread.object->elements.begin(), spread.object->elements.end()); } else a.push_back(Assignment().value); if (!Match(L","))break; }Expect(L")"); Value thisV = e.ref.valid ? e.ref.baseValue : Value::Undef(); e = { Call(e.value,a,thisV),{} }; }
                else if (Is(L"++") || Is(L"--")) { wstring op = Take().text; Value old = e.value; Assign(e.ref, Value::Num(ToNumber(old) + (op == L"++" ? 1 : -1))); e = { old,{} }; }
                else break;
            }
            return e;
        }
        Expr Unary() {
            if (Match(L"await")) {
                Expr x = Unary();
                if (IsPromiseValue(x.value)) {
                    auto si = x.value.object->properties.find(L"__promise_state");
                    auto vi = x.value.object->properties.find(L"__promise_value");
                    if (si != x.value.object->properties.end()) {
                        wstring state = ToString(si->second);
                        if (state == L"rejected") {
                            Value reason = vi == x.value.object->properties.end() ? Value::Undef() : vi->second;
                            throw JSException(reason, WToUtf8(ToString(reason)));
                        }
                        if (state == L"fulfilled") return { vi == x.value.object->properties.end() ? Value::Undef() : vi->second, {} };
                    }
                    // Pending Promise: suspend only when executing inside an async function.
                    AsyncTask* task = currentAsyncTask;
                    if (task && !task->completed) {
                        task->awaitReady = false;
                        task->awaitValue = Value::Undef();
                        task->awaitRejected = false;
                        std::weak_ptr<AsyncTask> weakTask;
                        for (const auto& active : activeAsyncTasks) if (active.get() == task) { weakTask = active; break; }
                        Value fulfilled = MakeNative([weakTask](Runtime& runtime, const vector<Value>& values, const Value&) {
                            if (auto pending = weakTask.lock()) {
                                if (!pending->awaitReady && !pending->completed) {
                                    pending->awaitValue = values.empty() ? Value::Undef() : values[0];
                                    pending->awaitRejected = false;
                                    pending->awaitReady = true;
                                    auto held = weakTask;
                                    QueueMicrotask([held](Runtime&) { if (auto ready = held.lock()) ResumeAsyncTask(ready); });
                                }
                            }
                            return Value::Undef();
                        });
                        Value rejected = MakeNative([weakTask](Runtime& runtime, const vector<Value>& values, const Value&) {
                            if (auto pending = weakTask.lock()) {
                                if (!pending->awaitReady && !pending->completed) {
                                    pending->awaitValue = values.empty() ? Value::Undef() : values[0];
                                    pending->awaitRejected = true;
                                    pending->awaitReady = true;
                                    auto held = weakTask;
                                    QueueMicrotask([held](Runtime&) { if (auto ready = held.lock()) ResumeAsyncTask(ready); });
                                }
                            }
                            return Value::Undef();
                        });
                        Value then = x.value.object->properties[L"then"];
                        Invoke(then, { fulfilled, rejected }, x.value);
                        YieldAsyncTask();
                        Value resumed = task->awaitValue;
                        bool wasRejected = task->awaitRejected;
                        task->awaitReady = false;
                        if (wasRejected) throw JSException(resumed, WToUtf8(ToString(resumed)));
                        return { resumed, {} };
                    }
                }
                return x;
            }
            if (Match(L"new")) {
                Expr ctor = Primary();
                while (Match(L".")) { wstring key = Take().text; Value base = ctor.value; Reference ref; ref.object = base.object; ref.key = key; ref.valid = base.type == Value::ObjectValue; ctor = { GetProperty(base,key),ref }; }
                vector<Value> args; if (Match(L"(")) { while (!Is(L")") && Peek().kind != 0) { args.push_back(Assignment().value); if (!Match(L","))break; }Expect(L")"); }
                auto instance = make_shared<Object>();
                if (ctor.value.type == Value::FunctionValue && ctor.value.function) { auto pi = ctor.value.function->properties.find(L"prototype"); if (pi != ctor.value.function->properties.end() && pi->second.type == Value::ObjectValue)instance->prototype = pi->second.object; }
                Value self = Value::Obj(instance); Value returned = Call(ctor.value, args, self); if (returned.type == Value::ObjectValue || returned.type == Value::FunctionValue)return { returned,{} }; return { self,{} };
            }
            if (Match(L"!")) { Expr x = Unary(); return { Value::Bool(!Truthy(x.value)),{} }; }
            if (Match(L"-")) { Expr x = Unary(); return { Value::Num(-ToNumber(x.value)),{} }; }
            if (Match(L"+")) { Expr x = Unary(); return { Value::Num(ToNumber(x.value)),{} }; }
            if (Match(L"typeof")) { Expr x = Unary(); wstring t = x.value.type == Value::Undefined ? L"undefined" : x.value.type == Value::Boolean ? L"boolean" : x.value.type == Value::Number ? L"number" : x.value.type == Value::String ? L"string" : x.value.type == Value::FunctionValue ? L"function" : L"object"; return { Value::Str(t),{} }; }
            if (Match(L"void")) { Unary(); return { Value::Undef(),{} }; }
            return Postfix();
        }
        Expr Multiplicative() { Expr x = Unary(); while (Is(L"*") || Is(L"/") || Is(L"%")) { wstring op = Take().text; x = Binary(x, op, Unary()); }return x; }
        Expr Additive() { Expr x = Multiplicative(); while (Is(L"+") || Is(L"-")) { wstring op = Take().text; x = Binary(x, op, Multiplicative()); }return x; }
        Expr Shift() { Expr x = Additive(); while (Is(L"<<") || Is(L">>") || Is(L">>>")) { wstring op = Take().text; x = Binary(x, op, Additive()); }return x; }
        Expr Relational() { Expr x = Shift(); while (Is(L"<") || Is(L">") || Is(L"<=") || Is(L">=") || Is(L"instanceof")) { wstring op = Take().text; x = Binary(x, op, Shift()); }return x; }
        Expr Equality() { Expr x = Relational(); while (Is(L"==") || Is(L"!=") || Is(L"===") || Is(L"!==") || Is(L"in")) { wstring op = Take().text; x = Binary(x, op, Relational()); }return x; }
        Expr BitAnd() { Expr x = Equality(); while (Match(L"&"))x = Binary(x, L"&", Equality()); return x; }
        Expr BitXor() { Expr x = BitAnd(); while (Match(L"^"))x = Binary(x, L"^", BitAnd()); return x; }
        Expr BitOr() { Expr x = BitXor(); while (Match(L"|"))x = Binary(x, L"|", BitXor()); return x; }
        Expr LogicalAnd() { Expr x = BitOr(); while (Match(L"&&")) { bool old = suppress; if (!Truthy(x.value))suppress = true; Expr y = BitOr(); suppress = old; x = Binary(x, L"&&", y); }return x; }
        Expr LogicalOr() { Expr x = LogicalAnd(); while (Match(L"||")) { bool old = suppress; if (Truthy(x.value))suppress = true; Expr y = LogicalAnd(); suppress = old; x = Binary(x, L"||", y); }return x; }
        Expr Conditional() { Expr x = LogicalOr(); if (Match(L"??")) { Expr y = Conditional(); return { (x.value.type == Value::Null || x.value.type == Value::Undefined) ? y.value : x.value,{} }; } if (Match(L"?")) { bool old = suppress; if (!Truthy(x.value))suppress = true; Expr a = Assignment(); suppress = old; Expect(L":"); if (Truthy(x.value))suppress = true; Expr b = Assignment(); suppress = old; return { Truthy(x.value) ? a.value : b.value,{} }; }return x; }
        Expr Assignment() { Expr x = Conditional(); if (Is(L"=") || Is(L"+=") || Is(L"-=") || Is(L"*=") || Is(L"/=") || Is(L"%=") || Is(L"|=") || Is(L"&=") || Is(L"^=") || Is(L"<<=") || Is(L">>=") || Is(L">>>=")) { wstring op = Take().text; Expr y = Assignment(); Value v = y.value; if (op != L"=") { wstring b = op.substr(0, op.size() - 1); v = Binary(x, b, y).value; }Assign(x.ref, v); return { v,{} }; }return x; }
        Expr Expression() { Expr x = Assignment(); while (Match(L","))x = Assignment(); return x; }

        void Statement() {
            if (Match(L";")) return;
            if (Match(L"{")) { while (!Is(L"}") && Peek().kind != 0) Statement(); Expect(L"}"); return; }
            if (Match(L"var") || Match(L"let") || Match(L"const")) {
                do { Token n = Take(); if (n.kind != 1)break; Value v = Match(L"=") ? Assignment().value : Value::Undef(); if (!suppress)environment->Define(n.text, v); } while (Match(L",")); OptionalSemi(); return;
            }
            bool asyncDeclaration = false;
            if (Is(L"async") && Peek(1).text == L"function") { Take(); asyncDeclaration = true; }
            if (Match(L"function")) {
                Token name = Take(); Expect(L"("); vector<wstring> ps, defaults; ParseParameters(ps, defaults); Expect(L")"); wstring body = CaptureBlock(); auto f = make_shared<Function>(); f->parameters = ps; f->defaultExpressions = defaults; f->body = body; f->isAsync = asyncDeclaration; f->closure = environment; if (!suppress)environment->Define(name.text, Value::Fn(f)); return;
            }
            if (Match(L"return")) { Value v = Is(L";") || Is(L"}") ? Value::Undef() : Expression().value; OptionalSemi(); if (!suppress)throw ReturnSignal{ v }; return; }
            if (Match(L"throw")) { Value v=Expression().value; OptionalSemi(); if(!suppress) throw JSException(v, WToUtf8(ToString(v))); return; }
            if (Match(L"if")) { wstring cond = CaptureParen(); wstring yes = CaptureStatement(); wstring no; if (Match(L"else"))no = CaptureStatement(); bool b = Truthy(Evaluate(cond)); if (!suppress)Execute(b ? yes : no); return; }
            if (Match(L"while")) { wstring cond = CaptureParen(); wstring body = CaptureStatement(); int count = 0; while (Truthy(Evaluate(cond)) && count++ < 10000 && !suppress) { breakRequested = false; continueRequested = false; Execute(body); if (breakRequested) { breakRequested = false; break; } }return; }
            if (Match(L"for")) {
                Expect(L"(");
                if (Match(L"const") || Match(L"let") || Match(L"var")) {
                    vector<wstring> bindings; bool destructured = false;
                    if (Match(L"[")) { destructured=true; while(!Is(L"]") && Peek().kind!=0) { Token nm=Take(); if(nm.kind==1) bindings.push_back(nm.text); if(!Match(L","))break; } Expect(L"]"); }
                    else { Token nm=Take(); if(nm.kind==1)bindings.push_back(nm.text); }
                    if (Match(L"in")) {
                        size_t first = pos; int dep = 0;
                        while (Peek().kind != 0 && !(Is(L")") && dep == 0)) {
                            if (Is(L"(") || Is(L"[") || Is(L"{")) ++dep;
                            else if (Is(L")") || Is(L"]") || Is(L"}")) --dep;
                            ++pos;
                        }
                        size_t last = pos; Expect(L")");
                        wstring expr = first < last ? source.substr(tokens[first].begin, tokens[last - 1].end - tokens[first].begin) : L"";
                        wstring body = CaptureStatement();
                        Value iterable = Evaluate(expr); vector<wstring> keys;
                        if (iterable.type == Value::ObjectValue && iterable.object) {
                            if (iterable.object->array) { for (size_t i = 0; i < iterable.object->elements.size(); ++i) keys.push_back(std::to_wstring(i)); }
                            else for (const auto& kv : iterable.object->properties) if (kv.first.rfind(L"__", 0) != 0) keys.push_back(kv.first);
                        } else if (iterable.type == Value::String) { for (size_t i = 0; i < iterable.string.size(); ++i) keys.push_back(std::to_wstring(i)); }
                        for (const auto& key : keys) {
                            if (!bindings.empty()) environment->Set(bindings[0], Value::Str(key));
                            Execute(body);
                            if (breakRequested) { breakRequested = false; break; }
                        }
                        return;
                    }
                    if (Match(L"of")) {
                        size_t first=pos; int dep=0; while(Peek().kind!=0 && !(Is(L")") && dep==0)){ if(Is(L"(")||Is(L"[")||Is(L"{"))++dep; else if(Is(L")")||Is(L"]")||Is(L"}"))--dep; ++pos; }
                        size_t last=pos; Expect(L")"); wstring expr=first<last?source.substr(tokens[first].begin,tokens[last-1].end-tokens[first].begin):L"[]"; wstring body=CaptureStatement(); Value iterable=Evaluate(expr); vector<Value> values;
                        if(iterable.type==Value::ObjectValue && iterable.object){ auto mapIt=iterable.object->properties.find(L"__map_entries"); if(mapIt!=iterable.object->properties.end() && mapIt->second.type==Value::ObjectValue && mapIt->second.object) values=mapIt->second.object->elements; else if(iterable.object->array)values=iterable.object->elements; else { for(const auto& kv:iterable.object->properties){auto pair=make_shared<Object>();pair->array=true;pair->elements={Value::Str(kv.first),kv.second};values.push_back(Value::Obj(pair));} } }
                        else if(iterable.type==Value::String){for(wchar_t ch:iterable.string)values.push_back(Value::Str(wstring(1,ch)));}
                        for(const Value& item:values){ if(destructured){ auto pair=item.type==Value::ObjectValue?item.object:nullptr; for(size_t bi=0;bi<bindings.size();++bi){Value v=pair&&pair->array&&bi<pair->elements.size()?pair->elements[bi]:Value::Undef();environment->Set(bindings[bi],v);} } else if(!bindings.empty())environment->Set(bindings[0],item); Execute(body); if(breakRequested){breakRequested=false;break;} }
                        return;
                    }
                    // Classic declaration form: for (var/let/const i = init; test; step).
                    // The declaration keyword and binding name were consumed above; parse
                    // the remaining header and execute the initializer in the current scope.
                    size_t b = pos; int depth = 0;
                    while (Peek().kind != 0 && !(Is(L")") && depth == 0)) {
                        if (Is(L"(" ) || Is(L"[") || Is(L"{")) ++depth;
                        else if (Is(L")") || Is(L"]") || Is(L"}")) --depth;
                        ++pos;
                    }
                    size_t e = pos; Expect(L")");
                    wstring rest = (b < e) ? source.substr(tokens[b].begin, tokens[e - 1].end - tokens[b].begin) : L"";
                    wstring header = L"var " + (bindings.empty() ? L"_" : bindings[0]) + rest;
                    size_t s1 = header.find(L';'), s2 = s1 == wstring::npos ? s1 : header.find(L';', s1 + 1);
                    if (s1 == wstring::npos) throw runtime_error("Classic for loop requires two semicolons");
                    wstring init = header.substr(0, s1);
                    wstring cond = s2 == wstring::npos ? L"true" : header.substr(s1 + 1, s2 - s1 - 1);
                    wstring step = s2 == wstring::npos ? L"" : header.substr(s2 + 1);
                    wstring body = CaptureStatement();
                    if (!init.empty()) Execute(init);
                    int count = 0;
                    while ((cond.empty() || Truthy(Evaluate(cond))) && count++ < 100000 && !suppress) {
                        breakRequested = false; continueRequested = false;
                        Execute(body);
                        if (breakRequested) { breakRequested = false; break; }
                        if (!step.empty()) Evaluate(step);
                    }
                    return;
                }
                size_t b = pos; int depth = 0; while (Peek().kind != 0 && !(Is(L")") && depth == 0)) { if (Is(L"("))++depth; else if (Is(L")"))--depth; ++pos; } size_t e = pos; Expect(L")");
                wstring header = (b < e) ? source.substr(tokens[b].begin, tokens[e - 1].end - tokens[b].begin) : L""; size_t s1 = header.find(L';'), s2 = s1 == wstring::npos ? s1 : header.find(L';', s1 + 1); wstring init = s1 == wstring::npos ? header : header.substr(0, s1), cond = s1 == wstring::npos ? L"true" : header.substr(s1 + 1, s2 - s1 - 1), step = s2 == wstring::npos ? L"" : header.substr(s2 + 1); wstring body = CaptureStatement(); if (!init.empty())Execute(init); int count = 0; while ((cond.empty() || Truthy(Evaluate(cond))) && count++ < 100000 && !suppress) { breakRequested = false; continueRequested = false; Execute(body); if (breakRequested) { breakRequested = false; break; }if (!step.empty())Evaluate(step); }return;
            }
            if (Match(L"try")) {
                wstring tryBody = CaptureStatement(), catchBody, finallyBody, catchName;
                if (Match(L"catch")) {
                    if (Match(L"(")) {
                        Token binding = Take();
                        if (binding.kind != 1) throw runtime_error("Expected catch binding");
                        catchName = binding.text;
                        Expect(L")");
                    }
                    catchBody = CaptureStatement();
                }
                if (Match(L"finally")) finallyBody = CaptureStatement();

                lastThrownValue = Value::Undef();
                lastError.clear();
                bool tryOk = Execute(tryBody);
                Value pendingThrown = tryOk ? Value::Undef() : lastThrownValue;
                wstring pendingMessage = tryOk ? L"" : lastError;

                if (!tryOk && !catchBody.empty()) {
                    auto previousEnvironment = environment;
                    environment = make_shared<Environment>(previousEnvironment);
                    if (!catchName.empty()) {
                        Value bindingValue = pendingThrown;
                        if (bindingValue.type == Value::Undefined) {
                            auto errorObject = make_shared<Object>();
                            errorObject->properties[L"name"] = Value::Str(L"Error");
                            errorObject->properties[L"message"] = Value::Str(pendingMessage);
                            bindingValue = Value::Obj(errorObject);
                        }
                        environment->Define(catchName, bindingValue);
                    }

                    bool catchOk = false;
                    try {
                        catchOk = Execute(catchBody);
                    } catch (...) {
                        environment = previousEnvironment;
                        throw;
                    }
                    environment = previousEnvironment;

                    if (!catchOk) {
                        pendingThrown = lastThrownValue;
                        pendingMessage = lastError;
                    } else {
                        // Clear the handled exception so it cannot poison later statements.
                        pendingThrown = Value::Undef();
                        pendingMessage.clear();
                        lastThrownValue = Value::Undef();
                        lastError.clear();
                    }
                    tryOk = catchOk;
                }

                // finally always runs. Its own exception takes precedence over the prior result.
                if (!finallyBody.empty()) {
                    bool finallyOk = Execute(finallyBody);
                    if (!finallyOk) {
                        Value finallyThrown = lastThrownValue;
                        wstring finallyMessage = lastError;
                        throw JSException(finallyThrown, WToUtf8(finallyMessage));
                    }
                }

                if (!tryOk) {
                    lastThrownValue = pendingThrown;
                    lastError = pendingMessage;
                    throw JSException(pendingThrown, WToUtf8(pendingMessage));
                }
                lastThrownValue = Value::Undef();
                lastError.clear();
                return;
            }
            if (Match(L"throw")) { Value v = Expression().value; OptionalSemi(); throw JSException(v, WToUtf8(ToString(v))); }
            if (Match(L"break")) { OptionalSemi(); if (!suppress)breakRequested = true; return; }
            if (Match(L"continue")) { OptionalSemi(); if (!suppress)continueRequested = true; return; }
            Expression(); OptionalSemi();
        }

    public:
        Runtime() : environment(make_shared<Environment>()), globalEnvironment(environment) {
            // Diagnostic, read-only-by-convention identifiers used to verify that
            // dedicated Worker runtimes actually execute on distinct native threads.
            std::ostringstream nativeThread;
            nativeThread << std::this_thread::get_id();
            Define(L"__nativeThreadId", Value::Str(Utf8ToW(nativeThread.str())));
            Define(L"isWorkerThread", Value::Bool(false));
            InstallBuiltins();
        }
        explicit Runtime(const shared_ptr<Environment>& env) : environment(env), globalEnvironment(env) { InstallBuiltins(); }
        shared_ptr<Environment> GlobalEnvironment() const { return globalEnvironment; }
        void Define(const wstring& name, const Value& value) { globalEnvironment->Define(name, value); }
        Value GetGlobal(const wstring& name) const { return globalEnvironment->Get(name); }
        void SetGlobal(const wstring& name, const Value& value) { globalEnvironment->Set(name, value); }
        Value Evaluate(const wstring& code) { auto oldEnv = environment; auto oldSrc = source; auto oldTokens = tokens; size_t oldPos = pos; ++executionDepth; Lex(code); Value out = Value::Undef(); try { out = Expression().value; } catch (...) { environment = oldEnv; source = oldSrc; tokens = std::move(oldTokens); pos = oldPos; --executionDepth; if(executionDepth==0)DrainMicrotasks(); throw; }environment = oldEnv; source = oldSrc; tokens = std::move(oldTokens); pos = oldPos; --executionDepth; if(executionDepth==0)DrainMicrotasks(); return out; }
        bool Execute(const wstring& code) {
            auto oldEnv = environment; auto oldSrc = source; auto oldTokens = tokens; size_t oldPos = pos; ++executionDepth; Lex(code);
            try { while (Peek().kind != 0) Statement(); }
            catch (const ReturnSignal&) { }
            catch (const JSException& e) {
                lastThrownValue=e.thrown;
                lastError.assign(e.what(), e.what() + strlen(e.what()));
#if !defined(_WIN32)
                std::fprintf(stderr, "[minihtml JS exception] %s\n", e.what());
#endif
                environment=oldEnv; source=oldSrc; tokens=std::move(oldTokens); pos=oldPos; --executionDepth; if(executionDepth==0)DrainMicrotasks(); return false;
            }
            catch (const std::exception& e) {
                lastThrownValue=Value::Undef();
                lastError.assign(e.what(), e.what() + strlen(e.what()));
#if !defined(_WIN32)
                std::fprintf(stderr, "[minihtml JS exception] %s\n", e.what());
                std::string preview = WToUtf8(code.substr(0, std::min((size_t)240, code.size())));
                std::fprintf(stderr, "[minihtml JS source] %.240s\n", preview.c_str());
#endif
                environment = oldEnv; source = oldSrc; tokens = std::move(oldTokens); pos = oldPos; --executionDepth; if(executionDepth==0)DrainMicrotasks(); return false;
            }
            environment = oldEnv; source = oldSrc; tokens = std::move(oldTokens); pos = oldPos; --executionDepth; if(executionDepth==0)DrainMicrotasks(); return true;
        }
        Value Invoke(const Value& fn, const vector<Value>& args, const Value& self = Value::Undef()) { Value out=Call(fn,args,self); if(executionDepth==0)DrainMicrotasks(); return out; }
        Value MakeFunction(const vector<wstring>& params, const wstring& body) { auto f = make_shared<Function>(); f->parameters = params; f->body = body; f->closure = environment; f->properties[L"prototype"] = Value::Obj(make_shared<Object>()); return Value::Fn(f); }
        Value MakeObject() { return Value::Obj(make_shared<Object>()); }
        Value MakeNative(std::function<Value(Runtime&, const vector<Value>&, const Value&)> fn) { auto f = make_shared<Function>(); f->native = std::move(fn); f->properties[L"prototype"] = Value::Obj(make_shared<Object>()); return Value::Fn(f); }
        wstring LastError() const { return lastError; }
        void SetSuppress(bool x) { suppress = x; }
        void PumpTasks() { if (executionDepth == 0) DrainMicrotasks(); }

    private:
        static bool IsPromiseValue(const Value& v) {
            return v.type == Value::ObjectValue && v.object && v.object->properties.count(L"__promise_state") != 0;
        }
        static void QueueMicrotask(std::function<void(Runtime&)> task) {
            microtaskQueue.push_back(std::move(task));
        }
        void DrainWorkerEvents() {
            std::deque<WorkerEvent> ready;
            {
                std::lock_guard<std::mutex> lock(workerEventMutex);
                ready.swap(workerEvents);
            }
            for (auto& message : ready) {
                auto owner = message.owner.lock();
                if (!owner) continue;
                Value eventValue = Value::Obj(make_shared<Object>());
                eventValue.object->properties[L"type"] = Value::Str(message.failed ? L"error" : L"message");
                eventValue.object->properties[L"target"] = Value::Obj(owner);
                eventValue.object->properties[L"currentTarget"] = Value::Obj(owner);
                if (message.failed) {
                    eventValue.object->properties[L"message"] = Value::Str(Utf8ToW(message.error));
                    auto errorHandler = owner->properties.find(L"onerror");
                    if (errorHandler != owner->properties.end() && errorHandler->second.type == Value::FunctionValue)
                        Invoke(errorHandler->second, { eventValue }, Value::Obj(owner));
                } else {
                    Value json = GetGlobal(L"JSON");
                    Value data = Value::Str(Utf8ToW(message.json));
                    if (json.type == Value::ObjectValue && json.object) {
                        auto parser = json.object->properties.find(L"parse");
                        if (parser != json.object->properties.end()) {
                            try { data = Invoke(parser->second, { data }); }
                            catch (...) { data = Value::Str(Utf8ToW(message.json)); }
                        }
                    }
                    eventValue.object->properties[L"data"] = data;
                    auto handler = owner->properties.find(L"onmessage");
                    if (handler != owner->properties.end() && handler->second.type == Value::FunctionValue)
                        Invoke(handler->second, { eventValue }, Value::Obj(owner));
                    auto listeners = owner->properties.find(L"__message_listeners");
                    if (listeners != owner->properties.end() && listeners->second.object && listeners->second.object->array) {
                        auto snapshot = listeners->second.object->elements;
                        for (const auto& callback : snapshot) if (callback.type == Value::FunctionValue)
                            Invoke(callback, { eventValue }, Value::Obj(owner));
                    }
                }
            }
            std::deque<std::function<void(Runtime&)>> tasks;
            {
                std::lock_guard<std::mutex> lock(externalTaskMutex);
                tasks.swap(externalTasks);
            }
            for (auto& task : tasks) {
                try { if (task) task(*this); }
                catch (const std::exception& error) {
                    lastError.assign(error.what(), error.what() + strlen(error.what()));
#if !defined(_WIN32)
                    std::fprintf(stderr, "[minihtml external task exception] %s\n", error.what());
#endif
                }
            }
        }
        void DrainMicrotasks() {
            if (drainingMicrotasks) return;
            drainingMicrotasks = true;
            DrainWorkerEvents();
            size_t count = 0;
            while (!microtaskQueue.empty() && count++ < 10000) {
                auto task = std::move(microtaskQueue.front());
                microtaskQueue.erase(microtaskQueue.begin());
                try { task(*this); }
                catch (const std::exception& e) {
                    lastError.assign(e.what(), e.what() + strlen(e.what()));
#if !defined(_WIN32)
                    std::fprintf(stderr, "[minihtml microtask exception] %s\\n", e.what());
#endif
                }
            }
            if (!microtaskQueue.empty()) microtaskQueue.clear();
            drainingMicrotasks = false;
        }
        static void RunPromiseReaction(Runtime& run, const Value& reaction, bool rejected, const Value& value) {
            if (!reaction.object) return;
            auto nextIt = reaction.object->properties.find(L"next");
            if (nextIt == reaction.object->properties.end() || !IsPromiseValue(nextIt->second)) return;
            Value next = nextIt->second;
            auto handlerIt = reaction.object->properties.find(rejected ? L"onRejected" : L"onFulfilled");
            if (handlerIt == reaction.object->properties.end() || handlerIt->second.type != Value::FunctionValue) {
                SettlePromise(run, next, rejected, value);
                return;
            }
            try {
                Value result = run.Invoke(handlerIt->second, { value });
                SettlePromise(run, next, false, result);
            } catch (const JSException& e) {
                SettlePromise(run, next, true, e.thrown);
            } catch (const std::exception& e) {
                SettlePromise(run, next, true, Value::Str(wstring(e.what(), e.what() + strlen(e.what()))));
            } catch (...) {
                SettlePromise(run, next, true, Value::Str(L"Unknown exception in Promise reaction"));
            }
        }
        static Value MakePromise(Runtime& rt, const wstring& state = L"pending", const Value& settled = Value::Undef()) {
            auto obj = make_shared<Object>();
            obj->properties[L"__promise_state"] = Value::Str(state);
            obj->properties[L"__promise_value"] = settled;
            auto reactions = make_shared<Object>(); reactions->array = true;
            obj->properties[L"__promise_reactions"] = Value::Obj(reactions);
            auto settle = [obj](Runtime& r, bool rejected, const Value& value) -> Value {
                auto si = obj->properties.find(L"__promise_state");
                if (si == obj->properties.end() || ToString(si->second) != L"pending") return Value::Undef();
                obj->properties[L"__promise_state"] = Value::Str(rejected ? L"rejected" : L"fulfilled");
                obj->properties[L"__promise_value"] = value;
                auto ri = obj->properties.find(L"__promise_reactions");
                if (ri != obj->properties.end() && ri->second.object) {
                    for (const Value& reaction : ri->second.object->elements) {
                        QueueMicrotask([reaction, rejected, value](Runtime& run) {
                            Runtime::RunPromiseReaction(run, reaction, rejected, value);
                        });
                    }
                }
                return Value::Undef();
            };
            obj->properties[L"__promise_settle"] = rt.MakeNative([settle](Runtime& r, const vector<Value>& a, const Value&) { bool rejected=!a.empty()&&Truthy(a[0]); return settle(r,rejected,a.size()>1?a[1]:Value::Undef()); });
            obj->properties[L"then"] = rt.MakeNative([obj](Runtime& r, const vector<Value>& a, const Value&) {
                Value next = MakePromise(r);
                auto reaction=make_shared<Object>();
                reaction->properties[L"onFulfilled"] = a.size()>0?a[0]:Value::Undef();
                reaction->properties[L"onRejected"] = a.size()>1?a[1]:Value::Undef();
                reaction->properties[L"next"] = next;
                auto ri=obj->properties.find(L"__promise_reactions");
                if(ri!=obj->properties.end()&&ri->second.object)ri->second.object->elements.push_back(Value::Obj(reaction));
                auto si=obj->properties.find(L"__promise_state");
                if(si!=obj->properties.end()&&ToString(si->second)!=L"pending") {
                    bool rejected=ToString(si->second)==L"rejected";
                    Value value=obj->properties[L"__promise_value"];
                    QueueMicrotask([reaction,rejected,value](Runtime& run){
                        Runtime::RunPromiseReaction(run, Value::Obj(reaction), rejected, value);
                    });
                }
                return next;
            });
            obj->properties[L"catch"] = rt.MakeNative([obj](Runtime& r,const vector<Value>& a,const Value&){Value then=obj->properties[L"then"];return r.Invoke(then,{Value::Undef(),a.empty()?Value::Undef():a[0]},Value::Obj(obj));});
            obj->properties[L"finally"] = rt.MakeNative([obj](Runtime& r,const vector<Value>& a,const Value&){
                Value cb = a.empty() ? Value::Undef() : a[0];
                Value then = obj->properties[L"then"];
                if (cb.type != Value::FunctionValue) {
                    Value pass = r.MakeNative([](Runtime&, const vector<Value>& v, const Value&) { return v.empty() ? Value::Undef() : v[0]; });
                    return r.Invoke(then, {pass, pass}, Value::Obj(obj));
                }
                Value fulfilled = r.MakeNative([cb](Runtime& rr, const vector<Value>& v, const Value&) {
                    Value original = v.empty() ? Value::Undef() : v[0];
                    Value cleanup = rr.Invoke(cb, {});
                    Value wait = MakePromise(rr);
                    SettlePromise(rr, wait, false, cleanup);
                    Value restore = rr.MakeNative([original](Runtime&, const vector<Value>&, const Value&) { return original; });
                    return rr.Invoke(wait.object->properties[L"then"], {restore}, wait);
                });
                Value rejected = r.MakeNative([cb](Runtime& rr, const vector<Value>& v, const Value&) {
                    Value original = v.empty() ? Value::Undef() : v[0];
                    Value cleanup = rr.Invoke(cb, {});
                    Value wait = MakePromise(rr);
                    SettlePromise(rr, wait, false, cleanup);
                    Value restoreRejection = rr.MakeNative([original](Runtime& rrr, const vector<Value>&, const Value&) {
                        Value rejectedPromise = MakePromise(rrr);
                        SettlePromise(rrr, rejectedPromise, true, original);
                        return rejectedPromise;
                    });
                    return rr.Invoke(wait.object->properties[L"then"], {restoreRejection}, wait);
                });
                return r.Invoke(then, {fulfilled, rejected}, Value::Obj(obj));
            });
            return Value::Obj(obj);
        }
        static void SettlePromise(Runtime& rt, const Value& promise, bool rejected, const Value& value) {
            if (!IsPromiseValue(promise)) return;
            auto current = promise.object->properties.find(L"__promise_state");
            if (current == promise.object->properties.end() || ToString(current->second) != L"pending") return;
            if (!rejected && IsPromiseValue(value)) {
                if (value.object == promise.object) { SettlePromise(rt,promise,true,Value::Str(L"TypeError: promise cannot resolve to itself")); return; }
                auto state=value.object->properties.find(L"__promise_state");
                if(state!=value.object->properties.end()&&ToString(state->second)!=L"pending") {
                    SettlePromise(rt,promise,ToString(state->second)==L"rejected",value.object->properties[L"__promise_value"]);
                } else {
                    auto already = make_shared<bool>(false);
                    Value ok=rt.MakeNative([promise,already](Runtime& r,const vector<Value>& a,const Value&){if(!*already){*already=true;SettlePromise(r,promise,false,a.empty()?Value::Undef():a[0]);}return Value::Undef();});
                    Value bad=rt.MakeNative([promise,already](Runtime& r,const vector<Value>& a,const Value&){if(!*already){*already=true;SettlePromise(r,promise,true,a.empty()?Value::Undef():a[0]);}return Value::Undef();});
                    rt.Invoke(value.object->properties[L"then"],{ok,bad},value);
                }
                return;
            }
            if (!rejected && value.type == Value::ObjectValue && value.object) {
                try {
                    auto adopting = promise.object->properties.find(L"__promise_adopting");
                    if (adopting != promise.object->properties.end() && adopting->second.type == Value::ObjectValue && adopting->second.object == value.object) {
                        SettlePromise(rt, promise, true, Value::Str(L"TypeError: thenable resolution cycle"));
                        return;
                    }
                    Value then = rt.GetProperty(value, L"then");
                    if (then.type == Value::FunctionValue) {
                        promise.object->properties[L"__promise_adopting"] = value;
                        auto already = make_shared<bool>(false);
                        Value ok = rt.MakeNative([promise,already](Runtime& r,const vector<Value>& a,const Value&){if(!*already){*already=true;SettlePromise(r,promise,false,a.empty()?Value::Undef():a[0]);}return Value::Undef();});
                        Value bad = rt.MakeNative([promise,already](Runtime& r,const vector<Value>& a,const Value&){if(!*already){*already=true;SettlePromise(r,promise,true,a.empty()?Value::Undef():a[0]);}return Value::Undef();});
                        rt.Invoke(then,{ok,bad},value);
                        return;
                    }
                } catch (const JSException& e) { SettlePromise(rt,promise,true,e.thrown); return; }
                catch (const std::exception& e) { SettlePromise(rt,promise,true,Value::Str(wstring(e.what(),e.what()+strlen(e.what())))); return; }
            }
            auto it=promise.object->properties.find(L"__promise_settle");
            if(it!=promise.object->properties.end())rt.Invoke(it->second,{Value::Bool(rejected),value},promise);
        }
    public:
        static Value CreatePromise(Runtime& rt, const wstring& state=L"pending", const Value& value=Value::Undef()) { return MakePromise(rt,state,value); }
        static void ResolvePromise(Runtime& rt,const Value& p,bool rejected,const Value& value) { SettlePromise(rt,p,rejected,value); }
        static bool IsPromise(const Value& v) { return IsPromiseValue(v); }
        static void EnqueueMicrotask(const Value& callback) { if(callback.type==Value::FunctionValue)QueueMicrotask([callback](Runtime& r){r.Invoke(callback,{});}); }
        static void PostWorkerEvent(const std::weak_ptr<Object>& owner, const std::string& json, bool failed = false, const std::string& error = std::string()) { QueueWorkerEvent(owner, json, failed, error); }
        static void PostExternalTask(std::function<void(Runtime&)> task) { QueueExternalTask(std::move(task)); }
        void InstallBuiltins() {
            auto arrayCtor = make_shared<Object>();
            auto arrayProto = make_shared<Object>();
            arrayProto->properties[L"slice"] = MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {
                auto out = make_shared<Object>(); out->array = true; if (self.type != Value::ObjectValue || !self.object)return Value::Obj(out);
                auto src = self.object; int n = (int)(src->array ? src->elements.size() : 0); int b = a.empty() ? 0 : (int)ToNumber(a[0]); int e = a.size() > 1 ? (int)ToNumber(a[1]) : n;
                if (b < 0)b = max(0, n + b); if (e < 0)e = max(0, n + e); b = min(n, max(0, b)); e = min(n, max(0, e));
                for (int i = b; i < e; ++i)out->elements.push_back(src->elements[(size_t)i]); return Value::Obj(out);
                });
            arrayProto->properties[L"indexOf"] = MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {if (self.type != Value::ObjectValue || !self.object || a.empty())return Value::Num(-1); for (size_t i = 0; i < self.object->elements.size(); ++i)if (StrictEqual(self.object->elements[i], a[0]))return Value::Num((double)i); return Value::Num(-1); });
            arrayCtor->properties[L"prototype"] = Value::Obj(arrayProto); Define(L"Array", Value::Obj(arrayCtor));
            auto console = make_shared<Object>(); console->properties[L"log"] = MakeNative([](Runtime&, const vector<Value>& a, const Value&) { for (size_t i = 0; i < a.size(); ++i) { if (i) std::fputc(' ', stderr); std::string line = WToUtf8(ToString(a[i])); std::fwrite(line.data(), 1, line.size(), stderr); } std::fputc('\n', stderr); std::fflush(stderr); return Value::Undef(); }); console->properties[L"error"] = console->properties[L"log"]; console->properties[L"warn"] = console->properties[L"log"]; Define(L"console", Value::Obj(console));
            auto math = make_shared<Object>(); math->properties[L"PI"] = Value::Num(3.141592653589793); math->properties[L"E"] = Value::Num(2.718281828459045);
            math->properties[L"abs"] = MakeNative([](Runtime&, const vector<Value>& a, const Value&) {return Value::Num(std::fabs(a.empty() ? 0 : ToNumber(a[0]))); });
            math->properties[L"floor"] = MakeNative([](Runtime&, const vector<Value>& a, const Value&) {return Value::Num(std::floor(a.empty() ? 0 : ToNumber(a[0]))); });
            math->properties[L"ceil"] = MakeNative([](Runtime&, const vector<Value>& a, const Value&) {return Value::Num(std::ceil(a.empty() ? 0 : ToNumber(a[0]))); });
            math->properties[L"round"] = MakeNative([](Runtime&, const vector<Value>& a, const Value&) {return Value::Num(std::round(a.empty() ? 0 : ToNumber(a[0]))); });
            math->properties[L"max"] = MakeNative([](Runtime&, const vector<Value>& a, const Value&) {double v = -INFINITY; for (auto& x : a)v = std::max(v, ToNumber(x)); return Value::Num(v); });
            math->properties[L"min"] = MakeNative([](Runtime&, const vector<Value>& a, const Value&) {double v = INFINITY; for (auto& x : a)v = std::min(v, ToNumber(x)); return Value::Num(v); });
            math->properties[L"sqrt"] = MakeNative([](Runtime&, const vector<Value>& a, const Value&) {return Value::Num(std::sqrt(a.empty() ? 0 : ToNumber(a[0]))); });
            Define(L"Math", Value::Obj(math));
            // RegExp constructor complements regex literals and exposes exec/test
            // through the same native regex-object path used by String.match/replace.
            auto regexpCtor = MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {
                auto re = self.type == Value::ObjectValue && self.object ? self.object : make_shared<Object>();
                re->properties[L"__minihtml_regex"] = Value::Str(a.empty() ? L"(?:)" : ToString(a[0]));
                re->properties[L"__minihtml_flags"] = Value::Str(a.size() > 1 ? ToString(a[1]) : L"");
                return Value::Obj(re);
            });
            regexpCtor.function->properties[L"prototype"] = Value::Obj(make_shared<Object>());
            Define(L"RegExp", regexpCtor);
            auto json = make_shared<Object>();
            json->properties[L"stringify"] = MakeNative([](Runtime&, const vector<Value>& a, const Value&) {
                function<wstring(const Value&)> encode = [&](const Value& v)->wstring {
                    if(v.type==Value::Undefined)return L"null"; if(v.type==Value::Null)return L"null"; if(v.type==Value::Boolean)return v.boolean?L"true":L"false"; if(v.type==Value::Number)return std::isfinite(v.number)?ToString(v):L"null";
                    if(v.type==Value::String){wstring out=L"\"";for(wchar_t c:v.string){if(c==L'\"')out+=L"\\\"";else if(c==L'\\')out+=L"\\\\";else if(c==L'\n')out+=L"\\n";else if(c==L'\r')out+=L"\\r";else if(c==L'\t')out+=L"\\t";else out+=c;}return out+L"\"";}
                    if(v.type==Value::ObjectValue&&v.object){wstring out=v.object->array?L"[":L"{";bool first=true;if(v.object->array){for(const auto& x:v.object->elements){if(!first)out+=L",";first=false;out+=encode(x);}return out+L"]";}for(const auto& kv:v.object->properties){if(!first)out+=L",";first=false;out+=encode(Value::Str(kv.first))+L":"+encode(kv.second);}return out+L"}";}return L"null";
                }; return Value::Str(a.empty()?L"undefined":encode(a[0]));
            });
            json->properties[L"parse"] = MakeNative([](Runtime&,const vector<Value>& a,const Value&){
                if(a.empty())return Value::Undef(); wstring s=ToString(a[0]); size_t p=0;
                function<void()> ws=[&](){while(p<s.size()&&iswspace(s[p]))++p;};
                function<wstring()> str=[&](){wstring out;if(p>=s.size()||s[p++]!=L'\"')return out;while(p<s.size()){wchar_t c=s[p++];if(c==L'\"')break;if(c==L'\\'&&p<s.size()){wchar_t e=s[p++];if(e==L'n')out+=L'\n';else if(e==L'r')out+=L'\r';else if(e==L't')out+=L'\t';else if(e==L'b')out+=L'\b';else if(e==L'f')out+=L'\f';else out+=e;}else out+=c;}return out;};
                function<Value()> parse=[&]()->Value{ws();if(p>=s.size())return Value::Undef();if(s[p]==L'\"')return Value::Str(str());if(s[p]==L'{'){++p;auto o=make_shared<Object>();ws();while(p<s.size()&&s[p]!=L'}'){wstring k=str();ws();if(p<s.size()&&s[p]==L':')++p;o->properties[k]=parse();ws();if(p<s.size()&&s[p]==L','){++p;ws();}else break;}if(p<s.size()&&s[p]==L'}')++p;return Value::Obj(o);}if(s[p]==L'['){++p;auto o=make_shared<Object>();o->array=true;ws();while(p<s.size()&&s[p]!=L']'){o->elements.push_back(parse());ws();if(p<s.size()&&s[p]==L','){++p;ws();}else break;}if(p<s.size()&&s[p]==L']')++p;return Value::Obj(o);}if(s.compare(p,4,L"true")==0){p+=4;return Value::Bool(true);}if(s.compare(p,5,L"false")==0){p+=5;return Value::Bool(false);}if(s.compare(p,4,L"null")==0){p+=4;return Value::NullValue();}size_t b=p;while(p<s.size()&&(iswdigit(s[p])||s[p]==L'-'||s[p]==L'+'||s[p]==L'.'||s[p]==L'e'||s[p]==L'E'))++p;try{return Value::Num(std::stod(s.substr(b,p-b)));}catch(...){return Value::Undef();}};
                return parse();
            });
            Define(L"JSON", Value::Obj(json));
            Define(L"NaN", Value::Num(std::nan(""))); Define(L"Infinity", Value::Num(INFINITY));
            Define(L"isNaN", MakeNative([](Runtime&, const vector<Value>& a, const Value&) {return Value::Bool(a.empty() || std::isnan(ToNumber(a[0]))); }));
            Define(L"isFinite", MakeNative([](Runtime&, const vector<Value>& a, const Value&) {return Value::Bool(!a.empty() && std::isfinite(ToNumber(a[0]))); }));
            Define(L"parseInt", MakeNative([](Runtime&, const vector<Value>& a, const Value&) {if (a.empty())return Value::Num(std::nan("")); try { int base = a.size() > 1 ? (int)ToNumber(a[1]) : 10; if (base == 0)base = 10; return Value::Num((double)std::stoll(ToString(a[0]), nullptr, base)); } catch (...) { return Value::Num(std::nan("")); }}));
            Define(L"parseFloat", MakeNative([](Runtime&, const vector<Value>& a, const Value&) {if (a.empty())return Value::Num(std::nan("")); try { return Value::Num(std::stod(ToString(a[0]))); } catch (...) { return Value::Num(std::nan("")); }}));
            Define(L"String", MakeNative([](Runtime&, const vector<Value>& a, const Value&) {return Value::Str(a.empty() ? L"" : ToString(a[0])); }));
            Define(L"Error", MakeNative([](Runtime&,const vector<Value>& a,const Value& self){Value out=self.type==Value::ObjectValue?self:Value::Obj(make_shared<Object>());out.object->properties[L"name"]=Value::Str(L"Error");out.object->properties[L"message"]=Value::Str(a.empty()?L"":ToString(a[0]));return out;}));
            auto numberCtor=MakeNative([](Runtime&,const vector<Value>& a,const Value&){return Value::Num(a.empty()?0:ToNumber(a[0]));});
            numberCtor.function->properties[L"isFinite"]=MakeNative([](Runtime&,const vector<Value>& a,const Value&){return Value::Bool(!a.empty()&&std::isfinite(ToNumber(a[0])));});
            numberCtor.function->properties[L"isSafeInteger"]=MakeNative([](Runtime&,const vector<Value>& a,const Value&){double x=a.empty()?NAN:ToNumber(a[0]);return Value::Bool(std::isfinite(x)&&std::floor(x)==x&&std::fabs(x)<=9007199254740991.0);});
            numberCtor.function->properties[L"isInteger"]=MakeNative([](Runtime&,const vector<Value>& a,const Value&){double x=a.empty()?NAN:ToNumber(a[0]);return Value::Bool(std::isfinite(x)&&std::floor(x)==x);});
            numberCtor.function->properties[L"isNaN"]=MakeNative([](Runtime&,const vector<Value>& a,const Value&){return Value::Bool(a.empty()||std::isnan(ToNumber(a[0])));});
            numberCtor.function->properties[L"MAX_SAFE_INTEGER"]=Value::Num(9007199254740991.0); Define(L"Number",numberCtor);
            Define(L"Boolean", MakeNative([](Runtime&, const vector<Value>& a, const Value&) {return Value::Bool(!a.empty() && Truthy(a[0])); }));
            auto objectCtor = MakeNative([](Runtime&, const vector<Value>& a, const Value&) {return !a.empty() && a[0].type == Value::ObjectValue ? a[0] : Value::Obj(make_shared<Object>()); });
            objectCtor.function->properties[L"entries"] = MakeNative([](Runtime&,const vector<Value>& a,const Value&){auto out=make_shared<Object>();out->array=true;if(!a.empty()&&a[0].type==Value::ObjectValue&&a[0].object){auto src=a[0].object;if(src->array){for(size_t i=0;i<src->elements.size();++i){auto p=make_shared<Object>();p->array=true;p->elements={Value::Str(to_wstring(i)),src->elements[i]};out->elements.push_back(Value::Obj(p));}}else for(const auto& kv:src->properties){auto p=make_shared<Object>();p->array=true;p->elements={Value::Str(kv.first),kv.second};out->elements.push_back(Value::Obj(p));}}return Value::Obj(out);});
            objectCtor.function->properties[L"values"] = MakeNative([](Runtime&,const vector<Value>& a,const Value&){auto out=make_shared<Object>();out->array=true;if(!a.empty()&&a[0].type==Value::ObjectValue&&a[0].object){auto src=a[0].object;if(src->array)out->elements=src->elements;else for(const auto& kv:src->properties)out->elements.push_back(kv.second);}return Value::Obj(out);});
            objectCtor.function->properties[L"keys"] = MakeNative([](Runtime&,const vector<Value>& a,const Value&){auto out=make_shared<Object>();out->array=true;if(!a.empty()&&a[0].type==Value::ObjectValue&&a[0].object){auto src=a[0].object;if(src->array){for(size_t i=0;i<src->elements.size();++i)out->elements.push_back(Value::Str(to_wstring(i)));}else for(const auto& kv:src->properties)out->elements.push_back(Value::Str(kv.first));}return Value::Obj(out);});
            objectCtor.function->properties[L"assign"] = MakeNative([](Runtime&,const vector<Value>& a,const Value&){auto out=make_shared<Object>();for(const auto& srcv:a)if(srcv.type==Value::ObjectValue&&srcv.object){for(const auto& kv:srcv.object->properties)out->properties[kv.first]=kv.second;if(srcv.object->array)for(size_t i=0;i<srcv.object->elements.size();++i)out->properties[to_wstring(i)]=srcv.object->elements[i];}return Value::Obj(out);});
            Define(L"Object", objectCtor);
            auto mapCtor=MakeNative([](Runtime&,const vector<Value>&,const Value& self){auto o=self.type==Value::ObjectValue&&self.object?self.object:make_shared<Object>();if(!o->properties.count(L"__map_entries")){auto entries=make_shared<Object>();entries->array=true;o->properties[L"__map_entries"]=Value::Obj(entries);}return Value::Obj(o);});
            mapCtor.function->properties[L"prototype"]=Value::Obj(make_shared<Object>());
            mapCtor.function->properties[L"prototype"].object->properties[L"set"]=MakeNative([](Runtime&,const vector<Value>& a,const Value& self){if(self.object&&a.size()>1){auto it=self.object->properties.find(L"__map_entries");if(it!=self.object->properties.end()&&it->second.object){auto& v=it->second.object->elements;for(auto& pair:v)if(pair.object&&pair.object->elements.size()>1&&StrictEqual(pair.object->elements[0],a[0])){pair.object->elements[1]=a[1];return self;}auto p=make_shared<Object>();p->array=true;p->elements={a[0],a[1]};v.push_back(Value::Obj(p));}}return self;});
            mapCtor.function->properties[L"prototype"].object->properties[L"get"]=MakeNative([](Runtime&,const vector<Value>& a,const Value& self){if(self.object&&!a.empty()){auto it=self.object->properties.find(L"__map_entries");if(it!=self.object->properties.end()&&it->second.object)for(auto& p:it->second.object->elements)if(p.object&&p.object->elements.size()>1&&StrictEqual(p.object->elements[0],a[0]))return p.object->elements[1];}return Value::Undef();});
            mapCtor.function->properties[L"prototype"].object->properties[L"has"]=MakeNative([](Runtime&,const vector<Value>& a,const Value& self){if(self.object&&!a.empty()){auto it=self.object->properties.find(L"__map_entries");if(it!=self.object->properties.end()&&it->second.object)for(auto& p:it->second.object->elements)if(p.object&&p.object->elements.size()>1&&StrictEqual(p.object->elements[0],a[0]))return Value::Bool(true);}return Value::Bool(false);});
            mapCtor.function->properties[L"prototype"].object->properties[L"delete"]=MakeNative([](Runtime&,const vector<Value>& a,const Value& self){if(self.object&&!a.empty()){auto it=self.object->properties.find(L"__map_entries");if(it!=self.object->properties.end()&&it->second.object){auto& v=it->second.object->elements;v.erase(std::remove_if(v.begin(),v.end(),[&](const Value& p){return p.object&&p.object->elements.size()>1&&StrictEqual(p.object->elements[0],a[0]);}),v.end());}}return Value::Bool(true);});
            mapCtor.function->properties[L"prototype"].object->properties[L"clear"]=MakeNative([](Runtime&,const vector<Value>&,const Value& self){if(self.object){auto it=self.object->properties.find(L"__map_entries");if(it!=self.object->properties.end()&&it->second.object)it->second.object->elements.clear();}return Value::Undef();});
            mapCtor.function->properties[L"prototype"].object->getter=[](Runtime&,const wstring& k){return Value::Undef();};
            Define(L"Map",mapCtor);
            Define(L"Array", MakeNative([](Runtime& r, const vector<Value>& a, const Value&) {auto o = make_shared<Object>(); o->array = true; Value ac = r.GetGlobal(L"Array"); if (ac.type == Value::FunctionValue) { auto p = ac.function->properties.find(L"prototype"); if (p != ac.function->properties.end() && p->second.type == Value::ObjectValue)o->prototype = p->second.object; }if (a.size() == 1 && a[0].type == Value::Number)o->elements.resize((size_t)std::max(0.0, ToNumber(a[0]))); else o->elements = a; return Value::Obj(o); }));
            { Value ac = GetGlobal(L"Array"); if (ac.type == Value::FunctionValue) ac.function->properties[L"prototype"] = Value::Obj(arrayProto); }
            { Value ac=GetGlobal(L"Array"); if(ac.type==Value::FunctionValue){ac.function->properties[L"isArray"]=MakeNative([](Runtime&,const vector<Value>& a,const Value&){return Value::Bool(!a.empty()&&a[0].type==Value::ObjectValue&&a[0].object&&a[0].object->array);});ac.function->properties[L"from"]=MakeNative([](Runtime&,const vector<Value>& a,const Value&){auto out=make_shared<Object>();out->array=true;if(!a.empty()){if(a[0].type==Value::String){for(wchar_t c:a[0].string)out->elements.push_back(Value::Str(wstring(1,c)));}else if(a[0].type==Value::ObjectValue&&a[0].object){if(a[0].object->array)out->elements=a[0].object->elements;else{auto it=a[0].object->properties.find(L"length");size_t n=it==a[0].object->properties.end()?0:(size_t)max(0.0,ToNumber(it->second));for(size_t i=0;i<n&&i<10000;++i){auto v=a[0].object->properties.find(to_wstring(i));out->elements.push_back(v==a[0].object->properties.end()?Value::Undef():v->second);}}}}return Value::Obj(out);});}}
            Define(L"encodeURIComponent", MakeNative([](Runtime&, const vector<Value>& a, const Value&) {wstring in = a.empty() ? L"" : ToString(a[0]), out; const wchar_t* hex = L"0123456789ABCDEF"; for (unsigned char c : std::string(in.begin(), in.end())) { if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' || c == 39 || c == '(' || c == ')')out += (wchar_t)c; else { out += L'%'; out += hex[c >> 4]; out += hex[c & 15]; } }return Value::Str(out); }));
            Define(L"decodeURIComponent", MakeNative([](Runtime&, const vector<Value>& a, const Value&) {wstring in = a.empty() ? L"" : ToString(a[0]), out; for (size_t i = 0; i < in.size(); ++i) { if (in[i] == L'%' && i + 2 < in.size()) { wchar_t h[3] = { in[i + 1],in[i + 2],0 }; out += (wchar_t)wcstol(h, nullptr, 16); i += 2; } else out += in[i]; }return Value::Str(out); }));
            math->properties[L"random"] = MakeNative([](Runtime&, const vector<Value>&, const Value&) {static unsigned seed = 0x13579BDF; seed = seed * 1664525u + 1013904223u; return Value::Num((double)(seed & 0x00FFFFFFu) / 16777216.0); });
            auto datePrototype = make_shared<Object>();
            datePrototype->properties[L"getTime"] = MakeNative([](Runtime&, const vector<Value>&, const Value& self) { if (self.type == Value::ObjectValue && self.object) { auto it = self.object->properties.find(L"__time"); if (it != self.object->properties.end()) return it->second; } return Value::Num(std::nan("")); });
            datePrototype->properties[L"valueOf"] = datePrototype->properties[L"getTime"];
            datePrototype->properties[L"toISOString"] = MakeNative([](Runtime&, const vector<Value>&, const Value& self) {
            double ms = 0;
            if (self.type == Value::ObjectValue && self.object) {
                auto it = self.object->properties.find(L"__time");
                if (it != self.object->properties.end()) ms = ToNumber(it->second);
            }
            std::time_t sec = (std::time_t)(ms / 1000.0);
            std::tm tmv{};
#if defined(_WIN32)
            gmtime_s(&tmv, &sec);
#else
            gmtime_r(&sec, &tmv);
#endif
            wchar_t buf[64];
            std::swprintf(buf, 64, L"%04d-%02d-%02dT%02d:%02d:%02d.000Z", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
            return Value::Str(buf);
        });
            datePrototype->properties[L"toLocaleDateString"] = MakeNative([](Runtime&,const vector<Value>&,const Value& self){double ms=0;if(self.object&&self.object->properties.count(L"__time"))ms=ToNumber(self.object->properties[L"__time"]);std::time_t sec=(std::time_t)(ms/1000.0);std::tm tmv{};
#if defined(_WIN32)
                localtime_s(&tmv,&sec);
#else
                localtime_r(&sec,&tmv);
#endif
                wchar_t buf[64];std::swprintf(buf,64,L"%02d/%02d/%04d",tmv.tm_mon+1,tmv.tm_mday,tmv.tm_year+1900);return Value::Str(buf);});
            auto dateNow = MakeNative([](Runtime&, const vector<Value>&, const Value&) { return Value::Num((double)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()); });
            auto dateCtor = MakeNative([dateNow](Runtime& r, const vector<Value>& a, const Value& self) {
                Value now = dateNow.function->native(r, {}, Value::Undef()); double ms = ToNumber(now);
                if (!a.empty()) ms = ToNumber(a[0]);
                if (self.type == Value::ObjectValue && self.object) { self.object->properties[L"__time"] = Value::Num(ms); return Value::Undef(); }
                return Value::Str(L"Thu Jan 01 1970");
            });
            dateCtor.function->properties[L"prototype"] = Value::Obj(datePrototype); dateCtor.function->properties[L"now"] = dateNow; Define(L"Date", dateCtor);
        }
    };

// Out-of-class definitions keep these Runtime-wide queues and scheduler state
// compatible with MSVC projects that parse static TLS members conservatively.
thread_local vector<std::function<void(Runtime&)>> Runtime::microtaskQueue;
thread_local bool Runtime::drainingMicrotasks = false;
thread_local int Runtime::executionDepth = 0;
thread_local Runtime::AsyncTask* Runtime::currentAsyncTask = nullptr;
thread_local vector<shared_ptr<Runtime::AsyncTask>> Runtime::activeAsyncTasks;
std::mutex Runtime::workerEventMutex;
std::deque<Runtime::WorkerEvent> Runtime::workerEvents;
std::mutex Runtime::externalTaskMutex;
std::deque<std::function<void(Runtime&)>> Runtime::externalTasks;
#if defined(_WIN32)
thread_local void* Runtime::rootFiber = nullptr;
#endif

} // namespace minihtml_js



using namespace std;
using namespace Gdiplus;

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

// ============================================================
// 0. UTILITIES & NETWORKING
// ============================================================

wstring Lower(wstring s) {
    transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
}

wstring Trim(const wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

vector<wstring> SplitValue(const wstring& s) {
    vector<wstring> out;
    wstring cur;
    wchar_t quote = 0;
    for (wchar_t c : s) {
        if (quote) {
            if (c == quote) quote = 0; else cur += c;
        }
        else if (c == L'"' || c == L'\'') {
            quote = c;
        }
        else if (iswspace(c)) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        }
        else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

static wstring DecodeFileURLPath(const wstring& input) {
    wstring path = input;
    if (path.rfind(L"file://", 0) == 0) path = path.substr(7);
    // file://localhost/path is equivalent to file:///path.
    if (path.rfind(L"localhost/", 0) == 0) path = path.substr(9);
    // Decode percent-encoded UTF-8 bytes without altering ordinary Unicode.
    string bytes;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == L'%' && i + 2 < path.size()) {
            auto hex = [](wchar_t c)->int {
                if (c >= L'0' && c <= L'9') return c - L'0';
                if (c >= L'a' && c <= L'f') return c - L'a' + 10;
                if (c >= L'A' && c <= L'F') return c - L'A' + 10;
                return -1;
            };
            int a = hex(path[i + 1]), b = hex(path[i + 2]);
            if (a >= 0 && b >= 0) { bytes.push_back((char)((a << 4) | b)); i += 2; continue; }
        }
        if ((unsigned)path[i] < 128) bytes.push_back((char)path[i]);
        else {
            // Preserve non-ASCII wide characters by converting this character.
            wstring one(1, path[i]);
            // Utf8 conversion is defined above this helper.
            bytes += WToUtf8(one);
        }
    }
    wstring decoded = Utf8ToW(bytes);
#if defined(_WIN32)
    // file:///C:/... has one leading slash before the drive letter.
    if (decoded.size() >= 3 && decoded[0] == L'/' && iswalpha(decoded[1]) && decoded[2] == L':')
        decoded.erase(decoded.begin());
#endif
    return decoded;
}

vector<BYTE> FetchBinaryURL(const wstring& url) {
    if (url.rfind(L"file://", 0) == 0) {
        vector<BYTE> result;
        try {
            std::ifstream file(std::filesystem::path(DecodeFileURLPath(url)), std::ios::binary);
            if (!file) return result;
            result.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        } catch (...) {}
        return result;
    }
#if defined(_WIN32)
    vector<BYTE> result;
    URL_COMPONENTS uc = { sizeof(uc) };
    uc.dwSchemeLength = (DWORD)-1; uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1; uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) return result;

    wstring host(uc.lpszHostName, uc.dwHostNameLength);
    wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (path.empty()) path = L"/";

    HINTERNET session = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) minihtml/4.1",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return result;

    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

    HINTERNET conn = WinHttpConnect(session, host.c_str(), uc.nPort, 0);
    if (conn) {
        DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (req) {
            DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
            WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));
            if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(req, nullptr)) {
                DWORD avail = 0;
                do {
                    if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
                    size_t oldSize = result.size();
                    result.resize(oldSize + avail);
                    DWORD read = 0;
                    if (!WinHttpReadData(req, result.data() + oldSize, avail, &read)) break;
                    result.resize(oldSize + read);
                } while (avail > 0);
            }
            WinHttpCloseHandle(req);
        }
        WinHttpCloseHandle(conn);
    }
    WinHttpCloseHandle(session);
    return result;
#else
    vector<BYTE> result;
    CURL* curl = curl_easy_init(); if (!curl)return result;
    std::string u = WToUtf8(url);
    curl_easy_setopt(curl, CURLOPT_URL, u.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 8L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 12L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 minihtml/5.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    CURLcode rc = curl_easy_perform(curl); long status = 0; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl); if (rc != CURLE_OK || status >= 400)result.clear(); return result;
#endif
}

struct HTTPResponse { bool ok = false; wstring body; wstring error; long status = 0; };
HTTPResponse FetchURL(const wstring& url) {
    HTTPResponse out;
    vector<BYTE> data = FetchBinaryURL(url);
    if (data.empty()) { out.error = L"Vacío o error de red"; return out; }
#if !defined(_WIN32)
    out.body = Utf8ToW(std::string((const char*)data.data(), data.size())); out.ok = !out.body.empty(); if (!out.ok)out.error = L"No se pudo decodificar UTF-8"; return out;
#else
    int wlen = MultiByteToWideChar(CP_UTF8, 0, (char*)data.data(), (int)data.size(), nullptr, 0);
    if (wlen > 0) {
        out.body.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, (char*)data.data(), (int)data.size(), &out.body[0], wlen);
        out.ok = true;
    }
    else {
        wlen = MultiByteToWideChar(CP_ACP, 0, (char*)data.data(), (int)data.size(), nullptr, 0);
        if (wlen > 0) {
            out.body.resize(wlen);
            MultiByteToWideChar(CP_ACP, 0, (char*)data.data(), (int)data.size(), &out.body[0], wlen);
            out.ok = true;
        }
    }
    return out;
#endif
}

wstring ResolveURL(const wstring& base, const wstring& href) {
    wstring h = Trim(href);
    if (h.empty()) return L"";
    wstring lh = Lower(h);
    if (lh.find(L"http://") == 0 || lh.find(L"https://") == 0 || lh.find(L"data:") == 0 || lh.find(L"mailto:") == 0 || lh.find(L"javascript:") == 0) return h;
    if (h[0] == L'#') { size_t q = base.find(L'#'); return (q == wstring::npos ? base : base.substr(0, q)) + h; }
    if (h.find(L"//") == 0) { size_t colon = base.find(L':'); return (colon != wstring::npos ? base.substr(0, colon + 1) : L"https:") + h; }
    wstring cleanBase = base; size_t frag = cleanBase.find_first_of(L"?#"); if (frag != wstring::npos) cleanBase.resize(frag);
    if (cleanBase.rfind(L"file://", 0) == 0) {
        // Resolve local relative and root-relative resources against a file URL.
        wstring filePath = DecodeFileURLPath(cleanBase);
        std::filesystem::path parent = std::filesystem::path(filePath).parent_path();
        std::filesystem::path resolved;
        if (!h.empty() && (h[0] == L'/' || h[0] == L'\\')) resolved = std::filesystem::path(DecodeFileURLPath(L"file://" + h));
        else resolved = parent / std::filesystem::path(h);
        resolved = resolved.lexically_normal();
        wstring native = resolved.wstring();
#if defined(_WIN32)
        if (native.size() >= 2 && native[1] == L':') return L"file:///" + native;
#endif
        return L"file://" + native;
    }
    size_t scheme = cleanBase.find(L"://"); if (scheme == wstring::npos) return h;
    size_t hostEnd = cleanBase.find(L'/', scheme + 3);
    wstring origin = hostEnd == wstring::npos ? cleanBase : cleanBase.substr(0, hostEnd);
    if (!h.empty() && h[0] == L'/') return origin + h;
    size_t slash = cleanBase.find_last_of(L'/');
    wstring dir = slash == wstring::npos ? cleanBase + L"/" : cleanBase.substr(0, slash + 1);
    wstring combined = dir + h;
    // Normalize ./ and ../ segments without changing the authority.
    size_t pathStart = combined.find(L"://"); pathStart = pathStart == wstring::npos ? 0 : combined.find(L'/', pathStart + 3);
    if (pathStart == wstring::npos) return combined;
    wstring prefix = combined.substr(0, pathStart), path = combined.substr(pathStart);
    vector<wstring> parts; std::wstringstream ss(path); wstring part;
    while (std::getline(ss, part, L'/')) { if (part.empty() || part == L".") continue; if (part == L"..") { if (!parts.empty()) parts.pop_back(); } else parts.push_back(part); }
    wstring normalized = prefix + L"/"; for (size_t i = 0; i < parts.size(); ++i) { if (i) normalized += L"/"; normalized += parts[i]; }
    return normalized;
}

// --- Image Cache ---
class ImageCache {
    unordered_map<wstring, Bitmap*> cache;
public:
    Bitmap* Get(const wstring& url) {
        auto it = cache.find(url);
        if (it != cache.end()) return it->second;
        vector<BYTE> bytes = FetchBinaryURL(url);
        if (bytes.empty()) return nullptr;
#if !defined(_WIN32)
        Bitmap* bmp = Bitmap::FromBytes(bytes.data(), bytes.size());
        if (bmp && bmp->GetLastStatus() == Ok) { cache[url] = bmp; return bmp; }
        delete bmp;
        return nullptr;
#else
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
        void* pMem = GlobalLock(hMem);
        memcpy(pMem, bytes.data(), bytes.size());
        GlobalUnlock(hMem);
        IStream* pStream = nullptr;
        CreateStreamOnHGlobal(hMem, TRUE, &pStream);
        Bitmap* bmp = Bitmap::FromStream(pStream);
        pStream->Release();
        if (bmp && bmp->GetLastStatus() == Ok) {
            cache[url] = bmp;
            return bmp;
        }
        return nullptr;
#endif
    }
    ~ImageCache() { for (auto& p : cache) delete p.second; }
};
static ImageCache g_imageCache;

// ============================================================
// 1. LENGTHS & COLORS
// ============================================================
int g_rootFontSize = 16;
static int g_layoutViewportWidth = 1100;
static int g_layoutViewportHeight = 738;

// Keep JavaScript disabled during the HTML/CSS layout test. Set true to run scripts.
static bool g_javascriptEnabled = true;

struct Length {
    enum Kind { Auto, Px, Percent, Em, Rem } kind = Auto;
    float value = 0;
    int Resolve(int ref, int rootRef = 16) const {
        if (kind == Auto) return -1;
        if (kind == Px) return (int)value;
        if (kind == Percent) return (int)(value * ref / 100.0f);
        if (kind == Em) return (int)(value * ref);
        if (kind == Rem) return (int)(value * rootRef);
        return 0;
    }
};

Length ParseLength(const wstring& v) {
    wstring s = Lower(Trim(v));
    if (s.empty() || s == L"auto") return { Length::Auto, 0 };
    try {
        if (s.back() == L'%') return { Length::Percent, stof(s.substr(0, s.size() - 1)) };
        if (s.size() > 2 && s.substr(s.size() - 2) == L"px") return { Length::Px, stof(s.substr(0, s.size() - 2)) };
        if (s.size() > 3 && s.substr(s.size() - 3) == L"rem") return { Length::Rem, stof(s.substr(0, s.size() - 3)) };
        if (s.size() > 2 && s.substr(s.size() - 2) == L"em") return { Length::Em, stof(s.substr(0, s.size() - 2)) };
        if (s.size() > 2 && s.substr(s.size() - 2) == L"pt") return { Length::Px, stof(s.substr(0, s.size() - 2)) * 4.0f / 3.0f };
        return { Length::Px, stof(s) };
    }
    catch (...) { return {}; }
}

COLORREF ParseColor(wstring value, COLORREF fallback) {
    value = Lower(Trim(value));
    if (value == L"transparent") return CLR_INVALID;
    static const unordered_map<wstring, COLORREF> named = {
        {L"black", RGB(0,0,0)}, {L"white", RGB(255,255,255)}, {L"red", RGB(255,0,0)},
        {L"green", RGB(0,128,0)}, {L"blue", RGB(0,0,255)}, {L"yellow", RGB(255,255,0)},
        {L"cyan", RGB(0,255,255)}, {L"gray", RGB(128,128,128)}, {L"grey", RGB(128,128,128)},
        {L"silver", RGB(192,192,192)}, {L"maroon", RGB(128,0,0)}, {L"purple", RGB(128,0,128)},
        {L"orange", RGB(255,165,0)}, {L"navy", RGB(0,0,128)}
    };
    auto it = named.find(value);
    if (it != named.end()) return it->second;
    if (value.size() >= 4 && value[0] == L'#') {
        wstring hex = value.substr(1);
        if (hex.size() == 3) {
            auto h = [](wchar_t c) { wchar_t b[2] = { c, 0 }; return (int)wcstoul(b, nullptr, 16) * 17; };
            return RGB(h(hex[0]), h(hex[1]), h(hex[2]));
        }
        if (hex.size() == 6) {
            wchar_t* endp = nullptr;
            unsigned long c = wcstoul(hex.c_str(), &endp, 16);
            if (endp && *endp == 0) return RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
        }
    }
    return fallback;
}

// ============================================================
// 2. DOM & HTML PARSER
// ============================================================
HTTPResponse FetchRequest(const wstring& url, const wstring& method, const wstring& body, const vector<wstring>& headers) {
    HTTPResponse out;
#if defined(_WIN32)
    URL_COMPONENTS uc = { sizeof(uc) };
    uc.dwSchemeLength = (DWORD)-1; uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1; uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) {
        out.error = L"Invalid URL"; return out;
    }
    wstring host(uc.lpszHostName, uc.dwHostNameLength);
    wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (path.empty()) path = L"/";

    HINTERNET session = WinHttpOpen(L"Mozilla/5.0 minihtml/5.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { out.error = L"WinHTTP session creation failed"; return out; }

    HINTERNET conn = WinHttpConnect(session, host.c_str(), uc.nPort, 0);
    if (!conn) {
        out.error = L"WinHTTP connection failed";
        WinHttpCloseHandle(session);
        return out;
    }

    wstring verb = method.empty() ? L"GET" : method;
    DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, verb.c_str(), path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
        out.error = L"WinHTTP request creation failed";
        WinHttpCloseHandle(conn); WinHttpCloseHandle(session);
        return out;
    }

    wstring hs;
    for (const auto& header : headers) { hs += header; hs += L"\r\n"; }
    string requestBody = WToUtf8(body);
    BOOL sent = WinHttpSendRequest(req,
        hs.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hs.c_str(),
        hs.empty() ? 0 : (DWORD)hs.size(),
        requestBody.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)requestBody.data(),
        (DWORD)requestBody.size(), (DWORD)requestBody.size(), 0);
    if (!sent) {
        out.error = L"WinHTTP failed to send request";
    } else if (!WinHttpReceiveResponse(req, nullptr)) {
        out.error = L"WinHTTP failed to receive response";
    } else {
        DWORD statusCode = 0, statusSize = sizeof(statusCode);
        if (WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
            out.status = (long)statusCode;
        }
        vector<char> bytes;
        DWORD available = 0;
        do {
            if (!WinHttpQueryDataAvailable(req, &available) || !available) break;
            size_t oldSize = bytes.size();
            bytes.resize(oldSize + available);
            DWORD received = 0;
            if (!WinHttpReadData(req, bytes.data() + oldSize, available, &received)) break;
            bytes.resize(oldSize + received);
        } while (available);
        out.body = Utf8ToW(string(bytes.begin(), bytes.end()));
        out.ok = out.status >= 200 && out.status < 400;
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
#else
    CURL* curl=curl_easy_init();if(!curl){out.error=L"curl initialization failed";return out;}vector<BYTE> bytes;string u=WToUtf8(url),b=WToUtf8(body);curl_easy_setopt(curl,CURLOPT_URL,u.c_str());curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);curl_easy_setopt(curl,CURLOPT_TIMEOUT,15L);curl_easy_setopt(curl,CURLOPT_USERAGENT,"Mozilla/5.0 minihtml/5.0");curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,CurlWrite);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&bytes);curl_easy_setopt(curl,CURLOPT_CUSTOMREQUEST,method.empty()?"GET":WToUtf8(method).c_str());if(!b.empty())curl_easy_setopt(curl,CURLOPT_POSTFIELDS,b.data());struct curl_slist* hs=nullptr;for(const auto& h:headers)hs=curl_slist_append(hs,WToUtf8(h).c_str());if(hs)curl_easy_setopt(curl,CURLOPT_HTTPHEADER,hs);CURLcode rc=curl_easy_perform(curl);curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&out.status);if(rc==CURLE_OK){out.body=Utf8ToW(string((const char*)bytes.data(),bytes.size()));out.ok=out.status>=200&&out.status<400;}else out.error=Utf8ToW(curl_easy_strerror(rc));if(hs)curl_slist_free_all(hs);curl_easy_cleanup(curl);
#endif
    return out;
}

struct Node {
    wstring tag;
    wstring text;
    unordered_map<wstring, wstring> attributes;
    vector<Node*> children;
    Node* parent = nullptr;
    ~Node() { for (Node* c : children) delete c; }
};
wstring Lower(wstring s);
wstring Trim(const wstring& s);
static void CollectNodeText(Node* node, wstring& out);
wstring GetAttribute(Node* n, const wstring& key) {
    if (!n) return L"";
    auto it = n->attributes.find(key);
    return it == n->attributes.end() ? L"" : it->second;
}

static bool IsFormControlNode(const Node* n) {
    return n && (n->tag == L"input" || n->tag == L"button" || n->tag == L"textarea" || n->tag == L"select");
}
static wstring InputType(Node* n) {
    if (!n || n->tag != L"input") return L"";
    wstring t = Lower(Trim(GetAttribute(n, L"type")));
    return t.empty() ? L"text" : t;
}
static bool IsTextEntryNode(Node* n) {
    if (!n) return false;
    if (n->tag == L"textarea") return true;
    if (n->tag != L"input") return false;
    wstring t = InputType(n);
    return t == L"text" || t == L"search" || t == L"email" || t == L"url" || t == L"tel" || t == L"password" || t == L"number";
}
static wstring ControlValue(Node* n) {
    if (!n) return L"";
    if (n->tag == L"textarea") return GetAttribute(n, L"value").empty() ? [&]() { wstring v; CollectNodeText(n, v); return v; }() : GetAttribute(n, L"value");
    if (n->tag == L"button") { wstring v; CollectNodeText(n, v); return v; }
    return GetAttribute(n, L"value");
}

const wchar_t* kVoidElements[] = { L"area", L"base", L"br", L"col", L"embed", L"hr", L"img", L"input", L"link", L"meta", L"param", L"source", L"track", L"wbr" };
const wchar_t* kRawTextElements[] = { L"script", L"style" };
bool IsVoidElement(const wstring& t) { for (auto e : kVoidElements) if (t == e) return true; return false; }
bool IsRawTextElement(const wstring& t) { for (auto e : kRawTextElements) if (t == e) return true; return false; }
bool IsBlockElement(const wstring& t) {
    static const wchar_t* blocks[] = { L"html", L"body", L"div", L"p", L"h1", L"h2", L"h3", L"h4", L"h5", L"h6", L"ul", L"ol", L"li", L"table", L"tr", L"td", L"th", L"section", L"article", L"header", L"footer", L"nav", L"main", L"aside", L"form", L"fieldset", L"blockquote", L"hr", L"pre", L"figure", L"figcaption" };
    for (auto e : blocks) if (t == e) return true; return false;
}

wstring DecodeEntities(const wstring& s) {
    wstring out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == L'&') {
            size_t semi = s.find(L';', i);
            if (semi != wstring::npos && semi - i <= 12) {
                wstring ent = s.substr(i + 1, semi - i - 1);
                if (!ent.empty() && ent[0] == L'#') {
                    wstring num = ent.substr(1); int base = 10;
                    if (!num.empty() && (num[0] == L'x' || num[0] == L'X')) { base = 16; num = num.substr(1); }
                    if (!num.empty()) {
                        wchar_t* endp = nullptr; unsigned long cp = wcstoul(num.c_str(), &endp, base);
                        if (endp && *endp == 0 && cp > 0 && cp < 0x110000) { out += (wchar_t)cp; i = semi + 1; continue; }
                    }
                }
                else {
                    if (ent == L"amp") out += L'&';
                    else if (ent == L"lt") out += L'<';
                    else if (ent == L"gt") out += L'>';
                    else if (ent == L"quot") out += L'"';
                    else if (ent == L"apos") out += L'\'';
                    else if (ent == L"nbsp") out += L' ';
                    else if (ent == L"copy") out += L'\u00A9';
                    else if (ent == L"reg") out += L'\u00AE';
                    else if (ent == L"mdash") out += L'\u2014';
                    else if (ent == L"ndash") out += L'\u2013';
                    else { out += L'&' + ent + L';'; i = semi + 1; continue; }
                    i = semi + 1; continue;
                }
            }
        }
        out += s[i++];
    }
    return out;
}

Node* ParseHTML(const wstring& html) {
    Node* document = new Node(); document->tag = L"#document";
    vector<Node*> stack; stack.push_back(document);
    size_t i = 0;
    while (i < html.size()) {
        if (html.compare(i, 4, L"<!--") == 0) {
            size_t end = html.find(L"-->", i + 4);
            i = (end == wstring::npos) ? html.size() : end + 3; continue;
        }
        if (html.compare(i, 2, L"</") == 0) {
            size_t end = html.find(L'>', i);
            if (end == wstring::npos) break;
            wstring tag = Lower(Trim(html.substr(i + 2, end - i - 2)));
            size_t sp = tag.find_first_of(L" \t\r\n"); if (sp != wstring::npos) tag.resize(sp);
            for (int k = (int)stack.size() - 1; k > 0; --k) {
                if (stack[k]->tag == tag) { stack.resize(k); break; }
            }
            i = end + 1; continue;
        }
        if (html[i] == L'<') {
            size_t end = html.find(L'>', i + 1);
            if (end == wstring::npos) break;
            wstring inside = Trim(html.substr(i + 1, end - i - 1));
            if (inside.empty() || inside[0] == L'!') { i = end + 1; continue; }

            bool selfClosing = inside.back() == L'/';
            size_t p = 0;
            while (p < inside.size() && !iswspace(inside[p]) && inside[p] != L'/') p++;
            wstring tag = Lower(inside.substr(0, p));
            if (tag.empty()) { i = end + 1; continue; }

            Node* parent = stack.back();
            while (parent && parent->tag != L"#document") {
                if (tag == L"p" && parent->tag == L"p") { stack.pop_back(); break; }
                if (tag == L"li" && parent->tag == L"li") { stack.pop_back(); break; }
                if ((tag == L"td" || tag == L"th") && (parent->tag == L"td" || parent->tag == L"th")) { stack.pop_back(); break; }
                if (IsBlockElement(tag) && parent->tag == L"p" && tag != L"a" && tag != L"span") { stack.pop_back(); break; }
                parent = parent->parent;
            }

            Node* node = new Node(); node->tag = tag; node->parent = stack.back();
            while (p < inside.size()) {
                while (p < inside.size() && (iswspace(inside[p]) || inside[p] == L'/')) p++;
                if (p >= inside.size()) break;
                size_t start = p;
                while (p < inside.size() && inside[p] != L'=' && !iswspace(inside[p])) p++;
                wstring key = Lower(inside.substr(start, p - start));
                while (p < inside.size() && iswspace(inside[p])) p++;
                wstring value;
                if (p < inside.size() && inside[p] == L'=') {
                    p++;
                    while (p < inside.size() && iswspace(inside[p])) p++;
                    if (p < inside.size() && (inside[p] == L'"' || inside[p] == L'\'')) {
                        wchar_t q = inside[p++]; size_t sv = p;
                        while (p < inside.size() && inside[p] != q) p++;
                        value = inside.substr(sv, p - sv);
                        if (p < inside.size()) p++;
                    }
                    else {
                        size_t sv = p;
                        while (p < inside.size() && !iswspace(inside[p])) p++;
                        value = inside.substr(sv, p - sv);
                    }
                }
                node->attributes[key] = DecodeEntities(value);
            }
            stack.back()->children.push_back(node);
            bool raw = IsRawTextElement(tag);
            if (!selfClosing && !IsVoidElement(tag)) stack.push_back(node);
            i = end + 1;
            if (raw && !selfClosing) {
                wstring closing = L"</" + tag;
                size_t close = Lower(html).find(closing, i);
                if (close != wstring::npos) {
                    Node* text = new Node(); text->tag = L"#text"; text->text = html.substr(i, close - i); text->parent = node;
                    node->children.push_back(text);
                    size_t ce = html.find(L'>', close); i = (ce == wstring::npos) ? html.size() : ce + 1;
                }
                stack.pop_back();
            }
            continue;
        }
        size_t end = html.find(L'<', i); if (end == wstring::npos) end = html.size();
        wstring content = DecodeEntities(html.substr(i, end - i));
        if (!content.empty()) {
            Node* text = new Node(); text->tag = L"#text"; text->text = content; text->parent = stack.back();
            stack.back()->children.push_back(text);
        }
        i = end;
    }
    return document;
}

// ============================================================
// 3. CSS ENGINE
// ============================================================
struct Style {
    COLORREF color = RGB(25, 25, 25);
    int fontSize = 16;
    int lineHeight = 0;
    bool bold = false, italic = false, underline = false, lineThrough = false;
    wstring fontFamily = L"Segoe UI";
    UINT textAlign = 0;
    wstring textTransform = L"none";
    wstring whiteSpace = L"normal";

    COLORREF background = CLR_INVALID;
    int marginTop = 0, marginRight = 0, marginBottom = 0, marginLeft = 0;
    bool marginLeftAuto = false, marginRightAuto = false;
    bool borderBox = false;
    bool positionFixed = false, positionAbsolute = false, positionRelative = false, positionSticky = false;
    float aspectRatio = 0.0f;
    wstring objectFit = L"fill";
    bool hasPosTop = false, hasPosRight = false, hasPosBottom = false, hasPosLeft = false;
    Length posTop = { Length::Auto, 0 }, posRight = { Length::Auto, 0 };
    Length posBottom = { Length::Auto, 0 }, posLeft = { Length::Auto, 0 };
    int paddingTop = 0, paddingRight = 0, paddingBottom = 0, paddingLeft = 0;
    int borderWidth = 0;
    COLORREF borderColor = CLR_INVALID;
    int borderRadius = 0;

    Length width = { Length::Auto, 0 };
    Length height = { Length::Auto, 0 };
    Length minWidth = { Length::Auto, 0 };
    Length maxWidth = { Length::Auto, 0 };
    Length minHeight = { Length::Auto, 0 };
    Length maxHeight = { Length::Auto, 0 };

    enum Display { Block, Inline, InlineBlock, Table, TableRow, TableCell, Flex, Grid, None } display = Inline;
    int floatDir = 0;
    int clear = 0;
    bool visibilityHidden = false;
    int flexDirection = 0;              // 0=row, 1=column
    int justifyContent = 0;             // start, end, center, between, around, evenly
    int alignItems = 0;                 // stretch, start, end, center
    int alignSelf = -1;                 // -1=auto, otherwise alignItems enum
    int alignContent = 0;               // start, end, center, stretch, between, around
    int justifyItems = 0;               // stretch, start, end, center
    bool flexWrap = false;
    float flexGrow = 0.0f, flexShrink = 1.0f;
    Length flexBasis = { Length::Auto, 0 };
    int gap = 0, rowGap = 0, columnGap = 0;
    int columnCount = 1;
    int order = 0;
    wstring gridTemplateColumns, gridTemplateRows, gridAutoFlow = L"row";
    int gridColumnStart = 0, gridRowStart = 0, gridColumnSpan = 1, gridRowSpan = 1;

    wstring listStyleType = L"disc";
};

struct AttributeSelector { wstring name, op, value; };

struct SimpleSelector {
    wstring tag;
    vector<AttributeSelector> attributes;
    wstring id;
    vector<wstring> classes;
    vector<wstring> pseudos;
    wstring pseudoElement;
};

struct ComplexSelector {
    vector<SimpleSelector> parts;
    vector<wchar_t> combinators;
    int specificity = 0;
};

struct CSSRule {
    ComplexSelector selector;
    vector<pair<wstring, wstring>> decls;
    bool important = false;
    wstring pseudoElement;
};

ComplexSelector ParseSelector(wstring s) {
    ComplexSelector sel;
    s = Trim(s);
    SimpleSelector cur;
    auto push = [&]() {
        if (!cur.tag.empty() || !cur.id.empty() || !cur.classes.empty() || !cur.attributes.empty() || !cur.pseudos.empty() || !cur.pseudoElement.empty()) {
            sel.parts.push_back(cur);
            cur = SimpleSelector();
        }
        };
    for (size_t i = 0; i < s.size(); i++) {
        wchar_t c = s[i];
        if (c == L' ' || c == L'>' || c == L'+') {
            push();
            if (c != L' ') sel.combinators.push_back(c);
            else if (i > 0 && i + 1 < s.size() && s[i + 1] != L'>' && s[i + 1] != L'+' && s[i - 1] != L'>' && s[i - 1] != L'+') sel.combinators.push_back(L' ');
        }
        else if (c == L'#') {
            size_t j = i + 1; while (j < s.size() && (iswalnum(s[j]) || s[j] == L'-' || s[j] == L'_')) j++;
            cur.id = s.substr(i + 1, j - i - 1); i = j - 1;
        }
        else if (c == L'.') {
            size_t j = i + 1; while (j < s.size() && (iswalnum(s[j]) || s[j] == L'-' || s[j] == L'_')) j++;
            cur.classes.push_back(s.substr(i + 1, j - i - 1)); i = j - 1;
        }
        else if (c == L'[') {
            size_t j = i + 1; wchar_t quote = 0;
            for (; j < s.size(); ++j) { if (quote) { if (s[j] == quote)quote = 0; } else if (s[j] == L'\'' || s[j] == L'"')quote = s[j]; else if (s[j] == L']')break; }
            wstring body = Trim(s.substr(i + 1, j - (i + 1))); AttributeSelector a;
            const wstring ops[] = { L"~=",L"|=",L"^=",L"$=",L"*=",L"=" }; size_t at = wstring::npos; size_t oplen = 0;
            for (const auto& op : ops) { size_t q = body.find(op); if (q != wstring::npos && (at == wstring::npos || q < at)) { at = q; oplen = op.size(); a.op = op; } }
            if (at == wstring::npos)a.name = Trim(body); else { a.name = Trim(body.substr(0, at)); a.value = Trim(body.substr(at + oplen)); if (a.value.size() >= 2 && ((a.value.front() == L'"' && a.value.back() == L'"') || (a.value.front() == L'\'' && a.value.back() == L'\'')))a.value = a.value.substr(1, a.value.size() - 2); }
            if (!a.name.empty())cur.attributes.push_back(a); i = j;
        }
        else if (c == L':') {
            bool element = (i + 1 < s.size() && s[i + 1] == L':');
            size_t b = i + (element ? 2 : 1), j = b;
            while (j < s.size() && (iswalnum(s[j]) || s[j] == L'-' || s[j] == L'_')) j++;
            wstring pseudoName = s.substr(b, j - b);
            if (j < s.size() && s[j] == L'(') { size_t k = j + 1; int depth = 1; wchar_t q = 0; for (; k < s.size() && depth; ++k) { if (q) { if (s[k] == q)q = 0; } else if (s[k] == L'\'' || s[k] == L'"')q = s[k]; else if (s[k] == L'(')++depth; else if (s[k] == L')')--depth; }if (depth == 0) { pseudoName += L"(" + s.substr(j + 1, k - j - 2) + L")"; j = k; } }
            if (element || pseudoName == L"before" || pseudoName == L"after") cur.pseudoElement = pseudoName;
            else cur.pseudos.push_back(pseudoName);
            i = j ? j - 1 : j;
        }
        else {
            cur.tag += c;
        }
    }
    push();
    vector<wchar_t> fixedComb;
    for (size_t i = 0; i < sel.combinators.size(); i++) {
        if (i < sel.parts.size() - 1) fixedComb.push_back(sel.combinators[i]);
    }
    while (fixedComb.size() < sel.parts.size() - 1) fixedComb.insert(fixedComb.begin(), L' ');
    sel.combinators = fixedComb;
    int spec = 0;
    for (auto& p : sel.parts) spec += p.id.empty() ? 0 : 100, spec += (int)p.classes.size() * 10, spec += (int)p.attributes.size() * 10, spec += (int)p.pseudos.size() * 10, spec += p.pseudoElement.empty() ? 0 : 1, spec += p.tag.empty() ? 0 : 1;
    sel.specificity = spec;
    return sel;
}

static size_t FindCSSMatchingBrace(const wstring& css, size_t openBrace) {
    int depth = 0;
    wchar_t quote = 0;
    bool escape = false;
    for (size_t i = openBrace; i < css.size(); ++i) {
        wchar_t c = css[i];
        if (quote) {
            if (escape) { escape = false; continue; }
            if (c == L'\\') { escape = true; continue; }
            if (c == quote) quote = 0;
            continue;
        }
        if (c == L'\'' || c == L'"') { quote = c; continue; }
        if (c == L'{') ++depth;
        else if (c == L'}' && --depth == 0) return i;
    }
    return wstring::npos;
}

static bool MediaFeatureMatches(const wstring& condition, int viewportWidth, int viewportHeight) {
    wstring q = Lower(Trim(condition));
    if (q.empty()) return true;
    if (q.find(L"print") != wstring::npos && q.find(L"screen") == wstring::npos) return false;
    if (q.find(L"not all") != wstring::npos) return false;
    auto readPx = [&](const wstring& feature, int actual) {
        std::wregex re(L"\\(" + feature + L"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)(px|em|rem)?\\)");
        std::wsmatch m;
        if (!std::regex_search(q, m, re)) return true;
        double value = std::wcstod(m[1].str().c_str(), nullptr);
        wstring unit = m.size() > 2 ? m[2].str() : L"px";
        if (unit == L"em" || unit == L"rem") value *= 16.0;
        if (feature == L"min-width" || feature == L"min-height") return actual >= (int)value;
        return actual <= (int)value;
        };
    if (!readPx(L"min-width", viewportWidth) || !readPx(L"max-width", viewportWidth) ||
        !readPx(L"min-height", viewportHeight) || !readPx(L"max-height", viewportHeight)) return false;
    if (q.find(L"(orientation: portrait)") != wstring::npos && viewportHeight < viewportWidth) return false;
    if (q.find(L"(orientation: landscape)") != wstring::npos && viewportWidth < viewportHeight) return false;
    return true;
}

static void FlattenCSSAtRules(wstring& css, int viewportWidth, int viewportHeight) {
    // Remove unsupported @import/@font-face blocks later; unwrap @supports and
    // evaluate @media against the actual viewport instead of applying mobile CSS
    // unconditionally on desktop.
    for (;;) {
        size_t media = css.find(L"@media");
        size_t supports = css.find(L"@supports");
        size_t at = media == wstring::npos ? supports : supports == wstring::npos ? media : min(media, supports);
        if (at == wstring::npos) break;
        bool isMedia = (at == media);
        size_t keywordLen = isMedia ? 6 : 9;
        size_t open = css.find(L'{', at + keywordLen);
        if (open == wstring::npos) { css.erase(at); continue; }
        size_t close = FindCSSMatchingBrace(css, open);
        if (close == wstring::npos) { css.erase(at); continue; }
        wstring condition = Trim(css.substr(at + keywordLen, open - (at + keywordLen)));
        wstring inner = css.substr(open + 1, close - open - 1);
        bool include = !isMedia || MediaFeatureMatches(condition, viewportWidth, viewportHeight);
        css.replace(at, close - at + 1, include ? inner : L"");
    }
}

static size_t FindCSSTopLevel(const wstring& text, wchar_t target, size_t start = 0) {
    int depth = 0; wchar_t quote = 0; bool escape = false;
    for (size_t i = start; i < text.size(); ++i) { wchar_t c = text[i]; if (quote) { if (escape) { escape = false; continue; }if (c == L'\\') { escape = true; continue; }if (c == quote)quote = 0; continue; }if (c == L'\'' || c == L'"') { quote = c; continue; }if (c == L'(' || c == L'[')++depth; else if (c == L')' || c == L']')depth = max(0, depth - 1); else if (c == target && depth == 0)return i; }
    return wstring::npos;
}

vector<CSSRule> ParseCSS(wstring css, int viewportWidth = 1200, int viewportHeight = 800) {
    for (;;) {
        size_t s = css.find(L"/*"); if (s == wstring::npos) break;
        size_t e = css.find(L"*/", s + 2); if (e == wstring::npos) { css.resize(s); break; }
        css.erase(s, e - s + 2);
    }
    FlattenCSSAtRules(css, viewportWidth, viewportHeight);

    vector<CSSRule> rules;
    size_t pos = 0;
    while ((pos = css.find(L'{', pos)) != wstring::npos) {
        size_t end = css.find(L'}', pos + 1);
        if (end == wstring::npos) break;
        size_t start = css.rfind(L'}', pos);
        if (start == wstring::npos) start = 0; else start++;
        wstring sels = Trim(css.substr(start, pos - start));
        wstring body = css.substr(pos + 1, end - pos - 1);
        pos = end + 1;
        if (sels.empty() || sels[0] == L'@') continue;

        size_t comma = 0;
        while (comma < sels.size()) {
            size_t next = FindCSSTopLevel(sels, L',', comma); if (next == wstring::npos) next = sels.size();
            wstring selStr = Trim(sels.substr(comma, next - comma));
            if (!selStr.empty()) {
                CSSRule r;
                r.selector = ParseSelector(selStr);
                if (!r.selector.parts.empty()) r.pseudoElement = r.selector.parts.back().pseudoElement;
                size_t d = 0;
                while (d < body.size()) {
                    size_t semi = FindCSSTopLevel(body, L';', d); if (semi == wstring::npos) semi = body.size();
                    wstring decl = Trim(body.substr(d, semi - d));
                    size_t colon = FindCSSTopLevel(decl, L':');
                    if (colon != wstring::npos) {
                        wstring k = Lower(Trim(decl.substr(0, colon)));
                        wstring v = Trim(decl.substr(colon + 1));
                        bool imp = false;
                        size_t impPos = Lower(v).find(L"!important");
                        if (impPos != wstring::npos) { imp = true; v = Trim(v.substr(0, impPos)); }
                        r.decls.push_back({ k, v });
                        r.important = r.important || imp;
                    }
                    d = semi + 1;
                }
                rules.push_back(r);
            }
            comma = next + 1;
        }
    }
    return rules;
}

bool MatchesSimple(Node* node, const SimpleSelector& sel) {
    if (node->tag[0] == L'#') return false;
    if (!sel.tag.empty() && sel.tag != L"*" && node->tag != sel.tag) return false;
    if (!sel.id.empty() && GetAttribute(node, L"id") != sel.id) return false;
    // Match every class as a whitespace-delimited token. Without this check,
    // a selector such as .grid would accidentally match every element.
    if (!sel.classes.empty()) {
        const wstring classValue = GetAttribute(node, L"class");
        for (const auto& wanted : sel.classes) {
            bool found = false;
            size_t at = 0;
            while (at < classValue.size()) {
                while (at < classValue.size() && iswspace(classValue[at])) ++at;
                size_t end = at;
                while (end < classValue.size() && !iswspace(classValue[end])) ++end;
                if (end > at && classValue.substr(at, end - at) == wanted) { found = true; break; }
                at = end;
            }
            if (!found) return false;
        }
    }
    for (const auto& a : sel.attributes) {
        auto it = node->attributes.find(a.name); if (it == node->attributes.end())return false;
        if (a.op.empty())continue; wstring actual = it->second, expected = a.value;
        if (a.op == L"=" && actual != expected)return false;
        if (a.op == L"~=" && ((L" " + actual + L" ").find(L" " + expected + L" ") == wstring::npos))return false;
        if (a.op == L"|=" && actual != expected && actual.find(expected + L"-") != 0)return false;
        if (a.op == L"^=" && actual.find(expected) != 0)return false;
        if (a.op == L"$=" && (actual.size() < expected.size() || actual.substr(actual.size() - expected.size()) != expected))return false;
        if (a.op == L"*=" && actual.find(expected) == wstring::npos)return false;
    }
    for (auto& p : sel.pseudos) {
        if (p == L"first-child" || p == L"last-child" || p == L"only-child") {
            if (!node->parent)return false; vector<Node*> elems; for (Node* x : node->parent->children)if (x->tag != L"#text")elems.push_back(x);
            if (p == L"first-child" && !elems.empty() && elems.front() != node)return false;
            if (p == L"last-child" && !elems.empty() && elems.back() != node)return false;
            if (p == L"only-child" && elems.size() != 1)return false;
        }
        if (p == L"root" && node->tag != L"html") return false;
        if (p == L"empty") { for (Node* c : node->children)if (c->tag != L"#text" || !Trim(c->text).empty())return false; }
        if (p == L"disabled" && !node->attributes.count(L"disabled")) return false;
        if (p == L"enabled" && node->attributes.count(L"disabled")) return false;
        if (p == L"checked" && !node->attributes.count(L"checked") && !node->attributes.count(L"selected")) return false;
        if (p == L"required" && !node->attributes.count(L"required")) return false;
        if (p == L"hover" || p == L"active" || p == L"focus" || p == L"focus-visible" || p == L"focus-within") return false;
        if (p.find(L"not(") == 0 || p.find(L"is(") == 0 || p.find(L"where(") == 0) {
            size_t b = p.find(L'('), e = p.rfind(L')'); if (e == wstring::npos || e <= b)return false; wstring inner = p.substr(b + 1, e - b - 1); bool any = false; size_t at = 0;
            while (at <= inner.size()) { size_t comma = inner.find(L',', at); if (comma == wstring::npos)comma = inner.size(); ComplexSelector sub = ParseSelector(Trim(inner.substr(at, comma - at))); if (!sub.parts.empty() && MatchesSimple(node, sub.parts.back()))any = true; if (comma == inner.size())break; at = comma + 1; }
            if (p.find(L"not(") == 0 ? any : !any)return false;
        }
        if (p.find(L"nth-child(") == 0 || p.find(L"nth-of-type(") == 0) {
            if (!node->parent)return false; bool ofType = p.find(L"nth-of-type(") == 0; size_t b = p.find(L'('), e = p.find(L')', b); if (e == wstring::npos)return false; wstring formula = Lower(Trim(p.substr(b + 1, e - b - 1))); int idx = 0;
            for (Node* x : node->parent->children) { if (x->tag == L"#text")continue; if (!ofType || x->tag == node->tag)++idx; if (x == node)break; }
            bool match = false; if (formula == L"odd")match = (idx % 2) == 1; else if (formula == L"even")match = (idx % 2) == 0; else { size_t npos = formula.find(L'n'); if (npos == wstring::npos) { try { match = idx == stoi(formula); } catch (...) {} } else { int a = 1, bias = 0; try { if (npos > 0 && formula.substr(0, npos) != L"+")a = stoi(formula.substr(0, npos)); } catch (...) {}try { if (npos + 1 < formula.size())bias = stoi(formula.substr(npos + 1)); } catch (...) {}if (a == 0)match = idx == bias; else { int d = idx - bias; match = (d * a >= 0 && d % a == 0); } } }if (!match)return false;
        }
    }
    return true;
}

bool Matches(Node* node, const ComplexSelector& sel, int partIdx) {
    if (partIdx < 0) return true;
    if (!MatchesSimple(node, sel.parts[partIdx])) return false;
    if (partIdx == 0) return true;
    wchar_t comb = sel.combinators[partIdx - 1];
    if (comb == L'>') {
        return node->parent && Matches(node->parent, sel, partIdx - 1);
    }
    else if (comb == L' ') {
        Node* p = node->parent;
        while (p) { if (Matches(p, sel, partIdx - 1)) return true; p = p->parent; }
        return false;
    }
    else if (comb == L'+') {
        if (!node->parent) return false;
        auto& siblings = node->parent->children;
        for (size_t i = 1; i < siblings.size(); i++) {
            if (siblings[i] == node) return Matches(siblings[i - 1], sel, partIdx - 1);
        }
        return false;
    }
    return false;
}

unordered_map<wstring, wstring> g_cssVars;
wstring ResolveVars(wstring v) {
    size_t pos;
    while ((pos = v.find(L"var(")) != wstring::npos) {
        size_t end = v.find(L')', pos);
        if (end == wstring::npos) break;
        wstring varName = Trim(v.substr(pos + 4, end - pos - 4));
        if (g_cssVars.count(varName)) v.replace(pos, end - pos + 1, g_cssVars[varName]);
        else v.erase(pos, end - pos + 1);
    }
    return v;
}

void ApplyDecls(Node* node, Style& s, const vector<pair<wstring, wstring>>& decls, int inheritedFontSize = -1) {
    for (auto& kv : decls) {
        wstring k = kv.first;
        wstring v = ResolveVars(kv.second);
        // font-size em is relative to the parent's computed size; other em
        // dimensions and line-height are relative to this element's font size.
        int baseSize = (k == L"font-size" && inheritedFontSize >= 0)
            ? inheritedFontSize : max(1, s.fontSize);
        if (k == L"--") { g_cssVars[L"--"] = v; continue; }
        if (k.size() > 2 && k.substr(0, 2) == L"--") { g_cssVars[k] = v; continue; }

        if (k == L"position") {
            wstring p = Lower(Trim(v));
            s.positionFixed = p == L"fixed";
            s.positionAbsolute = p == L"absolute";
            s.positionRelative = p == L"relative";
        }
        else if (k == L"top") { s.posTop = ParseLength(v); s.hasPosTop = s.posTop.kind != Length::Auto; }
        else if (k == L"right") { s.posRight = ParseLength(v); s.hasPosRight = s.posRight.kind != Length::Auto; }
        else if (k == L"bottom") { s.posBottom = ParseLength(v); s.hasPosBottom = s.posBottom.kind != Length::Auto; }
        else if (k == L"left") { s.posLeft = ParseLength(v); s.hasPosLeft = s.posLeft.kind != Length::Auto; }
        else if (k == L"inset") {
            auto p = SplitValue(v); if (!p.empty()) {
                Length t = ParseLength(p[0]), r = p.size() > 1 ? ParseLength(p[1]) : t;
                Length b = p.size() > 2 ? ParseLength(p[2]) : t, l = p.size() > 3 ? ParseLength(p[3]) : r;
                s.posTop = t; s.posRight = r; s.posBottom = b; s.posLeft = l;
                s.hasPosTop = t.kind != Length::Auto; s.hasPosRight = r.kind != Length::Auto;
                s.hasPosBottom = b.kind != Length::Auto; s.hasPosLeft = l.kind != Length::Auto;
            }
        }
        else if (k == L"color") s.color = ParseColor(v, s.color);
        else if (k == L"background" || k == L"background-color") s.background = ParseColor(v, s.background);
        else if (k == L"font-size") { Length l = ParseLength(v); s.fontSize = l.kind == Length::Px ? (int)l.value : l.Resolve(baseSize, g_rootFontSize); s.fontSize = max(6, min(150, s.fontSize)); }
        else if (k == L"font-weight") { wstring x = Lower(v); s.bold = (x == L"bold" || x == L"bolder" || x == L"600" || x == L"700" || x == L"800" || x == L"900"); }
        else if (k == L"font-style") s.italic = (Lower(v) == L"italic");
        else if (k == L"font") {
            // Common shorthand subset: [style] [weight] size[/line-height] family.
            vector<wstring> parts = SplitValue(v);
            size_t sizeIndex = parts.size();
            for (size_t i = 0; i < parts.size(); ++i) {
                wstring t = Lower(parts[i]);
                if (!t.empty() && (iswdigit(t[0]) || t[0] == L'.')) { sizeIndex = i; break; }
            }
            if (sizeIndex < parts.size()) {
                for (size_t i = 0; i < sizeIndex; ++i) {
                    wstring t = Lower(parts[i]);
                    if (t == L"italic" || t == L"oblique") s.italic = true;
                    if (t == L"bold" || t == L"bolder" || t == L"600" || t == L"700" || t == L"800" || t == L"900") s.bold = true;
                }
                wstring sizeToken = parts[sizeIndex];
                size_t slash = sizeToken.find(L'/');
                wstring sizePart = slash == wstring::npos ? sizeToken : sizeToken.substr(0, slash);
                Length fs = ParseLength(sizePart);
                if (fs.kind != Length::Auto) s.fontSize = max(6, min(150, fs.Resolve(baseSize, g_rootFontSize)));
                if (slash != wstring::npos) {
                    wstring lh = sizeToken.substr(slash + 1);
                    try { s.lineHeight = lh.find(L"px") != wstring::npos ? (int)stof(lh) : (int)(s.fontSize * stof(lh)); }
                    catch (...) {}
                }
                size_t familyPos = v.find(sizeToken);
                if (familyPos != wstring::npos) {
                    wstring family = Trim(v.substr(familyPos + sizeToken.size()));
                    if (!family.empty()) s.fontFamily = family;
                }
            }
        }
        else if (k == L"font-family") {
            wstring val = v;
            size_t c = val.find(L',');
            if (c != wstring::npos) val = val.substr(0, c);
            val = Trim(val);
            if (val.size() >= 2 && (val.front() == L'"' || val.front() == L'\'') && (val.back() == L'"' || val.back() == L'\'')) val = val.substr(1, val.size() - 2);
            s.fontFamily = val;
        }
        else if (k == L"text-decoration") s.underline = Lower(v).find(L"underline") != wstring::npos;
        else if (k == L"text-align") s.textAlign = Lower(v) == L"center" ? TA_CENTER : Lower(v) == L"right" ? TA_RIGHT : TA_LEFT;
        else if (k == L"text-transform") s.textTransform = Lower(v);
        else if (k == L"white-space") s.whiteSpace = Lower(v);
        else if (k == L"line-height") {
            wstring val = Lower(v);
            if (val.empty()) continue;
            if (val.back() == L'%') s.lineHeight = (int)(baseSize * stof(val.substr(0, val.size() - 1)) / 100.0f);
            else if (val.find(L"px") != wstring::npos) s.lineHeight = (int)stof(val);
            else {
                float mult = 1.2f;
                try { mult = stof(val); }
                catch (...) {}
                s.lineHeight = (int)(baseSize * mult);
            }
        }
        else if (k == L"display") { wstring x = Lower(v); s.display = x == L"block" ? Style::Block : (x == L"flex" || x == L"inline-flex") ? Style::Flex : (x == L"grid" || x == L"inline-grid") ? Style::Grid : x == L"table" ? Style::Table : x == L"table-row" ? Style::TableRow : x == L"table-cell" ? Style::TableCell : x == L"none" ? Style::None : x == L"inline-block" ? Style::InlineBlock : Style::Inline; }
        else if (k == L"visibility") s.visibilityHidden = (Lower(v) == L"hidden");
        else if (k == L"float") s.floatDir = Lower(v) == L"left" ? 1 : Lower(v) == L"right" ? 2 : 0;
        else if (k == L"clear") s.clear = Lower(v) == L"both" ? 3 : Lower(v) == L"left" ? 1 : Lower(v) == L"right" ? 2 : 0;
        else if (k == L"width") s.width = ParseLength(v);
        else if (k == L"height") s.height = ParseLength(v);
        else if (k == L"min-width") s.minWidth = ParseLength(v);
        else if (k == L"max-width") s.maxWidth = ParseLength(v);
        else if (k == L"min-height") s.minHeight = ParseLength(v);
        else if (k == L"max-height") s.maxHeight = ParseLength(v);
        else if (k == L"box-sizing") s.borderBox = (Lower(v) == L"border-box");
        else if (k == L"margin") {
            auto p = SplitValue(v);
            auto setH = [&](const wstring& q, bool left) { bool a = Lower(q) == L"auto"; if (left) { s.marginLeftAuto = a; s.marginLeft = a ? 0 : ParseLength(q).Resolve(baseSize, g_rootFontSize); } else { s.marginRightAuto = a; s.marginRight = a ? 0 : ParseLength(q).Resolve(baseSize, g_rootFontSize); } };
            if (p.size() == 1) { s.marginTop = s.marginBottom = ParseLength(p[0]).Resolve(baseSize, g_rootFontSize); setH(p[0], true); setH(p[0], false); }
            else if (p.size() == 2) { s.marginTop = s.marginBottom = ParseLength(p[0]).Resolve(baseSize, g_rootFontSize); setH(p[1], true); setH(p[1], false); }
            else if (p.size() == 3) { s.marginTop = ParseLength(p[0]).Resolve(baseSize, g_rootFontSize); setH(p[1], true); setH(p[1], false); s.marginBottom = ParseLength(p[2]).Resolve(baseSize, g_rootFontSize); }
            else if (p.size() >= 4) { s.marginTop = ParseLength(p[0]).Resolve(baseSize, g_rootFontSize); setH(p[1], false); s.marginBottom = ParseLength(p[2]).Resolve(baseSize, g_rootFontSize); setH(p[3], true); }
        }
        else if (k == L"margin-top") s.marginTop = ParseLength(v).Resolve(baseSize, g_rootFontSize);
        else if (k == L"margin-right") { s.marginRightAuto = Lower(v) == L"auto"; s.marginRight = s.marginRightAuto ? 0 : ParseLength(v).Resolve(baseSize, g_rootFontSize); }
        else if (k == L"margin-bottom") s.marginBottom = ParseLength(v).Resolve(baseSize, g_rootFontSize);
        else if (k == L"margin-left") { s.marginLeftAuto = Lower(v) == L"auto"; s.marginLeft = s.marginLeftAuto ? 0 : ParseLength(v).Resolve(baseSize, g_rootFontSize); }
        else if (k == L"padding") { auto p = SplitValue(v); if (p.size() == 1) s.paddingTop = s.paddingRight = s.paddingBottom = s.paddingLeft = ParseLength(p[0]).Resolve(baseSize, g_rootFontSize); else if (p.size() == 2) { s.paddingTop = s.paddingBottom = ParseLength(p[0]).Resolve(baseSize, g_rootFontSize); s.paddingRight = s.paddingLeft = ParseLength(p[1]).Resolve(baseSize, g_rootFontSize); } else if (p.size() == 3) { s.paddingTop = ParseLength(p[0]).Resolve(baseSize, g_rootFontSize); s.paddingRight = s.paddingLeft = ParseLength(p[1]).Resolve(baseSize, g_rootFontSize); s.paddingBottom = ParseLength(p[2]).Resolve(baseSize, g_rootFontSize); } else if (p.size() >= 4) { s.paddingTop = ParseLength(p[0]).Resolve(baseSize, g_rootFontSize); s.paddingRight = ParseLength(p[1]).Resolve(baseSize, g_rootFontSize); s.paddingBottom = ParseLength(p[2]).Resolve(baseSize, g_rootFontSize); s.paddingLeft = ParseLength(p[3]).Resolve(baseSize, g_rootFontSize); } }
        else if (k == L"padding-top") s.paddingTop = ParseLength(v).Resolve(baseSize, g_rootFontSize);
        else if (k == L"padding-right") s.paddingRight = ParseLength(v).Resolve(baseSize, g_rootFontSize);
        else if (k == L"padding-bottom") s.paddingBottom = ParseLength(v).Resolve(baseSize, g_rootFontSize);
        else if (k == L"padding-left") s.paddingLeft = ParseLength(v).Resolve(baseSize, g_rootFontSize);
        else if (k == L"border") {
            auto parts = SplitValue(v); s.borderWidth = 1;
            for (const auto& part0 : parts) {
                wstring part = Lower(part0); if (part == L"none" || part == L"hidden") { s.borderWidth = 0; continue; }
                Length width = ParseLength(part); if (width.kind != Length::Auto && width.value >= 0 && (part.find(L"px") != wstring::npos || part == L"0")) { s.borderWidth = max(0, width.Resolve(baseSize, g_rootFontSize)); continue; }
                COLORREF parsed = ParseColor(part, CLR_INVALID); if (parsed != CLR_INVALID)s.borderColor = parsed;
            }
            if (s.borderColor == CLR_INVALID)s.borderColor = RGB(0, 0, 0);
        }
        else if (k == L"border-width") { s.borderWidth = max(0, ParseLength(v).Resolve(baseSize, g_rootFontSize)); }
        else if (k == L"border-color") { COLORREF c = ParseColor(v, CLR_INVALID); if (c != CLR_INVALID)s.borderColor = c; }
        else if (k == L"border-style") { if (Lower(v) == L"none" || Lower(v) == L"hidden")s.borderWidth = 0; else if (s.borderWidth == 0)s.borderWidth = 1; }
        else if (k == L"border-radius") s.borderRadius = ParseLength(v).Resolve(baseSize, g_rootFontSize);
        else if (k == L"list-style-type" || k == L"list-style") s.listStyleType = Lower(v);
        else if (k == L"column-count" || k == L"-webkit-column-count" || k == L"-moz-column-count") { try { s.columnCount = max(1, min(8, stoi(v))); } catch (...) {} }
        else if (k == L"columns" || k == L"-webkit-columns" || k == L"-moz-columns") {
            auto parts = SplitValue(v);
            for (const auto& part : parts) { try { s.columnCount = max(1, min(8, stoi(part))); break; } catch (...) {} }
        }
        else if (k == L"flex-direction") { wstring x = Lower(v); s.flexDirection = (x == L"column" || x == L"column-reverse") ? 1 : 0; }
        else if (k == L"justify-content") { wstring x = Lower(v); s.justifyContent = x == L"flex-end" || x == L"end" ? 1 : x == L"center" ? 2 : x == L"space-between" ? 3 : x == L"space-around" ? 4 : x == L"space-evenly" ? 5 : 0; }
        else if (k == L"align-items") { wstring x = Lower(v); s.alignItems = x == L"flex-start" || x == L"start" ? 1 : x == L"flex-end" || x == L"end" ? 2 : x == L"center" ? 3 : 0; }
        else if (k == L"align-self") { wstring x = Lower(v); s.alignSelf = x == L"flex-start" || x == L"start" ? 1 : x == L"flex-end" || x == L"end" ? 2 : x == L"center" ? 3 : x == L"stretch" ? 0 : -1; }
        else if (k == L"align-content") { wstring x = Lower(v); s.alignContent = x == L"flex-end" || x == L"end" ? 1 : x == L"center" ? 2 : x == L"stretch" ? 3 : x == L"space-between" ? 4 : x == L"space-around" ? 5 : 0; }
        else if (k == L"justify-items" || k == L"justify-self") { wstring x = Lower(v); s.justifyItems = x == L"end" || x == L"flex-end" ? 2 : x == L"center" ? 3 : x == L"start" || x == L"flex-start" ? 1 : 0; }
        else if (k == L"place-items") { auto p = SplitValue(v); if (!p.empty()) { wstring a = Lower(p[0]); s.alignItems = a == L"end" ? 2 : a == L"center" ? 3 : a == L"start" ? 1 : 0; wstring j = p.size() > 1 ? Lower(p[1]) : a; s.justifyItems = j == L"end" ? 2 : j == L"center" ? 3 : j == L"start" ? 1 : 0; } }
        else if (k == L"place-content") { auto p = SplitValue(v); if (!p.empty()) { wstring a = Lower(p[0]); s.alignContent = a == L"end" ? 1 : a == L"center" ? 2 : a == L"stretch" ? 3 : a == L"space-between" ? 4 : a == L"space-around" ? 5 : 0; wstring j = p.size() > 1 ? Lower(p[1]) : a; s.justifyContent = j == L"end" ? 1 : j == L"center" ? 2 : j == L"space-between" ? 3 : j == L"space-around" ? 4 : j == L"space-evenly" ? 5 : 0; } }
        else if (k == L"flex-wrap") s.flexWrap = Lower(v) != L"nowrap";
        else if (k == L"flex-grow") { try { s.flexGrow = std::max(0.0f, stof(v)); } catch (...) {} }
        else if (k == L"flex-shrink") { try { s.flexShrink = std::max(0.0f, stof(v)); } catch (...) {} }
        else if (k == L"flex-basis") s.flexBasis = ParseLength(v);
        else if (k == L"flex") { auto p = SplitValue(v); if (!p.empty()) { if (p[0] == L"none") { s.flexGrow = 0; s.flexShrink = 0; s.flexBasis = { Length::Auto,0 }; } else { try { s.flexGrow = stof(p[0]); } catch (...) {} if (p.size() > 1) { try { s.flexShrink = stof(p[1]); } catch (...) {} } if (p.size() > 2)s.flexBasis = ParseLength(p[2]); else if (p.size() == 1)s.flexBasis = { Length::Percent,0 }; } } }
        else if (k == L"gap" || k == L"row-gap" || k == L"column-gap") { auto p = SplitValue(v); int a = p.empty() ? 0 : std::max(0, ParseLength(p[0]).Resolve(baseSize, g_rootFontSize)); int b = p.size() > 1 ? std::max(0, ParseLength(p[1]).Resolve(baseSize, g_rootFontSize)) : a; if (k == L"gap") { s.rowGap = a; s.columnGap = b; s.gap = a; } else if (k == L"row-gap")s.rowGap = a; else s.columnGap = a; }
        else if (k == L"order") { try { s.order = stoi(v); } catch (...) {} }
        else if (k == L"grid-template-columns") s.gridTemplateColumns = Trim(v);
        else if (k == L"grid-template-rows") s.gridTemplateRows = Trim(v);
        else if (k == L"grid-template") { size_t slash = v.find(L'/'); if (slash != wstring::npos) { s.gridTemplateRows = Trim(v.substr(0, slash)); s.gridTemplateColumns = Trim(v.substr(slash + 1)); } else if (Lower(Trim(v)) == L"none") { s.gridTemplateRows.clear(); s.gridTemplateColumns.clear(); } }
        else if (k == L"grid-auto-flow") s.gridAutoFlow = Lower(Trim(v));
        else if (k == L"grid-column" || k == L"grid-row") { size_t slash = v.find(L'/'); wstring a = Trim(slash == wstring::npos ? v : v.substr(0, slash)), b = slash == wstring::npos ? wstring() : Trim(v.substr(slash + 1)); int start = 0, span = 1; if (a.find(L"span") == 0) { try { span = max(1, stoi(Trim(a.substr(4)))); } catch (...) {} } else { try { start = stoi(a); } catch (...) {} } if (!b.empty()) { if (Lower(b).find(L"span") == 0) { try { span = max(1, stoi(Trim(b.substr(4)))); } catch (...) {} } else { try { int end = stoi(b); if (start > 0)span = end == -1 ? max(1, 32 - start) : max(1, end - start); } catch (...) {} } } if (k == L"grid-column") { s.gridColumnStart = start; s.gridColumnSpan = span; } else { s.gridRowStart = start; s.gridRowSpan = span; } }
        else if (k == L"grid-column-start") { try { s.gridColumnStart = stoi(v); } catch (...) {} }
        else if (k == L"grid-row-start") { try { s.gridRowStart = stoi(v); } catch (...) {} }
        else if (k == L"grid-column-end" || k == L"grid-row-end") { wstring x = Lower(Trim(v)); if (x.find(L"span") == 0) { try { int n = stoi(Trim(x.substr(4))); if (k == L"grid-column-end")s.gridColumnSpan = max(1, n); else s.gridRowSpan = max(1, n); } catch (...) {} } }
    }
}

// CSS Grid track parser: supports px/em/rem/%/auto/fr, repeat(N, track),
// and the common minmax(0, 1fr) / minmax(auto, 1fr) pattern.
struct GridTrack { int fixed = 0; float fr = 0.0f; bool automatic = false; };
static vector<wstring> SplitCSSWhitespace(const wstring& input) {
    vector<wstring> out; wstring token; int depth = 0; wchar_t quote = 0;
    for (wchar_t c : input) { if (quote) { token += c; if (c == quote)quote = 0; continue; }if (c == L'\'' || c == L'"') { quote = c; token += c; continue; }if (c == L'(')++depth; else if (c == L')')--depth; if (iswspace(c) && depth == 0) { if (!token.empty()) { out.push_back(token); token.clear(); } } else token += c; }
    if (!token.empty())out.push_back(token); return out;
}
static vector<GridTrack> ParseGridTracks(const wstring& raw, int available) {
    vector<GridTrack> out; vector<wstring> tokens = SplitCSSWhitespace(raw);
    for (const wstring& token0 : tokens) {
        wstring token = Lower(Trim(token0));
        if (token.find(L"repeat(") == 0 && token.back() == L')') {
            wstring inside = token.substr(7, token.size() - 8); size_t comma = inside.find(L','); if (comma != wstring::npos) { wstring countText = Lower(Trim(inside.substr(0, comma))); int count = 1; wstring repeated = Trim(inside.substr(comma + 1)); if (countText == L"auto-fit" || countText == L"auto-fill") { size_t mb = repeated.find(L"minmax("); if (mb != wstring::npos) { size_t me = repeated.find(L')', mb); wstring args = repeated.substr(mb + 7, me == wstring::npos ? wstring::npos : me - mb - 7); size_t mc = args.find(L','); Length minTrack = ParseLength(Trim(args.substr(0, mc))); int minPx = max(1, minTrack.Resolve(available, g_rootFontSize)); count = max(1, min(32, available / minPx)); } } else { try { count = max(1, min(32, stoi(countText))); } catch (...) {} }for (int i = 0; i < count; ++i) { auto one = ParseGridTracks(repeated, available); out.insert(out.end(), one.begin(), one.end()); }continue; }
        }
        if (token.find(L"minmax(") == 0 && token.back() == L')') { wstring inside = token.substr(7, token.size() - 8); size_t comma = inside.rfind(L','); if (comma != wstring::npos)token = Trim(inside.substr(comma + 1)); }
        GridTrack t;
        if (token == L"auto" || token == L"min-content" || token == L"max-content")t.automatic = true;
        else if (token.size() >= 2 && token.substr(token.size() - 2) == L"fr") { try { t.fr = max(0.0f, stof(token.substr(0, token.size() - 2))); } catch (...) { t.fr = 1.0f; } }
        else { Length l = ParseLength(token); if (l.kind != Length::Auto)t.fixed = max(0, l.Resolve(available, g_rootFontSize)); else t.automatic = true; }
        out.push_back(t);
    }
    if (out.empty())out.push_back(GridTrack{ 0,1.0f,false });
    return out;
}
static vector<int> ResolveGridTracks(const vector<GridTrack>& tracks, int available, int gap) {
    int gaps = max(0, (int)tracks.size() - 1) * gap; int fixed = gaps; float fr = 0; int autos = 0;
    for (const auto& t : tracks) { fixed += t.fixed; fr += t.fr; if (t.automatic)++autos; }
    int remaining = max(0, available - fixed); if (fr <= 0 && autos > 0)fr = (float)autos;
    vector<int> sizes; sizes.reserve(tracks.size());
    for (const auto& t : tracks) { int n = t.fixed; if (t.fr > 0 && fr > 0)n += (int)std::floor(remaining * t.fr / fr); else if (t.automatic && fr > 0)n += (int)std::floor(remaining / fr); sizes.push_back(max(0, n)); }
    int used = 0; for (int n : sizes)used += n; int extra = max(0, available - gaps - used); for (size_t i = 0; i < sizes.size() && extra>0; ++i) { ++sizes[i]; --extra; }
    return sizes;
}

// ============================================================
// 4. FONTS & LAYOUT
// ============================================================
struct FontKey {
    int size; bool b, i, u; wstring f;
    bool operator==(const FontKey& o) const { return size == o.size && b == o.b && i == o.i && u == o.u && f == o.f; }
};
struct FontKeyHash {
    size_t operator()(const FontKey& k) const {
        size_t h = hash<int>()(k.size); h ^= hash<bool>()(k.b) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= hash<bool>()(k.i) + 0x9e3779b9 + (h << 6) + (h >> 2); h ^= hash<bool>()(k.u) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= hash<wstring>()(k.f) + 0x9e3779b9 + (h << 6) + (h >> 2); return h;
    }
};

struct FontCache {
    unordered_map<FontKey, HFONT, FontKeyHash> fonts;
    HFONT Get(const Style& s) {
        FontKey k{ s.fontSize, s.bold, s.italic, s.underline, s.fontFamily };
        auto it = fonts.find(k); if (it != fonts.end()) return it->second;
        HFONT f = CreateFontW(-s.fontSize, 0, 0, 0, s.bold ? FW_BOLD : FW_NORMAL, s.italic ? TRUE : FALSE, FALSE, s.underline ? TRUE : FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, s.fontFamily.c_str());
        fonts[k] = f; return f;
    }
    ~FontCache() { for (auto& p : fonts) DeleteObject(p.second); }
};

struct Fragment {
    wstring text;
    Style style;
    wstring href;
    int x = 0, y = 0, w = 0, h = 0;
    Bitmap* img = nullptr;
    Node* linkNode = nullptr;
    Node* controlNode = nullptr;
};

struct LayoutBox {
    Node* node = nullptr;
    Style style;
    int x = 0, y = 0, w = 0, h = 0;
    vector<LayoutBox> children;
    vector<Fragment> frags;
    bool isHr = false, isList = false;
};

static void OffsetLayoutBox(LayoutBox& box, int dx, int dy) {
    box.x += dx; box.y += dy;
    for (auto& f : box.frags) { f.x += dx; f.y += dy; }
    for (auto& child : box.children) OffsetLayoutBox(child, dx, dy);
}

static void CollectNodeText(Node* node, wstring& out) {
    if (!node) return;
    if (node->tag == L"#text") { out += node->text; return; }
    for (Node* child : node->children) CollectNodeText(child, out);
}

int MeasureText(HDC dc, FontCache& fc, const Style& s, const wstring& txt) {
    HGDIOBJ old = SelectObject(dc, fc.Get(s));
    SIZE sz; GetTextExtentPoint32W(dc, txt.c_str(), (int)txt.size(), &sz);
    SelectObject(dc, old); return sz.cx;
}

Style ComputeStyle(Node* node, const Style& parent, const vector<CSSRule>& rules, bool isRoot) {
    Style s = parent;
    const int inheritedFontSize = parent.fontSize;
    s.marginTop = s.marginRight = s.marginBottom = s.marginLeft = 0;
    s.marginLeftAuto = s.marginRightAuto = false;
    s.paddingTop = s.paddingRight = s.paddingBottom = s.paddingLeft = 0;
    s.background = CLR_INVALID; s.borderWidth = 0; s.floatDir = 0; s.clear = 0;
    s.width = Length{ Length::Auto, 0 }; s.height = Length{ Length::Auto, 0 };
    s.minWidth = Length{ Length::Auto, 0 }; s.maxWidth = Length{ Length::Auto, 0 };
    s.minHeight = Length{ Length::Auto, 0 }; s.maxHeight = Length{ Length::Auto, 0 };
    s.borderBox = false;
    s.positionFixed = false; s.hasPosTop = s.hasPosRight = s.hasPosBottom = s.hasPosLeft = false;
    s.posTop = s.posRight = s.posBottom = s.posLeft = Length{ Length::Auto,0 };
    s.display = Style::Inline; s.visibilityHidden = false;
    s.flexDirection = 0; s.justifyContent = 0; s.alignItems = 0; s.alignSelf = -1;
    s.flexWrap = false; s.flexGrow = 0.0f; s.flexShrink = 1.0f;
    s.alignContent = 0; s.justifyItems = 0;
    s.flexBasis = { Length::Auto, 0 }; s.gap = s.rowGap = s.columnGap = 0; s.columnCount = 1; s.order = 0;
    s.gridTemplateColumns.clear(); s.gridTemplateRows.clear(); s.gridAutoFlow = L"row";
    s.gridColumnStart = s.gridRowStart = 0; s.gridColumnSpan = s.gridRowSpan = 1;
    if (node->tag == L"#text") return s;

    static const wchar_t* blocks[] = { L"html", L"body", L"div", L"p", L"h1", L"h2", L"h3", L"h4", L"h5", L"h6", L"ul", L"ol", L"li", L"table", L"tr", L"section", L"article", L"header", L"footer", L"nav", L"main", L"aside", L"form", L"fieldset", L"blockquote", L"hr", L"pre", L"figure", L"noscript" };
    for (auto b : blocks) if (node->tag == b) s.display = Style::Block;
    if (node->tag == L"td" || node->tag == L"th") s.display = Style::TableCell;
    if (node->tag == L"head" || node->tag == L"title" || node->tag == L"meta" || node->tag == L"link" || node->tag == L"style" || node->tag == L"script") s.display = Style::None;

    // Browser-like user-agent defaults for replaced/form controls. Author CSS
    // is applied below and can override every one of these values.
    if (node->tag == L"input" || node->tag == L"button" || node->tag == L"select" || node->tag == L"textarea") {
        s.display = Style::InlineBlock;
        s.background = RGB(255, 255, 255);
        s.borderWidth = 1;
        s.borderColor = RGB(160, 165, 175);
        s.paddingTop = s.paddingBottom = 5;
        s.paddingLeft = s.paddingRight = 8;
        s.fontSize = max(13, min(16, s.fontSize));
        if (node->tag == L"button") {
            s.background = RGB(242, 243, 245);
            s.paddingTop = s.paddingBottom = 7;
            s.paddingLeft = s.paddingRight = 12;
            s.textAlign = TA_CENTER;
        }
        if (node->tag == L"textarea") { s.paddingTop = s.paddingBottom = 7; }
        if (node->tag == L"input") {
            wstring type = InputType(node);
            if (type == L"hidden") s.display = Style::None;
            if (type == L"checkbox" || type == L"radio") {
                s.paddingTop = s.paddingBottom = s.paddingLeft = s.paddingRight = 0;
                s.width = { Length::Px, 18 }; s.height = { Length::Px, 18 };
            }
            if (type == L"range") { s.width = { Length::Px, 160 }; }
        }
    }

    if (node->tag == L"button") s.textAlign = TA_CENTER;
    if (node->tag == L"b" || node->tag == L"strong" || node->tag == L"h1" || node->tag == L"h2") s.bold = true;
    if (node->tag == L"i" || node->tag == L"em") s.italic = true;
    if (node->tag == L"a") { s.color = RGB(0, 0, 238); s.underline = true; }
    if (node->tag == L"h1") s.fontSize = 32; if (node->tag == L"h2") s.fontSize = 26; if (node->tag == L"h3") s.fontSize = 21;
    if (node->tag == L"ul" || node->tag == L"ol") { s.paddingLeft = 40; s.marginTop = 10; s.marginBottom = 10; }
    if (node->tag == L"p") { s.marginTop = 10; s.marginBottom = 10; }

    vector<const CSSRule*> matches;
    for (auto& r : rules) if (r.pseudoElement.empty() && Matches(node, r.selector, (int)r.selector.parts.size() - 1)) matches.push_back(&r);
    stable_sort(matches.begin(), matches.end(), [](const CSSRule* a, const CSSRule* b) {
        if (a->important != b->important) return !a->important;
        return a->selector.specificity < b->selector.specificity;
        });
    for (auto r : matches) ApplyDecls(node, s, r->decls, inheritedFontSize);

    wstring inlineCSS = GetAttribute(node, L"style");
    if (!inlineCSS.empty()) {
        vector<pair<wstring, wstring>> d;
        size_t p = 0; while (p < inlineCSS.size()) {
            size_t e = inlineCSS.find(L';', p); if (e == wstring::npos) e = inlineCSS.size();
            wstring decl = Trim(inlineCSS.substr(p, e - p)); size_t c = decl.find(L':');
            if (c != wstring::npos) d.push_back({ Lower(Trim(decl.substr(0, c))), Trim(decl.substr(c + 1)) });
            p = e + 1;
        }
        ApplyDecls(node, s, d, inheritedFontSize);
    }

    // A button with an explicit CSS width needs a real box in this renderer.
    // Otherwise it falls through the inline-text path and loses its background,
    // padding, border and width (e.g. LearnFeed's full-width Sign in button).
    if (node->tag == L"button" && s.width.kind != Length::Auto && s.display == Style::Inline)
        s.display = Style::Block;

    // A browser only hides <noscript> when scripting is enabled.
    if (node->attributes.count(L"hidden") || (node->tag == L"noscript" && g_javascriptEnabled))
        s.display = Style::None;

#if !defined(_WIN32)
    static int styleDebug = 0;
    if (styleDebug++ < 100) std::fprintf(stderr, "[style] <%s> display=%d cols=%d font=%d margin=%d,%d padding=%d,%d widthKind=%d width=%g bg=%s\n", WToUtf8(node->tag).c_str(), (int)s.display, s.columnCount, s.fontSize, s.marginTop, s.marginLeft, s.paddingTop, s.paddingLeft, (int)s.width.kind, s.width.value, s.background == CLR_INVALID ? "none" : "set");
#endif
    if (node->tag == L"html") g_rootFontSize = s.fontSize;

    return s;
}

void LayoutInline(HDC dc, FontCache& fc, const vector<Node*>& nodes, const Style& parentStyle, int startX, int& startY, int availW, vector<Fragment>& outFrags, const vector<CSSRule>& rules) {
    struct InlineItem { wstring txt; Style s; wstring href; Bitmap* img = nullptr; int w = 0, h = 0; bool br = false; Node* linkNode = nullptr; Node* controlNode = nullptr; };
    vector<InlineItem> items;
    function<void(Node*, Style, const wstring&, Node*)> flatten = [&](Node* n, Style cs, const wstring& inheritedHref, Node* anchorNode) {
        if (n->tag == L"#text") {
            wstring t = n->text, text; bool lastSp = false;
            for (wchar_t c : t) { if (iswspace(c)) { if (!lastSp && cs.whiteSpace != L"nowrap") { text += L' '; lastSp = true; } } else { text += c; lastSp = false; } }
            size_t i = 0; while (i < text.size()) {
                size_t j = text.find(L' ', i); if (j == wstring::npos) j = text.size();
                if (j > i) {
                    wstring word = text.substr(i, j - i);
                    if (cs.textTransform == L"uppercase") for (auto& c : word) c = towupper(c);
                    if (cs.textTransform == L"lowercase") for (auto& c : word) c = towlower(c);
                    items.push_back({ word, cs, inheritedHref, nullptr, MeasureText(dc, fc, cs, word), 0, false, anchorNode });
                }
                if (j < text.size()) items.push_back({ L" ", cs, inheritedHref, nullptr, MeasureText(dc, fc, cs, L" "), 0, false, anchorNode });
                i = j + 1;
            }
            return;
        }
        if (n->tag == L"br") { items.push_back({ L"", cs, inheritedHref, nullptr, 0, 0, true, anchorNode }); return; }
        if (IsFormControlNode(n)) {
            wstring type = InputType(n);
            if (n->tag == L"input" && type == L"hidden") return;
            wstring label = ControlValue(n);
            if (n->tag == L"input" && (type == L"button" || type == L"submit" || type == L"reset") && label.empty())
                label = type == L"reset" ? L"Reset" : L"Submit";
            if (n->tag == L"input" && (type == L"checkbox" || type == L"radio" || type == L"range")) label.clear();
            int cw = 0, ch = 0;
            int extraW = cs.paddingLeft + cs.paddingRight + cs.borderWidth * 2;
            int extraH = cs.paddingTop + cs.paddingBottom + cs.borderWidth * 2;
            if (cs.width.kind != Length::Auto) cw = cs.width.Resolve(availW, g_rootFontSize) + (cs.borderBox ? 0 : extraW);
            else if (n->attributes.count(L"width")) cw = ParseLength(GetAttribute(n, L"width")).Resolve(availW, g_rootFontSize) + extraW;
            else if (n->tag == L"button" || (n->tag == L"input" && (type == L"button" || type == L"submit" || type == L"reset"))) cw = MeasureText(dc, fc, cs, Trim(label)) + extraW + 2;
            else if (n->tag == L"input" && (type == L"checkbox" || type == L"radio")) cw = 18;
            else if (n->tag == L"input" && type == L"range") cw = 160;
            else if (n->tag == L"select") cw = 140;
            else if (n->tag == L"textarea") cw = max(180, (int)wcstol(GetAttribute(n, L"cols").c_str(), nullptr, 10) * max(6, cs.fontSize / 2)) + extraW;
            else cw = 210;
            if (cs.height.kind != Length::Auto) ch = cs.height.Resolve(availW, g_rootFontSize) + (cs.borderBox ? 0 : extraH);
            else if (n->tag == L"textarea") ch = max(72, (int)wcstol(GetAttribute(n, L"rows").c_str(), nullptr, 10) * (cs.fontSize + 5)) + extraH;
            else if (n->tag == L"input" && (type == L"checkbox" || type == L"radio")) ch = 18;
            else if (n->tag == L"input" && type == L"range") ch = 24;
            else ch = max(30, (int)(cs.fontSize * 1.5f) + extraH);
            items.push_back({ label, cs, inheritedHref, nullptr, max(1, cw), max(1, ch), false, anchorNode, n });
            return;
        }
        if (n->tag == L"img") {
            wstring src = ResolveURL(GetAttribute(n, L"base"), GetAttribute(n, L"src"));
            if (src.empty()) src = GetAttribute(n, L"src");
            Bitmap* bmp = g_imageCache.Get(src);
            int w = 16, h = 16;
            if (bmp) { w = bmp->GetWidth(); h = bmp->GetHeight(); }
            if (n->attributes.count(L"width")) w = ParseLength(GetAttribute(n, L"width")).Resolve(w, g_rootFontSize);
            if (n->attributes.count(L"height")) h = ParseLength(GetAttribute(n, L"height")).Resolve(h, g_rootFontSize);
            items.push_back({ L"", cs, inheritedHref, bmp, w, h, false, anchorNode });
            return;
        }
        wstring href = inheritedHref; Node* anchor = anchorNode;
        if (n->tag == L"a") { href = GetAttribute(n, L"href"); anchor = n; }
        for (Node* c : n->children) {
            Style childS = ComputeStyle(c, cs, rules, false);
            if (childS.display == Style::None || childS.visibilityHidden) continue;
            if (childS.display == Style::Block) childS.display = Style::Inline;
            flatten(c, childS, href, anchor);
        }
        };
    for (Node* n : nodes) {
        Style s = ComputeStyle(n, parentStyle, rules, false);
        if (s.display == Style::None || s.visibilityHidden) continue;
        wstring href = n->tag == L"a" ? GetAttribute(n, L"href") : L"";
        flatten(n, s, href, href.empty() ? nullptr : n);
    }

#if !defined(_WIN32)
    static int inlineDebug = 0; if (inlineDebug++ < 18) { std::fprintf(stderr, "[inline] input nodes=%zu items=%zu first='%s' width=%d\n", nodes.size(), items.size(), items.empty() ? "" : WToUtf8(items.front().txt).c_str(), availW); }
#endif
    const size_t firstFragment = outFrags.size();
    int curX = startX, lineH = 0;
    for (auto& it : items) {
        if (it.br) {
            startY += max(20, lineH);
            curX = startX; lineH = 0;
            continue;
        }
        int itemH = (it.img || it.controlNode) ? it.h : (it.s.lineHeight ? it.s.lineHeight : (int)(it.s.fontSize * 1.3f));
        if (curX + it.w > startX + availW && curX > startX) {
            startY += max(20, lineH);
            curX = startX; lineH = 0;
        }
        if (curX == startX && it.txt == L" ") continue;
        lineH = max(lineH, itemH);
        Fragment f; f.text = it.txt; f.style = it.s; f.x = curX; f.y = startY; f.w = it.w; f.img = it.img; f.h = it.controlNode ? it.h : itemH; f.href = it.href; f.linkNode = it.linkNode; f.controlNode = it.controlNode;
        outFrags.push_back(f);
        curX += it.w;
    }
    // A whitespace-only text node between block elements collapses to no line
    // box. Previously it advanced startY by 20px despite producing no fragment,
    // doubling the vertical spacing in forms and card layouts.
    if (lineH > 0) startY += max(20, lineH);
#if !defined(_WIN32)
    static int fragDebug = 0; if (fragDebug++ < 18)std::fprintf(stderr, "[inline-out] fragments=%zu startY=%d\n", outFrags.size() - firstFragment, startY);
#endif

    // text-align is inherited by inline content; shift each visual line within
    // the available line box after wrapping has been completed.
    if (parentStyle.textAlign == TA_CENTER || parentStyle.textAlign == TA_RIGHT) {
        unordered_map<int, pair<int, int>> lineBounds;
        for (size_t i = firstFragment; i < outFrags.size(); ++i) {
            const Fragment& f = outFrags[i];
            auto it = lineBounds.find(f.y);
            if (it == lineBounds.end()) lineBounds.emplace(f.y, make_pair(f.x, f.x + f.w));
            else { it->second.first = min(it->second.first, f.x); it->second.second = max(it->second.second, f.x + f.w); }
        }
        for (size_t i = firstFragment; i < outFrags.size(); ++i) {
            const Fragment& f = outFrags[i];
            auto bounds = lineBounds[f.y];
            int lineWidth = bounds.second - bounds.first;
            int free = max(0, availW - lineWidth);
            int shift = parentStyle.textAlign == TA_CENTER ? free / 2 : free;
            outFrags[i].x += shift;
        }
    }
}

static void LayoutNodeCore(HDC dc, FontCache& fc, Node* node, const Style& parentS, const Style& s, int x, int y, int availW, LayoutBox& out, const vector<CSSRule>& rules);
void LayoutNode(HDC dc, FontCache& fc, Node* node, const Style& parentS, const Style& s, int x, int y, int availW, LayoutBox& out, const vector<CSSRule>& rules);
static void LayoutNodeCore(HDC dc, FontCache& fc, Node* node, const Style& parentS, const Style& s, int x, int y, int availW, LayoutBox& out, const vector<CSSRule>& rules) {
#if !defined(_WIN32)
    static int nodeDebug = 0; if (nodeDebug++ < 100) std::fprintf(stderr, "[node] <%s> display=%d cols=%d x=%d y=%d avail=%d children=%zu text=%s\n", WToUtf8(node->tag).c_str(), (int)s.display, s.columnCount, x, y, availW, node->children.size(), WToUtf8(node->text).substr(0, 32).c_str());
#endif
    // Fixed-position overlays are sized against the viewport instead of the
    // parent's normal content flow (e.g. #auth with position:fixed; inset:0).
    int fixedHeight = -1;
    if (s.positionFixed) {
        const int left = s.hasPosLeft ? s.posLeft.Resolve(g_layoutViewportWidth, g_rootFontSize) : 0;
        const int right = s.hasPosRight ? s.posRight.Resolve(g_layoutViewportWidth, g_rootFontSize) : 0;
        const int top = s.hasPosTop ? s.posTop.Resolve(g_layoutViewportHeight, g_rootFontSize) : 0;
        const int bottom = s.hasPosBottom ? s.posBottom.Resolve(g_layoutViewportHeight, g_rootFontSize) : 0;
        if (s.hasPosLeft) x = left;
        if (s.hasPosTop) y = top;
        if (s.hasPosLeft && s.hasPosRight) availW = max(0, g_layoutViewportWidth - left - right);
        else if (s.hasPosLeft) availW = max(0, g_layoutViewportWidth - left);
        else if (s.hasPosRight) availW = max(0, g_layoutViewportWidth - right);
        if (s.hasPosTop && s.hasPosBottom) fixedHeight = max(0, g_layoutViewportHeight - top - bottom);
    }
    out.node = node; out.style = s; out.y = y + s.marginTop;
    int horizontalExtras = s.borderWidth * 2 + s.paddingLeft + s.paddingRight;
    const int availableOuterWidth = max(0, availW - s.marginLeft - s.marginRight);
    const bool autoWidth = s.width.kind == Length::Auto;
    int w = autoWidth ? availableOuterWidth : s.width.Resolve(availW, g_rootFontSize);
    if (!autoWidth && !s.borderBox) w += horizontalExtras;
    // min/max-width values follow the same content-box/border-box rule as width.
    if (s.minWidth.kind != Length::Auto) {
        int minW = s.minWidth.Resolve(availW, g_rootFontSize);
        if (!s.borderBox) minW += horizontalExtras;
        w = max(w, minW);
    }
    if (s.maxWidth.kind != Length::Auto) {
        int maxW = s.maxWidth.Resolve(availW, g_rootFontSize);
        if (!s.borderBox) maxW += horizontalExtras;
        w = min(w, maxW);
    }
    w = max(0, min(w, availableOuterWidth));
    int freeHorizontal = max(0, availW - w - s.marginLeft - s.marginRight);
    int effectiveLeft = s.marginLeft + (s.marginLeftAuto ? (s.marginRightAuto ? freeHorizontal / 2 : freeHorizontal) : 0);
    int effectiveRight = s.marginRight + (s.marginRightAuto && !s.marginLeftAuto ? freeHorizontal : (s.marginLeftAuto && s.marginRightAuto ? freeHorizontal - freeHorizontal / 2 : 0));
    out.x = x + effectiveLeft;
    int innerW = w - horizontalExtras;
    if (innerW < 0) innerW = 0;

    if (node->tag == L"hr") { out.w = w; out.h = 2; out.isHr = true; return; }

    if (s.display == Style::Table) {
        vector<Node*> rows;
        function<void(Node*)> findRows = [&](Node* n) {
            for (Node* c : n->children) {
                if (c->tag == L"tr") rows.push_back(c);
                else if (c->tag == L"thead" || c->tag == L"tbody" || c->tag == L"tfoot") findRows(c);
            }
            };
        findRows(node);
        int curY = out.y + s.borderWidth + s.paddingTop;
        int maxW = w;
        for (Node* r : rows) {
            int numCols = 0;
            for (Node* c : r->children) if (c->tag == L"td" || c->tag == L"th") numCols += max(1, c->attributes.count(L"colspan") ? _wtoi(c->attributes[L"colspan"].c_str()) : 1);
            if (numCols == 0) continue;
            int colW = maxW / numCols;
            int curX = out.x + s.borderWidth + s.paddingLeft;
            int rowH = 0;
            LayoutBox rb; rb.node = r; rb.style = ComputeStyle(r, s, rules, false);
            for (Node* c : r->children) {
                if (c->tag != L"td" && c->tag != L"th") continue;
                Style cs = ComputeStyle(c, s, rules, false);
                int span = c->attributes.count(L"colspan") ? _wtoi(c->attributes[L"colspan"].c_str()) : 1;
                int cellW = colW * span;
                LayoutBox cb;
                LayoutNode(dc, fc, c, s, cs, curX, curY, cellW, cb, rules);
                cb.w = cellW;
                rowH = max(rowH, cb.h);
                rb.children.push_back(cb);
                curX += cellW;
            }
            rb.w = maxW; rb.h = rowH;
            for (auto& cb : rb.children) cb.h = rowH;
            out.children.push_back(rb);
            curY += rowH;
        }
        out.w = w; out.h = curY - out.y + s.borderWidth + s.paddingBottom;
        return;
    }

    // Basic CSS multi-column flow. The page uses columns for editorial text.
    // CSS Grid (row auto-flow, fixed/percent/fr/auto tracks, simple explicit placement).
    if (s.display == Style::Grid) {
        const int colGap = s.columnGap ? s.columnGap : s.gap, rowGap = s.rowGap ? s.rowGap : s.gap;
        vector<GridTrack> colTracks = ParseGridTracks(s.gridTemplateColumns, innerW);
        vector<int> colWidths = ResolveGridTracks(colTracks, innerW, colGap);
        const int cols = (int)colWidths.size();
        struct GridItem { Node* node = nullptr; Style style; int row = 0, col = 0, rowSpan = 1, colSpan = 1; LayoutBox measured; };
        vector<GridItem> items; int autoIndex = 0, maxRow = 0;
        for (Node* child : node->children) {
            Style cs = ComputeStyle(child, s, rules, false); if (cs.positionAbsolute || cs.display == Style::None || cs.visibilityHidden)continue;
            GridItem item; item.node = child; item.style = cs; item.rowSpan = max(1, cs.gridRowSpan); item.colSpan = max(1, min(cols, cs.gridColumnSpan));
            if (cs.gridColumnStart > 0)item.col = min(cols - 1, cs.gridColumnStart - 1); else item.col = autoIndex % cols;
            if (cs.gridRowStart > 0)item.row = cs.gridRowStart - 1; else item.row = autoIndex / cols;
            if (item.col + item.colSpan > cols)item.colSpan = cols - item.col;
            maxRow = max(maxRow, item.row + item.rowSpan); ++autoIndex; items.push_back(std::move(item));
        }
        if (items.empty()) { out.w = w; out.h = s.height.kind == Length::Auto ? s.paddingTop + s.paddingBottom + s.borderWidth * 2 : s.height.Resolve(availW, g_rootFontSize); return; }
        vector<int> rowHeights((size_t)max(1, maxRow), 0);
        vector<int> colX((size_t)cols, 0);
        int cx = out.x + s.borderWidth + s.paddingLeft; for (int c = 0; c < cols; ++c) { colX[(size_t)c] = cx; cx += colWidths[(size_t)c] + colGap; }
        // First measure each item to determine auto row sizes.
        for (auto& it : items) {
            int cellW = 0; for (int c = it.col; c < it.col + it.colSpan; ++c)cellW += colWidths[(size_t)c]; cellW += colGap * (it.colSpan - 1);
            LayoutBox probe; LayoutNode(dc, fc, it.node, s, it.style, out.x + s.borderWidth + s.paddingLeft, out.y + s.borderWidth + s.paddingTop, cellW, probe, rules); it.measured = std::move(probe);
            int perRow = max(1, (it.measured.h + it.rowSpan - 1) / it.rowSpan); for (int rr = it.row; rr < it.row + it.rowSpan && rr < (int)rowHeights.size(); ++rr)rowHeights[(size_t)rr] = max(rowHeights[(size_t)rr], perRow);
        }
        vector<GridTrack> rowTracks; if (!s.gridTemplateRows.empty())rowTracks = ParseGridTracks(s.gridTemplateRows, s.height.kind == Length::Auto ? innerW : max(0, s.height.Resolve(availW, g_rootFontSize) - s.paddingTop - s.paddingBottom - s.borderWidth * 2));
        if ((int)rowTracks.size() > (int)rowHeights.size())rowHeights.resize(rowTracks.size(), 0);
        if (!rowTracks.empty()) { int target = s.height.kind == Length::Auto ? 0 : max(0, s.height.Resolve(availW, g_rootFontSize) - s.paddingTop - s.paddingBottom - s.borderWidth * 2); if (target > 0) { auto resolved = ResolveGridTracks(rowTracks, target, rowGap); for (size_t i = 0; i < resolved.size() && i < rowHeights.size(); ++i)rowHeights[i] = max(rowHeights[i], resolved[i]); } }
        vector<int> rowY(rowHeights.size(), out.y + s.borderWidth + s.paddingTop); for (size_t r = 1; r < rowY.size(); ++r)rowY[r] = rowY[r - 1] + rowHeights[r - 1] + rowGap;
        int contentBottom = out.y + s.borderWidth + s.paddingTop;
        for (auto& it : items) {
            int cellW = 0, cellH = 0; for (int c = it.col; c < it.col + it.colSpan; ++c)cellW += colWidths[(size_t)c]; cellW += colGap * (it.colSpan - 1); for (int rr = it.row; rr < it.row + it.rowSpan && rr < (int)rowHeights.size(); ++rr)cellH += rowHeights[(size_t)rr]; cellH += rowGap * (it.rowSpan - 1);
            int cellX = colX[(size_t)it.col], cellY = rowY[(size_t)min(it.row, (int)rowY.size() - 1)]; int availChildW = max(0, cellW - it.style.marginLeft - it.style.marginRight);
            Style finalStyle = it.style; int childW = availChildW; int justify = it.style.justifyItems != 0 ? it.style.justifyItems : s.justifyItems;
            if (justify != 0 && it.style.width.kind == Length::Auto) { wstring txt; CollectNodeText(it.node, txt); int intrinsic = min(availChildW, MeasureText(dc, fc, it.style, Trim(txt)) + it.style.paddingLeft + it.style.paddingRight + it.style.borderWidth * 2); childW = max(0, intrinsic); }
            finalStyle.width = { Length::Px,(float)childW };
            LayoutBox box; LayoutNode(dc, fc, it.node, s, finalStyle, cellX, cellY, cellW, box, rules);
            int align = it.style.alignSelf < 0 ? s.alignItems : it.style.alignSelf; int freeY = max(0, cellH - box.h - it.style.marginTop - it.style.marginBottom); if (align == 2)OffsetLayoutBox(box, 0, freeY); else if (align == 3)OffsetLayoutBox(box, 0, freeY / 2);
            if (justify == 2)OffsetLayoutBox(box, max(0, cellW - box.w - it.style.marginLeft - it.style.marginRight), 0); else if (justify == 3)OffsetLayoutBox(box, max(0, (cellW - box.w - it.style.marginLeft - it.style.marginRight) / 2), 0);
            box.x = cellX + it.style.marginLeft + (justify == 2 ? max(0, cellW - box.w - it.style.marginLeft - it.style.marginRight) : justify == 3 ? max(0, (cellW - box.w - it.style.marginLeft - it.style.marginRight) / 2) : 0); box.y = cellY + it.style.marginTop + (align == 2 ? freeY : align == 3 ? freeY / 2 : 0); out.children.push_back(std::move(box)); contentBottom = max(contentBottom, cellY + cellH);
        }
        out.w = w; out.h = s.height.kind == Length::Auto ? contentBottom - out.y + s.paddingBottom + s.borderWidth : s.height.Resolve(availW, g_rootFontSize); return;
    }

    if (s.columnCount > 1 && s.display != Style::Flex && s.display != Style::Table) {
        int count = max(1, min(s.columnCount, max(1, innerW / 100)));
        int colGap = max(0, s.columnGap);
        int colW = max(1, (innerW - colGap * (count - 1)) / count);
        int contentX = out.x + s.borderWidth + s.paddingLeft;
        int contentTop = out.y + s.borderWidth + s.paddingTop;

        struct ColumnItem { LayoutBox box; int totalHeight = 0; };
        vector<ColumnItem> items;
        vector<Node*> pendingInline;

        auto flushColumnInline = [&]() {
            if (pendingInline.empty()) return;
            ColumnItem item;
            item.box.x = contentX;
            item.box.y = contentTop;
            item.box.w = colW;
            int inlineY = contentTop;
            LayoutInline(dc, fc, pendingInline, s, contentX, inlineY, colW, item.box.frags, rules);
            item.box.h = max(1, inlineY - contentTop);
            item.totalHeight = item.box.h;
            items.push_back(std::move(item));
            pendingInline.clear();
            };

        for (Node* child : node->children) {
            Style cs = ComputeStyle(child, s, rules, false);
            if (cs.display == Style::None || cs.visibilityHidden) continue;
            if (cs.display == Style::Block || cs.display == Style::Flex ||
                cs.display == Style::Table || cs.display == Style::TableCell) {
                flushColumnInline();
                ColumnItem item;
                LayoutNode(dc, fc, child, s, cs, contentX, contentTop, colW, item.box, rules);
                item.totalHeight = max(0, cs.marginTop) + item.box.h + max(0, cs.marginBottom);
                items.push_back(std::move(item));
            }
            else {
                pendingInline.push_back(child);
            }
        }
        flushColumnInline();

        // Balance whole block items across columns while preserving document
        // order. This is substantially closer to CSS column-fill:balance than
        // assigning each next paragraph to the first column that overflows.
        const int n = (int)items.size();
        const int activeColumns = max(1, min(count, max(1, n)));
        vector<int> prefix((size_t)n + 1, 0);
        for (int i = 0; i < n; ++i)
            prefix[(size_t)i + 1] = prefix[(size_t)i] + max(1, items[(size_t)i].totalHeight);

        const int INF = (std::numeric_limits<int>::max)() / 4;
        vector<vector<int>> dp((size_t)n + 1, vector<int>((size_t)activeColumns + 1, INF));
        vector<vector<int>> split((size_t)n + 1, vector<int>((size_t)activeColumns + 1, -1));
        dp[0][0] = 0;
        for (int i = 1; i <= n; ++i) {
            for (int c = 1; c <= activeColumns; ++c) {
                for (int j = c - 1; j < i; ++j) {
                    if (dp[(size_t)j][(size_t)c - 1] == INF) continue;
                    int segmentHeight = prefix[(size_t)i] - prefix[(size_t)j];
                    int candidate = max(dp[(size_t)j][(size_t)c - 1], segmentHeight);
                    if (candidate < dp[(size_t)i][(size_t)c]) {
                        dp[(size_t)i][(size_t)c] = candidate;
                        split[(size_t)i][(size_t)c] = j;
                    }
                }
            }
        }

        vector<pair<int, int>> ranges((size_t)activeColumns);
        int endIndex = n;
        for (int c = activeColumns; c >= 1; --c) {
            int beginIndex = split[(size_t)endIndex][(size_t)c];
            if (beginIndex < 0) beginIndex = max(0, endIndex - 1);
            ranges[(size_t)c - 1] = { beginIndex, endIndex };
            endIndex = beginIndex;
        }

        vector<int> columnY((size_t)activeColumns, contentTop);
        for (int column = 0; column < activeColumns; ++column) {
            const int dx = column * (colW + colGap);
            for (int i = ranges[(size_t)column].first; i < ranges[(size_t)column].second; ++i) {
                ColumnItem& item = items[(size_t)i];
                int dy = columnY[(size_t)column] - contentTop;
                OffsetLayoutBox(item.box, dx, dy);
                out.children.push_back(std::move(item.box));
                columnY[(size_t)column] += item.totalHeight;
            }
        }

        int bottom = contentTop;
        for (int yCol : columnY) bottom = max(bottom, yCol);
        out.w = w;
        out.h = bottom - out.y + s.borderWidth + s.paddingBottom;
        if (s.height.kind != Length::Auto) out.h = s.height.Resolve(availW, g_rootFontSize);
        return;
    }

    // Multi-line flex wrapping. Each line gets its own grow/shrink and justification pass.
    if (s.display == Style::Flex && s.flexWrap) {
        struct WrapItem { Node* node = nullptr; Style style; int base = 0, main = 0, cross = 0; LayoutBox measured; };
        vector<WrapItem> items;
        const bool rowAxis = (s.flexDirection == 0);
        const int mainGap = rowAxis ? (s.columnGap ? s.columnGap : s.gap) : (s.rowGap ? s.rowGap : s.gap);
        const int crossGap = rowAxis ? (s.rowGap ? s.rowGap : s.gap) : (s.columnGap ? s.columnGap : s.gap);
        int mainAvail = rowAxis ? innerW : (s.height.kind != Length::Auto ? max(0, s.height.Resolve(availW, g_rootFontSize) - s.paddingTop - s.paddingBottom - s.borderWidth * 2) : 0);
        for (Node* child : node->children) {
            Style cs = ComputeStyle(child, s, rules, false); if (cs.positionAbsolute || cs.display == Style::None || cs.visibilityHidden)continue;
            WrapItem it; it.node = child; it.style = cs; Length basis = cs.flexBasis; if (basis.kind == Length::Auto)basis = rowAxis ? cs.width : cs.height;
            int probeW = rowAxis ? max(1, innerW) : max(1, cs.width.kind == Length::Auto ? innerW : cs.width.Resolve(innerW, g_rootFontSize));
            Style probeStyle = cs; if (rowAxis && basis.kind != Length::Auto)probeStyle.width = basis;
            LayoutNode(dc, fc, child, s, probeStyle, out.x + s.borderWidth + s.paddingLeft, out.y + s.borderWidth + s.paddingTop, probeW, it.measured, rules);
            if (basis.kind != Length::Auto)it.base = max(0, basis.Resolve(rowAxis ? innerW : max(1, mainAvail), g_rootFontSize));
            else if (rowAxis) { wstring txt; CollectNodeText(child, txt); it.base = child->tag == L"img" ? max(16, ParseLength(GetAttribute(child, L"width")).Resolve(64, g_rootFontSize)) : min(max(36, MeasureText(dc, fc, cs, Trim(txt)) + cs.paddingLeft + cs.paddingRight + cs.borderWidth * 2), max(36, innerW)); probeStyle.width = { Length::Px,(float)it.base }; LayoutNode(dc, fc, child, s, probeStyle, out.x + s.borderWidth + s.paddingLeft, out.y + s.borderWidth + s.paddingTop, it.base, it.measured, rules); }
            else it.base = max(0, it.measured.h);
            it.base = max(0, it.base); it.main = it.base; it.cross = rowAxis ? it.measured.h : it.measured.w; items.push_back(std::move(it));
        }
        stable_sort(items.begin(), items.end(), [](const WrapItem& a, const WrapItem& b) {return a.style.order < b.style.order; });
        if (!rowAxis && mainAvail <= 0) { for (const auto& it : items)mainAvail += it.base + it.style.marginTop + it.style.marginBottom; mainAvail += items.empty() ? 0 : mainGap * (int)(items.size() - 1); }
        vector<vector<size_t>> lines(1); int lineUsed = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            auto& it = items[i]; int margins = rowAxis ? it.style.marginLeft + it.style.marginRight : it.style.marginTop + it.style.marginBottom; int needed = it.base + margins + (lines.back().empty() ? 0 : mainGap);
            if (!lines.back().empty() && lineUsed + needed > mainAvail) { lines.emplace_back(); lineUsed = 0; needed = it.base + margins; }
            lines.back().push_back(i); lineUsed += needed;
        }
        if (lines.size() == 1 && lines[0].empty())lines.clear();
        vector<int> lineCross(lines.size(), 0);
        for (size_t li = 0; li < lines.size(); ++li) { for (size_t idx : lines[li]) { auto& it = items[idx]; lineCross[li] = max(lineCross[li], it.cross + (rowAxis ? it.style.marginTop + it.style.marginBottom : it.style.marginLeft + it.style.marginRight)); } }
        int naturalCross = 0; for (int h : lineCross)naturalCross += h; if (lineCross.size() > 1)naturalCross += crossGap * (int)(lineCross.size() - 1);
        int containerCross = rowAxis ? (fixedHeight >= 0 ? max(0, fixedHeight - s.paddingTop - s.paddingBottom - s.borderWidth * 2) : (s.height.kind != Length::Auto ? max(0, s.height.Resolve(availW, g_rootFontSize) - s.paddingTop - s.paddingBottom - s.borderWidth * 2) : naturalCross)) : innerW;
        int crossFree = max(0, containerCross - naturalCross), crossLead = 0, crossBetween = crossGap;
        if (s.alignContent == 1)crossLead = crossFree; else if (s.alignContent == 2)crossLead = crossFree / 2; else if (s.alignContent == 4 && lines.size() > 1)crossBetween += crossFree / (int)(lines.size() - 1); else if (s.alignContent == 5 && !lines.empty()) { crossBetween += crossFree / (int)lines.size(); crossLead = (crossFree / (int)lines.size()) / 2; }
        else if (s.alignContent == 3 && !lines.empty()) { int add = crossFree / (int)lines.size(); for (int& h : lineCross)h += add; }
        int crossCursor = (rowAxis ? out.y + s.borderWidth + s.paddingTop : out.x + s.borderWidth + s.paddingLeft) + crossLead;
        for (size_t li = 0; li < lines.size(); ++li) {
            auto& line = lines[li]; int used = mainGap * (line.empty() ? 0 : (int)line.size() - 1); float grow = 0, shrink = 0;
            for (size_t idx : line) { auto& it = items[idx]; used += it.base + (rowAxis ? it.style.marginLeft + it.style.marginRight : it.style.marginTop + it.style.marginBottom); grow += it.style.flexGrow; shrink += it.style.flexShrink * max(1, it.base); it.main = it.base; }
            int free = mainAvail - used; if (free > 0 && grow > 0) { for (size_t idx : line) { auto& it = items[idx]; it.main = it.base + (int)std::round(free * it.style.flexGrow / grow); } }
            else if (free < 0 && shrink>0) { for (size_t idx : line) { auto& it = items[idx]; float weight = it.style.flexShrink * max(1, it.base); it.main = max(0, it.base - (int)std::round((-free) * weight / shrink)); } }
            int actual = mainGap * (line.empty() ? 0 : (int)line.size() - 1); for (size_t idx : line) { auto& it = items[idx]; actual += it.main + (rowAxis ? it.style.marginLeft + it.style.marginRight : it.style.marginTop + it.style.marginBottom); }int remain = max(0, mainAvail - actual), lead = 0, between = mainGap;
            if (s.justifyContent == 1)lead = remain; else if (s.justifyContent == 2)lead = remain / 2; else if (s.justifyContent == 3 && line.size() > 1)between += remain / (int)(line.size() - 1); else if (s.justifyContent == 4 && !line.empty()) { between += remain / (int)line.size(); lead = (remain / (int)line.size()) / 2; }
            else if (s.justifyContent == 5 && !line.empty()) { between += remain / (int)(line.size() + 1); lead = between - mainGap; }
            int mainCursor = (rowAxis ? out.x + s.borderWidth + s.paddingLeft : out.y + s.borderWidth + s.paddingTop) + lead;
            for (size_t idx : line) {
                auto& it = items[idx]; int mStart = rowAxis ? it.style.marginLeft : it.style.marginTop, mCrossStart = rowAxis ? it.style.marginTop : it.style.marginLeft, mCrossEnd = rowAxis ? it.style.marginBottom : it.style.marginRight;
                int crossFreeItem = max(0, lineCross[li] - it.cross - mCrossStart - mCrossEnd); int align = it.style.alignSelf < 0 ? s.alignItems : it.style.alignSelf; int crossOffset = align == 2 ? crossFreeItem : align == 3 ? crossFreeItem / 2 : 0;
                int childW = rowAxis ? it.main : (align == 0 ? max(0, lineCross[li] - it.style.marginLeft - it.style.marginRight) : it.measured.w); int childH = rowAxis ? (align == 0 ? max(0, lineCross[li] - it.style.marginTop - it.style.marginBottom) : it.measured.h) : it.main;
                Style finalStyle = it.style; finalStyle.width = { Length::Px,(float)childW }; if (!rowAxis || align == 0)finalStyle.height = { Length::Px,(float)childH };
                int childX = rowAxis ? mainCursor + mStart : crossCursor + crossOffset + mCrossStart; int childY = rowAxis ? crossCursor + crossOffset + mCrossStart : mainCursor + mStart;
                LayoutBox box; LayoutNode(dc, fc, it.node, s, finalStyle, childX - it.style.marginLeft, childY - it.style.marginTop, max(1, childW), box, rules); box.x = childX; box.y = childY; box.w = childW; out.children.push_back(std::move(box));
                mainCursor += it.main + mStart + (rowAxis ? it.style.marginRight : it.style.marginBottom) + between;
            }
            crossCursor += lineCross[li] + crossBetween;
        }
        out.w = w; if (fixedHeight >= 0)out.h = fixedHeight; else if (s.height.kind != Length::Auto)out.h = s.height.Resolve(availW, g_rootFontSize); else if (rowAxis)out.h = naturalCross + s.paddingTop + s.paddingBottom + s.borderWidth * 2; else out.h = mainAvail + s.paddingTop + s.paddingBottom + s.borderWidth * 2; return;
    }

    // Flex formatting context. Implements single-line row/column flex, grow/shrink,
    // basis, gaps, order, justify-content and align-items. Multi-line wrapping and
    // reverse directions are deliberately not implemented yet.
    if (s.display == Style::Flex) {
        struct FlexItem { Node* node = nullptr; Style style; LayoutBox box; int base = 0; int main = 0; int cross = 0; };
        vector<FlexItem> items;
        auto textOf = [&](Node* root) {
            wstring result;
            function<void(Node*)> walk = [&](Node* n) { if (n->tag == L"#text") result += n->text; else for (Node* c : n->children) walk(c); };
            walk(root); return Trim(result);
            };
        int flexInnerX = out.x + s.borderWidth + s.paddingLeft;
        int flexStartY = out.y + s.borderWidth + s.paddingTop;
        int mainAvail = s.flexDirection == 0 ? innerW : (s.height.kind != Length::Auto ? max(0, s.height.Resolve(availW, g_rootFontSize) - s.paddingTop - s.paddingBottom - s.borderWidth * 2) : 0);
        for (Node* child : node->children) {
            Style cs = ComputeStyle(child, s, rules, false);
            if (cs.display == Style::None || cs.visibilityHidden) continue;
            FlexItem item; item.node = child; item.style = cs;
            Length basis = cs.flexBasis;
            if (basis.kind == Length::Auto) basis = s.flexDirection == 0 ? cs.width : cs.height;
            if (basis.kind != Length::Auto) item.base = max(0, basis.Resolve(s.flexDirection == 0 ? innerW : mainAvail, g_rootFontSize));
            else if (s.flexDirection == 0) {
                if (child->tag == L"img") item.base = max(16, ParseLength(GetAttribute(child, L"width")).Resolve(64, g_rootFontSize));
                else item.base = min(max(36, MeasureText(dc, fc, cs, textOf(child)) + cs.paddingLeft + cs.paddingRight + cs.borderWidth * 2), max(36, innerW));
            }
            // Flex base size is the item's hypothetical main size. Apply min/max
            // constraints before flex distribution so width:100%; max-width:400px
            // cards do not keep a viewport-sized flex basis.
            if (s.flexDirection == 0) {
                const int extras = cs.borderWidth * 2 + cs.paddingLeft + cs.paddingRight;
                const int boxExtra = cs.borderBox ? 0 : extras;
                if (cs.minWidth.kind != Length::Auto)
                    item.base = max(item.base, cs.minWidth.Resolve(innerW, g_rootFontSize) + boxExtra);
                if (cs.maxWidth.kind != Length::Auto)
                    item.base = min(item.base, cs.maxWidth.Resolve(innerW, g_rootFontSize) + boxExtra);
                item.base = min(item.base, max(0, innerW));
            }
            LayoutBox initial;
            Style initialStyle = cs;
            if (s.flexDirection == 0) initialStyle.width = { Length::Px,(float)item.base };
            else if (basis.kind != Length::Auto) initialStyle.height = { Length::Px,(float)item.base };
            LayoutNode(dc, fc, child, s, initialStyle, flexInnerX, flexStartY, innerW, initial, rules);
            item.box = std::move(initial);
            if (s.flexDirection == 1 && basis.kind == Length::Auto) item.base = item.box.h;
            item.main = item.base;
            item.cross = s.flexDirection == 0 ? item.box.h : item.box.w;
            items.push_back(std::move(item));
        }
        stable_sort(items.begin(), items.end(), [](const FlexItem& a, const FlexItem& b) {return a.style.order < b.style.order; });
        int mainGap = s.flexDirection == 0 ? (s.columnGap ? s.columnGap : s.gap) : (s.rowGap ? s.rowGap : s.gap);
        if (s.flexDirection == 1 && s.height.kind == Length::Auto) {
            mainAvail = 0;
            for (const auto& it : items) mainAvail += it.base + it.style.marginTop + it.style.marginBottom;
            if (!items.empty()) mainAvail += mainGap * (int)(items.size() - 1);
        }
        int fixedGaps = items.empty() ? 0 : mainGap * (int)(items.size() - 1);
        int total = fixedGaps; float growTotal = 0, shrinkTotal = 0;
        for (auto& it : items) { total += it.base + (s.flexDirection == 0 ? it.style.marginLeft + it.style.marginRight : it.style.marginTop + it.style.marginBottom); growTotal += it.style.flexGrow; shrinkTotal += it.style.flexShrink * max(1, it.base); }
        int free = mainAvail - total;
        if (free > 0 && growTotal > 0) { for (auto& it : items)it.main = it.base + (int)std::round(free * it.style.flexGrow / growTotal); }
        else if (free < 0 && shrinkTotal>0) { for (auto& it : items) { float weight = it.style.flexShrink * max(1, it.base); it.main = max(0, it.base - (int)std::round((-free) * weight / shrinkTotal)); } }
        int used = fixedGaps;
        for (auto& it : items)used += it.main + (s.flexDirection == 0 ? it.style.marginLeft + it.style.marginRight : it.style.marginTop + it.style.marginBottom);
        int remaining = max(0, mainAvail - used), lead = 0, between = mainGap;
        if (s.justifyContent == 1)lead = remaining;
        else if (s.justifyContent == 2)lead = remaining / 2;
        else if (s.justifyContent == 3 && items.size() > 1)between = mainGap + remaining / (int)(items.size() - 1);
        else if (s.justifyContent == 4 && !items.empty()) { between = mainGap + remaining / (int)items.size(); lead = (remaining / (int)items.size()) / 2; }
        else if (s.justifyContent == 5 && !items.empty()) { between = mainGap + remaining / (int)(items.size() + 1); lead = between - mainGap; }
        int maxCross = 0;
        for (auto& it : items) maxCross = max(maxCross, it.cross + (s.flexDirection == 0 ? it.style.marginTop + it.style.marginBottom : it.style.marginLeft + it.style.marginRight));
        int containerCross = s.flexDirection == 0 ? (fixedHeight >= 0 ? max(0, fixedHeight - s.paddingTop - s.paddingBottom - s.borderWidth * 2) : (s.height.kind != Length::Auto ? max(0, s.height.Resolve(availW, g_rootFontSize) - s.paddingTop - s.paddingBottom - s.borderWidth * 2) : maxCross)) : innerW;
        int cursor = lead;
        for (auto& it : items) {
            int mainPos = cursor + (s.flexDirection == 0 ? it.style.marginLeft : it.style.marginTop);
            int crossFree = max(0, containerCross - it.cross - (s.flexDirection == 0 ? it.style.marginTop + it.style.marginBottom : it.style.marginLeft + it.style.marginRight));
            int align = it.style.alignSelf < 0 ? s.alignItems : it.style.alignSelf;
            int crossPos = align == 2 ? crossFree : align == 3 ? crossFree / 2 : 0;
            Style finalStyle = it.style;
            int childW = s.flexDirection == 0 ? max(0, it.main) : (align == 0 ? max(0, containerCross - it.style.marginLeft - it.style.marginRight) : it.box.w);
            int childH = s.flexDirection == 1 ? max(0, it.main) : (align == 0 && s.height.kind != Length::Auto ? max(0, containerCross - it.style.marginTop - it.style.marginBottom) : it.box.h);
            finalStyle.width = { Length::Px,(float)childW };
            if (s.flexDirection == 1 || (align == 0 && s.height.kind != Length::Auto)) finalStyle.height = { Length::Px,(float)childH };
            LayoutBox finalBox;
            int childX = s.flexDirection == 0 ? flexInnerX + mainPos : flexInnerX + crossPos + it.style.marginLeft;
            int childY = s.flexDirection == 0 ? flexStartY + crossPos + it.style.marginTop : flexStartY + mainPos;
            LayoutNode(dc, fc, it.node, s, finalStyle, childX - (it.style.marginLeft), childY - (it.style.marginTop), childW, finalBox, rules);
            // Keep the calculated flex position after the child has completed its own layout.
            finalBox.x = childX; finalBox.y = childY; finalBox.w = childW; if (s.flexDirection == 1 && finalStyle.height.kind != Length::Auto)finalBox.h = childH;
            if (s.flexDirection == 0)maxCross = max(maxCross, finalBox.h + it.style.marginTop + it.style.marginBottom);
            out.children.push_back(std::move(finalBox));
            cursor += it.main + (s.flexDirection == 0 ? it.style.marginLeft + it.style.marginRight : it.style.marginTop + it.style.marginBottom) + between;
        }
        out.w = w;
        if (fixedHeight >= 0) out.h = fixedHeight;
        else if (s.height.kind != Length::Auto) out.h = s.height.Resolve(availW, g_rootFontSize);
        else if (s.flexDirection == 0) out.h = maxCross + s.paddingTop + s.paddingBottom + s.borderWidth * 2;
        else out.h = used + s.paddingTop + s.paddingBottom + s.borderWidth * 2;
        return;
    }

    vector<Node*> pendingInline;
    int curY = out.y + s.borderWidth + s.paddingTop;
    int innerX = out.x + s.borderWidth + s.paddingLeft;
    int floatLeftX = innerX, floatRightX = innerX + innerW;
    int floatBottom = curY;
    bool hasFloats = false;

    auto flushInline = [&]() {
        if (pendingInline.empty()) return;
        LayoutInline(dc, fc, pendingInline, s, innerX, curY, innerW, out.frags, rules);
        pendingInline.clear();
        };

    for (Node* child : node->children) {
        Style cs = ComputeStyle(child, s, rules, false);
        if (cs.positionAbsolute || cs.display == Style::None || cs.visibilityHidden) continue;

        if (cs.clear != 0) {
            flushInline();
            curY = max(curY, floatBottom);
            floatLeftX = innerX; floatRightX = innerX + innerW;
            floatBottom = curY; hasFloats = false;
        }

        if (cs.floatDir != 0) {
            flushInline();
            wstring text;
            CollectNodeText(child, text);
            int intrinsic = MeasureText(dc, fc, cs, Trim(text)) + cs.paddingLeft + cs.paddingRight + cs.borderWidth * 2;
            int fw = cs.width.kind != Length::Auto ? cs.width.Resolve(innerW, g_rootFontSize) : max(1, intrinsic);
            if (cs.maxWidth.kind != Length::Auto) fw = min(fw, cs.maxWidth.Resolve(innerW, g_rootFontSize));
            if (cs.minWidth.kind != Length::Auto) fw = max(fw, cs.minWidth.Resolve(innerW, g_rootFontSize));
            fw = max(1, min(fw, innerW));

            int floatX = cs.floatDir == 1 ? floatLeftX + cs.marginLeft : floatRightX - fw - cs.marginRight;
            LayoutBox fb;
            Style floatStyle = cs;
            floatStyle.width = { Length::Px, (float)fw };
            LayoutNode(dc, fc, child, s, floatStyle, floatX - cs.marginLeft, curY, fw, fb, rules);
            fb.x = floatX; fb.w = fw;
            if (cs.floatDir == 1) floatLeftX = floatX + fw + cs.marginRight;
            else floatRightX = floatX - cs.marginLeft;
            floatBottom = max(floatBottom, fb.y + fb.h + cs.marginBottom);
            hasFloats = true;
            out.children.push_back(std::move(fb));
            continue;
        }

        if (hasFloats && (cs.display == Style::Block || cs.display == Style::Flex || cs.display == Style::Grid ||
            cs.display == Style::Table || cs.display == Style::TableCell)) {
            flushInline();
            curY = max(curY, floatBottom);
            floatLeftX = innerX; floatRightX = innerX + innerW;
            floatBottom = curY; hasFloats = false;
        }

        if (cs.display == Style::Block || cs.display == Style::Flex || cs.display == Style::Grid || cs.display == Style::Table || cs.display == Style::TableCell) {
            flushInline();
            LayoutBox cb;
            LayoutNode(dc, fc, child, s, cs, innerX, curY, innerW, cb, rules);
            curY = cb.y + cb.h + cs.marginBottom;
            out.children.push_back(std::move(cb));
        }
        else {
            pendingInline.push_back(child);
        }
    }
    flushInline();

    out.w = w;
    out.h = max(curY, floatBottom) - out.y + s.borderWidth + s.paddingBottom;
    // Empty form controls still have an intrinsic text line box. Include it in
    // addition to their padding/border so CSS-padded inputs are not only ~20px tall.
    if (node->tag == L"input" || node->tag == L"select" || node->tag == L"button" || node->tag == L"textarea") {
        int intrinsicContentHeight = node->tag == L"textarea" ? 70 : (int)std::ceil(s.fontSize * 1.5);
        if (node->tag == L"button") intrinsicContentHeight += 4;
        const int intrinsicControlHeight = intrinsicContentHeight + s.paddingTop + s.paddingBottom + s.borderWidth * 2;
        out.h = max(out.h, intrinsicControlHeight);
    }
    if (fixedHeight >= 0) out.h = fixedHeight;
    else if (s.height.kind != Length::Auto) out.h = s.height.Resolve(availW, g_rootFontSize);
    else if (s.aspectRatio > 0.0001f) out.h = max(1, (int)std::lround((double)max(1, out.w - s.paddingLeft - s.paddingRight - s.borderWidth * 2) / s.aspectRatio) + s.paddingTop + s.paddingBottom + s.borderWidth * 2);
}


// Layout wrapper: relative offsets preserve the original flow slot. Absolute
// children are laid out after normal-flow children and therefore do not affect
// their parent's intrinsic height. This implementation uses the immediate
// parent's padding box as the containing block; full positioned-ancestor
// propagation and static-position rules remain future work.
void LayoutNode(HDC dc, FontCache& fc, Node* node, const Style& parentS, const Style& s,
    int x, int y, int availW, LayoutBox& out, const vector<CSSRule>& rules) {
    LayoutNodeCore(dc, fc, node, parentS, s, x, y, availW, out, rules);

    if (s.positionRelative && !s.positionFixed && !s.positionAbsolute) {
        int dx = 0, dy = 0;
        if (s.hasPosLeft) dx = s.posLeft.Resolve(availW, g_rootFontSize);
        else if (s.hasPosRight) dx = -s.posRight.Resolve(availW, g_rootFontSize);
        if (s.hasPosTop) dy = s.posTop.Resolve(g_layoutViewportHeight, g_rootFontSize);
        else if (s.hasPosBottom) dy = -s.posBottom.Resolve(g_layoutViewportHeight, g_rootFontSize);
        if (dx || dy) OffsetLayoutBox(out, dx, dy);
    }

    const int contentX = out.x + s.borderWidth + s.paddingLeft;
    const int contentY = out.y + s.borderWidth + s.paddingTop;
    const int contentW = max(0, out.w - s.borderWidth * 2 - s.paddingLeft - s.paddingRight);
    for (Node* child : node->children) {
        Style cs = ComputeStyle(child, s, rules, false);
        if (!cs.positionAbsolute || cs.display == Style::None || cs.visibilityHidden) continue;

        int childX = contentX, childY = contentY;
        int childAvailW = contentW;
        if (cs.hasPosLeft) childX += cs.posLeft.Resolve(contentW, g_rootFontSize);
        else if (cs.hasPosRight) childX += max(0, contentW - cs.posRight.Resolve(contentW, g_rootFontSize));
        if (cs.hasPosTop) childY += cs.posTop.Resolve(max(0, out.h - s.paddingTop - s.paddingBottom), g_rootFontSize);
        else if (cs.hasPosBottom) childY += max(0, out.h - s.paddingTop - s.paddingBottom - cs.posBottom.Resolve(out.h, g_rootFontSize));

        if (cs.hasPosLeft && cs.hasPosRight && cs.width.kind == Length::Auto)
            childAvailW = max(0, contentW - cs.posLeft.Resolve(contentW, g_rootFontSize) - cs.posRight.Resolve(contentW, g_rootFontSize));
        else if (cs.width.kind != Length::Auto)
            childAvailW = max(0, cs.width.Resolve(contentW, g_rootFontSize));

        LayoutBox absBox;
        LayoutNode(dc, fc, child, s, cs, childX - cs.marginLeft, childY - cs.marginTop, childAvailW, absBox, rules);
        absBox.x = childX + cs.marginLeft;
        absBox.y = childY + cs.marginTop;
        out.children.push_back(std::move(absBox));
    }
}

// Materialize the commonly used CSS generated-content subset (::before/::after).
// This keeps generated content in the same DOM/layout/paint pipeline as ordinary nodes.
static wstring CSSContentText(const wstring& raw, Node* owner) {
    wstring v = Trim(raw); if (v == L"none" || v == L"normal" || v.empty())return L"";
    wstring out;
    for (size_t i = 0; i < v.size();) {
        if (v[i] == L'\'' || v[i] == L'"') { wchar_t q = v[i++]; while (i < v.size() && v[i] != q) { if (v[i] == L'\\' && i + 1 < v.size())++i; out += v[i++]; }if (i < v.size())++i; }
        else if (v.compare(i, 5, L"attr(") == 0) { size_t e = v.find(L')', i + 5); if (e == wstring::npos)break; wstring a = Trim(v.substr(i + 5, e - i - 5)); out += GetAttribute(owner, a); i = e + 1; }
        else if (iswspace(v[i]) || v[i] == L'+' || v[i] == L',')++i;
        else { size_t e = i; while (e < v.size() && !iswspace(v[e]) && v[e] != L'+')++e; wstring token = v.substr(i, e - i); if (token == L"open-quote")out += L"“"; else if (token == L"close-quote")out += L"”"; i = e; }
    }
    return out;
}
static void MaterializeGeneratedContent(Node* root, const vector<CSSRule>& rules) {
    if (!root)return;
    vector<Node*> original; function<void(Node*)> collect = [&](Node* n) {if (!n)return; original.push_back(n); for (Node* c : n->children)collect(c); }; collect(root);
    for (Node* n : original) {
        if (!n || n->tag == L"#text")continue;
        for (const wstring pseudo : {L"before", L"after"}) {
            for (const CSSRule& r : rules) {
                if (r.pseudoElement != pseudo || !Matches(n, r.selector, (int)r.selector.parts.size() - 1))continue;
                wstring content, style; for (const auto& d : r.decls) { if (d.first == L"content")content = d.second; else { if (!style.empty())style += L";"; style += d.first + L":" + d.second; } }
                wstring text = CSSContentText(content, n); if (text.empty() && content.find(L"url(") == wstring::npos)continue;
                Node* gen = new Node(); gen->tag = L"span"; gen->parent = n; gen->attributes[L"style"] = style; gen->attributes[L"data-minihtml-generated"] = pseudo;
                if (!text.empty()) { Node* t = new Node(); t->tag = L"#text"; t->text = text; t->parent = gen; gen->children.push_back(t); }
                if (pseudo == L"before")n->children.insert(n->children.begin(), gen); else n->children.push_back(gen);
            }
        }
    }
}

// ============================================================
// 5. RENDERER & BROWSER
// ============================================================
struct LinkRegion { RECT rect; wstring href; Node* element = nullptr; };

class HTMLBrowser {
public:
    Node* document = nullptr;
    vector<CSSRule> defaultRules, authorRules;
    wstring authorCSS;
    LayoutBox rootLayout;
    FontCache fonts;
    HDC measureDC = nullptr;
    int scrollY = 0, contentHeight = 0, viewportW = 800, viewportH = 600;
    wstring currentURL;
    vector<LinkRegion> links;
    Node* focusedControl = nullptr;
    int caretPosition = 0;
    wstring focusStartValue;
    unique_ptr<minihtml_js::Runtime> js;
    unordered_map<Node*, shared_ptr<minihtml_js::Object>> domObjects;
    vector<Node*> retiredNodes; // Keep detached nodes alive until page teardown; event callbacks may still reference them.
    unordered_map<Node*, unordered_map<wstring, vector<minihtml_js::Value>>> domListeners;
    unordered_map<wstring, vector<minihtml_js::Value>> windowListeners;
    minihtml_js::Value documentValue;
    minihtml_js::Value windowValue;
    struct JSTimer {
        minihtml_js::Value callback; bool interval = false;
#if !defined(_WIN32)
        DWORD delay = 1; std::chrono::steady_clock::time_point due{};
#endif
    };
    unordered_map<UINT_PTR, JSTimer> jsTimers;
    vector<shared_ptr<minihtml_js::WorkerState>> workers;
    static unordered_map<wstring,wstring> localStorageData;
    static unordered_map<wstring,wstring> sessionStorageData;

    wstring ValueOf(Node* n) const { return ControlValue(n); }
    bool IsFocusable(Node* n) const {
        if (!n || GetAttribute(n, L"disabled") == L"disabled" || n->attributes.count(L"disabled")) return false;
        return IsFormControlNode(n) || n->tag == L"a" || n->attributes.count(L"tabindex") != 0;
    }
    void FocusControl(Node* n, int requestedCaret = -1) {
        if (n && !IsFocusable(n)) n = nullptr;
        if (focusedControl == n) {
            if (n && requestedCaret >= 0) caretPosition = min((int)ValueOf(n).size(), requestedCaret);
            return;
        }
        if (focusedControl) {
            Node* old = focusedControl;
            if (ValueOf(old) != focusStartValue) DispatchEvent(old, L"change");
            DispatchEvent(old, L"blur");
        }
        focusedControl = n;
        if (n) {
            focusStartValue = ValueOf(n);
            caretPosition = requestedCaret >= 0 ? min((int)ValueOf(n).size(), requestedCaret) : (int)ValueOf(n).size();
            DispatchEvent(n, L"focus");
        } else { focusStartValue.clear(); caretPosition = 0; }
    }
    Node* FindForm(Node* n) const { for (Node* p = n; p; p = p->parent) if (p->tag == L"form") return p; return nullptr; }
    void SetControlValue(Node* n, const wstring& v) {
        if (!n) return;
        if (n->tag == L"textarea") SetTextContent(n, v);
        else n->attributes[L"value"] = v;
    }
    Node* FindById(const wstring& id) const {
        Node* found = nullptr;
        function<void(Node*)> walk = [&](Node* n) {if (!n || found)return; if (GetAttribute(n, L"id") == id && !id.empty()) { found = n; return; }for (Node* c : n->children)walk(c); };
        walk(document); return found;
    }
    Node* FindByTag(const wstring& tag) const {
        Node* found = nullptr; function<void(Node*)>walk = [&](Node* n) {if (!n || found)return; if (n->tag == tag) { found = n; return; }for (Node* c : n->children)walk(c); }; walk(document); return found;
    }
    wstring GetTextContent(Node* n) const {
        if (!n)return L""; if (n->tag == L"#text")return n->text; wstring out; for (Node* c : n->children)out += GetTextContent(c); return out;
    }
    void RetireChildren(Node* n) { if(!n)return; for(Node* c:n->children){c->parent=nullptr;retiredNodes.push_back(c);} n->children.clear(); }
    void SetTextContent(Node* n, const wstring& value) {
        if (!n)return; RetireChildren(n); Node* t = new Node(); t->tag = L"#text"; t->text = value; t->parent = n; n->children.push_back(t);
    }
    void SetInlineStyle(Node* n, const wstring& key, const wstring& value) {
        if (!n)return; wstring css = GetAttribute(n, L"style"), prop;
        for (wchar_t c : key) { if (iswupper(c)) { prop += L'-'; prop += towlower(c); } else prop += c; }
        vector<pair<wstring, wstring>> decls; size_t p = 0;
        while (p < css.size()) { size_t e = css.find(L';', p); if (e == wstring::npos)e = css.size(); wstring d = Trim(css.substr(p, e - p)); size_t c = d.find(L':'); if (c != wstring::npos && Lower(Trim(d.substr(0, c))) != prop)decls.push_back({ Lower(Trim(d.substr(0,c))),Trim(d.substr(c + 1)) }); p = e + 1; }
        decls.push_back({ prop,value }); wstring rebuilt; for (auto& d : decls) { if (!rebuilt.empty())rebuilt += L"; "; rebuilt += d.first + L": " + d.second; }n->attributes[L"style"] = rebuilt;
    }
    bool MatchesDOMSelector(Node* n, wstring selector) const {
        if (!n || n->tag == L"#text") return false;
        selector = Trim(selector); if (selector.empty()) return false;
        // This DOM-query subset supports the compound selectors used by LearnFeed:
        // tag, #id, .class, [attribute], [attribute=value], and common form pseudos.
        size_t pseudo = selector.find(L':'); wstring pseudoName; if (pseudo != wstring::npos) { pseudoName = Lower(selector.substr(pseudo + 1)); selector = selector.substr(0, pseudo); }
        bool ok = true; size_t lb = selector.find(L'['); wstring head = lb == wstring::npos ? selector : selector.substr(0, lb);
        if (!head.empty()) {
            if (head[0] == L'#') ok = GetAttribute(n, L"id") == head.substr(1);
            else if (head[0] == L'.') ok = (L" " + GetAttribute(n, L"class") + L" ").find(L" " + head.substr(1) + L" ") != wstring::npos;
            else ok = n->tag == Lower(head);
        }
        size_t pos = lb;
        while (ok && pos != wstring::npos && pos < selector.size()) {
            size_t end = selector.find(L']', pos + 1); if (end == wstring::npos) return false;
            wstring expr = Trim(selector.substr(pos + 1, end - pos - 1)); size_t eq = expr.find(L'=');
            wstring name = Lower(Trim(expr.substr(0, eq)));
            if (!n->attributes.count(name)) ok = false;
            else if (eq != wstring::npos) { wstring expected = Trim(expr.substr(eq + 1)); if (expected.size() >= 2 && ((expected.front()==L'"' && expected.back()==L'"') || (expected.front()==L'\'' && expected.back()==L'\''))) expected=expected.substr(1,expected.size()-2); if (GetAttribute(n,name) != expected) ok=false; }
            pos = selector.find(L'[', end + 1);
        }
        if (!ok) return false;
        if (pseudoName == L"checked") return n->attributes.count(L"checked") != 0;
        if (pseudoName == L"disabled") return n->attributes.count(L"disabled") != 0;
        if (pseudoName == L"required") return n->attributes.count(L"required") != 0;
        if (pseudoName == L"first-child") return n->parent && !n->parent->children.empty() && n->parent->children.front() == n;
        if (pseudoName == L"last-child") return n->parent && !n->parent->children.empty() && n->parent->children.back() == n;
        return pseudoName.empty();
    }
    minihtml_js::Value WrapNode(Node* n) {
        using namespace minihtml_js;
        if (!n)return Value::NullValue(); auto it = domObjects.find(n); if (it != domObjects.end())return Value::Obj(it->second);
        auto o = make_shared<Object>(); o->native = n;
        if (js) {
            auto getPrototype = [&](const wstring& ctorName)->shared_ptr<Object> { Value ctor = js->GetGlobal(ctorName); if (ctor.type == Value::FunctionValue && ctor.function) { auto it = ctor.function->properties.find(L"prototype"); if (it != ctor.function->properties.end() && it->second.type == Value::ObjectValue) return it->second.object; } return nullptr; };
            static const vector<wstring> knownHtmlTags = { L"html",L"head",L"body",L"title",L"meta",L"link",L"style",L"script",L"noscript",L"div",L"span",L"p",L"a",L"img",L"input",L"button",L"select",L"option",L"textarea",L"form",L"label",L"fieldset",L"legend",L"h1",L"h2",L"h3",L"h4",L"h5",L"h6",L"ul",L"ol",L"li",L"table",L"thead",L"tbody",L"tfoot",L"tr",L"td",L"th",L"canvas",L"video",L"audio",L"source",L"iframe",L"main",L"section",L"article",L"nav",L"header",L"footer",L"aside",L"strong",L"em",L"b",L"i",L"small",L"br",L"hr",L"pre",L"code",L"blockquote",L"progress",L"meter",L"details",L"summary",L"template",L"picture",L"svg" };
            if (n->tag == L"#document") o->prototype = getPrototype(L"Node");
            else o->prototype = getPrototype(std::find(knownHtmlTags.begin(), knownHtmlTags.end(), n->tag) != knownHtmlTags.end() ? L"HTMLElement" : L"HTMLUnknownElement");
        }
        domObjects[n] = o;
        o->getter = [this, n](Runtime&, const wstring& key)->Value {
            using namespace minihtml_js;
            if (key == L"tagName" || key == L"nodeName")return Value::Str(n->tag == L"#text" ? L"#text" : wstring(n->tag.begin(), n->tag.end()));
            if (key == L"id" || key == L"className" || key == L"href" || key == L"src" || key == L"title" || key == L"name")return Value::Str(GetAttribute(n, key == L"className" ? L"class" : key));
            if (key == L"type") return Value::Str(n->tag == L"input" ? InputType(n) : (n->tag == L"button" ? (GetAttribute(n, L"type").empty() ? L"submit" : GetAttribute(n, L"type")) : GetAttribute(n, L"type")));
            if (key == L"value") return Value::Str(ControlValue(n));
            if (key == L"hidden") return Value::Bool(n->attributes.count(L"hidden") != 0);
            if (key == L"dataset") { auto data=make_shared<Object>(); for(const auto& kv:n->attributes) if(kv.first.rfind(L"data-",0)==0) { wstring k=kv.first.substr(5); for(size_t i=0;i<k.size();++i) if(k[i]==L'-' && i+1<k.size()){ k.erase(i,1); k[i]=(wchar_t)towupper(k[i]); } data->properties[k]=Value::Str(kv.second); } return Value::Obj(data); }
            if (key == L"classList") { auto cl=make_shared<Object>(); cl->properties[L"add"] = js->MakeNative([n](Runtime&,const vector<Value>& a,const Value&){ wstring cls=L" "+GetAttribute(n,L"class")+L" "; for(const auto& v:a){wstring x=ToString(v); if(cls.find(L" "+x+L" ")==wstring::npos) cls+=x+L" ";} n->attributes[L"class"]=Trim(cls); return Value::Undef(); }); cl->properties[L"remove"] = js->MakeNative([n](Runtime&,const vector<Value>& a,const Value&){ wstring cls=L" "+GetAttribute(n,L"class")+L" "; for(const auto& v:a){wstring x=L" "+ToString(v)+L" "; size_t p; while((p=cls.find(x))!=wstring::npos) cls.erase(p,x.size());} n->attributes[L"class"]=Trim(cls); return Value::Undef(); }); cl->properties[L"contains"] = js->MakeNative([n](Runtime&,const vector<Value>& a,const Value&){return Value::Bool(!a.empty() && (L" "+GetAttribute(n,L"class")+L" ").find(L" "+ToString(a[0])+L" ")!=wstring::npos); }); cl->properties[L"toggle"] = js->MakeNative([n](Runtime&,const vector<Value>& a,const Value&){ if(a.empty())return Value::Bool(false); wstring x=ToString(a[0]), cls=L" "+GetAttribute(n,L"class")+L" "; bool has=cls.find(L" "+x+L" ")!=wstring::npos; bool force=a.size()>1 ? Truthy(a[1]) : !has; if(force&&!has)cls+=x+L" "; if(!force&&has){size_t p; while((p=cls.find(L" "+x+L" "))!=wstring::npos)cls.erase(p,x.size()+1);} n->attributes[L"class"]=Trim(cls); return Value::Bool(force); }); return Value::Obj(cl); }
            if (key == L"checked" || key == L"disabled" || key == L"required" || key == L"readOnly") return Value::Bool(n->attributes.count(key == L"readOnly" ? L"readonly" : key) != 0);
            if (key == L"placeholder" || key == L"min" || key == L"max" || key == L"step") return Value::Str(GetAttribute(n, Lower(key)));
            if (key == L"selectionStart" || key == L"selectionEnd") return Value::Num(n == focusedControl ? caretPosition : (double)ControlValue(n).size());
            if (key == L"selectionDirection") return Value::Str(L"none");
            if (key == L"innerHTML" || key == L"textContent" || key == L"innerText")return Value::Str(GetTextContent(n));
            if (key == L"children" || key == L"childNodes") { auto a = make_shared<Object>(); a->array = true; for (Node* c : n->children)a->elements.push_back(WrapNode(c)); return Value::Obj(a); }
            if (key == L"firstChild" || key == L"firstElementChild")return n->children.empty() ? Value::NullValue() : WrapNode(n->children.front());
            if (key == L"lastChild" || key == L"lastElementChild")return n->children.empty() ? Value::NullValue() : WrapNode(n->children.back());
            if (key == L"nextSibling" || key == L"previousSibling") { if (!n->parent)return Value::NullValue(); auto& sib = n->parent->children; auto it = std::find(sib.begin(), sib.end(), n); if (it == sib.end())return Value::NullValue(); if (key == L"nextSibling")return (++it == sib.end()) ? Value::NullValue() : WrapNode(*it); return it == sib.begin() ? Value::NullValue() : WrapNode(*(--it)); }
            if (key == L"parentNode" || key == L"parentElement")return WrapNode(n->parent);
            if (key == L"style") {
                auto st = make_shared<Object>(); st->getter = [this, n](Runtime&, const wstring& prop) {if (prop == L"cssText")return minihtml_js::Value::Str(GetAttribute(n, L"style")); wstring css = GetAttribute(n, L"style"), name; for (wchar_t c : prop) { if (iswupper(c)) { name += L'-'; name += towlower(c); } else name += c; }size_t p = 0; while (p < css.size()) { size_t e = css.find(L';', p); if (e == wstring::npos)e = css.size(); wstring d = Trim(css.substr(p, e - p)); size_t c = d.find(L':'); if (c != wstring::npos && Lower(Trim(d.substr(0, c))) == name)return minihtml_js::Value::Str(Trim(d.substr(c + 1))); p = e + 1; }return minihtml_js::Value::Undef(); };
                st->setter = [this, n](Runtime&, const wstring& prop, const Value& v) {if (prop == L"cssText")n->attributes[L"style"] = ToString(v); else SetInlineStyle(n, prop, ToString(v)); };
                st->properties[L"getPropertyValue"] = js->MakeNative([this, n](Runtime&, const vector<Value>& a, const Value&) {if (a.empty())return minihtml_js::Value::Str(L""); return minihtml_js::Value::Str([&]() {wstring name = Lower(minihtml_js::ToString(a[0])), css = GetAttribute(n, L"style"); size_t p = 0; while (p < css.size()) { size_t e = css.find(L';', p); if (e == wstring::npos)e = css.size(); wstring d = Trim(css.substr(p, e - p)); size_t c = d.find(L':'); if (c != wstring::npos && Lower(Trim(d.substr(0, c))) == name)return Trim(d.substr(c + 1)); p = e + 1; }return wstring(); }()); });
                st->properties[L"setProperty"] = js->MakeNative([this, n](Runtime&, const vector<Value>& a, const Value&) {if (a.size() > 1)SetInlineStyle(n, minihtml_js::ToString(a[0]), minihtml_js::ToString(a[1])); return minihtml_js::Value::Undef(); });
                st->properties[L"removeProperty"] = js->MakeNative([this, n](Runtime&, const vector<Value>& a, const Value&) {if (!a.empty())SetInlineStyle(n, minihtml_js::ToString(a[0]), L""); return minihtml_js::Value::Undef(); });
                return Value::Obj(st);
            }
            if (key == L"onclick" || key == L"onchange" || key == L"oninput") { auto it = domListeners[n].find(key.substr(2)); if (it != domListeners[n].end() && !it->second.empty())return it->second.back(); }
            if (key == L"children" || key == L"childNodes")return Value::Undef();
            auto a = n->attributes.find(key); if (a != n->attributes.end())return Value::Str(a->second);
            return Value::Undef();
            };
        o->has = [n](Runtime&, const wstring& key) {return n->attributes.count(key) != 0 || key == L"id" || key == L"className" || key == L"innerHTML" || key == L"textContent" || key == L"appendChild" || key == L"removeChild" || key == L"setAttribute" || key == L"getAttribute" || key == L"addEventListener" || key == L"tagName" || key == L"parentNode" || key == L"style" || key == L"firstChild" || key == L"lastChild" || key == L"firstElementChild" || key == L"lastElementChild" || key == L"nextSibling" || key == L"previousSibling"; };
        o->setter = [this, n](Runtime&, const wstring& key, const Value& v) {
            if (key == L"textContent" || key == L"innerText") { SetTextContent(n, minihtml_js::ToString(v)); return; }
            if (key == L"innerHTML") { RetireChildren(n); Node* frag = ParseHTML(minihtml_js::ToString(v)); for (Node* c : frag->children) { c->parent = n; n->children.push_back(c); }frag->children.clear(); delete frag; return; }
            if (key == L"className")n->attributes[L"class"] = minihtml_js::ToString(v);
            else if (key == L"hidden") { if (minihtml_js::Truthy(v)) n->attributes[L"hidden"] = L""; else n->attributes.erase(L"hidden"); }
            else if (key == L"value") { if (n->tag == L"textarea")SetTextContent(n, minihtml_js::ToString(v)); else n->attributes[L"value"] = minihtml_js::ToString(v); }
            else if (key == L"checked" || key == L"disabled" || key == L"required") { if (minihtml_js::Truthy(v))n->attributes[key] = L""; else n->attributes.erase(key); }
            else if (key == L"onclick" || key == L"onchange" || key == L"oninput" || key == L"onfocus" || key == L"onblur" || key == L"onkeydown" || key == L"onkeyup" || key == L"onkeypress")domListeners[n][key.substr(2)] = { v };
            else n->attributes[key] = minihtml_js::ToString(v);
            };
        auto addMethod = [&](const wstring& name, auto fn) {o->properties[name] = js->MakeNative(fn); };
        addMethod(L"appendChild", [this, n](Runtime&, const vector<Value>& a, const Value&) {if (a.empty() || a[0].type != Value::ObjectValue || !a[0].object)return Value::NullValue(); Node* c = (Node*)a[0].object->native; if (!c)return Value::NullValue(); if (c->parent) { auto& v = c->parent->children; v.erase(std::remove(v.begin(), v.end(), c), v.end()); }c->parent = n; n->children.push_back(c);
        if (c->tag == L"script") { wstring src = GetAttribute(c, L"src"); if (!src.empty()) { HTTPResponse response = FetchURL(ResolveURL(GetAttribute(c, L"base"), src)); if (response.ok && js)js->Execute(response.body); } }
        else if (c->tag == L"link") { wstring rel = Lower(GetAttribute(c, L"rel")); if (rel.find(L"stylesheet") != wstring::npos) { HTTPResponse response = FetchURL(ResolveURL(GetAttribute(c, L"base"), GetAttribute(c, L"href"))); if (response.ok) { auto parsed = ParseCSS(response.body); authorRules.insert(authorRules.end(), parsed.begin(), parsed.end()); } } }
        return WrapNode(c); });
        addMethod(L"removeChild", [this, n](Runtime&, const vector<Value>& a, const Value&) {if (a.empty() || a[0].type != Value::ObjectValue)return Value::NullValue(); Node* c = (Node*)a[0].object->native; auto& v = n->children; auto it = std::find(v.begin(), v.end(), c); if (it != v.end()) { v.erase(it); c->parent = nullptr; }return WrapNode(c); });
        addMethod(L"setAttribute", [n](Runtime&, const vector<Value>& a, const Value&) {if (a.size() > 1) { wstring k = Lower(minihtml_js::ToString(a[0])); if (k == L"classname")k = L"class"; n->attributes[k] = minihtml_js::ToString(a[1]); }return Value::Undef(); });
        addMethod(L"removeAttribute", [n](Runtime&, const vector<Value>& a, const Value&) {if (!a.empty())n->attributes.erase(Lower(minihtml_js::ToString(a[0]))); return Value::Undef(); });
        addMethod(L"hasAttribute", [n](Runtime&, const vector<Value>& a, const Value&) {return minihtml_js::Value::Bool(!a.empty() && n->attributes.count(Lower(minihtml_js::ToString(a[0]))) != 0); });
        addMethod(L"getAttribute", [n](Runtime&, const vector<Value>& a, const Value&) {if (a.empty())return Value::NullValue(); auto it = n->attributes.find(minihtml_js::ToString(a[0])); return it == n->attributes.end() ? Value::NullValue() : Value::Str(it->second); });
        addMethod(L"addEventListener", [this, n](Runtime&, const vector<Value>& a, const Value&) {if (a.size() > 1)domListeners[n][minihtml_js::ToString(a[0])].push_back(a[1]); return Value::Undef(); });
        addMethod(L"removeEventListener", [this, n](Runtime&, const vector<Value>& a, const Value&) {if (a.size() > 1) { auto& v = domListeners[n][minihtml_js::ToString(a[0])]; v.erase(std::remove_if(v.begin(), v.end(), [&](const Value& x) {return x.function == a[1].function; }), v.end()); }return Value::Undef(); });
        addMethod(L"click", [this, n](Runtime&, const vector<Value>&, const Value&) {DispatchEvent(n, L"click"); return Value::Undef(); });
        addMethod(L"focus", [this, n](Runtime&, const vector<Value>&, const Value&) {FocusControl(n); return Value::Undef(); });
        addMethod(L"blur", [this, n](Runtime&, const vector<Value>&, const Value&) {if (focusedControl == n)FocusControl(nullptr); return Value::Undef(); });
        addMethod(L"select", [this, n](Runtime&, const vector<Value>&, const Value&) {FocusControl(n, 0); caretPosition = (int)ValueOf(n).size(); return Value::Undef(); });
        addMethod(L"matches", [this,n](Runtime&,const vector<Value>& a,const Value&){return Value::Bool(!a.empty() && MatchesDOMSelector(n,ToString(a[0])));});
        addMethod(L"closest", [this,n](Runtime&,const vector<Value>& a,const Value&){if(a.empty())return Value::NullValue(); for(Node* p=n;p;p=p->parent)if(MatchesDOMSelector(p,ToString(a[0])))return WrapNode(p); return Value::NullValue();});
        addMethod(L"remove", [n](Runtime&,const vector<Value>&,const Value&){if(n->parent){auto& v=n->parent->children;v.erase(std::remove(v.begin(),v.end(),n),v.end());n->parent=nullptr;}return Value::Undef();});
        addMethod(L"insertAdjacentHTML", [this,n](Runtime&,const vector<Value>& a,const Value&){if(a.size()<2)return Value::Undef(); wstring where=Lower(ToString(a[0])); Node* frag=ParseHTML(ToString(a[1])); vector<Node*> add=frag->children; frag->children.clear(); delete frag; if(where==L"afterbegin"){for(auto it=add.rbegin();it!=add.rend();++it){(*it)->parent=n;n->children.insert(n->children.begin(),*it);}}else if(where==L"beforeend"){for(Node* c:add){c->parent=n;n->children.push_back(c);}}else if(n->parent){auto& siblings=n->parent->children;auto at=std::find(siblings.begin(),siblings.end(),n);if(where==L"beforebegin"){for(Node* c:add){c->parent=n->parent;at=siblings.insert(at,c);++at;}}else if(where==L"afterend"){++at;for(Node* c:add){c->parent=n->parent;at=siblings.insert(at,c);++at;}}}return Value::Undef();});
        auto elementQuery = [this,n](const vector<Value>& a,bool all)->Value { auto arr=make_shared<Object>(); arr->array=true; Node* found=nullptr; function<void(Node*)> walk=[&](Node* x){for(Node* c:x->children){if(MatchesDOMSelector(c,a.empty()?L"":ToString(a[0]))){if(!all){found=c;return;}arr->elements.push_back(WrapNode(c));}walk(c);if(found)return;}}; walk(n); return all?Value::Obj(arr):WrapNode(found); };
        addMethod(L"querySelector", [elementQuery](Runtime&,const vector<Value>& a,const Value&){return elementQuery(a,false);});
        addMethod(L"querySelectorAll", [elementQuery](Runtime&,const vector<Value>& a,const Value&){return elementQuery(a,true);});
        return Value::Obj(o);
    }
    bool DispatchEvent(Node* n, const wstring& type, const wstring& key = L"") {
        if (!js || !n)return true;
        auto ev = js->MakeObject(); auto obj = ev.object;
        obj->properties[L"type"] = minihtml_js::Value::Str(type);
        obj->properties[L"target"] = WrapNode(n);
        obj->properties[L"currentTarget"] = WrapNode(n);
        obj->properties[L"bubbles"] = minihtml_js::Value::Bool(true);
        obj->properties[L"cancelable"] = minihtml_js::Value::Bool(true);
        obj->properties[L"defaultPrevented"] = minihtml_js::Value::Bool(false);
        obj->properties[L"key"] = minihtml_js::Value::Str(key);
        obj->properties[L"preventDefault"] = js->MakeNative([obj](minihtml_js::Runtime&, const vector<minihtml_js::Value>&, const minihtml_js::Value&) {obj->properties[L"defaultPrevented"] = minihtml_js::Value::Bool(true); return minihtml_js::Value::Undef(); });
        obj->properties[L"stopPropagation"] = js->MakeNative([obj](minihtml_js::Runtime&, const vector<minihtml_js::Value>&, const minihtml_js::Value&) {obj->properties[L"__stopPropagation"] = minihtml_js::Value::Bool(true); return minihtml_js::Value::Undef(); });
        bool propagationStopped = false;
        for (Node* current = n; current; current = current->parent) {
            obj->properties[L"currentTarget"] = WrapNode(current);
            auto it = domListeners.find(current);
            if (it != domListeners.end()) {
                auto hi = it->second.find(type);
                if (hi != it->second.end()) {
                    auto handlers = hi->second;
                    for (auto& h : handlers) {
                        try { js->Invoke(h, { ev }, WrapNode(current)); }
                        catch (const std::exception& e) { OutputDebugStringA((string("minihtml event JS error: ") + e.what() + "\n").c_str()); }
                    }
                }
            }
            auto stop = obj->properties.find(L"__stopPropagation");
            if (stop != obj->properties.end() && minihtml_js::Truthy(stop->second)) { propagationStopped = true; break; }
            if (current == document || current->tag == L"#document") break;
        }
        // Window listeners run after the document bubbling phase. Also honor
        // window.onclick-style event handler properties.
        obj->properties[L"currentTarget"] = windowValue;
        auto windowIt = windowListeners.find(type);
        if (windowIt != windowListeners.end()) {
            auto handlers = windowIt->second;
            for (auto& h : handlers) {
                try { js->Invoke(h, { ev }, windowValue); }
                catch (const std::exception& e) { OutputDebugStringA((string("minihtml window event JS error: ") + e.what() + "\n").c_str()); }
            }
        }
        minihtml_js::Value propertyHandler = js->GetGlobal(L"on" + type);
        if (propertyHandler.type == minihtml_js::Value::FunctionValue) {
            try { js->Invoke(propertyHandler, { ev }, windowValue); }
            catch (const std::exception& e) { OutputDebugStringA((string("minihtml window handler JS error: ") + e.what() + "\n").c_str()); }
        }
        auto p = obj->properties.find(L"defaultPrevented"); return p == obj->properties.end() || !minihtml_js::Truthy(p->second);
    }
    bool ActivateElement(Node* target) {
        if (!target) return false;
        Node* control = target;
        while (control && !IsFormControlNode(control)) control = control->parent;
        if (control && IsFocusable(control)) FocusControl(control);
        bool allowDefault = DispatchEvent(target, L"click");
        if (!allowDefault || !control) return allowDefault;
        if (control->attributes.count(L"disabled")) return false;
        wstring type = InputType(control);
        if (control->tag == L"input" && (type == L"checkbox" || type == L"radio")) {
            if (type == L"radio") {
                wstring name = GetAttribute(control, L"name");
                if (control->parent) for (Node* sibling : control->parent->children)
                    if (sibling != control && sibling->tag == L"input" && InputType(sibling) == L"radio" && GetAttribute(sibling, L"name") == name) sibling->attributes.erase(L"checked");
                control->attributes[L"checked"] = L"";
            } else if (control->attributes.count(L"checked")) control->attributes.erase(L"checked");
            else control->attributes[L"checked"] = L"";
            DispatchEvent(control, L"input"); DispatchEvent(control, L"change");
        } else if ((control->tag == L"button" && GetAttribute(control, L"type") != L"button" && GetAttribute(control, L"type") != L"reset") ||
                   (control->tag == L"input" && (type == L"submit" || type == L"image"))) {
            Node* form = FindForm(control);
            if (form) DispatchEvent(form, L"submit");
        }
        return allowDefault;
    }
    Node* HitTestBox(const LayoutBox& box, int x, int y) const {
        if (box.style.display == Style::None || box.style.visibilityHidden) return nullptr;
        for (auto it = box.children.rbegin(); it != box.children.rend(); ++it) {
            Node* hit = HitTestBox(*it, x, y); if (hit) return hit;
        }
        for (auto it = box.frags.rbegin(); it != box.frags.rend(); ++it) {
            const Fragment& f = *it;
            if (x >= f.x && x < f.x + f.w && y >= f.y && y < f.y + f.h) {
                if (f.controlNode) return f.controlNode;
                if (f.linkNode) return f.linkNode;
            }
        }
        if (box.node && x >= box.x && x < box.x + box.w && y >= box.y && y < box.y + box.h) {
            if (IsFormControlNode(box.node)) return box.node;
            auto li = domListeners.find(box.node);
            if (li != domListeners.end() && (li->second.count(L"click") || li->second.count(L"mousedown"))) return box.node;
        }
        return nullptr;
    }
    bool InsertText(const wstring& text) {
        Node* n = focusedControl;
        if (!IsTextEntryNode(n) || GetAttribute(n, L"disabled") == L"disabled" || n->attributes.count(L"readonly")) return false;
        bool allowed = DispatchEvent(n, L"keypress", text); if (!allowed) return true;
        allowed = DispatchEvent(n, L"beforeinput"); if (!allowed) return true;
        wstring value = ValueOf(n);
        size_t maxLen = n->attributes.count(L"maxlength") ? (size_t)max(0, _wtoi(GetAttribute(n, L"maxlength").c_str())) : 1000000;
        size_t at = (size_t)max(0, min((int)value.size(), caretPosition));
        wstring next = value; next.insert(at, text);
        if (next.size() > maxLen) next.resize(maxLen);
        SetControlValue(n, next); caretPosition = min((int)next.size(), caretPosition + (int)text.size());
        DispatchEvent(n, L"input"); Relayout(viewportW); return true;
    }
    bool HandleControlKey(int key, bool shift = false, bool control = false) {
        Node* n = focusedControl; if (!n) return false;
        DispatchEvent(n, L"keydown", std::to_wstring(key));
        if (control && (key == 'a' || key == 'A')) { caretPosition = (int)ValueOf(n).size(); DispatchEvent(n, L"keyup", L"a"); return true; }
        if (!IsTextEntryNode(n)) { if (key == 13) { DispatchEvent(n, L"click"); } DispatchEvent(n, L"keyup", std::to_wstring(key)); return true; }
        wstring value = ValueOf(n); bool changed = false;
        if (key == 8 && caretPosition > 0) { value.erase((size_t)caretPosition - 1, 1); --caretPosition; changed = true; }
        else if (key == 127 && caretPosition < (int)value.size()) { value.erase((size_t)caretPosition, 1); changed = true; }
        else if (key == 1000 || key == 1000) caretPosition = max(0, caretPosition - 1);
        else if (key == 1001 || key == 1001) caretPosition = min((int)value.size(), caretPosition + 1);
        else if (key ==  1002 || key == 1002) caretPosition = 0;
        else if (key == 1003 || key == 1003) caretPosition = (int)value.size();
        else if (key == 13) {
            if (n->tag == L"textarea") { InsertText(L"\n"); }
            else { DispatchEvent(n, L"change"); Node* form = FindForm(n); if (form) DispatchEvent(form, L"submit"); }
        }
        if (changed) { SetControlValue(n, value); DispatchEvent(n, L"input"); Relayout(viewportW); }
        DispatchEvent(n, L"keyup", std::to_wstring(key)); return true;
    }
    void StopWorkers() {
        for (auto& worker : workers) {
            if (!worker) continue;
            worker->terminated.store(true);
            worker->condition.notify_all();
            if (worker->thread.joinable()) worker->thread.detach();
        }
        workers.clear();
    }
    void PumpJSTasks() { if (js) js->PumpTasks(); }
    UINT_PTR AddJSTimer(const minihtml_js::Value& callback, DWORD delay, bool interval) {
#if defined(_WIN32)
        UINT_PTR id = SetTimer(nullptr, 0, max<DWORD>(1, delay), nullptr); if (id)jsTimers[id] = { callback,interval }; return id;
#else
        static UINT_PTR nextId = 1; UINT_PTR id = nextId++; JSTimer t; t.callback = callback; t.interval = interval; t.delay = std::max<DWORD>(1, delay); t.due = std::chrono::steady_clock::now() + std::chrono::milliseconds(t.delay); jsTimers[id] = t; return id;
#endif
    }
    void ClearJSTimer(UINT_PTR id) {
#if defined(_WIN32)
        KillTimer(nullptr, id);
#endif
        jsTimers.erase(id);
    }
    void HandleDueJSTimers() {
#if !defined(_WIN32)
        auto now = std::chrono::steady_clock::now(); std::vector<UINT_PTR> due; for (auto& kv : jsTimers)if (kv.second.due <= now)due.push_back(kv.first); for (UINT_PTR id : due) { HandleJSTimer(id); auto it = jsTimers.find(id); if (it != jsTimers.end() && it->second.interval)it->second.due = std::chrono::steady_clock::now() + std::chrono::milliseconds(it->second.delay); }
#endif
    }
    void HandleJSTimer(UINT_PTR id) {
        auto it = jsTimers.find(id);
        if (it == jsTimers.end() || !js)return;
        minihtml_js::Value cb = it->second.callback;
        bool repeat = it->second.interval;
        try {
            if (cb.type == minihtml_js::Value::FunctionValue) js->Invoke(cb, {});
            else if (cb.type == minihtml_js::Value::String) js->Execute(cb.string);
        }
        catch (const std::exception& e) {
            OutputDebugStringA((string("minihtml timer JS error: ") + e.what() + "\n").c_str());
        }
        if (!repeat) ClearJSTimer(id);
        Relayout(viewportW);
    }
    void InitializeJS() {
        using namespace minihtml_js; js = std::make_unique<Runtime>(); domObjects.clear(); domListeners.clear();
        auto doc = make_shared<Object>(); documentValue = Value::Obj(doc);
        doc->getter = [this](Runtime&, const wstring& key)->Value {
            if (key == L"body")return WrapNode(FindByTag(L"body")); if (key == L"head")return WrapNode(FindByTag(L"head")); if (key == L"documentElement")return WrapNode(FindByTag(L"html"));
            if (key == L"activeElement") return WrapNode(focusedControl ? focusedControl : FindByTag(L"body"));
            if (key == L"compatMode") return Value::Str(L"CSS1Compat");
            if (key == L"title") { Node* n = FindByTag(L"title"); return Value::Str(GetTextContent(n)); }
            return Value::Undef();
            };
        doc->has = [](Runtime&, const wstring& key) {return key == L"body" || key == L"head" || key == L"documentElement" || key == L"activeElement" || key == L"compatMode" || key == L"title" || key == L"getElementById" || key == L"createElement" || key == L"createTextNode" || key == L"getElementsByTagName" || key == L"getElementsByClassName" || key == L"querySelector" || key == L"querySelectorAll" || key == L"addEventListener" || key == L"write"; };
        doc->setter = [this](Runtime&, const wstring& key, const Value& v) {if (key == L"title") { Node* n = FindByTag(L"title"); if (n)SetTextContent(n, ToString(v)); }};
        doc->properties[L"getElementById"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {return a.empty() ? Value::NullValue() : WrapNode(FindById(ToString(a[0]))); });
        doc->properties[L"createElement"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {Node* n = new Node(); n->tag = a.empty() ? L"div" : Lower(ToString(a[0])); n->attributes[L"base"] = currentURL; return WrapNode(n); });
        doc->properties[L"createTextNode"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {Node* n = new Node(); n->tag = L"#text"; n->text = a.empty() ? L"" : ToString(a[0]); return WrapNode(n); });
        doc->properties[L"getElementsByTagName"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {auto arr = make_shared<Object>(); arr->array = true; if (!a.empty()) { wstring tag = Lower(ToString(a[0])); function<void(Node*)>walk = [&](Node* n) {if (!n)return; if (n->tag == tag)arr->elements.push_back(WrapNode(n)); for (Node* c : n->children)walk(c); }; walk(document); }return Value::Obj(arr); });
        doc->properties[L"getElementsByClassName"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {auto arr = make_shared<Object>(); arr->array = true; if (!a.empty()) { wstring cls = ToString(a[0]); function<void(Node*)>walk = [&](Node* n) {if (!n)return; wstring c = L" " + GetAttribute(n, L"class") + L" "; if (c.find(L" " + cls + L" ") != wstring::npos)arr->elements.push_back(WrapNode(n)); for (Node* x : n->children)walk(x); }; walk(document); }return Value::Obj(arr); });
        auto queryMatches = [this](Node* n, const wstring& q) { return MatchesDOMSelector(n,q); };
        doc->properties[L"querySelector"] = js->MakeNative([this, queryMatches](Runtime&, const vector<Value>& a, const Value&) {if (a.empty())return Value::NullValue(); wstring q = ToString(a[0]); Node* found = nullptr; function<void(Node*)>walk = [&](Node* n) {if (!n || found)return; if (queryMatches(n, q)) { found = n; return; }for (Node* c : n->children)walk(c); }; walk(document); return WrapNode(found); });
        doc->properties[L"querySelectorAll"] = js->MakeNative([this, queryMatches](Runtime&, const vector<Value>& a, const Value&) {auto arr = make_shared<Object>(); arr->array = true; if (!a.empty()) { wstring q = ToString(a[0]); function<void(Node*)>walk = [&](Node* n) {if (!n)return; if (queryMatches(n, q))arr->elements.push_back(WrapNode(n)); for (Node* c : n->children)walk(c); }; walk(document); }return Value::Obj(arr); });
        doc->properties[L"addEventListener"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {if (a.size() > 1 && ToString(a[0]) == L"DOMContentLoaded")js->SetGlobal(L"__DOMContentLoaded", a[1]); return Value::Undef(); });
        doc->properties[L"write"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {Node* b = FindByTag(L"body"); if (!b)return Value::Undef(); Node* f = ParseHTML(a.empty() ? L"" : ToString(a[0])); for (Node* c : f->children) { c->parent = b; b->children.push_back(c); }f->children.clear(); delete f; return Value::Undef(); });
        js->Define(L"document", documentValue);
        auto makeStorage = [this](unordered_map<wstring,wstring>* store) { auto st=make_shared<Object>(); st->getter=[store](Runtime&,const wstring& k){if(k==L"length")return Value::Num((double)store->size());return Value::Undef();}; st->properties[L"getItem"]=js->MakeNative([store](Runtime&,const vector<Value>& a,const Value&){if(a.empty())return Value::NullValue();auto it=store->find(ToString(a[0]));return it==store->end()?Value::NullValue():Value::Str(it->second);}); st->properties[L"setItem"]=js->MakeNative([store](Runtime&,const vector<Value>& a,const Value&){if(a.size()>1)(*store)[ToString(a[0])]=ToString(a[1]);return Value::Undef();}); st->properties[L"removeItem"]=js->MakeNative([store](Runtime&,const vector<Value>& a,const Value&){if(!a.empty())store->erase(ToString(a[0]));return Value::Undef();}); st->properties[L"clear"]=js->MakeNative([store](Runtime&,const vector<Value>&,const Value&){store->clear();return Value::Undef();}); st->properties[L"key"]=js->MakeNative([store](Runtime&,const vector<Value>& a,const Value&){if(a.empty())return Value::NullValue();size_t n=(size_t)max(0.0,ToNumber(a[0]));if(n>=store->size())return Value::NullValue();auto it=store->begin();std::advance(it,n);return Value::Str(it->first);}); return Value::Obj(st); };
        js->Define(L"localStorage",makeStorage(&localStorageData)); js->Define(L"sessionStorage",makeStorage(&sessionStorageData));
        // Basic DOM interface constructors and prototype chain, used by feature
        // detection such as `element instanceof HTMLElement`.
        auto nodePrototype = make_shared<Object>();
        auto elementPrototype = make_shared<Object>(); elementPrototype->prototype = nodePrototype;
        auto htmlPrototype = make_shared<Object>(); htmlPrototype->prototype = elementPrototype;
        auto unknownPrototype = make_shared<Object>(); unknownPrototype->prototype = htmlPrototype;
        auto makeInterface = [this](const wstring& name, const shared_ptr<Object>& proto) {
            auto ctor = js->MakeNative([](Runtime&, const vector<Value>&, const Value&) { return Value::Undef(); });
            ctor.function->properties[L"prototype"] = Value::Obj(proto); js->Define(name, ctor);
        };
        makeInterface(L"Node", nodePrototype); makeInterface(L"Element", elementPrototype);
        makeInterface(L"HTMLElement", htmlPrototype); makeInterface(L"HTMLUnknownElement", unknownPrototype);
        auto nav = make_shared<Object>();
#if defined(_WIN32)
        nav->properties[L"userAgent"] = Value::Str(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) minihtml/5.0"); nav->properties[L"platform"] = Value::Str(L"Win32");
#else
        nav->properties[L"userAgent"] = Value::Str(L"Mozilla/5.0 (X11; Linux x86_64) minihtml/5.0"); nav->properties[L"platform"] = Value::Str(L"Linux x86_64");
#endif
        js->Define(L"navigator", Value::Obj(nav));
        auto screen = make_shared<Object>(); screen->properties[L"width"] = Value::Num(GetSystemMetrics(SM_CXSCREEN)); screen->properties[L"height"] = Value::Num(GetSystemMetrics(SM_CYSCREEN)); js->Define(L"screen", Value::Obj(screen));
        auto win = make_shared<Object>(); windowValue = Value::Obj(win);
        win->getter = [this](Runtime&, const wstring& key)->Value {if (key == L"document")return documentValue; if (key == L"window" || key == L"self" || key == L"globalThis")return windowValue; if (key == L"navigator")return js->GetGlobal(L"navigator"); if (key == L"screen")return js->GetGlobal(L"screen"); if (key == L"location")return js->GetGlobal(L"location"); if (key == L"history")return js->GetGlobal(L"history"); if (key == L"innerWidth")return Value::Num(viewportW); if (key == L"innerHeight")return Value::Num(viewportH); return js->GetGlobal(key); };
        win->has = [this](Runtime&, const wstring& key) {return js->GlobalEnvironment()->Has(key) || key == L"document" || key == L"navigator" || key == L"screen" || key == L"innerWidth" || key == L"innerHeight" || key == L"setTimeout" || key == L"setInterval" || key == L"clearTimeout" || key == L"clearInterval"; };
        win->setter = [this](Runtime&, const wstring& key, const Value& v) {js->SetGlobal(key, v); };
        win->properties[L"setTimeout"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {return Value::Num(a.empty() ? 0 : (double)AddJSTimer(a[0], a.size() > 1 ? (DWORD)max(1.0, minihtml_js::ToNumber(a[1])) : 1, false)); });
        win->properties[L"setInterval"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {return Value::Num(a.empty() ? 0 : (double)AddJSTimer(a[0], a.size() > 1 ? (DWORD)max(1.0, minihtml_js::ToNumber(a[1])) : 1, true)); });
        win->properties[L"clearTimeout"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {if (!a.empty())ClearJSTimer((UINT_PTR)minihtml_js::ToNumber(a[0])); return Value::Undef(); });
        win->properties[L"clearInterval"] = win->properties[L"clearTimeout"];
        win->properties[L"addEventListener"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {
            if (a.size() > 1) windowListeners[ToString(a[0])].push_back(a[1]);
            return Value::Undef();
        });
        js->Define(L"window", windowValue); js->Define(L"self", windowValue); js->Define(L"globalThis", windowValue);
        js->Define(L"setTimeout", win->properties[L"setTimeout"]); js->Define(L"setInterval", win->properties[L"setInterval"]); js->Define(L"clearTimeout", win->properties[L"clearTimeout"]); js->Define(L"clearInterval", win->properties[L"clearInterval"]);
        auto loc = make_shared<Object>(); loc->getter = [this](Runtime&, const wstring& key) {if (key == L"href" || key == L"toString")return minihtml_js::Value::Str(currentURL); size_t scheme=currentURL.find(L"://"), hostStart=scheme==wstring::npos?0:scheme+3, pathStart=currentURL.find(L'/',hostStart); wstring origin=scheme==wstring::npos?L"":currentURL.substr(0,pathStart==wstring::npos?currentURL.size():pathStart); wstring path=pathStart==wstring::npos?L"/":currentURL.substr(pathStart); size_t hash=path.find(L'#');if(hash!=wstring::npos)path=path.substr(0,hash); size_t query=path.find(L'?'); wstring search=query==wstring::npos?L"":path.substr(query); if(query!=wstring::npos)path=path.substr(0,query); if(key==L"origin")return minihtml_js::Value::Str(origin);if(key==L"pathname")return minihtml_js::Value::Str(path);if(key==L"search")return minihtml_js::Value::Str(search); if (key == L"host" || key == L"hostname") { size_t e = currentURL.find(L'/', hostStart); wstring host=currentURL.substr(hostStart,e==wstring::npos?wstring::npos:e-hostStart); size_t port=host.find(L':');return minihtml_js::Value::Str(key==L"hostname"&&port!=wstring::npos?host.substr(0,port):host); }return minihtml_js::Value::Undef(); };
        loc->setter = [this](Runtime&, const wstring& key, const Value& v) {if (key == L"href") { wstring u = ToString(v); if (u.find(L"://") == wstring::npos)u = ResolveURL(currentURL, u); Navigate(u); }};
        loc->properties[L"assign"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {if (!a.empty())Navigate(ResolveURL(currentURL, ToString(a[0]))); return minihtml_js::Value::Undef(); });
        loc->properties[L"replace"] = loc->properties[L"assign"]; loc->properties[L"reload"] = js->MakeNative([this](Runtime&, const vector<Value>&, const Value&) {Navigate(currentURL); return minihtml_js::Value::Undef(); }); js->Define(L"location", Value::Obj(loc));
        auto hist = make_shared<Object>(); hist->properties[L"pushState"] = js->MakeNative([this](Runtime&, const vector<Value>& a, const Value&) {if (a.size() > 2) { wstring u = ResolveURL(currentURL, ToString(a[2])); currentURL = u; }return Value::Undef(); }); hist->properties[L"replaceState"] = hist->properties[L"pushState"]; hist->properties[L"back"] = js->MakeNative([](Runtime&, const vector<Value>&, const Value&) {return Value::Undef(); }); hist->properties[L"forward"] = hist->properties[L"back"]; js->Define(L"history", Value::Obj(hist));
        js->Define(L"URLSearchParams",js->MakeNative([](Runtime& r,const vector<Value>& a,const Value& self){Value out=self.type==Value::ObjectValue?self:Value::Obj(make_shared<Object>());wstring raw=a.empty()?L"":ToString(a[0]);if(!raw.empty()&&raw[0]==L'?')raw.erase(raw.begin());auto params=make_shared<unordered_map<wstring,wstring>>();size_t p=0;while(p<raw.size()){size_t e=raw.find(L'&',p);if(e==wstring::npos)e=raw.size();wstring item=raw.substr(p,e-p);size_t eq=item.find(L'=');(*params)[item.substr(0,eq)]=eq==wstring::npos?L"":item.substr(eq+1);p=e+1;}out.object->properties[L"get"]=r.MakeNative([params](Runtime&,const vector<Value>& v,const Value&){if(v.empty())return Value::NullValue();auto it=params->find(ToString(v[0]));return it==params->end()?Value::NullValue():Value::Str(it->second);});out.object->properties[L"has"]=r.MakeNative([params](Runtime&,const vector<Value>& v,const Value&){return Value::Bool(!v.empty()&&params->count(ToString(v[0]))!=0);});return out;}));
        js->Define(L"URL",js->MakeNative([](Runtime&,const vector<Value>& a,const Value& self){Value out=self.type==Value::ObjectValue?self:Value::Obj(make_shared<Object>());wstring raw=a.empty()?L"":ToString(a[0]),base=a.size()>1?ToString(a[1]):L"";wstring url=raw.find(L"://")!=wstring::npos?raw:ResolveURL(base,raw);out.object->properties[L"href"]=Value::Str(url);size_t sch=url.find(L"://"),hs=sch==wstring::npos?0:sch+3,ps=url.find(L'/',hs);wstring origin=sch==wstring::npos?L"":url.substr(0,ps==wstring::npos?url.size():ps);wstring path=ps==wstring::npos?L"/":url.substr(ps);size_t q=path.find(L'?');out.object->properties[L"origin"]=Value::Str(origin);out.object->properties[L"pathname"]=Value::Str(q==wstring::npos?path:path.substr(0,q));out.object->properties[L"search"]=Value::Str(q==wstring::npos?L"":path.substr(q));size_t hsEnd=url.find(L'/',hs);wstring host=url.substr(hs,hsEnd==wstring::npos?wstring::npos:hsEnd-hs);out.object->properties[L"hostname"]=Value::Str(host.substr(0,host.find(L':')));return out;}));
        js->Define(L"queueMicrotask",js->MakeNative([](Runtime&,const vector<Value>& a,const Value&){if(!a.empty())Runtime::EnqueueMicrotask(a[0]);return Value::Undef();}));
        js->Define(L"fetch", js->MakeNative([this](Runtime& runtime, const vector<Value>& args, const Value&) {
            wstring raw = args.empty() ? L"" : ToString(args[0]);
            wstring url = raw.find(L"://") != wstring::npos || raw.find(L"file://") == 0 ? raw : ResolveURL(currentURL, raw);
            wstring method = L"GET", body;
            vector<wstring> headers;
            if (args.size() > 1 && args[1].type == Value::ObjectValue && args[1].object) {
                auto options = args[1].object;
                auto methodIt = options->properties.find(L"method");
                if (methodIt != options->properties.end()) method = ToString(methodIt->second);
                auto bodyIt = options->properties.find(L"body");
                if (bodyIt != options->properties.end()) body = ToString(bodyIt->second);
                auto headersIt = options->properties.find(L"headers");
                if (headersIt != options->properties.end() && headersIt->second.type == Value::ObjectValue && headersIt->second.object)
                    for (const auto& header : headersIt->second.object->properties) headers.push_back(header.first + L": " + ToString(header.second));
            }
            Value promise = Runtime::CreatePromise(runtime);
            std::thread([promise, url, method, body, headers]() {
                HTTPResponse response = FetchRequest(url, method, body, headers);
                Runtime::PostExternalTask([promise, url, response = std::move(response)](Runtime& mainRuntime) mutable {
                    if (!response.error.empty()) {
                        auto error = make_shared<Object>();
                        error->properties[L"name"] = Value::Str(L"TypeError");
                        error->properties[L"message"] = Value::Str(response.error);
                        Runtime::ResolvePromise(mainRuntime, promise, true, Value::Obj(error));
                        return;
                    }
                    auto responseObject = make_shared<Object>();
                    responseObject->properties[L"ok"] = Value::Bool(response.ok);
                    responseObject->properties[L"status"] = Value::Num(response.status);
                    responseObject->properties[L"url"] = Value::Str(url);
                    wstring responseBody = std::move(response.body);
                    responseObject->properties[L"text"] = mainRuntime.MakeNative([responseBody](Runtime&, const vector<Value>&, const Value&) { return Value::Str(responseBody); });
                    responseObject->properties[L"json"] = mainRuntime.MakeNative([responseBody](Runtime& runtime, const vector<Value>&, const Value&) {
                        Value json = runtime.GetGlobal(L"JSON");
                        if (json.type == Value::ObjectValue && json.object) {
                            auto parser = json.object->properties.find(L"parse");
                            if (parser != json.object->properties.end()) return runtime.Invoke(parser->second, { Value::Str(responseBody) });
                        }
                        return Value::Undef();
                    });
                    Runtime::ResolvePromise(mainRuntime, promise, false, Value::Obj(responseObject));
                });
            }).detach();
            return promise;
        }));
        // XMLHttpRequest: asynchronous HTTP requests with the response modes used by
        // common sites and the HTML5 compatibility suite. Network work happens off
        // the UI thread; completion is delivered through the runtime task queue.
        auto xhrPrototype = make_shared<Object>();
        auto xhrCtor = js->MakeNative([this](Runtime& runtime, const vector<Value>&, const Value& self) {
            if (self.type != Value::ObjectValue || !self.object) return Value::Undef();
            auto xhr = self.object;
            xhr->properties[L"readyState"] = Value::Num(0);
            xhr->properties[L"status"] = Value::Num(0);
            xhr->properties[L"statusText"] = Value::Str(L"");
            xhr->properties[L"responseType"] = Value::Str(L"");
            xhr->properties[L"responseText"] = Value::Str(L"");
            xhr->properties[L"response"] = Value::NullValue();
            xhr->properties[L"responseXML"] = Value::NullValue();
            xhr->properties[L"responseURL"] = Value::Str(L"");
            xhr->properties[L"timeout"] = Value::Num(0);
            xhr->properties[L"withCredentials"] = Value::Bool(false);
            xhr->properties[L"upload"] = Value::Obj(make_shared<Object>());
            xhr->properties[L"__xhr_method"] = Value::Str(L"GET");
            xhr->properties[L"__xhr_url"] = Value::Str(L"");
            xhr->properties[L"__xhr_headers"] = Value::Obj(make_shared<Object>());
            xhr->properties[L"open"] = runtime.MakeNative([this](Runtime& rt, const vector<Value>& a, const Value& receiver) {
                if (receiver.type != Value::ObjectValue || !receiver.object || a.size() < 2) return Value::Undef();
                auto o = receiver.object;
                wstring method = ToString(a[0]), raw = ToString(a[1]);
                wstring url = raw.find(L"://") != wstring::npos || raw.rfind(L"file://", 0) == 0 ? raw : ResolveURL(currentURL, raw);
                o->properties[L"__xhr_method"] = Value::Str(method);
                o->properties[L"__xhr_url"] = Value::Str(url);
                o->properties[L"readyState"] = Value::Num(1);
                auto cb = o->properties[L"onreadystatechange"];
                if (cb.type == Value::FunctionValue) rt.Invoke(cb, {}, receiver);
                return Value::Undef();
            });
            xhr->properties[L"setRequestHeader"] = runtime.MakeNative([](Runtime&, const vector<Value>& a, const Value& receiver) {
                if (receiver.type == Value::ObjectValue && receiver.object && a.size() > 1) {
                    auto headers = receiver.object->properties[L"__xhr_headers"];
                    if (headers.type == Value::ObjectValue && headers.object) headers.object->properties[ToString(a[0])] = Value::Str(ToString(a[1]));
                }
                return Value::Undef();
            });
            xhr->properties[L"getResponseHeader"] = runtime.MakeNative([](Runtime&, const vector<Value>& a, const Value&) { return a.empty() ? Value::NullValue() : Value::NullValue(); });
            xhr->properties[L"getAllResponseHeaders"] = runtime.MakeNative([](Runtime&, const vector<Value>&, const Value&) { return Value::Str(L""); });
            xhr->properties[L"abort"] = runtime.MakeNative([](Runtime& rt, const vector<Value>&, const Value& receiver) {
                if (receiver.type == Value::ObjectValue && receiver.object) {
                    receiver.object->properties[L"readyState"] = Value::Num(0);
                    receiver.object->properties[L"status"] = Value::Num(0);
                    auto cb = receiver.object->properties[L"onreadystatechange"];
                    if (cb.type == Value::FunctionValue) rt.Invoke(cb, {}, receiver);
                }
                return Value::Undef();
            });
            xhr->properties[L"send"] = runtime.MakeNative([this](Runtime&, const vector<Value>& a, const Value& receiver) {
                if (receiver.type != Value::ObjectValue || !receiver.object) return Value::Undef();
                auto o = receiver.object;
                wstring url = ToString(o->properties[L"__xhr_url"]);
                wstring method = ToString(o->properties[L"__xhr_method"]);
                wstring body = a.empty() ? L"" : ToString(a[0]);
                vector<wstring> headers;
                auto h = o->properties[L"__xhr_headers"];
                if (h.type == Value::ObjectValue && h.object) for (const auto& kv : h.object->properties) headers.push_back(kv.first + L": " + ToString(kv.second));
                std::weak_ptr<Object> weak = o;
                std::thread([weak, url, method, body, headers]() {
                    HTTPResponse response = FetchRequest(url, method, body, headers);
                    Runtime::PostExternalTask([weak, url, response = std::move(response)](Runtime& rt) mutable {
                        auto target = weak.lock();
                        if (!target) return;
                        long status = response.status;
                        target->properties[L"status"] = Value::Num((double)status);
                        target->properties[L"statusText"] = Value::Str(status >= 200 && status < 300 ? L"OK" : L"Error");
                        target->properties[L"responseURL"] = Value::Str(url);
                        target->properties[L"responseText"] = Value::Str(response.body);
                        wstring responseType = ToString(target->properties[L"responseType"]);
                        Value result = Value::Str(response.body);
                        if (responseType == L"document") {
                            auto doc = make_shared<Object>();
                            wstring lower = Lower(response.body);
                            size_t a = lower.find(L"<title>"), b = lower.find(L"</title>");
                            wstring title = (a != wstring::npos && b != wstring::npos && b >= a + 7) ? response.body.substr(a + 7, b - (a + 7)) : L"";
                            size_t p = 0; while ((p = title.find(L"&amp;", p)) != wstring::npos) { title.replace(p, 5, L"&"); ++p; }
                            doc->properties[L"title"] = Value::Str(title);
                            result = Value::Obj(doc);
                            target->properties[L"responseXML"] = result;
                        }
                        else if (responseType == L"arraybuffer") {
                            auto buffer = make_shared<Object>();
                            auto ctor = rt.GetGlobal(L"ArrayBuffer");
                            if (ctor.type == Value::FunctionValue && ctor.function) {
                                auto pi = ctor.function->properties.find(L"prototype");
                                if (pi != ctor.function->properties.end() && pi->second.type == Value::ObjectValue) buffer->prototype = pi->second.object;
                            }
                            buffer->properties[L"byteLength"] = Value::Num((double)response.body.size());
                            buffer->properties[L"__arraybuffer"] = Value::Bool(true);
                            result = Value::Obj(buffer);
                        }
                        else if (responseType == L"blob") {
                            auto blob = make_shared<Object>();
                            auto ctor = rt.GetGlobal(L"Blob");
                            if (ctor.type == Value::FunctionValue && ctor.function) {
                                auto pi = ctor.function->properties.find(L"prototype");
                                if (pi != ctor.function->properties.end() && pi->second.type == Value::ObjectValue) blob->prototype = pi->second.object;
                            }
                            blob->properties[L"size"] = Value::Num((double)response.body.size());
                            blob->properties[L"type"] = Value::Str(L"text/html");
                            result = Value::Obj(blob);
                        }
                        target->properties[L"response"] = result;
                        target->properties[L"readyState"] = Value::Num(4);
                        auto cb = target->properties[L"onreadystatechange"];
                        if (cb.type == Value::FunctionValue) rt.Invoke(cb, {}, Value::Obj(target));
                        auto load = target->properties[L"onload"];
                        if (load.type == Value::FunctionValue) rt.Invoke(load, {}, Value::Obj(target));
                        auto end = target->properties[L"onloadend"];
                        if (end.type == Value::FunctionValue) rt.Invoke(end, {}, Value::Obj(target));
                    });
                }).detach();
                return Value::Undef();
            });
            return Value::Obj(xhr);
        });
        xhrCtor.function->properties[L"prototype"] = Value::Obj(xhrPrototype);
        js->Define(L"XMLHttpRequest", xhrCtor);

        auto promiseCtor=js->MakeNative([](Runtime& rt,const vector<Value>& a,const Value&){
            Value promise=Runtime::CreatePromise(rt);
            Value resolve=rt.MakeNative([promise](Runtime& r,const vector<Value>& v,const Value&){Runtime::ResolvePromise(r,promise,false,v.empty()?Value::Undef():v[0]);return Value::Undef();});
            Value reject=rt.MakeNative([promise](Runtime& r,const vector<Value>& v,const Value&){Runtime::ResolvePromise(r,promise,true,v.empty()?Value::Undef():v[0]);return Value::Undef();});
            promise.object->properties[L"__promise_resolve_fn"]=resolve;
            promise.object->properties[L"__promise_reject_fn"]=reject;
            if(!a.empty()&&a[0].type==Value::FunctionValue){try{rt.Invoke(a[0],{resolve,reject});}catch(...){Runtime::ResolvePromise(rt,promise,true,Value::Str(L"Promise executor threw"));}}
            return promise;
        });
        promiseCtor.function->properties[L"resolve"]=js->MakeNative([](Runtime& r,const vector<Value>& a,const Value&){Value p=Runtime::CreatePromise(r);Runtime::ResolvePromise(r,p,false,a.empty()?Value::Undef():a[0]);return p;});
        promiseCtor.function->properties[L"reject"]=js->MakeNative([](Runtime& r,const vector<Value>& a,const Value&){Value p=Runtime::CreatePromise(r);Runtime::ResolvePromise(r,p,true,a.empty()?Value::Undef():a[0]);return p;});
        promiseCtor.function->properties[L"all"]=js->MakeNative([](Runtime& r,const vector<Value>& a,const Value&){Value out=Runtime::CreatePromise(r);vector<Value> values;if(!a.empty()&&a[0].object&&a[0].object->array)values=a[0].object->elements;auto result=make_shared<Object>();result->array=true;result->elements.resize(values.size());if(values.empty()){Runtime::ResolvePromise(r,out,false,Value::Obj(result));return out;}auto remaining=make_shared<int>((int)values.size());for(size_t i=0;i<values.size();++i){Value p=values[i];if(Runtime::IsPromise(p)){Value cb=r.MakeNative([out,result,remaining,i](Runtime& rr,const vector<Value>& v,const Value&){result->elements[i]=v.empty()?Value::Undef():v[0];if(--*remaining==0)Runtime::ResolvePromise(rr,out,false,Value::Obj(result));return Value::Undef();});Value bad=r.MakeNative([out](Runtime& rr,const vector<Value>& v,const Value&){Runtime::ResolvePromise(rr,out,true,v.empty()?Value::Undef():v[0]);return Value::Undef();});r.Invoke(p.object->properties[L"then"],{cb,bad},p);}else{result->elements[i]=p;if(--*remaining==0)Runtime::ResolvePromise(r,out,false,Value::Obj(result));}}return out;});
        promiseCtor.function->properties[L"allSettled"]=js->MakeNative([](Runtime& r,const vector<Value>& a,const Value&){Value out=Runtime::CreatePromise(r);vector<Value> values;if(!a.empty()&&a[0].object&&a[0].object->array)values=a[0].object->elements;auto result=make_shared<Object>();result->array=true;result->elements.resize(values.size());if(values.empty()){Runtime::ResolvePromise(r,out,false,Value::Obj(result));return out;}auto remaining=make_shared<int>((int)values.size());for(size_t i=0;i<values.size();++i){auto finish=[out,result,remaining,i](Runtime& rr,const Value& v,bool rejected){auto item=make_shared<Object>();item->properties[L"status"]=Value::Str(rejected?L"rejected":L"fulfilled");item->properties[rejected?L"reason":L"value"]=v;result->elements[i]=Value::Obj(item);if(--*remaining==0)Runtime::ResolvePromise(rr,out,false,Value::Obj(result));};Value p=values[i];if(Runtime::IsPromise(p)){Value ok=r.MakeNative([finish](Runtime& rr,const vector<Value>& v,const Value&){finish(rr,v.empty()?Value::Undef():v[0],false);return Value::Undef();});Value bad=r.MakeNative([finish](Runtime& rr,const vector<Value>& v,const Value&){finish(rr,v.empty()?Value::Undef():v[0],true);return Value::Undef();});r.Invoke(p.object->properties[L"then"],{ok,bad},p);}else finish(r,p,false);}return out;});
        promiseCtor.function->properties[L"race"]=js->MakeNative([](Runtime& r,const vector<Value>& a,const Value&){Value out=Runtime::CreatePromise(r);if(!a.empty()&&a[0].object&&a[0].object->array)for(Value p:a[0].object->elements){if(Runtime::IsPromise(p)){Value ok=r.MakeNative([out](Runtime& rr,const vector<Value>& v,const Value&){Runtime::ResolvePromise(rr,out,false,v.empty()?Value::Undef():v[0]);return Value::Undef();});Value bad=r.MakeNative([out](Runtime& rr,const vector<Value>& v,const Value&){Runtime::ResolvePromise(rr,out,true,v.empty()?Value::Undef():v[0]);return Value::Undef();});r.Invoke(p.object->properties[L"then"],{ok,bad},p);}else Runtime::ResolvePromise(r,out,false,p);}return out;});
        // Promise.any: fulfill on the first fulfillment; reject with an AggregateError-like object only when every input rejects.
        promiseCtor.function->properties[L"any"]=js->MakeNative([](Runtime& r,const vector<Value>& a,const Value&){
            Value out=Runtime::CreatePromise(r); vector<Value> values;
            if(!a.empty()&&a[0].type==Value::ObjectValue&&a[0].object&&a[0].object->array) values=a[0].object->elements;
            auto errors=make_shared<Object>(); errors->array=true; errors->elements.resize(values.size());
            if(values.empty()){auto err=make_shared<Object>();err->properties[L"name"]=Value::Str(L"AggregateError");err->properties[L"message"]=Value::Str(L"All promises were rejected");err->properties[L"errors"]=Value::Obj(errors);Runtime::ResolvePromise(r,out,true,Value::Obj(err));return out;}
            auto remaining=make_shared<int>((int)values.size());
            auto done=make_shared<bool>(false);
            for(size_t i=0;i<values.size();++i){
                auto fulfill=r.MakeNative([out,done](Runtime& rr,const vector<Value>& v,const Value&){if(!*done){*done=true;Runtime::ResolvePromise(rr,out,false,v.empty()?Value::Undef():v[0]);}return Value::Undef();});
                auto reject=r.MakeNative([out,errors,remaining,done,i](Runtime& rr,const vector<Value>& v,const Value&){if(*done)return Value::Undef();errors->elements[i]=v.empty()?Value::Undef():v[0];if(--*remaining==0){*done=true;auto err=make_shared<Object>();err->properties[L"name"]=Value::Str(L"AggregateError");err->properties[L"message"]=Value::Str(L"All promises were rejected");err->properties[L"errors"]=Value::Obj(errors);Runtime::ResolvePromise(rr,out,true,Value::Obj(err));}return Value::Undef();});
                Value item=values[i]; if(Runtime::IsPromise(item))r.Invoke(item.object->properties[L"then"],{fulfill,reject},item);else r.Invoke(fulfill,{item});
            }
            return out;
        });
        promiseCtor.function->properties[L"try"]=js->MakeNative([](Runtime& r,const vector<Value>& a,const Value&){
            Value out=Runtime::CreatePromise(r);
            if(a.empty()||a[0].type!=Value::FunctionValue){Runtime::ResolvePromise(r,out,true,Value::Str(L"TypeError: Promise.try requires a function"));return out;}
            vector<Value> args; if(a.size()>1)args.assign(a.begin()+1,a.end());
            try { Value result=r.Invoke(a[0],args); Runtime::ResolvePromise(r,out,false,result); }
            catch(const JSException& e){Runtime::ResolvePromise(r,out,true,e.thrown);}
            catch(const std::exception& e){Runtime::ResolvePromise(r,out,true,Value::Str(wstring(e.what(),e.what()+strlen(e.what()))));}
            return out;
        });
        promiseCtor.function->properties[L"withResolvers"]=js->MakeNative([](Runtime& r,const vector<Value>&,const Value&){
            Value promise=Runtime::CreatePromise(r);auto result=make_shared<Object>();
            result->properties[L"promise"]=promise;
            result->properties[L"resolve"]=r.MakeNative([promise](Runtime& rr,const vector<Value>& a,const Value&){Runtime::ResolvePromise(rr,promise,false,a.empty()?Value::Undef():a[0]);return Value::Undef();});
            result->properties[L"reject"]=r.MakeNative([promise](Runtime& rr,const vector<Value>& a,const Value&){Runtime::ResolvePromise(rr,promise,true,a.empty()?Value::Undef():a[0]);return Value::Undef();});
            return Value::Obj(result);
        });
        js->Define(L"Promise",promiseCtor);
        js->Define(L"Worker", js->MakeNative([this](Runtime& mainRuntime, const vector<Value>& args, const Value& self) {
            if (self.type != Value::ObjectValue || !self.object) return Value::Undef();
            auto owner = self.object;
            auto state = make_shared<WorkerState>();
            state->owner = owner;
            owner->workerState = state;
            owner->properties[L"onmessage"] = Value::Undef();
            owner->properties[L"onerror"] = Value::Undef();
            auto listeners = make_shared<Object>(); listeners->array = true;
            owner->properties[L"__message_listeners"] = Value::Obj(listeners);
            wstring raw = args.empty() ? L"" : ToString(args[0]);
            wstring base = currentURL;
            wstring scriptURL = raw.find(L"://") != wstring::npos || raw.find(L"file://") == 0 ? raw : ResolveURL(base, raw);

            owner->properties[L"postMessage"] = mainRuntime.MakeNative([state](Runtime& runtime, const vector<Value>& values, const Value&) {
                if (values.empty()) return Value::Undef();
                Value json = runtime.GetGlobal(L"JSON"), serialized = Value::Str(L"null");
                if (json.type == Value::ObjectValue && json.object) {
                    auto encoder = json.object->properties.find(L"stringify");
                    if (encoder != json.object->properties.end()) serialized = runtime.Invoke(encoder->second, { values[0] });
                }
                std::string payload = WToUtf8(ToString(serialized));
                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    if (state->terminated.load()) return Value::Undef();
                    state->incoming.push_back(payload);
                }
                state->condition.notify_one();
                return Value::Undef();
            });
            owner->properties[L"terminate"] = mainRuntime.MakeNative([state](Runtime&, const vector<Value>&, const Value&) {
                state->terminated.store(true); state->condition.notify_all(); return Value::Undef();
            });
            owner->properties[L"addEventListener"] = mainRuntime.MakeNative([listeners](Runtime&, const vector<Value>& values, const Value&) {
                if (values.size() > 1 && ToString(values[0]) == L"message" && values[1].type == Value::FunctionValue) listeners->elements.push_back(values[1]);
                return Value::Undef();
            });
            owner->properties[L"removeEventListener"] = mainRuntime.MakeNative([listeners](Runtime&, const vector<Value>& values, const Value&) {
                if (values.size() > 1 && ToString(values[0]) == L"message") listeners->elements.erase(std::remove_if(listeners->elements.begin(), listeners->elements.end(), [&](const Value& v) { return v.function == values[1].function; }), listeners->elements.end());
                return Value::Undef();
            });

            workers.push_back(state);
            state->thread = std::thread([state, scriptURL]() {
                try {
                    HTTPResponse response = FetchURL(scriptURL);
                    if (!response.ok) {
                        Runtime::PostWorkerEvent(state->owner, std::string(), true, "Unable to load Worker script: " + WToUtf8(scriptURL));
                        return;
                    }
                    Runtime workerRuntime;
                    workerRuntime.SetGlobal(L"isWorkerThread", Value::Bool(true));
                    auto workerGlobal = make_shared<Object>();
                    Value workerGlobalValue = Value::Obj(workerGlobal);
                    workerGlobal->getter = [&workerRuntime](Runtime&, const wstring& key) { return workerRuntime.GetGlobal(key); };
                    workerGlobal->setter = [&workerRuntime](Runtime&, const wstring& key, const Value& value) { workerRuntime.SetGlobal(key, value); };
                    workerGlobal->has = [&workerRuntime](Runtime&, const wstring& key) { return workerRuntime.GlobalEnvironment()->Has(key); };
                    auto workerListeners = make_shared<vector<Value>>();
                    workerGlobal->properties[L"addEventListener"] = workerRuntime.MakeNative([workerListeners](Runtime&, const vector<Value>& values, const Value&) {
                        if (values.size() > 1 && ToString(values[0]) == L"message" && values[1].type == Value::FunctionValue) workerListeners->push_back(values[1]);
                        return Value::Undef();
                    });
                    workerGlobal->properties[L"removeEventListener"] = workerRuntime.MakeNative([workerListeners](Runtime&, const vector<Value>& values, const Value&) {
                        if (values.size() > 1 && ToString(values[0]) == L"message") workerListeners->erase(std::remove_if(workerListeners->begin(), workerListeners->end(), [&](const Value& v) { return v.function == values[1].function; }), workerListeners->end());
                        return Value::Undef();
                    });
                    auto workerPostMessage = workerRuntime.MakeNative([state](Runtime& runtime, const vector<Value>& values, const Value&) {
                        if (values.empty()) return Value::Undef();
                        Value json = runtime.GetGlobal(L"JSON"), serialized = Value::Str(L"null");
                        if (json.type == Value::ObjectValue && json.object) {
                            auto encoder = json.object->properties.find(L"stringify");
                            if (encoder != json.object->properties.end()) serialized = runtime.Invoke(encoder->second, { values[0] });
                        }
                        Runtime::PostWorkerEvent(state->owner, WToUtf8(ToString(serialized)));
                        return Value::Undef();
                    });
                    workerGlobal->properties[L"postMessage"] = workerPostMessage;
                    workerRuntime.Define(L"postMessage", workerPostMessage);
                    workerGlobal->properties[L"close"] = workerRuntime.MakeNative([state](Runtime&, const vector<Value>&, const Value&) {
                        state->terminated.store(true); state->condition.notify_all(); return Value::Undef();
                    });
                    workerRuntime.Define(L"close", workerGlobal->properties[L"close"]);
                    workerRuntime.Define(L"self", workerGlobalValue);
                    workerRuntime.Define(L"globalThis", workerGlobalValue);
                    workerRuntime.Define(L"onmessage", Value::Undef());
                    workerRuntime.Define(L"onerror", Value::Undef());
                    auto importScripts = workerRuntime.MakeNative([&workerRuntime, scriptURL](Runtime&, const vector<Value>& imports, const Value&) {
                        for (const auto& item : imports) {
                            wstring url = ResolveURL(scriptURL, ToString(item));
                            HTTPResponse imported = FetchURL(url);
                            if (!imported.ok) throw std::runtime_error("importScripts failed");
                            if (!workerRuntime.Execute(imported.body)) throw std::runtime_error("importScripts JavaScript error");
                        }
                        return Value::Undef();
                    });
                    workerGlobal->properties[L"importScripts"] = importScripts;
                    workerRuntime.Define(L"importScripts", importScripts);
                    if (!workerRuntime.Execute(response.body))
                        throw std::runtime_error("Worker script error: " + WToUtf8(workerRuntime.LastError()));
                    while (!state->terminated.load()) {
                        std::string payload;
                        {
                            std::unique_lock<std::mutex> lock(state->mutex);
                            state->condition.wait(lock, [&]() { return state->terminated.load() || !state->incoming.empty(); });
                            if (state->terminated.load()) break;
                            payload = std::move(state->incoming.front());
                            state->incoming.pop_front();
                        }
                        Value data = Value::Undef();
                        Value json = workerRuntime.GetGlobal(L"JSON");
                        if (json.type == Value::ObjectValue && json.object) {
                            auto parser = json.object->properties.find(L"parse");
                            if (parser != json.object->properties.end()) data = workerRuntime.Invoke(parser->second, { Value::Str(Utf8ToW(payload)) });
                        }
                        auto event = make_shared<Object>();
                        event->properties[L"type"] = Value::Str(L"message");
                        event->properties[L"data"] = data;
                        event->properties[L"target"] = workerGlobalValue;
                        Value eventValue = Value::Obj(event);
                        Value handler = workerRuntime.GetGlobal(L"onmessage");
                        if (handler.type == Value::FunctionValue) workerRuntime.Invoke(handler, { eventValue }, workerGlobalValue);
                        auto snapshot = *workerListeners;
                        for (const auto& callback : snapshot) if (callback.type == Value::FunctionValue) workerRuntime.Invoke(callback, { eventValue }, workerGlobalValue);
                    }
                } catch (const std::exception& error) {
                    Runtime::PostWorkerEvent(state->owner, std::string(), true, error.what());
                } catch (...) {
                    Runtime::PostWorkerEvent(state->owner, std::string(), true, "Unknown Worker exception");
                }
            });
            return self;
        }));
        js->Define(L"Blob",js->MakeNative([](Runtime&,const vector<Value>& a,const Value& self){if(self.type!=Value::ObjectValue||!self.object)return Value::Undef();size_t bytes=0;if(!a.empty()&&a[0].type==Value::ObjectValue&&a[0].object){for(const auto& v:a[0].object->elements)bytes+=ToString(v).size();}self.object->properties[L"size"]=Value::Num((double)bytes);self.object->properties[L"type"]=a.size()>1&&a[1].object&&a[1].object->properties.count(L"type")?a[1].object->properties[L"type"]:Value::Str(L"");return Value::Undef();}));
        auto arrayBufferCtor = js->MakeNative([](Runtime&, const vector<Value>& a, const Value& self) {
            if (self.type != Value::ObjectValue || !self.object) return Value::Undef();
            size_t length = a.empty() ? 0 : (size_t)max(0.0, min(64.0 * 1024.0 * 1024.0, ToNumber(a[0])));
            self.object->properties[L"byteLength"] = Value::Num((double)length);
            self.object->properties[L"__arraybuffer"] = Value::Bool(true);
            return Value::Undef();
        });
        arrayBufferCtor.function->properties[L"prototype"] = Value::Obj(make_shared<Object>());
        js->Define(L"ArrayBuffer", arrayBufferCtor);
        js->Define(L"alert", js->MakeNative([](Runtime&, const vector<Value>& a, const Value&) {wstring m = a.empty() ? L"" : ToString(a[0]); MessageBoxW(nullptr, m.c_str(), L"minihtml JavaScript", MB_OK | MB_ICONINFORMATION); return Value::Undef(); }));
        js->Define(L"confirm", js->MakeNative([](Runtime&, const vector<Value>& a, const Value&) {wstring m = a.empty() ? L"" : ToString(a[0]); return Value::Bool(MessageBoxW(nullptr, m.c_str(), L"minihtml JavaScript", MB_YESNO | MB_ICONQUESTION) == IDYES); }));
        // Compile inline event-handler attributes into ordinary JS functions.
        function<void(Node*)> bindEvents = [&](Node* n) {if (!n)return; for (const auto& kv : n->attributes) { if (kv.first.size() > 2 && kv.first.substr(0, 2) == L"on") { try { Value fn = js->Evaluate(L"function(event){" + kv.second + L"}"); if (fn.type == Value::FunctionValue)domListeners[n][kv.first.substr(2)].push_back(fn); } catch (...) {} } }for (Node* c : n->children)bindEvents(c); }; bindEvents(document);
        Node* body = FindByTag(L"body"); if (body && body->attributes.count(L"onload")) { try { js->SetGlobal(L"onload", js->Evaluate(L"function(event){" + GetAttribute(body, L"onload") + L"}")); } catch (...) {} }
    }
    HTMLBrowser() { measureDC = GetDC(nullptr); defaultRules = ParseCSS(L"div {display:block} p {display:block} h1 {display:block} h2 {display:block} h3 {display:block} img {display:inline-block} table {display:table} tr {display:table-row} td {display:table-cell}"); }
    ~HTMLBrowser() {
        StopWorkers();
#if defined(_WIN32)
        for (auto& t : jsTimers)KillTimer(nullptr, t.first);
#endif
        jsTimers.clear(); delete document; for(Node* n:retiredNodes)delete n; retiredNodes.clear(); ReleaseDC(nullptr, measureDC);
    }

    void LoadHTML(const wstring& html, const wstring& baseURL) {
#if defined(_WIN32)
        for (auto& t : jsTimers)KillTimer(nullptr, t.first);
#endif
        jsTimers.clear();
        delete document; document = ParseHTML(html); currentURL = baseURL; authorRules.clear(); authorCSS.clear(); g_cssVars.clear();
        function<void(Node*)> setBase = [&](Node* n) {if (!n)return; if (n->tag != L"#text" && !n->attributes.count(L"base"))n->attributes[L"base"] = baseURL; for (Node* c : n->children)setBase(c); };
        setBase(document);
        // Fetch linked stylesheets and parse inline style blocks. Braces are intentional:
        // the old dangling-else attached the LINK branch to the inner #text test.
        function<void(Node*)> collectCSS = [&](Node* n) {if (!n)return;
        if (n->tag == L"style") {
            for (Node* c : n->children) if (c->tag == L"#text") { authorCSS += L"\n" + c->text; }
        }
        else if (n->tag == L"link") {
            wstring rel = Lower(GetAttribute(n, L"rel")); bool isStylesheet = false;
            for (const auto& part : SplitValue(rel)) if (part == L"stylesheet") isStylesheet = true;
            if (isStylesheet) {
                wstring href = GetAttribute(n, L"href"); wstring url = ResolveURL(baseURL, href);
                if (!url.empty()) {
                    HTTPResponse response = FetchURL(url);
                    if (response.ok) authorCSS += L"\n" + response.body;
                }
            }
        }
        for (Node* c : n->children)collectCSS(c);
            }; collectCSS(document);
        StopWorkers();
        js.reset();
        if (g_javascriptEnabled) {
            InitializeJS();
            // Classic inline and external scripts execute in document order. ES modules are skipped.
            function<void(Node*)> runScripts = [&](Node* n) {
                if (!n)return;
                if (n->tag == L"script") {
                    wstring type = Lower(Trim(GetAttribute(n, L"type")));
                    bool classic = type.empty() || type == L"text/javascript" || type == L"application/javascript" || type == L"text/ecmascript";
                    if (classic) {
                        wstring code, src = GetAttribute(n, L"src");
                        if (!src.empty()) {
                            HTTPResponse response = FetchURL(ResolveURL(baseURL, src));
                            if (response.ok)code = response.body;
                        }
                        else {
                            for (Node* c : n->children)if (c->tag == L"#text")code += c->text;
                        }
                        if (!code.empty()) {
                            bool executed = js->Execute(code);
#if !defined(_WIN32)
                            if (!executed) std::fprintf(stderr, "[minihtml JS error] %s\n", WToUtf8(js->LastError()).c_str());
#else
                            if (!executed) OutputDebugStringW((L"minihtml JS error: " + js->LastError() + L"\n").c_str());
#endif
                        }
                    }
                    return;
                }
                for (Node* c : n->children)runScripts(c);
                };
            runScripts(document);
#if !defined(_WIN32)
            std::fprintf(stderr, "[minihtml] scripts parsed; body text before DOMContentLoaded: %s\n", WToUtf8(GetTextContent(FindByTag(L"body"))).c_str());
#endif
            minihtml_js::Value ready = js->GetGlobal(L"__DOMContentLoaded");
            if (ready.type == minihtml_js::Value::FunctionValue) {
                try { js->Invoke(ready, {}); }
                catch (const std::exception& e) { OutputDebugStringA((string("minihtml DOMContentLoaded error: ") + e.what() + "\n").c_str()); }
            }
#if !defined(_WIN32)
            std::fprintf(stderr, "[minihtml] body text after DOMContentLoaded: %s\n", WToUtf8(GetTextContent(FindByTag(L"body"))).c_str());
#endif
            minihtml_js::Value load = js->GetGlobal(L"onload");
            if (load.type == minihtml_js::Value::FunctionValue) {
                try { js->Invoke(load, {}); }
                catch (const std::exception& e) { OutputDebugStringA((string("minihtml load handler error: ") + e.what() + "\n").c_str()); }
            }
        }
        // Build the current CSS rule set before creating ::before/::after nodes.
        auto generatedRules = ParseCSS(authorCSS, viewportW, viewportH);
        MaterializeGeneratedContent(document, generatedRules);
        Relayout(viewportW);
    }

    void Navigate(const wstring& url) {
        currentURL = url;
        HTTPResponse r = FetchURL(url);
        if (!r.ok) LoadHTML(L"<h1>Error</h1><p>" + r.error + L"</p>", url);
        else LoadHTML(r.body, url);
    }

    void Relayout(int width) {
        viewportW = width; g_layoutViewportWidth = width; g_layoutViewportHeight = viewportH; rootLayout = LayoutBox();
        if (!document) return;
        authorRules = ParseCSS(authorCSS, width, viewportH);
        vector<CSSRule> combinedRules = defaultRules;
        combinedRules.insert(combinedRules.end(), authorRules.begin(), authorRules.end());
#if !defined(_WIN32)
        static bool rulesDumped = false; if (!rulesDumped) { rulesDumped = true; for (auto& r : combinedRules) { std::fprintf(stderr, "[rule] selectorParts=%zu", r.selector.parts.size()); for (auto& sp : r.selector.parts)std::fprintf(stderr, " tag='%s' id='%s' cls=%zu", WToUtf8(sp.tag).c_str(), WToUtf8(sp.id).c_str(), sp.classes.size()); for (auto& d : r.decls)std::fprintf(stderr, " %s:%s", WToUtf8(d.first).c_str(), WToUtf8(d.second).c_str()); std::fprintf(stderr, "\n"); } }
#endif
        // The root element must receive author rules too (font sizing, colors,
        // display and other inherited values are established here).
        Style rootS = ComputeStyle(document, Style(), combinedRules, true);
        rootS.display = Style::Block;
        LayoutNode(measureDC, fonts, document, Style(), rootS, 0, 0, width, rootLayout, combinedRules);
        contentHeight = rootLayout.h;
#if !defined(_WIN32)
        std::fprintf(stderr, "[layout] root %d,%d %dx%d children=%zu frags=%zu content=%d\n", rootLayout.x, rootLayout.y, rootLayout.w, rootLayout.h, rootLayout.children.size(), rootLayout.frags.size(), contentHeight);
#endif
    }

    void SetViewport(int w, int h) {
        if (w <= 0 || h <= 0) return;
        bool changed = (w != viewportW || h != viewportH);
        viewportH = h;
        if (changed) Relayout(w);
    }
    void ScrollBy(int dy) { scrollY += dy; int maxS = max(0, contentHeight - viewportH); scrollY = max(0, min(scrollY, maxS)); }

    void PaintFormControl(HDC dc, Node* node, const Style& style, int x, int y, int w, int h, const wstring& fragmentText = L"") {
        if (!node || w <= 0 || h <= 0) return;
        wstring type = InputType(node);
        if (node->tag == L"input" && type == L"hidden") return;
        COLORREF bg = style.background == CLR_INVALID ? (node->tag == L"button" ? RGB(242,243,245) : RGB(255,255,255)) : style.background;
        COLORREF border = style.borderColor == CLR_INVALID ? RGB(160,165,175) : style.borderColor;
        if (node->attributes.count(L"disabled")) { bg = RGB(235,237,240); border = RGB(195,198,203); }
        HBRUSH brush = CreateSolidBrush(bg); RECT r{ x, y, x + w, y + h };
        if (style.borderRadius > 0) FillRoundedRect(dc, &r, brush, style.borderRadius); else FillRect(dc, &r, brush);
        DeleteObject(brush);
        HPEN pen = CreatePen(PS_SOLID, max(1, style.borderWidth), node == focusedControl ? RGB(55,125,230) : border);
        HGDIOBJ oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, x, y, x + w, y + h);
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen);
        if (node->tag == L"input" && (type == L"checkbox" || type == L"radio")) {
            if (node->attributes.count(L"checked")) {
                HPEN mark = CreatePen(PS_SOLID, 2, RGB(35,95,190)); HGDIOBJ old = SelectObject(dc, mark);
                MoveToEx(dc, x + 4, y + h / 2, nullptr); LineTo(dc, x + w / 2 - 1, y + h - 4); LineTo(dc, x + w - 3, y + 3); SelectObject(dc, old); DeleteObject(mark);
            }
            return;
        }
        if (node->tag == L"input" && type == L"range") {
            HPEN track = CreatePen(PS_SOLID, 3, RGB(180,185,195)); HGDIOBJ old = SelectObject(dc, track);
            MoveToEx(dc, x + 8, y + h / 2, nullptr); LineTo(dc, x + w - 8, y + h / 2); SelectObject(dc, old); DeleteObject(track); return;
        }
        bool hasChildText = node->tag == L"button" || node->tag == L"textarea";
        wstring value = fragmentText;
        if (value.empty() && !hasChildText) value = ControlValue(node);
        if (node->tag == L"select") {
            value.clear(); function<void(Node*)> findOption = [&](Node* n) { if (!n || !value.empty()) return; if (n->tag == L"option" && n->attributes.count(L"selected")) { CollectNodeText(n, value); return; } for (Node* c : n->children) findOption(c); };
            findOption(node); if (value.empty()) { function<void(Node*)> firstOption = [&](Node* n) { if (!n || !value.empty())return; if (n->tag == L"option") { CollectNodeText(n, value); return; } for (Node* c : n->children)firstOption(c); }; firstOption(node); }
        }
        if (node->tag == L"input" && value.empty()) value = GetAttribute(node, L"placeholder");
        if (node->tag == L"input" && type == L"password") value.assign(value.size(), L'•');
        if (node->tag == L"input" && type == L"submit" && value.empty()) value = L"Submit";
        if (node->tag == L"input" && type == L"reset" && value.empty()) value = L"Reset";
        if (node->tag == L"button" && !fragmentText.empty()) value = fragmentText;
        if (!value.empty() && (!hasChildText || !fragmentText.empty())) {
            int tx = x + max(4, style.paddingLeft + 2);
            int ty = y + max(1, (h - style.fontSize) / 2);
            SetTextColor(dc, (node->tag == L"input" && ControlValue(node).empty() && !GetAttribute(node, L"placeholder").empty()) ? RGB(135,140,150) : style.color);
            HGDIOBJ old = SelectObject(dc, fonts.Get(style)); SetBkMode(dc, TRANSPARENT); TextOutW(dc, tx, ty, value.c_str(), (int)value.size()); SelectObject(dc, old);
        }
        if (node->tag == L"select") { SetTextColor(dc, RGB(75,80,90)); TextOutW(dc, x + w - 18, y + max(1,(h-style.fontSize)/2), L"▾", 1); }
        if (node == focusedControl && IsTextEntryNode(node)) {
            wstring visible = ControlValue(node).substr(0, (size_t)max(0, min(caretPosition, (int)ControlValue(node).size())));
            int cx = x + max(4, style.paddingLeft + 2) + MeasureText(dc, fonts, style, visible);
            HPEN caret = CreatePen(PS_SOLID, 1, RGB(25,80,180)); HGDIOBJ old = SelectObject(dc, caret); MoveToEx(dc, cx, y + 5, nullptr); LineTo(dc, cx, y + h - 5); SelectObject(dc, old); DeleteObject(caret);
        }
    }
    void Paint(HDC dc, const RECT& client) {
        HBRUSH bg = CreateSolidBrush(RGB(244, 244, 244)); FillRect(dc, &client, bg); DeleteObject(bg);
        links.clear();
        Graphics gfx(dc);
        gfx.SetSmoothingMode(SmoothingModeAntiAlias);
        PaintBox(dc, gfx, rootLayout, scrollY, client);
    }

    void PaintBox(HDC dc, Graphics& gfx, const LayoutBox& box, int sy, const RECT& clip) {
        int by = box.y - sy;
        int stickyDelta = box.style.positionSticky && by < 0 ? -by : 0;
        by += stickyDelta;
#if !defined(_WIN32)
        static int paintDebug = 0; if (paintDebug++ < 24)std::fprintf(stderr, "[box] tag=%s x=%d y=%d w=%d h=%d frags=%zu kids=%zu clip=%ld..%ld\n", WToUtf8(box.node ? box.node->tag : L"#root").c_str(), box.x, by, box.w, box.h, box.frags.size(), box.children.size(), clip.top, clip.bottom);
#endif
        if (box.node && box.w > 0 && box.h > 0) {
            if (IsFormControlNode(box.node)) {
                PaintFormControl(dc, box.node, box.style, box.x, by, box.w, box.h);
                LinkRegion controlHit; controlHit.rect = { box.x, by, box.x + box.w, by + box.h }; controlHit.element = box.node; links.push_back(std::move(controlHit));
            }
            auto listeners = domListeners.find(box.node);
            if (listeners != domListeners.end()) {
                auto click = listeners->second.find(L"click");
                if (click != listeners->second.end() && !click->second.empty()) {
                    LinkRegion hit; hit.rect = { box.x, by, box.x + box.w, by + box.h }; hit.element = box.node;
                    links.push_back(std::move(hit));
                }
            }
        }
        if (!IsFormControlNode(box.node) && box.style.background != CLR_INVALID && box.w > 0 && box.h > 0) {
            RECT r = { box.x, by, box.x + box.w, by + box.h }; RECT out;
            if (IntersectRect(&out, &r, &clip)) {
                HBRUSH b = CreateSolidBrush(box.style.background);
                if (box.style.borderRadius > 0) FillRoundedRect(dc, &out, b, box.style.borderRadius);
                else FillRect(dc, &out, b);
                DeleteObject(b);
            }
        }
        if (!IsFormControlNode(box.node) && box.style.borderWidth > 0) {
            HPEN pen = CreatePen(PS_SOLID, box.style.borderWidth, box.style.borderColor);
            HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, box.x, by, box.x + box.w, by + box.h);
            SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(pen);
        }
        if (box.isHr) {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
            HGDIOBJ old = SelectObject(dc, pen); MoveToEx(dc, box.x, by, nullptr); LineTo(dc, box.x + box.w, by);
            SelectObject(dc, old); DeleteObject(pen);
        }

        for (auto& f : box.frags) {
            int fx = f.x, fy = f.y - sy + stickyDelta;
            if (fy > clip.bottom || fy + f.h < clip.top) continue;
            if (f.controlNode) {
                PaintFormControl(dc, f.controlNode, f.style, fx, fy, f.w, f.h, f.text);
                LinkRegion controlHit; controlHit.rect = { fx, fy, fx + f.w, fy + f.h }; controlHit.element = f.controlNode; links.push_back(std::move(controlHit));
            }
            else if (f.img) {
                int iw=(int)f.img->GetWidth(), ih=(int)f.img->GetHeight();
                if (f.style.objectFit == L"contain" && iw>0 && ih>0 && f.w>0 && f.h>0) { double scale=min((double)f.w/iw,(double)f.h/ih); int dw=max(1,(int)std::lround(iw*scale)),dh=max(1,(int)std::lround(ih*scale)); gfx.DrawImage(f.img,fx+(f.w-dw)/2,fy+(f.h-dh)/2,dw,dh); }
                else gfx.DrawImage(f.img, fx, fy, f.w, f.h);
            }
            else {
                HGDIOBJ of = SelectObject(dc, fonts.Get(f.style));
                SetTextColor(dc, f.style.color); SetBkMode(dc, TRANSPARENT);
                TextOutW(dc, fx, fy, f.text.c_str(), (int)f.text.size());
                SelectObject(dc, of);
            }
            if (!f.href.empty()) {
                LinkRegion lr; lr.rect = { fx, fy, fx + f.w, fy + f.h }; lr.href = ResolveURL(currentURL, f.href); lr.element = f.linkNode;
                links.push_back(lr);
            }
        }
        for (auto& c : box.children) PaintBox(dc, gfx, c, sy - stickyDelta, clip);
    }
};

unordered_map<wstring,wstring> HTMLBrowser::localStorageData;
unordered_map<wstring,wstring> HTMLBrowser::sessionStorageData;

#if defined(_WIN32)
// ============================================================
// 6. WIN32 APP
// ============================================================
#define IDC_ADDRESS 1001
#define IDC_GO      1002
#define IDC_BACK    1003
#define IDC_FWD     1004
#define IDC_RELOAD  1005
#define IDC_STATUS  1006

static HTMLBrowser* g_browser = nullptr;
static HWND g_hwndView, g_hwndAddr, g_hwndStatus, g_hwndBack, g_hwndFwd, g_hwndReload, g_hwndGo;
static vector<wstring> g_history; static int g_histPos = -1;

static void LayoutChildren(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    int bW = 90, bH = 26, y = 8;
    MoveWindow(g_hwndBack, 6, y, bW, bH, TRUE);
    MoveWindow(g_hwndFwd, 102, y, bW, bH, TRUE);
    MoveWindow(g_hwndReload, 198, y, bW, bH, TRUE);
    int aX = 300, aW = rc.right - 300 - 60 - 6;
    MoveWindow(g_hwndAddr, aX, y, aW, bH, TRUE);
    MoveWindow(g_hwndGo, aX + aW + 6, y, 50, bH, TRUE);
    MoveWindow(g_hwndView, 0, 44, rc.right, rc.bottom - 44 - 20, TRUE);
    MoveWindow(g_hwndStatus, 0, rc.bottom - 20, rc.right, 20, TRUE);
}

static void DoNavigate(HWND hwnd, wstring url, bool push) {
    if (url.find(L"://") == wstring::npos) url = L"https://" + url;
    SetWindowTextW(g_hwndStatus, (L"Cargando " + url + L"...").c_str());
    SetWindowTextW(g_hwndAddr, url.c_str());
    g_browser->Navigate(url);
    if (push) {
        if (g_histPos + 1 < (int)g_history.size()) g_history.resize(g_histPos + 1);
        g_history.push_back(url); g_histPos = (int)g_history.size() - 1;
    }
    EnableWindow(g_hwndBack, g_histPos > 0);
    EnableWindow(g_hwndFwd, g_histPos + 1 < (int)g_history.size());
    SetWindowTextW(g_hwndStatus, url.c_str());
    InvalidateRect(g_hwndView, nullptr, TRUE);
}

static wstring ReadAddr() {
    int len = GetWindowTextLengthW(g_hwndAddr); if (len <= 0) return L"";
    wstring out(len + 1, 0); GetWindowTextW(g_hwndAddr, &out[0], len + 1); out.resize(wcslen(out.c_str())); return out;
}

LRESULT CALLBACK ViewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
        pt.y += g_browser->scrollY;
        for (auto& l : g_browser->links) { if (PtInRect(&l.rect, pt)) { SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE; } }
        SetCursor(LoadCursor(nullptr, IDC_ARROW)); return TRUE;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        pt.y += g_browser->scrollY;
        for (auto it = g_browser->links.rbegin(); it != g_browser->links.rend(); ++it) if (PtInRect(&it->rect, pt)) {
            bool allowDefault = g_browser->ActivateElement(it->element);
            if (!it->href.empty() && allowDefault)DoNavigate(GetParent(hwnd), it->href, true);
            else { g_browser->Relayout(g_browser->viewportW); InvalidateRect(hwnd, nullptr, TRUE); }
            break;
        }
        SetFocus(hwnd); return 0;
    }
    case WM_CHAR:
        if (g_browser && g_browser->focusedControl) { if (wParam >= 32 || (wParam == 13 && g_browser->focusedControl->tag == L"textarea")) g_browser->InsertText(wParam == 13 ? L"\n" : wstring(1, (wchar_t)wParam)); InvalidateRect(hwnd, nullptr, TRUE); return 0; }
        break;
    case WM_KEYDOWN:
        if (g_browser && g_browser->focusedControl) {
            int key = (int)wParam;
            if (key == VK_LEFT) key = 1000; else if (key == VK_RIGHT) key = 1001; else if (key == VK_HOME) key = 1002; else if (key == VK_END) key = 1003; else if (key == VK_DELETE) key = 127;
            if (key == VK_BACK || key == VK_RETURN || key == 1000 || key == 1001 || key == 1002 || key == 1003 || key == 127) { g_browser->HandleControlKey(key, (GetKeyState(VK_SHIFT) & 0x8000) != 0, (GetKeyState(VK_CONTROL) & 0x8000) != 0); InvalidateRect(hwnd, nullptr, TRUE); return 0; }
        }
        break;
    case WM_SIZE: g_browser->SetViewport(LOWORD(lParam), HIWORD(lParam)); InvalidateRect(hwnd, nullptr, TRUE); return 0;
    case WM_MOUSEWHEEL: g_browser->ScrollBy(-GET_WHEEL_DELTA_WPARAM(wParam) * 50 / WHEEL_DELTA); InvalidateRect(hwnd, nullptr, TRUE); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
        g_browser->Paint(mem, rc);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        SetTimer(hwnd, 0x7FFE, 16, nullptr); // Pump completed Dedicated Worker messages.
        WNDCLASSEXW vc = { sizeof(vc) }; vc.lpfnWndProc = ViewProc; vc.hInstance = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
        vc.hCursor = LoadCursor(nullptr, IDC_ARROW); vc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH); vc.lpszClassName = L"MiniHTMLView";
        RegisterClassExW(&vc);
        g_browser = new HTMLBrowser();

        g_hwndBack = CreateWindowW(L"BUTTON", L"< Atrás", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)IDC_BACK, nullptr, nullptr);
        g_hwndFwd = CreateWindowW(L"BUTTON", L"Adelante >", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)IDC_FWD, nullptr, nullptr);
        g_hwndReload = CreateWindowW(L"BUTTON", L"Recargar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)IDC_RELOAD, nullptr, nullptr);
        g_hwndAddr = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 0, 0, hwnd, (HMENU)IDC_ADDRESS, nullptr, nullptr);
        g_hwndGo = CreateWindowW(L"BUTTON", L"Ir", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)IDC_GO, nullptr, nullptr);
        g_hwndStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SUNKEN, 0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS, nullptr, nullptr);
        g_hwndView = CreateWindowExW(WS_EX_CLIENTEDGE, L"MiniHTMLView", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

        LayoutChildren(hwnd);
        DoNavigate(hwnd, L"https://html5test.com/", true);
        return 0;
    }
    case WM_SIZE: LayoutChildren(hwnd); return 0;
    case WM_TIMER:
        if ((UINT_PTR)wParam == 0x7FFE) { if (g_browser) g_browser->PumpJSTasks(); return 0; }
        if (g_browser) { g_browser->HandleJSTimer((UINT_PTR)wParam); g_browser->PumpJSTasks(); }
        return 0;
    case WM_GETMINMAXINFO: ((MINMAXINFO*)lParam)->ptMinTrackSize = { 480, 320 }; return 0;
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_GO) DoNavigate(hwnd, ReadAddr(), true);
        else if (id == IDC_BACK && g_histPos > 0) DoNavigate(hwnd, g_history[--g_histPos], false);
        else if (id == IDC_FWD && g_histPos + 1 < (int)g_history.size()) DoNavigate(hwnd, g_history[++g_histPos], false);
        else if (id == IDC_RELOAD) DoNavigate(hwnd, ReadAddr(), false);
        return 0;
    }
    case WM_DESTROY: KillTimer(hwnd, 0x7FFE); delete g_browser; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE h, HINSTANCE, PWSTR, int show) {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { sizeof(wc) }; wc.lpfnWndProc = MainProc; wc.hInstance = h; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); wc.lpszClassName = L"MiniHTML"; RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"MiniHTML", L"minihtml v5.0", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 800, nullptr, nullptr, h, nullptr);
    ShowWindow(hwnd, show); UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_TIMER && msg.hwnd == nullptr && g_browser) { g_browser->HandleJSTimer((UINT_PTR)msg.wParam); InvalidateRect(g_hwndView, nullptr, TRUE); }
        else if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && GetFocus() == g_hwndAddr) DoNavigate(hwnd, ReadAddr(), true);
        else { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
#else // Linux native X11 application backend

static HTMLBrowser* g_browser = nullptr;
static std::vector<std::wstring> g_history; static int g_histPos = -1;
static std::wstring g_address; static std::wstring g_status; static bool g_addressFocus = false;
static void LinuxNavigate(const std::wstring& input, bool push = true) {
    std::wstring url = Trim(input); if (url.empty())return;
    if (url.find(L"://") == std::wstring::npos && url.find(L"file://") != 0)url = L"https://" + url;
    g_status = L"Loading " + url;
    if (url.rfind(L"file://", 0) == 0) { std::string path = WToUtf8(url.substr(7)); std::ifstream f(path, std::ios::binary); std::string data((std::istreambuf_iterator<char>(f)), {}); if (f || !data.empty())g_browser->LoadHTML(Utf8ToW(data), url); else g_browser->LoadHTML(L"<h1>Could not open local file</h1>", url); }
    else g_browser->Navigate(url);
    g_address = url; g_status = url;
    if (push) { if (g_histPos + 1 < (int)g_history.size())g_history.resize(g_histPos + 1); g_history.push_back(url); g_histPos = (int)g_history.size() - 1; }
    g_browser->Relayout(g_browser->viewportW);
}
static void LinuxDrawChrome(HDC dc, int w, int h) {
    RECT top{ 0,0,w,42 }; HBRUSH bg = CreateSolidBrush(RGB(232, 235, 240)); FillRect(dc, &top, bg); DeleteObject(bg);
    SetTextColor(dc, RGB(25, 30, 38)); TextOutW(dc, 10, 8, L"MiniHTML", 8);
    RECT addr{ 96,5,w - 12,34 }; HBRUSH white = CreateSolidBrush(RGB(255, 255, 255)); FillRect(dc, &addr, white); DeleteObject(white);
    HPEN p = CreatePen(0, 1, RGB(170, 178, 190)); HGDIOBJ old = SelectObject(dc, p); Rectangle(dc, addr.left, addr.top, addr.right, addr.bottom); SelectObject(dc, old); DeleteObject(p);
    SetTextColor(dc, RGB(45, 50, 60)); TextOutW(dc, 104, 10, g_address.c_str(), (int)g_address.size());
    RECT status{ 0,h - 20,w,h }; HBRUSH sb = CreateSolidBrush(RGB(240, 242, 245)); FillRect(dc, &status, sb); DeleteObject(sb); SetTextColor(dc, RGB(90, 95, 105)); TextOutW(dc, 8, h - 18, g_status.c_str(), (int)std::min<size_t>(g_status.size(), 140));
}
static void LinuxPaint() {
    if (!g_xDisplay || !g_xWindow || !g_browser)return; XWindowAttributes a{}; XGetWindowAttributes(g_xDisplay, g_xWindow, &a); HDC dc = LinuxMakeDC(g_xWindow); RECT clip{ 0,42,a.width,a.height - 20 }; RECT all{ 0,0,a.width,a.height }; HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255)); FillRect(dc, &all, bg); DeleteObject(bg); LinuxDrawChrome(dc, a.width, a.height); g_browser->SetViewport(a.width, a.height - 62); // browser layout width/height
    // Paint browser content in a translated viewport-compatible surface.
    // The main renderer uses absolute y coordinates; chrome height is reserved by offsetting the clip and draw origin.
    g_browser->Paint(dc, clip);
    // Mask the top chrome after page painting.
    LinuxDrawChrome(dc, a.width, a.height);
    XFlush(g_xDisplay); ReleaseDC(nullptr, dc);
}
int main(int argc, char** argv) {
    std::setlocale(LC_ALL, ""); curl_global_init(CURL_GLOBAL_DEFAULT);
    g_xDisplay = XOpenDisplay(nullptr); if (!g_xDisplay) { std::fprintf(stderr, "minihtml: cannot open X11 display\n"); return 2; }g_xScreen = DefaultScreen(g_xDisplay); g_xScreenW = DisplayWidth(g_xDisplay, g_xScreen); g_xScreenH = DisplayHeight(g_xDisplay, g_xScreen);
    int width = 1100, height = 800; g_xWindow = XCreateSimpleWindow(g_xDisplay, RootWindow(g_xDisplay, g_xScreen), 30, 30, width, height, 1, BlackPixel(g_xDisplay, g_xScreen), WhitePixel(g_xDisplay, g_xScreen)); XStoreName(g_xDisplay, g_xWindow, "minihtml v5.0 (Linux native)"); XSelectInput(g_xDisplay, g_xWindow, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask); Atom wmDelete = XInternAtom(g_xDisplay, "WM_DELETE_WINDOW", False); XSetWMProtocols(g_xDisplay, g_xWindow, &wmDelete, 1); XMapWindow(g_xDisplay, g_xWindow);
    g_browser = new HTMLBrowser(); g_browser->SetViewport(width, height - 62);
    std::wstring initial = L"https://example.com"; if (argc > 1) { std::string arg = argv[1]; initial = Utf8ToW(arg); if (arg.find("://") == std::string::npos && arg.find('/') != std::string::npos)initial = L"file://" + initial; }
    LinuxNavigate(initial, true);
    bool running = true; while (running) {
        while (XPending(g_xDisplay)) {
            XEvent e; XNextEvent(g_xDisplay, &e);
            if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == wmDelete) { running = false; break; }
            if (e.type == Expose) { LinuxPaint(); }
            else if (e.type == ConfigureNotify) { g_browser->SetViewport(e.xconfigure.width, e.xconfigure.height - 62); LinuxPaint(); }
            else if (e.type == ButtonPress) {
                if (e.xbutton.button == 4 || e.xbutton.button == 5) { g_browser->ScrollBy(e.xbutton.button == 4 ? -60 : 60); LinuxPaint(); }
                else if (e.xbutton.y < 42) { g_addressFocus = true; g_browser->FocusControl(nullptr); }
                else {
                    g_addressFocus = false;
                    int y = e.xbutton.y + g_browser->scrollY; POINT p{ e.xbutton.x,y };
                    bool handled = false;
                    for (auto it = g_browser->links.rbegin(); it != g_browser->links.rend(); ++it) if (PtInRect(&it->rect, p)) {
                        Node* target = it->element;
                        if (IsFormControlNode(target)) {
                            int caret = -1;
                            if (IsTextEntryNode(target)) { wstring val = ControlValue(target); int xIn = e.xbutton.x - it->rect.left - 8; caret = 0; while (caret < (int)val.size() && xIn > caret * 8) { xIn -= 8; ++caret; } }
                            g_browser->FocusControl(target, caret);
                        }
                        bool allow = g_browser->ActivateElement(target);
                        if (!it->href.empty() && allow) LinuxNavigate(it->href, true);
                        else { g_browser->Relayout(g_browser->viewportW); LinuxPaint(); }
                        handled = true; break;
                    }
                    if (!handled) { g_browser->FocusControl(nullptr); }
                }
            }
            else if (e.type == KeyPress) {
                char buf[128]; KeySym sym; int n = XLookupString(&e.xkey, buf, sizeof(buf), &sym, nullptr);
                if (g_addressFocus) {
                    if (sym == XK_Return) { g_addressFocus = false; LinuxNavigate(g_address, true); }
                    else if (sym == XK_BackSpace) { if (!g_address.empty())g_address.pop_back(); LinuxPaint(); }
                    else if (n > 0) { g_address += Utf8ToW(std::string(buf, (size_t)n)); LinuxPaint(); }
                }
                else if (g_browser->focusedControl) {
                    if (sym == XK_Tab) {
                        vector<Node*> focusables; function<void(Node*)> collect = [&](Node* n) { if (!n)return; if (g_browser->IsFocusable(n) && GetAttribute(n,L"tabindex") != L"-1")focusables.push_back(n); for (Node* c : n->children)collect(c); }; collect(g_browser->document);
                        auto it = std::find(focusables.begin(), focusables.end(), g_browser->focusedControl); int idx = it == focusables.end() ? -1 : (int)(it - focusables.begin()); if (!focusables.empty()) { idx = (idx + ((e.xkey.state & ShiftMask) ? (int)focusables.size()-1 : 1)) % (int)focusables.size(); g_browser->FocusControl(focusables[(size_t)idx]); }
                    }
                    else if (sym == XK_Left) g_browser->HandleControlKey(1000);
                    else if (sym == XK_Right) g_browser->HandleControlKey(1001);
                    else if (sym == XK_Home) g_browser->HandleControlKey(1002);
                    else if (sym == XK_End) g_browser->HandleControlKey(1003);
                    else if (sym == XK_BackSpace) g_browser->HandleControlKey(8);
                    else if (sym == XK_Delete) g_browser->HandleControlKey(127);
                    else if (sym == XK_Return) g_browser->HandleControlKey(13);
                    else if (sym == XK_Escape) g_browser->FocusControl(nullptr);
                    else if ((e.xkey.state & ControlMask) && (sym == XK_a || sym == XK_A)) g_browser->HandleControlKey('a', false, true);
                    else if (n > 0) g_browser->InsertText(Utf8ToW(std::string(buf, (size_t)n)));
                    LinuxPaint();
                }
                else if (sym == XK_F5) { LinuxNavigate(g_address, false); }
                else if (sym == XK_Escape) { g_addressFocus = true; }
            }
        }
        g_browser->HandleDueJSTimers();
        g_browser->PumpJSTasks();
        if (XPending(g_xDisplay) == 0) { struct timespec ts { 0, 12000000 }; nanosleep(&ts, nullptr); }
    }
    delete g_browser; XDestroyWindow(g_xDisplay, g_xWindow); XCloseDisplay(g_xDisplay); g_xDisplay = nullptr; curl_global_cleanup(); return 0;
}
#endif
