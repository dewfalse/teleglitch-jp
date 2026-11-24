#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <gl/GL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <share.h>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// The game's MSVC x86 string layout, verified against v9.3's accessors.
// Do not pass a modern std::string across this ABI boundary.
struct GameString {
    union { char inline_text[16]; const char* heap_text; } storage;
    uint32_t size;
    uint32_t capacity;
};
static_assert(sizeof(GameString) == 24, "x86 ABI required");
using DrawLine = void (__cdecl*)(const GameString*, float, float, float);
static DrawLine original_draw;
static std::map<std::string, std::wstring> translations;
static std::map<std::string, std::vector<std::wstring>> tutorial_messages;
static std::map<std::string, std::vector<std::wstring>> inventory_descriptions;
static std::map<std::string, std::vector<std::wstring>> monster_descriptions;
static std::wstring module_dir;
static FILE* logfile;
static bool installed;
static bool initialized;
static std::set<std::string> logged_sources;
static std::set<std::string> narrative_titles;
static float body_scale = 0.70f;
using SwapBuffersFn = BOOL (WINAPI*)(HDC);
static SwapBuffersFn original_swap_buffers;
static bool overlay_ready;

static void Log(const char* msg) {
    if (logfile) { std::fprintf(logfile, "%s\n", msg); std::fflush(logfile); }
}

static std::wstring DecodeUtf8(const std::string& text) {
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                   static_cast<int>(text.size()), nullptr, 0);
    if (!size) return {};
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), &result[0], size);
    return result;
}

static void LoadDictionary() {
    wchar_t setting[32]{};
    GetPrivateProfileStringW(L"text", L"body_scale", L"0.70", setting, 32,
                            (module_dir + L"text_settings.ini").c_str());
    const double configured_scale = std::wcstod(setting, nullptr);
    if (std::isfinite(configured_scale) && configured_scale >= 0.5 && configured_scale <= 1.5)
        body_scale = static_cast<float>(configured_scale);
    std::ifstream stream(module_dir + L"translations.tsv", std::ios::binary);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        auto value = DecodeUtf8(line.substr(tab + 1));
        if (!value.empty()) translations[line.substr(0, tab)] = std::move(value);
    }
    char msg[100];
    std::snprintf(msg, sizeof(msg), "Dictionary entries: %u", unsigned(translations.size()));
    Log(msg);
    const auto read_messages = [&](const wchar_t* filename, auto& destination, bool use_english_key) {
    std::ifstream message_stream(module_dir + filename, std::ios::binary);
    while (std::getline(message_stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        auto first = line.find('\t');
        auto second = first == std::string::npos ? first : line.find('\t', first + 1);
        if (second == std::string::npos) continue;
        std::string encoded = line.substr(second + 1);
        std::vector<std::wstring> rows;
        size_t start = 0;
        do {
            size_t end = encoded.find("\\n", start);
            auto row = DecodeUtf8(encoded.substr(start, end == std::string::npos ? end : end - start));
            rows.push_back(std::move(row));
            if (end == std::string::npos) break;
            start = end + 2;
        } while (start <= encoded.size());
        const auto key = use_english_key ? line.substr(first+1,second-first-1) : line.substr(0,first);
        destination[key] = std::move(rows);
    }
    };
    read_messages(L"tutorial_messages.tsv", tutorial_messages, false);
    read_messages(L"narrative_messages.tsv", tutorial_messages, false);
    read_messages(L"inventory_ja.tsv", inventory_descriptions, true);
    read_messages(L"monsters_ja.tsv", monster_descriptions, true);
    // Display translated catalog titles without changing the internal discovery keys.
    std::ifstream title_stream(module_dir + L"narrative_ja.tsv", std::ios::binary);
    while (std::getline(title_stream, line)) {
        if (line.empty() || line[0] != 's') continue;
        const auto first = line.find('\t');
        const auto second = first == std::string::npos ? first : line.find('\t', first + 1);
        if (second == std::string::npos) continue;
        const auto end = line.find("\\n", second + 1);
        const auto key = line.substr(first + 1, second - first - 1);
        auto title = DecodeUtf8(line.substr(second + 1, end == std::string::npos ? end : end - second - 1));
        if (!title.empty()) { translations[key] = std::move(title); narrative_titles.insert(key); }
    }
    std::snprintf(msg, sizeof(msg), "Message records: %u; body scale: %.2f", unsigned(tutorial_messages.size()), body_scale);
    Log(msg);
}

