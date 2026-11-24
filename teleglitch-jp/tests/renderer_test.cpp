#include "../src/renderer.cpp"

static bool TestOpening() {
    // The v9.3 executable stores the English dossier here, including blank
    // lines, trailing spaces and the final newline used by fast-forward.
    std::ifstream exe("Teleglitch.exe",std::ios::binary);
    exe.seekg(0x13EB00);
    std::string source;
    if (!std::getline(exe,source,'\0') || source.find("MILITECH Corporation Database") != 0 ||
        !LoadOpening(source) || opening_rows.size() != 22) return false;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;
    bool valid = true;
    for (int zoom : {2,3}) {
        const int px = FinalFontPixels(opening_font_height*zoom);
        HFONT font = CreateJapaneseFont(px);
        if (!font) { valid = false; break; }
        auto previous = SelectObject(dc,font);
        int widest = 0, bottom = 0, index = 0;
        for (const auto& row : opening_rows) {
            const int top = int((2 + index++*opening_row_step)*zoom);
            if (row.japanese.empty()) continue;
            SIZE size{};
            const int n = int(row.japanese.size());
            std::vector<WORD> glyphs(n);
            if (!GetTextExtentPoint32W(dc,row.japanese.data(),n,&size) ||
                GetGlyphIndicesW(dc,row.japanese.data(),n,glyphs.data(),GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR ||
                std::find(glyphs.begin(),glyphs.end(),WORD(0xffff)) != glyphs.end()) valid = false;
            widest = std::max(widest,int(size.cx));
            if (top < bottom || 2*zoom + size.cx > 320*zoom) valid = false;
            bottom = top + size.cy;
            if (row.VisibleCharacters(row.start) != 0 || row.VisibleCharacters(row.start+1) == 0 ||
                row.VisibleCharacters(row.start+row.length) != row.japanese.size()) valid = false;
        }
        if (bottom > 230*zoom) valid = false;
        std::printf("Opening %dx%d: %dpx font, max width %dpx, last row ends at y=%d, prompt y=%d\n",
                    320*zoom,240*zoom,px,widest,bottom,230*zoom);
        SelectObject(dc,previous);
        DeleteObject(font);
    }
    DeleteDC(dc);
    return valid;
}

static bool TestInventory() {
    if (inventory_descriptions.size() != 62 ||
        InventoryKey("  first\n second \t\r\n") != "first second") return false;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;
    bool valid = true;
    for (int zoom : {2,3}) {
        int px = FinalFontPixels(inventory_font_height*zoom);
        HFONT font = CreateJapaneseFont(px);
        if (!font) { valid = false; break; }
        auto previous = SelectObject(dc,font);
        int widest = 0, bottom = 0;
        for (const auto& entry : inventory_descriptions) {
            const auto& rows = entry.second;
            if (rows.empty() || rows.size() > 3) valid = false;
            int y = int(InventoryTop(217,1,rows.size())*zoom);
            for (const auto& row : rows) {
                SIZE size{};
                const int n = int(row.size());
                std::vector<WORD> glyphs(n);
                if (!n || !GetTextExtentPoint32W(dc,row.data(),n,&size) ||
                    GetGlyphIndicesW(dc,row.data(),n,glyphs.data(),GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR ||
                    std::find(glyphs.begin(),glyphs.end(),WORD(0xffff)) != glyphs.end() ||
                    size.cx > 120*zoom || size.cy > inventory_row_step*zoom || y+size.cy > 238*zoom)
                    valid = false;
                widest = std::max(widest,int(size.cx));
                bottom = std::max(bottom,int(y+size.cy));
                y += int(inventory_row_step*zoom);
            }
        }
        std::printf("Inventory %dx%d: %dpx font, max width %d/%dpx, bottom %dpx\n",
                    320*zoom,240*zoom,px,widest,120*zoom,bottom);
        SelectObject(dc,previous);
        DeleteObject(font);
    }
    DeleteDC(dc);
    return valid;
}

static bool TestMonsterInfo() {
    if (monster_descriptions.size() != 17) return false;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;
    bool valid = true;
    for (int zoom : {2,3}) {
        HFONT font = CreateJapaneseFont(FinalFontPixels(monster_font_height*zoom));
        if (!font) { valid = false; break; }
        auto previous = SelectObject(dc,font);
        int widest = 0, bottom = 0;
        for (const auto& entry : monster_descriptions) {
            int y = 108*zoom;
            for (const auto& row : entry.second) {
                SIZE size{};
                const int n = int(row.size());
                std::vector<WORD> glyphs(n);
                if (!n || !GetTextExtentPoint32W(dc,row.data(),n,&size) ||
                    GetGlyphIndicesW(dc,row.data(),n,glyphs.data(),GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR ||
                    std::find(glyphs.begin(),glyphs.end(),WORD(0xffff)) != glyphs.end() ||
                    51*zoom + size.cx > 310*zoom || size.cy > monster_row_step*zoom || y+size.cy > 230*zoom)
                    valid = false;
                widest = std::max(widest,int(size.cx));
                bottom = std::max(bottom,int(y+size.cy));
                y += int(monster_row_step*zoom);
            }
        }
        std::printf("Monster info %dx%d: max width %dpx, bottom %dpx\n",320*zoom,240*zoom,widest,bottom);
        SelectObject(dc,previous);
        DeleteObject(font);
    }
    DeleteDC(dc);
    return valid;
}

int main() {
    module_dir=L"teleglitch-jp\\";
    if (!TestOpening()) return 11;
    LoadDictionary();
    if (!TestInventory()) return 12;
    if (!TestMonsterInfo()) return 13;
    if (tutorial_messages.size()!=264 || narrative_titles.size()!=131 || std::abs(body_scale-0.7f)>0.0001f) return 1;
    const int font_pixels=RasterFontPixels(1.0f,true);
    if (font_pixels!=8 || RasterFontPixels(2.0f,true)!=17 || RasterFontPixels(2.0f,false)!=12) return 9;
    GLdouble identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    GLdouble projection[16]={2.0/320,0,0,0,0,-2.0/240,0,0,0,0,-1,0,-1,1,0,1};
    double x,y,bx,by;
    if (!ProjectToNdc(3,4,identity,projection,x,y) ||
        !ProjectToNdc(3,4+12*body_scale,identity,projection,bx,by) ||
        std::abs((x+1)*480-9)>0.001 || std::abs((1-y)*360-12)>0.001 ||
        FinalFontPixels((y-by)*360)!=24 || FinalFontPixels((y-by)*240)!=16) return 10;
    // At the observed 640x480 target, use a 16px font, never an enlarged 8px glyph.
    const int screen_font_pixels=16;
    HDC dc=CreateCompatibleDC(nullptr);
    HFONT font=CreateJapaneseFont(screen_font_pixels);
    if (!dc || !font) return 2;
    auto old=SelectObject(dc,font);
    int rows=0, max_width=0;
    for (const auto& record:tutorial_messages) {
        for (size_t row=0;row<record.second.size();++row) {
            std::string token="@TJ/"+record.first+"/"+std::to_string(row+1)+"/25/25@";
            std::wstring decoded;
            if (!DecodeTutorialToken(token+"   ",decoded) ||
                decoded.find(L"%d")!=std::wstring::npos) return 3;
            if (decoded.empty()) { if(!record.second[row].empty()) return 3; else continue; }
            SIZE size{};
            if (!GetTextExtentPoint32W(dc,decoded.data(),int(decoded.size()),&size) || size.cx>288*2) return 4;
            max_width=std::max(max_width,int(size.cx));
            std::vector<WORD> glyphs(decoded.size());
            if (GetGlyphIndicesW(dc,decoded.data(),int(decoded.size()),glyphs.data(),GGI_MARK_NONEXISTING_GLYPHS)==GDI_ERROR ||
                std::find(glyphs.begin(),glyphs.end(),WORD(0xFFFF))!=glyphs.end()) return 5;
            // The native typewriter must never leak a partial marker onscreen.
            for (size_t i=1;i<token.find('@',4);++i) {
                auto partial=token.substr(0,i);
                std::wstring ignored;
                if (!IsTutorialControlText(partial) || DecodeTutorialToken(partial,ignored)) return 6;
            }
            ++rows;
        }
    }
    for (const auto& token:{"@TJ/missing/1@","@TJ/welcome/0@","@TJ/welcome/99@",
                           "@TJ/welcome/x@","@TJ/targets/1@","@TJ/targets/1/2/-1@",
                           "@TJ/targets/1/2/25@trailing"}) {
        std::wstring decoded;
        if (DecodeTutorialToken(token,decoded) || !decoded.empty()) return 7;
    }
    if (IsTutorialControlText("Continue") || IsTutorialControlText("Plain English")) return 8;
    SelectObject(dc,old); DeleteObject(font); DeleteDC(dc);
    std::printf("PASS: native token decoding; all %d Japanese rows have glyphs at %dpx; max rendered width %d/576px; 131 catalog titles; 640x480 and 960x720 projection; partial token handling\n",rows,screen_font_pixels,max_width);
    return 0;
}