static bool DecodeTutorialToken(const std::string& source, std::wstring& output, size_t* row_index = nullptr) {
    if (source.compare(0, 4, "@TJ/") != 0) return false;
    const size_t close = source.find('@', 4);
    if (close == std::string::npos || source.find_first_not_of(' ', close + 1) != std::string::npos) return false;
    const std::string body = source.substr(4, close - 4);
    std::vector<std::string> parts;
    size_t start = 0;
    do {
        auto end = body.find('/', start);
        parts.push_back(body.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    } while (start <= body.size());
    if (parts.size() < 2 || parts.size() > 4) return false;
    for (size_t i = 1; i < parts.size(); ++i) {
        if (parts[i].empty() || parts[i].size() > 4 ||
            !std::all_of(parts[i].begin(), parts[i].end(), [](char c) { return c >= '0' && c <= '9'; })) return false;
    }
    auto record = tutorial_messages.find(parts[0]);
    if (record == tutorial_messages.end()) return false;
    size_t row = std::stoul(parts[1]);
    if (row == 0 || row > record->second.size()) return false;
    std::wstring translated = record->second[row - 1];
    size_t arg = 2, offset = 0;
    while ((offset = translated.find(L"%d", offset)) != std::wstring::npos) {
        if (arg >= parts.size()) return false;
        std::wstring number(parts[arg].begin(), parts[arg].end());
        translated.replace(offset, 2, number);
        offset += number.size();
        ++arg;
    }
    output = std::move(translated);
    if (row_index) *row_index = row - 1;
    return true;
}

static bool IsTutorialControlText(const std::string& source) {
    // The dialog reveals source bytes over time. Hide incomplete control text.
    if (source.empty()) return false;
    return source.size() < 4 ? std::string("@TJ/").compare(0, source.size(), source) == 0
                             : source.compare(0, 4, "@TJ/") == 0;
}

struct TextTexture { GLuint id; int width, height, tex_width, tex_height; HGLRC context; };
// The same string may be shown as a menu label and as a smaller body row.
using TextureKey = std::pair<int, std::wstring>;
static std::map<TextureKey, TextTexture> textures;

static int RasterFontPixels(float scale, bool body_text) {
    return body_text ? std::max(1, static_cast<int>(std::lround(12.0f * body_scale * scale))) : 12;
}

static int FinalFontPixels(double requested) {
    // MS Gothic uses half-width ASCII; even sizes keep that advance exact and
    // avoid a 25px request becoming 26px-wide full-width glyphs in GDI.
    return std::max(2, static_cast<int>(std::floor(requested*0.5+0.00001))*2);
}

static HFONT CreateJapaneseFont(int pixels) {
    return CreateFontW(-pixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"MS Gothic");
}

static bool MakeTexture(const std::wstring& text, int font_pixels, TextTexture& result) {
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;
    HFONT font = CreateJapaneseFont(font_pixels);
    if (!font) { DeleteDC(dc); return false; }
    auto old_font = SelectObject(dc, font);
    SIZE size{};
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()), &size);
    int width = std::max(1L, size.cx), height = std::max(LONG(font_pixels), metrics.tmHeight);
    int tw = 1, th = 1;
    while (tw < width) tw <<= 1;
    while (th < height) th <<= 1;
    if (tw > 4096 || th > 256) {
        SelectObject(dc, old_font); DeleteObject(font); DeleteDC(dc); return false;
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = tw;
    info.bmiHeader.biHeight = -th;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) {
        SelectObject(dc, old_font); DeleteObject(font); DeleteDC(dc); return false;
    }
    auto old_bitmap = SelectObject(dc, bitmap);
    std::memset(pixels, 0, size_t(tw) * th * 4);
    SetBkColor(dc, RGB(0, 0, 0));
    SetTextColor(dc, RGB(255, 255, 255));
    TextOutW(dc, 0, 0, text.data(), static_cast<int>(text.size()));
    GdiFlush();
    auto rgba = static_cast<unsigned char*>(pixels);
    for (int i = 0; i < tw * th; ++i) {
        unsigned char coverage = std::max(rgba[i * 4], std::max(rgba[i * 4 + 1], rgba[i * 4 + 2]));
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = coverage;
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    SelectObject(dc, old_bitmap); SelectObject(dc, old_font);
    DeleteObject(bitmap); DeleteObject(font); DeleteDC(dc);
    result = {texture, width, height, tw, th, wglGetCurrentContext()};
    return texture != 0;
}

static bool DrawJapanese(const std::wstring& text, float x, float y, float scale, bool body_text = false, int final_font_pixels = 0) {
    HGLRC context = wglGetCurrentContext();
    if (!context || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(scale)) return false;
    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    const int font_pixels = final_font_pixels > 0 ? final_font_pixels : RasterFontPixels(scale, body_text);
    const TextureKey key{font_pixels, text};
    auto it = textures.find(key);
    if (it != textures.end() && it->second.context != context) {
        // A texture name from a destroyed context is not valid in the new context.
        textures.erase(it); it = textures.end();
    }
    if (it == textures.end()) {
        if (textures.size() >= 256) {
            for (auto& entry : textures) {
                if (entry.second.context == context) glDeleteTextures(1, &entry.second.id);
            }
            textures.clear();
        }
        TextTexture texture{};
        if (!MakeTexture(text, font_pixels, texture)) {
            glPopClientAttrib(); glPopAttrib(); return false;
        }
        it = textures.emplace(key, texture).first;
    }
    const auto& t = it->second;
    // Menu text uses scale=2: a 12px Japanese bitmap matches its 12px line height.
    // Rasterize body text at its final integer pixel size, then draw it 1:1.
    // A 12px font at 70% becomes an 8px GDI font, never a reduced 12px bitmap.
    const float ratio = body_text ? 1.0f : 0.5f * scale;
    if (body_text) { x = std::round(x); y = std::round(y); }
    const float w = t.width * ratio, h = t.height * ratio;
    const float u = float(t.width) / t.tex_width, v = float(t.height) / t.tex_height;
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(u, 0); glVertex2f(x + w, y);
    glTexCoord2f(u, v); glVertex2f(x + w, y + h);
    glTexCoord2f(0, v); glVertex2f(x, y + h);
    glEnd();
    glPopClientAttrib(); glPopAttrib();
    return true;
}

// Body text is laid out in the game's low-resolution target, but rasterized only
// after that target has been enlarged. This gives GDI the actual screen font size.
struct OverlayRow {
    std::wstring text;
    double x, y, height;
    GLfloat color[4];
    HGLRC context;
};
static std::vector<OverlayRow> overlay_rows;

static bool ProjectToNdc(double x, double y, const GLdouble* model, const GLdouble* projection,
                         double& out_x, double& out_y) {
    double eye[4], clip[4];
    for (int i=0; i<4; ++i) eye[i] = model[i]*x + model[4+i]*y + model[12+i];
    for (int i=0; i<4; ++i)
        clip[i] = projection[i]*eye[0] + projection[4+i]*eye[1] + projection[8+i]*eye[2] + projection[12+i]*eye[3];
    if (!std::isfinite(clip[3]) || std::abs(clip[3]) < 0.000001) return false;
    out_x = clip[0]/clip[3]; out_y = clip[1]/clip[3];
    return std::isfinite(out_x) && std::isfinite(out_y);
}

static bool QueueJapanese(const std::wstring& text, float x, float y, double logical_height) {
    if (!overlay_ready || overlay_rows.size() >= 2048 || !wglGetCurrentContext()) return false;
    GLdouble model[16], projection[16];
    glGetDoublev(GL_MODELVIEW_MATRIX, model); glGetDoublev(GL_PROJECTION_MATRIX, projection);
    double nx, ny, bottom_x, bottom_y;
    if (!ProjectToNdc(x,y,model,projection,nx,ny) ||
        !ProjectToNdc(x,y+logical_height,model,projection,bottom_x,bottom_y) ||
        std::abs(bottom_x-nx)>0.0001 || bottom_y>=ny) return false;
    OverlayRow row{text,(nx+1)*0.5,(1-ny)*0.5,(ny-bottom_y)*0.5,{},wglGetCurrentContext()};
    glGetFloatv(GL_CURRENT_COLOR,row.color);
    if (overlay_rows.empty()) {
        static bool logged = false;
        if (!logged) {
            GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp);
            char info[180];
            std::snprintf(info,sizeof(info),"Body source viewport: %dx%d; normalized font height: %.6f",vp[2],vp[3],row.height);
            Log(info); logged=true;
        }
    }
    overlay_rows.push_back(std::move(row));
    return true;
}

static void DrawBodyOverlay() {
    if (overlay_rows.empty()) return;
    const HGLRC context=wglGetCurrentContext();
    if (!context) { overlay_rows.clear(); return; }
    GLint viewport[4], matrix_mode;
    glGetIntegerv(GL_VIEWPORT,viewport); glGetIntegerv(GL_MATRIX_MODE,&matrix_mode);
    if (viewport[2]<=0 || viewport[3]<=0) { overlay_rows.clear(); return; }
    using UseProgramFn=void (APIENTRY*)(GLuint);
    using ActiveTextureFn=void (APIENTRY*)(GLenum);
    const auto use_program=reinterpret_cast<UseProgramFn>(wglGetProcAddress("glUseProgram"));
    const auto active_texture=reinterpret_cast<ActiveTextureFn>(wglGetProcAddress("glActiveTexture"));
    GLint program=0, texture_unit=0x84C0;
    if (use_program) { glGetIntegerv(0x8B8D,&program); use_program(0); }
    if (active_texture) { glGetIntegerv(0x84E0,&texture_unit); active_texture(0x84C0); }
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE); glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0,viewport[2],viewport[3],0,-1,1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    static bool logged=false;
    for (const auto& row:overlay_rows) {
        if (row.context!=context) continue;
        const int pixels=FinalFontPixels(row.height*viewport[3]);
        if (!logged) {
            char info[160];
            std::snprintf(info,sizeof(info),"Body final viewport: %dx%d; direct font: %dpx; texture drawn 1:1",viewport[2],viewport[3],pixels);
            Log(info); logged=true;
        }
        glColor4fv(row.color);
        DrawJapanese(row.text,float(row.x*viewport[2]),float(row.y*viewport[3]),1.0f,true,pixels);
    }
    glMatrixMode(GL_MODELVIEW); glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(matrix_mode); glPopAttrib();
    if (active_texture) active_texture(texture_unit);
    if (use_program) use_program(program);
    overlay_rows.clear();
}

static BOOL WINAPI HookSwapBuffers(HDC dc) {
    DrawBodyOverlay();
    return original_swap_buffers(dc);
}

static bool InstallSwapOverlay() {
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(L"SDL2.dll"));
    if (!base) return false;
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    const auto rva=pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!rva) return false;
    auto descriptor=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+rva);
    for (;descriptor->Name;++descriptor) {
        if (_stricmp(reinterpret_cast<const char*>(base+descriptor->Name),"GDI32.dll")!=0 || !descriptor->OriginalFirstThunk) continue;
        auto name=reinterpret_cast<IMAGE_THUNK_DATA*>(base+descriptor->OriginalFirstThunk);
        auto slot=reinterpret_cast<IMAGE_THUNK_DATA*>(base+descriptor->FirstThunk);
        for (;name->u1.AddressOfData;++name,++slot) {
            if (IMAGE_SNAP_BY_ORDINAL(name->u1.Ordinal)) continue;
            auto symbol=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+name->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(symbol->Name),"SwapBuffers")!=0) continue;
            DWORD protection;
            if (!VirtualProtect(&slot->u1.Function,sizeof(slot->u1.Function),PAGE_READWRITE,&protection)) return false;
            original_swap_buffers=reinterpret_cast<SwapBuffersFn>(slot->u1.Function);
            slot->u1.Function=reinterpret_cast<ULONG_PTR>(&HookSwapBuffers);
            DWORD ignored;
            VirtualProtect(&slot->u1.Function,sizeof(slot->u1.Function),protection,&ignored);
            Log("Final-resolution body text overlay installed (SDL2 SwapBuffers import).");
            return true;
        }
    }
    Log("Final-resolution overlay unavailable; using direct logical-size fonts.");
    return false;
}

static void __cdecl HookDrawLine(const GameString* value, float x, float y, float scale) {
    const char* bytes = value->capacity < 16 ? value->storage.inline_text : value->storage.heap_text;
    if (!bytes || value->size > 65536) { original_draw(value, x, y, scale); return; }
    std::string source(bytes, value->size);
    auto it = translations.find(source);
    std::wstring japanese;
    size_t row_index = 0;
    const bool tutorial = DecodeTutorialToken(source, japanese, &row_index);
    if (!tutorial && IsTutorialControlText(source)) return;
    if (tutorial && japanese.empty()) return;
    if (tutorial) {
        // Lua packs 9.8px text rows into the engine's 7px line grid at 70%.
        const float index = static_cast<float>(row_index);
        const float native_row = std::floor(index * 2.0f * body_scale + 0.00001f);
        y += (index * 14.0f * body_scale - native_row * 7.0f) * scale;
    }
    if (!tutorial) {
        if (it != translations.end()) japanese = it->second;
        else if (std::any_of(source.begin(), source.end(), [](unsigned char c) { return c >= 0x80; }))
            japanese = DecodeUtf8(source);
    }
    const bool body_text=tutorial || narrative_titles.count(source) != 0;
    if (!japanese.empty() && ((body_text && QueueJapanese(japanese,x,y,12.0*body_scale*scale)) || DrawJapanese(japanese, x, y, scale, body_text))) {
        const std::string log_key = tutorial ? source.substr(0, source.find('@', 4) + 1) : source;
        if (logged_sources.size() < 4096 && logged_sources.insert(log_key).second) {
            char message[400];
            std::snprintf(message, sizeof(message), "Rendered: %s (x=%.1f y=%.1f scale=%.1f)", log_key.c_str(), x, y, scale);
            Log(message);
        }
        return;
    }
    original_draw(value, x, y, scale);
}

// The opening passes the entire revealed English prefix to the multiline
// renderer. Use that byte count so fast-forward, sound and input stay in sync.
struct OpeningRow {
    size_t start, length;
    std::wstring japanese;
    size_t VisibleCharacters(size_t bytes) const {
        if (!length || bytes <= start) return 0;
        return (std::min(bytes-start,length)*japanese.size() + length-1) / length;
    }
};
static std::vector<OpeningRow> opening_rows;
static std::string_view opening_source;
static DrawLine original_multiline;
constexpr float opening_font_height = 8.0f; // logical pixels in the 320x240 projection
constexpr float opening_row_step = 10.0f;

static bool LoadOpening(std::string_view source) {
    std::ifstream input(module_dir + L"opening_ja.txt", std::ios::binary);
    std::vector<OpeningRow> rows;
    std::string line;
    size_t start = 0;
    while (std::getline(input,line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto end = source.find('\n',start);
        if (end == std::string_view::npos) return false;
        auto translated = DecodeUtf8(line);
        if ((end == start) != line.empty() || translated.empty() != line.empty()) return false;
        rows.push_back({start,end-start,std::move(translated)});
        start = end + 1;
    }
    if (!input.eof() || start != source.size() || rows.size() != 22) return false;
    opening_source = source;
    opening_rows = std::move(rows);
    return true;
}

static void __cdecl DrawOpening(const GameString* value, float x, float y, float scale) {
    const char* bytes = value->capacity < 16 ? value->storage.inline_text : value->storage.heap_text;
    if (!bytes || value->size > opening_source.size() ||
        opening_source.compare(0,value->size,bytes,value->size) != 0) {
        original_multiline(value,x,y,scale);
        return;
    }
    const size_t first = overlay_rows.size();
    for (const auto& row : opening_rows) {
        if (row.start >= value->size) break;
        const size_t count = row.VisibleCharacters(value->size);
        if (count && !QueueJapanese(row.japanese.substr(0,count),x,y,opening_font_height*scale)) {
            overlay_rows.resize(first);
            original_multiline(value,x,y,scale);
            return;
        }
        y += opening_row_step*scale;
    }
}

// HUD coordinates are in the 320x240 projection. Reserve the bottom two
// logical pixels, shifting only three-line descriptions up from y=217.
constexpr float inventory_font_height = 7.0f;
constexpr float inventory_row_step = 8.0f;
static float InventoryTop(float y, float scale, size_t count) {
    const float height = inventory_font_height + (float(count)-1)*inventory_row_step;
    return std::min(y,238.0f - height*scale);
}

static std::string InventoryKey(std::string_view source) {
    std::string key;
    bool space = false;
    for (char c : source) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            space = !key.empty();
        } else {
            if (space) key += ' ';
            // The scientist entry in monsters.lua has a Windows-1252 en dash.
            if (static_cast<unsigned char>(c) == 0x96) key += u8"–";
            else key += c;
            space = false;
        }
    }
    return key;
}

constexpr float monster_font_height = 8.0f;
constexpr float monster_row_step = 10.0f;

static void DrawDescription(const GameString* value, float x, float y, float scale,
                            const std::map<std::string, std::vector<std::wstring>>& descriptions,
                            float font_height, float row_step, bool bottom_align_if_needed) {
    const char* bytes = value->capacity < 16 ? value->storage.inline_text : value->storage.heap_text;
    if (bytes && value->size <= 65536) {
        auto it = descriptions.find(InventoryKey({bytes,value->size}));
        if (it != descriptions.end()) {
            const size_t first = overlay_rows.size();
            float top = bottom_align_if_needed ? InventoryTop(y,scale,it->second.size()) : y;
            for (const auto& row : it->second) {
                if (!QueueJapanese(row,x,top,font_height*scale)) {
                    overlay_rows.resize(first);
                    original_multiline(value,x,y,scale);
                    return;
                }
                top += row_step*scale;
            }
            return;
        }
    }
    original_multiline(value,x,y,scale);
}

static void __cdecl DrawInventory(const GameString* value, float x, float y, float scale) {
    DrawDescription(value,x,y,scale,inventory_descriptions,inventory_font_height,inventory_row_step,true);
}

static void __cdecl DrawMonsterInfo(const GameString* value, float x, float y, float scale) {
    DrawDescription(value,x,y,scale,monster_descriptions,monster_font_height,monster_row_step,false);
}

static bool RedirectMultilineCall(unsigned char* base, size_t rva, DrawLine replacement) {
    auto call = base + rva;
    int32_t displacement;
    std::memcpy(&displacement,call+1,sizeof(displacement));
    if (call[0] != 0xE8 || displacement != (base+0xB6450) - (call+5)) return false;
    DWORD protection, ignored;
    if (!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&protection)) return false;
    original_multiline = reinterpret_cast<DrawLine>(base + 0xB6450);
    const uint32_t offset = reinterpret_cast<uintptr_t>(replacement) - reinterpret_cast<uintptr_t>(call+5);
    std::memcpy(call+1,&offset,sizeof(offset));
    FlushInstructionCache(GetCurrentProcess(),call,5);
    VirtualProtect(call,5,protection,&ignored);
    return true;
}

static bool InstallOpening(unsigned char* base) {
    const char* source = reinterpret_cast<const char*>(base + 0x140500);
    const size_t length = strnlen(source,2048);
    return length < 2048 && std::strncmp(source,"MILITECH Corporation Database",28) == 0 &&
           LoadOpening({source,length}) && RedirectMultilineCall(base,0x3B72C,&DrawOpening);
}

static bool Install() {
    if (installed) return true;
    auto base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto pe = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        pe->FileHeader.TimeDateStamp != 0x537B6C03 || pe->OptionalHeader.SizeOfImage != 0x1AA000) {
        Log("Unsupported executable build. No hook installed."); return false;
    }
    auto target = base + 0xB6390;
    const unsigned char expected[] = {0x55,0x8B,0xEC,0x51,0xC7,0x45,0xFC,0xCC,0xCC,0xCC,0xCC};
    if (std::memcmp(target, expected, sizeof(expected)) != 0) {
        Log("Unsupported executable: function signature mismatch. No hook installed."); return false;
    }
    auto trampoline = static_cast<unsigned char*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!trampoline) return false;
    std::memcpy(trampoline, target, sizeof(expected));
    trampoline[11] = 0xE9;
    *reinterpret_cast<int32_t*>(trampoline + 12) = int32_t((target + 11) - (trampoline + 16));
    DWORD old_trampoline;
    if (!VirtualProtect(trampoline, 32, PAGE_EXECUTE_READ, &old_trampoline)) {
        VirtualFree(trampoline, 0, MEM_RELEASE); return false;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, 16);
    DWORD old_protection;
    if (!VirtualProtect(target, sizeof(expected), PAGE_EXECUTE_READWRITE, &old_protection)) {
        VirtualFree(trampoline, 0, MEM_RELEASE); return false;
    }
    original_draw = reinterpret_cast<DrawLine>(trampoline);
    target[0] = 0xE9;
    *reinterpret_cast<int32_t*>(target + 1) = int32_t(reinterpret_cast<unsigned char*>(&HookDrawLine) - (target + 5));
    std::memset(target + 5, 0x90, 6);
    DWORD ignored;
    VirtualProtect(target, sizeof(expected), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(expected));
    installed = true;
    overlay_ready = InstallSwapOverlay();
    Log(overlay_ready && InstallOpening(base) ? "Opening translation ready (22 lines)." :
                                               "Opening translation unavailable; using game text.");
    Log(overlay_ready && !inventory_descriptions.empty() && RedirectMultilineCall(base,0x5C483,&DrawInventory)
        ? "Inventory description translation ready." : "Inventory descriptions using game text.");
    // The v9.3 detail panels pass item.longtext at (88,86) and enemy.description at (51,108).
    Log(overlay_ready && !inventory_descriptions.empty() && RedirectMultilineCall(base,0x48506,&DrawInventory)
        ? "Item info translation ready." : "Item info using game text.");
    Log(overlay_ready && !monster_descriptions.empty() && RedirectMultilineCall(base,0x492BE,&DrawMonsterInfo)
        ? "Monster info translation ready." : "Monster info using game text.");
    Log("Japanese renderer installed (v9.3 x86, line RVA 0xB6390).");
    return true;
}

extern "C" __declspec(dllexport) int __cdecl luaopen_teleglitch_jp(void* state) {
    if (!initialized) {
        logfile = _wfsopen((module_dir + L"renderer.log").c_str(), L"w", _SH_DENYNO);
        LoadDictionary();
        initialized = true;
    }
    const bool ready = Install() && !tutorial_messages.empty();
    using PushBoolean = void (__cdecl*)(void*, int);
    auto push_boolean = reinterpret_cast<PushBoolean>(GetProcAddress(GetModuleHandleW(L"lua52.dll"), "lua_pushboolean"));
    if (!push_boolean) return 0;
    push_boolean(state, ready ? 1 : 0);
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(instance, path, MAX_PATH);
        module_dir = path;
        module_dir.erase(module_dir.find_last_of(L"\\/") + 1);
    }
    return TRUE;
}
