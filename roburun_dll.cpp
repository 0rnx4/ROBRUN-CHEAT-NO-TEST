// roburun_dll.cpp - Final Audited Edition (3 Bugs Fixed)
// build: cl /std:c++20 /O2 /EHsc /D_UNICODE /DUNICODE /LD roburun_dll.cpp ^
//        /link gdiplus.lib comctl32.lib user32.lib gdi32.lib

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Gdiplus;

// ============================================================
// 1. Localization
// ============================================================
enum Lang { LANG_JA, LANG_EN };
static Lang g_lang = LANG_JA;

enum StrID {
    STR_TITLE, STR_TAB_ESP, STR_TAB_AIM, STR_TAB_MISC,
    STR_BOX, STR_SKEL, STR_HP, STR_NAME, STR_DIST, STR_SNAP, STR_TEAM, STR_HEAD_DOT,
    STR_AIM_ON, STR_TRIGGER, STR_FOV, STR_SMOOTH, STR_MAX_DIST, STR_PREDICT, STR_RECOIL,
    STR_STREAM, STR_KEY_MENU, STR_KEY_ESP, STR_KEY_AIM, STR_KEY_PANIC,
    STR_SAVE, STR_LOAD, STR_PRESS_KEY, STR_LANG_LABEL, STR_FOV_CIRCLE,
    STR_LANG_JA, STR_LANG_EN
};

static const wchar_t* g_strs[][2] = {
    { L"ROBRUN 究極", L"ROBRUN Ultimate" },
    { L"見た目", L"Visuals" }, { L"狙い", L"Aimbot" }, { L"その他", L"Misc" },
    { L"四角い枠", L"Box" }, { L"骨格", L"Skeleton" }, { L"HPバー", L"HP Bar" },
    { L"名前", L"Name" }, { L"距離", L"Distance" }, { L"スナップライン", L"Snapline" },
    { L"チーム除外", L"Team Check" }, { L"頭の点", L"Head Dot" },
    { L"エイム有効", L"Enable Aimbot" }, { L"トリガーボット", L"Triggerbot" },
    { L"FOV (px)", L"FOV (px)" }, { L"スムーズ", L"Smooth" },
    { L"最大距離", L"Max Distance" }, { L"未来予測", L"Prediction" }, { L"反動制御", L"Recoil Control" },
    { L"配信対策", L"Stream Proof" }, { L"メニューキー", L"Menu Key" },
    { L"ESPキー", L"ESP Key" }, { L"エイムキー", L"Aimbot Key" },
    { L"パニックキー", L"Panic Key" }, { L"設定保存", L"Save" }, { L"設定読込", L"Load" },
    { L"キー入力待ち...", L"Press any key..." }, { L"言語", L"Language" },
    { L"FOVサークル", L"FOV Circle" },
    { L"日本語", L"Japanese" }, { L"英語", L"English" }
};
static const wchar_t* T(StrID id) { return g_strs[id][g_lang == LANG_JA ? 0 : 1]; }

static const wchar_t* get_key_name(int vk) {
    static wchar_t buf[32];
    if (vk >= VK_F1 && vk <= VK_F12) { swprintf_s(buf, L"F%d", vk - VK_F1 + 1); return buf; }
    if (vk >= 'A' && vk <= 'Z') { swprintf_s(buf, L"%c", vk); return buf; }
    if (vk >= '0' && vk <= '9') { swprintf_s(buf, L"%c", vk); return buf; }
    switch (vk) {
        case VK_INSERT: return L"Insert"; case VK_HOME: return L"Home"; case VK_END: return L"End";
        case VK_DELETE: return L"Delete"; case VK_SHIFT: return L"Shift"; case VK_CONTROL: return L"Ctrl";
        case VK_MENU: return L"Alt"; case VK_SPACE: return L"Space"; case VK_TAB: return L"Tab";
        case VK_LBUTTON: return L"Mouse L"; case VK_RBUTTON: return L"Mouse R"; case VK_MBUTTON: return L"Mouse M";
        default: swprintf_s(buf, L"Key %02X", vk); return buf;
    }
}

// ============================================================
// 2. Config & Offsets
// ============================================================
struct Offsets {
    uintptr_t static_unity_entry = 0;
    uint32_t off_game = 0x28, off_game_model = 0x0D0, off_battle = 0x10;
    uint32_t bm_player = 0x18, bm_entities = 0x90, bm_camera = 0x358;
    uint32_t em_models_dict = 0x50, dict_entries = 0x18, dict_count = 0x20, entry_stride = 0x18, entry_value = 0x10;
    uint32_t ch_id = 0x38, ch_transform = 0x58, ch_private = 0x10, ch_skeleton = 0x138;
    uint32_t tr_x = 0x10, tr_y = 0x14, tr_z = 0x18;
    uint32_t pm_health = 0x1B0, pm_name = 0x50, hm_health_ptr = 0x10, health_value = 0x10;
    uint32_t cb_fov = 0xB0, cb_raw_pos = 0xFC, cb_raw_rot = 0x108, cb_pos_corr = 0x128, cb_rot_corr = 0x134;
    uint32_t sk_bones_data = 0x18, bone_hierarchy = 0x28;
    uint32_t bone_trs_arr = 0x18, bone_parents = 0x20, bone_native_arr = 0x30;
    uint32_t ntf_gameobject = 0x20, go_name = 0x50;
};
static Offsets O;

struct Config {
    bool esp = true, esp_box = true, esp_skeleton = true, esp_hp = true;
    bool esp_name = true, esp_dist = true, esp_snap = true, esp_team = true, esp_head_dot = true;
    bool aim = true, trigger = false, fov_circle = true, predict = true, recoil = true;
    float fov = 150.f, smooth = 15.f, max_dist = 500.f;
    bool stream_proof = true;
    int key_menu = VK_F4, key_esp = VK_F1, key_aim = VK_F2, key_panic = VK_F3;
    Lang lang = LANG_JA;
};
static Config g_cfg;
static std::mutex g_cfg_mtx;

static Config cfg_snapshot() {
    std::lock_guard<std::mutex> lk(g_cfg_mtx);
    return g_cfg;
}

// ============================================================
// 3. Safe Memory
// ============================================================
static uintptr_t g_base = 0;

static bool IsValidPtrFast(uintptr_t addr) {
    if (!addr) return false;
    thread_local std::unordered_map<uintptr_t, bool> page_cache;
    uintptr_t page = addr & ~0xFFF;
    auto it = page_cache.find(page);
    if (it != page_cache.end()) return it->second;
    MEMORY_BASIC_INFORMATION mbi;
    bool valid = VirtualQuery((LPCVOID)page, &mbi, sizeof(mbi)) != 0 &&
                 mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
    if (page_cache.size() > 8192) page_cache.clear();
    page_cache[page] = valid;
    return valid;
}

template <class T> T SafeRead(uintptr_t addr) {
    if (!IsValidPtrFast(addr)) return T{};
    __try { return *(T*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return T{}; }
}

static bool SafeReadBuf(uintptr_t addr, void* out, size_t n) {
    if (!IsValidPtrFast(addr) || !out) return false;
    __try { memcpy(out, (void*)addr, n); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static std::wstring ReadWStr(uintptr_t addr) {
    if (!IsValidPtrFast(addr)) return L"";
    int len = SafeRead<int>(addr + 0x10);
    if (len <= 0 || len > 64) return L"";
    std::wstring s(len, L'\0');
    if (!SafeReadBuf(addr + 0x14, s.data(), (size_t)len * 2)) return L"";
    return s;
}

// ============================================================
// 4. Math
// ============================================================
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(const Vec3& o) const { return { x+o.x, y+o.y, z+o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x-o.x, y-o.y, z-o.z }; }
    Vec3 operator*(float s)      const { return { x*s, y*s, z*s }; }
    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    float len()  const { return sqrtf(x*x + y*y + z*z); }
    float len2() const { return x*x + y*y + z*z; }
    bool zero()   const { return x==0.f && y==0.f && z==0.f; }
    bool finite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
};
struct Quat { float x = 0, y = 0, z = 0, w = 1; };
static inline Quat qmul(const Quat& a, const Quat& b) {
    return { a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y, a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x, a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w, a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z };
}
static inline Quat qnorm(const Quat& q) {
    float n = sqrtf(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (n < 1e-8f) return { 0, 0, 0, 1 };
    return { q.x/n, q.y/n, q.z/n, q.w/n };
}
static inline Vec3 qrot(const Quat& q, const Vec3& v) {
    Vec3 u{ q.x, q.y, q.z }; float s = q.w;
    Vec3 uv{ u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z, u.x*v.y - u.y*v.x };
    return u * (2.f * u.dot(v)) + v * (s*s - u.dot(u)) + uv * (2.f * s);
}

// ============================================================
// 5. Frame
// ============================================================
struct Entity { uintptr_t addr = 0; int id = 0; Vec3 pos{}; Vec3 vel{}; int health = 0; std::wstring name; uintptr_t skeleton = 0; };
struct TransformTRS { float pos[4]; float rot[4]; float scale[4]; };
struct CameraData { Vec3 pos{}, right{1,0,0}, up{0,1,0}, fwd{0,0,1}; float fov = 60.f; };

constexpr int MAX_ENTS = 256;
struct Frame {
    Entity ents[MAX_ENTS];
    int ent_count = 0;
    CameraData cam;
    int sw = 0, sh = 0;
    bool valid = false;
    int local_id = -1;
};

static std::mutex g_frame_mtx;
static std::shared_ptr<const Frame> g_frame;
static std::atomic<bool> g_run{ true };
static std::unordered_map<std::wstring, int> g_bone_name_to_idx;
static std::unordered_map<int, Vec3> g_prev_positions;
static auto g_last_frame_time = std::chrono::steady_clock::now();

// ============================================================
// 6. Reader (BUG FIX: IMAGE_FIRST_SECTION macro)
// ============================================================
static uintptr_t auto_scan() {
    uintptr_t base = g_base; if (!base) return 0;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    uintptr_t start = 0, end = 0;
    // BUG FIX: 標準マクロ IMAGE_FIRST_SECTION を使用
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (sec[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
            start = base + sec[i].VirtualAddress;
            end = start + sec[i].Misc.VirtualSize;
            break;
        }
    }
    if (!start) return 0;
    std::vector<uint8_t> buf(0x100000);
    for (uintptr_t a = start; a < end; a += buf.size()) {
        size_t sz = std::min((size_t)(end - a), buf.size());
        if (!SafeReadBuf(a, buf.data(), sz)) continue;
        for (size_t i = 0; i + 8 <= sz; i += 8) {
            uintptr_t ptr = *(uintptr_t*)(buf.data() + i);
            if (ptr < 0x10000 || ptr > 0x7FFFFFFFFFFF) continue;
            uintptr_t game = SafeRead<uintptr_t>(ptr + O.off_game); if (!game) continue;
            uintptr_t gm = SafeRead<uintptr_t>(game + O.off_game_model); if (!gm) continue;
            uintptr_t battle = SafeRead<uintptr_t>(gm + O.off_battle); if (!battle) continue;
            if (SafeRead<uintptr_t>(battle + O.bm_camera) && SafeRead<uintptr_t>(battle + O.bm_entities)) return (a + i) - base;
        }
    }
    return 0;
}

static Vec3 read_bone_world(uintptr_t hierarchy, int index) {
    uintptr_t trs = SafeRead<uintptr_t>(hierarchy + O.bone_trs_arr);
    uintptr_t par = SafeRead<uintptr_t>(hierarchy + O.bone_parents);
    if (!trs || !par || index < 0) return {};
    TransformTRS t = SafeRead<TransformTRS>(trs + (size_t)index * 0x30);
    Vec3 p = { t.pos[0], t.pos[1], t.pos[2] };
    int parent = SafeRead<int>(par + (size_t)index * 4), depth = 0;
    while (parent >= 0 && parent < 256 && depth++ < 64) {
        TransformTRS pt = SafeRead<TransformTRS>(trs + (size_t)parent * 0x30);
        Quat q = { pt.rot[0], pt.rot[1], pt.rot[2], pt.rot[3] };
        Vec3 s = { pt.scale[0], pt.scale[1], pt.scale[2] }, pp = { pt.pos[0], pt.pos[1], pt.pos[2] };
        p = pp + qrot(qnorm(q), { s.x * p.x, s.y * p.y, s.z * p.z });
        parent = SafeRead<int>(par + (size_t)parent * 4);
    }
    return p;
}

static void build_bone_cache(uintptr_t skel) {
    g_bone_name_to_idx.clear();
    uintptr_t data = SafeRead<uintptr_t>(skel + O.sk_bones_data); if (!data) return;
    uintptr_t hier = SafeRead<uintptr_t>(data + O.bone_hierarchy); if (!hier) return;
    uintptr_t native_arr = SafeRead<uintptr_t>(hier + O.bone_native_arr); if (!native_arr) return;

    for (int i = 0; i < 256; ++i) {
        uintptr_t ntf = SafeRead<uintptr_t>(native_arr + (size_t)i * 8);
        if (!ntf) continue;
        uintptr_t go = SafeRead<uintptr_t>(ntf + O.ntf_gameobject); if (!go) continue;
        uintptr_t np = SafeRead<uintptr_t>(go + O.go_name); if (!np) continue;
        char buf[64]; if (!SafeReadBuf(np, buf, sizeof(buf))) continue;
        buf[63] = '\0';
        std::string name(buf);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.find("head") != std::string::npos) g_bone_name_to_idx[L"head"] = i;
        if (name.find("pelvis") != std::string::npos || name.find("hip") != std::string::npos) g_bone_name_to_idx[L"pelvis"] = i;
        if (name.find("foot") != std::string::npos) {
            if (name.find("l_") == 0 || name.find("left") != std::string::npos) g_bone_name_to_idx[L"l_foot"] = i;
            if (name.find("r_") == 0 || name.find("right") != std::string::npos) g_bone_name_to_idx[L"r_foot"] = i;
        }
    }
}

static void read_frame(int sw, int sh, Frame& out) {
    out.ent_count = 0; out.sw = sw; out.sh = sh; out.valid = false;
    if (!O.static_unity_entry) return;
    uintptr_t ue = SafeRead<uintptr_t>(g_base + O.static_unity_entry); if (!ue) return;
    uintptr_t battle = SafeRead<uintptr_t>(SafeRead<uintptr_t>(SafeRead<uintptr_t>(ue + O.off_game) + O.off_game_model) + O.off_battle);
    if (!battle) return;

    uintptr_t cam = SafeRead<uintptr_t>(battle + O.bm_camera);
    if (cam) {
        out.cam.pos = SafeRead<Vec3>(cam + O.cb_raw_pos) + SafeRead<Vec3>(cam + O.cb_pos_corr);
        Quat q = qnorm(qmul(SafeRead<Quat>(cam + O.cb_raw_rot), SafeRead<Quat>(cam + O.cb_rot_corr)));
        out.cam.right = qrot(q, {1,0,0}); out.cam.up = qrot(q, {0,1,0}); out.cam.fwd = qrot(q, {0,0,1});
        out.cam.fov = SafeRead<float>(cam + O.cb_fov);
        if (out.cam.fov < 10.f || out.cam.fov > 170.f) out.cam.fov = 60.f;
    }
    out.valid = true;

    uintptr_t pm = SafeRead<uintptr_t>(battle + O.bm_player);
    if (pm) {
        uintptr_t ch = SafeRead<uintptr_t>(pm + O.ch_private);
        if (ch) out.local_id = SafeRead<int>(ch + O.ch_id);
        else out.local_id = SafeRead<int>(pm + O.ch_id);
    }

    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - g_last_frame_time).count();
    g_last_frame_time = now;
    if (dt <= 0.0f || dt > 1.0f) dt = 1.0f / 60.0f;
    if (g_prev_positions.size() > 1024) g_prev_positions.clear();

    uintptr_t em = SafeRead<uintptr_t>(battle + O.bm_entities); if (!em) return;
    uintptr_t dict = SafeRead<uintptr_t>(em + O.em_models_dict); if (!dict) return;
    uintptr_t entries = SafeRead<uintptr_t>(dict + O.dict_entries);
    int32_t count = SafeRead<int32_t>(dict + O.dict_count);
    if (!entries || count <= 0 || count > 5000) return;

    for (int32_t i = 0; i < count && out.ent_count < MAX_ENTS; ++i) {
        uintptr_t e = entries + (size_t)i * O.entry_stride;
        if (SafeRead<int32_t>(e) < 0) continue;
        uintptr_t ch = SafeRead<uintptr_t>(e + O.entry_value); if (!ch) continue;

        Entity& ent = out.ents[out.ent_count];
        ent.addr = ch; ent.id = SafeRead<int>(ch + O.ch_id);

        uintptr_t tr = SafeRead<uintptr_t>(ch + O.ch_transform);
        if (tr) {
            ent.pos = { SafeRead<float>(tr + O.tr_x), SafeRead<float>(tr + O.tr_y), SafeRead<float>(tr + O.tr_z) };
            auto it = g_prev_positions.find(ent.id);
            if (it != g_prev_positions.end()) ent.vel = (ent.pos - it->second) * (1.0f / dt);
            g_prev_positions[ent.id] = ent.pos;
        }
        uintptr_t private_m = SafeRead<uintptr_t>(ch + O.ch_private);
        if (private_m) {
            uintptr_t hm = SafeRead<uintptr_t>(private_m + O.pm_health);
            if (hm) { uintptr_t v = SafeRead<uintptr_t>(hm + O.hm_health_ptr); if (v) ent.health = (int)SafeRead<float>(v + O.health_value); }
            uintptr_t np = SafeRead<uintptr_t>(private_m + O.pm_name); if (np) ent.name = ReadWStr(np);
        }
        ent.skeleton = SafeRead<uintptr_t>(ch + O.ch_skeleton);
        if (ent.pos.finite() && !ent.pos.zero()) out.ent_count++;
    }

    if (out.local_id == -1 && out.ent_count > 0) {
        float min_d = FLT_MAX;
        for (int i = 0; i < out.ent_count; ++i) {
            float d = (out.ents[i].pos - out.cam.pos).len2();
            if (d < min_d) { min_d = d; out.local_id = out.ents[i].id; }
        }
    }
}

// ============================================================
// 7. Renderer
// ============================================================
static bool world_to_screen(const Vec3& world, float& sx, float& sy, const CameraData& cam, int sw, int sh) {
    if (sw <= 0 || sh <= 0) return false;
    Vec3 d = world - cam.pos;
    float x = d.dot(cam.right), y = d.dot(cam.up), z = d.dot(cam.fwd);
    if (z <= 0.05f) return false;
    float t = tanf(cam.fov * 3.14159265f / 360.f), a = (float)sw / (float)sh;
    sx = (0.5f + 0.5f * x / (z * t * a)) * sw;
    sy = (0.5f - 0.5f * y / (z * t)) * sh;
    return std::isfinite(sx) && std::isfinite(sy);
}

static ULONG_PTR g_gdip = 0;
static HWND g_ovl = nullptr, g_game = nullptr, g_menu = nullptr;

struct Gfx {
    HDC screen = nullptr, mem = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ old_bmp = nullptr;
    void* bits = nullptr;
    int w = 0, h = 0;
    FontFamily* ff = nullptr; Font* font = nullptr;
    SolidBrush* br_white = nullptr, *br_shadow = nullptr, *br_enemy = nullptr, *br_team = nullptr, *br_fov = nullptr;
    Pen* pen_box_e = nullptr, *pen_box_t = nullptr, *pen_bone_e = nullptr, *pen_bone_t = nullptr, *pen_snap = nullptr, *pen_fov = nullptr;

    bool init() {
        if (ff) return true;
        ff = new (std::nothrow) FontFamily(L"Consolas");
        if (!ff) return false;
        font = new (std::nothrow) Font(ff, 12.f, FontStyleRegular, UnitPixel);
        br_white = new (std::nothrow) SolidBrush(Color(240,240,240,240));
        br_shadow = new (std::nothrow) SolidBrush(Color(200,0,0,0));
        br_enemy = new (std::nothrow) SolidBrush(Color(255,255,60,60));
        br_team = new (std::nothrow) SolidBrush(Color(255,60,140,255));
        br_fov = new (std::nothrow) SolidBrush(Color(30,255,255,255));
        pen_box_e = new (std::nothrow) Pen(Color(230,255,60,60), 1.4f);
        pen_box_t = new (std::nothrow) Pen(Color(230,60,140,255), 1.4f);
        pen_bone_e = new (std::nothrow) Pen(Color(255,255,255,255), 1.2f);
        pen_bone_t = new (std::nothrow) Pen(Color(150,255,255,255), 1.2f);
        pen_snap = new (std::nothrow) Pen(Color(100,255,255,255), 1.0f);
        pen_fov = new (std::nothrow) Pen(Color(100,200,200,200), 1.0f);
        return font && br_white && br_shadow && br_enemy && br_team && br_fov
            && pen_box_e && pen_box_t && pen_bone_e && pen_bone_t && pen_snap && pen_fov;
    }
    bool resize(int nw, int nh) {
        if (nw <= 0 || nh <= 0) return false;
        if (nw == w && nh == h && mem && dib) return true;
        if (mem && old_bmp) SelectObject(mem, old_bmp);
        if (dib) { DeleteObject(dib); dib = nullptr; }
        if (mem) { DeleteDC(mem); mem = nullptr; }
        if (!screen) screen = GetDC(nullptr);
        if (!screen) return false;
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = nw; bmi.bmiHeader.biHeight = -nh;
        bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        mem = CreateCompatibleDC(screen);
        if (!mem) return false;
        dib = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib) { DeleteDC(mem); mem = nullptr; return false; }
        old_bmp = SelectObject(mem, dib); w = nw; h = nh; return true;
    }
    void shutdown() {
        delete font; delete ff;
        delete br_white; delete br_shadow; delete br_enemy; delete br_team; delete br_fov;
        delete pen_box_e; delete pen_box_t; delete pen_bone_e; delete pen_bone_t; delete pen_snap; delete pen_fov;
        font = nullptr; ff = nullptr;
        br_white = nullptr; br_shadow = nullptr; br_enemy = nullptr; br_team = nullptr; br_fov = nullptr;
        pen_box_e = nullptr; pen_box_t = nullptr; pen_bone_e = nullptr; pen_bone_t = nullptr; pen_snap = nullptr; pen_fov = nullptr;
        if (mem && old_bmp) SelectObject(mem, old_bmp);
        if (dib) { DeleteObject(dib); dib = nullptr; }
        if (mem) { DeleteDC(mem); mem = nullptr; }
        if (screen) { ReleaseDC(nullptr, screen); screen = nullptr; }
    }
};
static Gfx g_gfx;

static void draw_bone(Graphics& g, Pen* pen, const Vec3& a, const Vec3& b, const CameraData& cam, int sw, int sh) {
    float ax, ay, bx, by;
    if (world_to_screen(a, ax, ay, cam, sw, sh) && world_to_screen(b, bx, by, cam, sw, sh))
        g.DrawLine(pen, ax, ay, bx, by);
}

static void render() {
    RECT r{}; GetClientRect(g_game, &r);
    if (r.right <= 0 || r.bottom <= 0) return;
    POINT tl{ 0, 0 }; ClientToScreen(g_game, &tl);
    static RECT last_r{}; static POINT last_tl{};
    if (r.right != last_r.right || r.bottom != last_r.bottom || tl.x != last_tl.x || tl.y != last_tl.y) {
        SetWindowPos(g_ovl, HWND_TOPMOST, tl.x, tl.y, r.right, r.bottom, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        last_r = r; last_tl = tl;
    }
    if (!g_gfx.resize(r.right, r.bottom)) return;
    memset(g_gfx.bits, 0, (size_t)r.right * r.bottom * 4);

    std::shared_ptr<const Frame> f;
    { std::lock_guard<std::mutex> lk(g_frame_mtx); f = g_frame; }
    Config cfg = cfg_snapshot();

    {
        Bitmap bmp(r.right, r.bottom, r.right * 4, PixelFormat32bppPARGB, (BYTE*)g_gfx.bits);
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        if (f && f->valid) {
            if (cfg.fov_circle) {
                float cx = f->sw * 0.5f, cy = f->sh * 0.5f;
                g.FillEllipse(g_gfx.br_fov, cx - cfg.fov, cy - cfg.fov, cfg.fov * 2, cfg.fov * 2);
                g.DrawEllipse(g_gfx.pen_fov, cx - cfg.fov, cy - cfg.fov, cfg.fov * 2, cfg.fov * 2);
            }
            if (cfg.esp) {
                for (int i = 0; i < f->ent_count; ++i) {
                    const auto& e = f->ents[i];
                    if (e.health <= 0) continue;
                    bool is_team = (e.id == f->local_id);
                    if (is_team && cfg.esp_team) continue;
                    if ((e.pos - f->cam.pos).len() > cfg.max_dist) continue;

                    SolidBrush* brush = is_team ? g_gfx.br_team : g_gfx.br_enemy;
                    Pen* box_pen = is_team ? g_gfx.pen_box_t : g_gfx.pen_box_e;
                    Pen* bone_pen = is_team ? g_gfx.pen_bone_t : g_gfx.pen_bone_e;

                    Vec3 head = { e.pos.x, e.pos.y + 1.6f, e.pos.z }, feet = e.pos;
                    float hx, hy, fx, fy;
                    if (!world_to_screen(head, hx, hy, f->cam, f->sw, f->sh)) continue;
                    if (!world_to_screen(feet, fx, fy, f->cam, f->sw, f->sh)) continue;
                    float h = fy - hy; if (h < 4.f || h > f->sh) continue;
                    float w = h * 0.45f, x0 = fx - w * 0.5f;

                    if (cfg.esp_box) g.DrawRectangle(box_pen, x0, hy, w, h);
                    if (cfg.esp_hp) {
                        float hp_frac = std::clamp(e.health / 100.f, 0.f, 1.f);
                        g.FillRectangle(g_gfx.br_shadow, x0 - 5.f, hy, 3.f, h);
                        g.FillRectangle(brush, x0 - 5.f, hy + h * (1.f - hp_frac), 3.f, h * hp_frac);
                    }
                    if (cfg.esp_name && !e.name.empty()) {
                        g.DrawString(e.name.c_str(), -1, g_gfx.font, PointF(x0, hy - 15), g_gfx.br_shadow);
                        g.DrawString(e.name.c_str(), -1, g_gfx.font, PointF(x0 - 1, hy - 16), brush);
                    }
                    if (cfg.esp_dist) {
                        wchar_t buf[32]; float d = (e.pos - f->cam.pos).len(); swprintf_s(buf, L"[%.0fm]", d);
                        g.DrawString(buf, -1, g_gfx.font, PointF(x0, fy + 2), g_gfx.br_shadow);
                        g.DrawString(buf, -1, g_gfx.font, PointF(x0 - 1, fy + 1), brush);
                    }
                    if (cfg.esp_snap) g.DrawLine(g_gfx.pen_snap, f->sw * 0.5f, (float)f->sh, fx, fy);
                    if (cfg.esp_head_dot) g.FillEllipse(brush, hx - 2.f, hy - 2.f, 4.f, 4.f);

                    if (cfg.esp_skeleton && e.skeleton) {
                        static int cache_id = -1;
                        if (cache_id != e.id) { build_bone_cache(e.skeleton); cache_id = e.id; }
                        if (g_bone_name_to_idx.count(L"pelvis") && g_bone_name_to_idx.count(L"head")) {
                            uintptr_t data = SafeRead<uintptr_t>(e.skeleton + O.sk_bones_data);
                            uintptr_t hier = SafeRead<uintptr_t>(data + O.bone_hierarchy);
                            Vec3 pelvis = read_bone_world(hier, g_bone_name_to_idx[L"pelvis"]);
                            Vec3 head_b = read_bone_world(hier, g_bone_name_to_idx[L"head"]);
                            if (!pelvis.zero() && !head_b.zero()) {
                                draw_bone(g, bone_pen, pelvis, head_b, f->cam, f->sw, f->sh);
                                if (g_bone_name_to_idx.count(L"l_foot")) draw_bone(g, bone_pen, pelvis, read_bone_world(hier, g_bone_name_to_idx[L"l_foot"]), f->cam, f->sw, f->sh);
                                if (g_bone_name_to_idx.count(L"r_foot")) draw_bone(g, bone_pen, pelvis, read_bone_world(hier, g_bone_name_to_idx[L"r_foot"]), f->cam, f->sw, f->sh);
                            }
                        }
                    }
                }
            }
        }
    }
    POINT src{ 0, 0 }; SIZE sz{ r.right, r.bottom };
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_ovl, g_gfx.screen, &tl, &sz, g_gfx.mem, &src, 0, &bf, ULW_ALPHA);
}

// ============================================================
// 8. Aimbot
// ============================================================
static void aim_tick() {
    std::shared_ptr<const Frame> f;
    { std::lock_guard<std::mutex> lk(g_frame_mtx); f = g_frame; }
    if (!f || !f->valid || f->ent_count == 0) return;

    Config cfg = cfg_snapshot();
    if (!cfg.aim && !cfg.trigger) return;
    if (!(GetAsyncKeyState(cfg.key_aim) & 0x8000)) return;

    float cx = f->sw * 0.5f, cy = f->sh * 0.5f;
    float best_dist = cfg.fov, tx = 0, ty = 0;
    bool has = false;

    for (int i = 0; i < f->ent_count; ++i) {
        const auto& e = f->ents[i];
        if (e.health <= 0 || e.id == f->local_id) continue;
        if ((e.pos - f->cam.pos).len() > cfg.max_dist) continue;

        Vec3 aim_pos = { e.pos.x, e.pos.y + 1.6f, e.pos.z };
        if (cfg.predict) {
            float dist = (aim_pos - f->cam.pos).len();
            float time = dist / 300.f;
            aim_pos = aim_pos + (e.vel * time);
        }
        float sx, sy;
        if (!world_to_screen(aim_pos, sx, sy, f->cam, f->sw, f->sh)) continue;
        float dx = sx - cx, dy = sy - cy, d = sqrtf(dx*dx + dy*dy);
        if (d < best_dist) { best_dist = d; tx = sx; ty = sy; has = true; }
    }
    if (!has) return;

    float mx = (tx - cx) * (cfg.smooth / 100.f), my = (ty - cy) * (cfg.smooth / 100.f);
    if (fabsf(mx) < 0.5f && fabsf(my) < 0.5f) return;

    INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_MOVE;
    in.mi.dx = (LONG)mx; in.mi.dy = (LONG)my;
    SendInput(1, &in, sizeof(INPUT));

    if (cfg.recoil && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        INPUT in2{}; in2.type = INPUT_MOUSE; in2.mi.dwFlags = MOUSEEVENTF_MOVE; in2.mi.dy = 2;
        SendInput(1, &in2, sizeof(INPUT));
    }
    if (cfg.trigger && best_dist < 5.0f) {
        INPUT in2[2]{};
        in2[0].type = INPUT_MOUSE; in2[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        in2[1].type = INPUT_MOUSE; in2[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, in2, sizeof(INPUT));
        Sleep(10);
    }
}

// ============================================================
// 9. GUI
// ============================================================
enum { ID_TAB = 1000, ID_ESP, ID_SKEL, ID_BOX, ID_HP, ID_NAME, ID_DIST, ID_SNAP, ID_TEAM, ID_DOT,
       ID_AIM, ID_TRIG, ID_FOV, ID_SMOOTH, ID_MAX_DIST_SL, ID_PREDICT, ID_RECOIL,
       ID_STREAM, ID_KEY_MENU, ID_KEY_ESP, ID_KEY_AIM, ID_KEY_PANIC, ID_SAVE, ID_LOAD, ID_LANG, ID_FOV_CIRCLE,
       ID_LBL_FOV, ID_LBL_SMOOTH, ID_LBL_MAX_DIST, ID_LBL_LANG };

static std::vector<HWND> g_page_esp, g_page_aim, g_page_misc;
static int g_capture_key = 0;
static HWND g_btn_key[4] = {};
static HWND g_tab = nullptr;

static void save_cfg_locked() {
    std::ofstream f("roburun_dll.ini"); if (!f) return;
    f << "esp=" << g_cfg.esp << "\nbox=" << g_cfg.esp_box << "\nskel=" << g_cfg.esp_skeleton << "\n";
    f << "hp=" << g_cfg.esp_hp << "\nname=" << g_cfg.esp_name << "\ndist=" << g_cfg.esp_dist << "\n";
    f << "snap=" << g_cfg.esp_snap << "\nteam=" << g_cfg.esp_team << "\ndot=" << g_cfg.esp_head_dot << "\n";
    f << "aim=" << g_cfg.aim << "\ntrigger=" << g_cfg.trigger << "\nfov_circle=" << g_cfg.fov_circle << "\n";
    f << "predict=" << g_cfg.predict << "\nrecoil=" << g_cfg.recoil << "\n";
    f << "fov=" << g_cfg.fov << "\nsmooth=" << g_cfg.smooth << "\nmax_dist=" << g_cfg.max_dist << "\n";
    f << "stream=" << g_cfg.stream_proof << "\n";
    f << "key_menu=" << g_cfg.key_menu << "\nkey_esp=" << g_cfg.key_esp << "\nkey_aim=" << g_cfg.key_aim << "\nkey_panic=" << g_cfg.key_panic << "\n";
    f << "lang=" << (int)g_cfg.lang << "\n";
}
static void save_cfg() { std::lock_guard<std::mutex> lk(g_cfg_mtx); save_cfg_locked(); }

static void load_cfg_locked() {
    std::ifstream f("roburun_dll.ini"); if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('='); if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        int v = 0;
        try { v = std::stoi(line.substr(eq + 1)); } catch (...) { continue; }
        if (k == "esp") g_cfg.esp = v; else if (k == "box") g_cfg.esp_box = v;
        else if (k == "skel") g_cfg.esp_skeleton = v; else if (k == "hp") g_cfg.esp_hp = v;
        else if (k == "name") g_cfg.esp_name = v; else if (k == "dist") g_cfg.esp_dist = v;
        else if (k == "snap") g_cfg.esp_snap = v; else if (k == "team") g_cfg.esp_team = v;
        else if (k == "dot") g_cfg.esp_head_dot = v; else if (k == "aim") g_cfg.aim = v;
        else if (k == "trigger") g_cfg.trigger = v; else if (k == "fov_circle") g_cfg.fov_circle = v;
        else if (k == "predict") g_cfg.predict = v; else if (k == "recoil") g_cfg.recoil = v;
        else if (k == "fov") g_cfg.fov = (float)v; else if (k == "smooth") g_cfg.smooth = (float)v;
        else if (k == "max_dist") g_cfg.max_dist = (float)v; else if (k == "stream") g_cfg.stream_proof = v;
        else if (k == "key_menu") g_cfg.key_menu = v; else if (k == "key_esp") g_cfg.key_esp = v;
        else if (k == "key_aim") g_cfg.key_aim = v; else if (k == "key_panic") g_cfg.key_panic = v;
        else if (k == "lang") g_cfg.lang = (Lang)v;
    }
    g_lang = g_cfg.lang;
}
static void load_cfg() { std::lock_guard<std::mutex> lk(g_cfg_mtx); load_cfg_locked(); }

static void update_ui_text() {
    SetWindowTextW(g_menu, T(STR_TITLE));
    TCITEM tie{}; tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)T(STR_TAB_ESP); TabCtrl_SetItem(g_tab, 0, &tie);
    tie.pszText = (LPWSTR)T(STR_TAB_AIM); TabCtrl_SetItem(g_tab, 1, &tie);
    tie.pszText = (LPWSTR)T(STR_TAB_MISC); TabCtrl_SetItem(g_tab, 2, &tie);

    SetWindowTextW(GetDlgItem(g_menu, ID_BOX), T(STR_BOX));
    SetWindowTextW(GetDlgItem(g_menu, ID_SKEL), T(STR_SKEL));
    SetWindowTextW(GetDlgItem(g_menu, ID_HP), T(STR_HP));
    SetWindowTextW(GetDlgItem(g_menu, ID_NAME), T(STR_NAME));
    SetWindowTextW(GetDlgItem(g_menu, ID_DIST), T(STR_DIST));
    SetWindowTextW(GetDlgItem(g_menu, ID_SNAP), T(STR_SNAP));
    SetWindowTextW(GetDlgItem(g_menu, ID_TEAM), T(STR_TEAM));
    SetWindowTextW(GetDlgItem(g_menu, ID_DOT), T(STR_HEAD_DOT));
    SetWindowTextW(GetDlgItem(g_menu, ID_SAVE), T(STR_SAVE));
    SetWindowTextW(GetDlgItem(g_menu, ID_AIM), T(STR_AIM_ON));
    SetWindowTextW(GetDlgItem(g_menu, ID_TRIG), T(STR_TRIGGER));
    SetWindowTextW(GetDlgItem(g_menu, ID_PREDICT), T(STR_PREDICT));
    SetWindowTextW(GetDlgItem(g_menu, ID_RECOIL), T(STR_RECOIL));
    SetWindowTextW(GetDlgItem(g_menu, ID_LBL_FOV), T(STR_FOV));
    SetWindowTextW(GetDlgItem(g_menu, ID_LBL_SMOOTH), T(STR_SMOOTH));
    SetWindowTextW(GetDlgItem(g_menu, ID_LBL_MAX_DIST), T(STR_MAX_DIST));
    SetWindowTextW(GetDlgItem(g_menu, ID_LOAD), T(STR_LOAD));
    SetWindowTextW(GetDlgItem(g_menu, ID_STREAM), T(STR_STREAM));
    SetWindowTextW(GetDlgItem(g_menu, ID_FOV_CIRCLE), T(STR_FOV_CIRCLE));
    SetWindowTextW(GetDlgItem(g_menu, ID_LBL_LANG), T(STR_LANG_LABEL));

    if (g_btn_key[0]) SetWindowTextW(g_btn_key[0], (std::wstring(T(STR_KEY_MENU)) + L": " + get_key_name(g_cfg.key_menu)).c_str());
    if (g_btn_key[1]) SetWindowTextW(g_btn_key[1], (std::wstring(T(STR_KEY_ESP)) + L": " + get_key_name(g_cfg.key_esp)).c_str());
    if (g_btn_key[2]) SetWindowTextW(g_btn_key[2], (std::wstring(T(STR_KEY_AIM)) + L": " + get_key_name(g_cfg.key_aim)).c_str());
    if (g_btn_key[3]) SetWindowTextW(g_btn_key[3], (std::wstring(T(STR_KEY_PANIC)) + L": " + get_key_name(g_cfg.key_panic)).c_str());

    HWND cb = GetDlgItem(g_menu, ID_LANG);
    SendMessageW(cb, CB_DELETESTRING, 0, 0); SendMessageW(cb, CB_DELETESTRING, 0, 0);
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)T(STR_LANG_JA));
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)T(STR_LANG_EN));
    SendMessageW(cb, CB_SETCURSEL, g_lang == LANG_JA ? 0 : 1, 0);
}

static void switch_tab(int idx) {
    for (auto h : g_page_esp) ShowWindow(h, idx == 0 ? SW_SHOW : SW_HIDE);
    for (auto h : g_page_aim) ShowWindow(h, idx == 1 ? SW_SHOW : SW_HIDE);
    for (auto h : g_page_misc) ShowWindow(h, idx == 2 ? SW_SHOW : SW_HIDE);
}

static LRESULT CALLBACK menu_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NOTIFY) {
        LPNMHDR nm = (LPNMHDR)l;
        if (nm->code == TCN_SELCHANGE && nm->idFrom == ID_TAB) switch_tab(TabCtrl_GetCurSel(g_tab));
    } else if (m == WM_COMMAND) {
        int id = LOWORD(w);
        if (HIWORD(w) == BN_CLICKED) {
            if (id == ID_SAVE) { save_cfg(); return 0; }
            if (id == ID_LOAD) { load_cfg(); update_ui_text(); return 0; }
            if (id == ID_KEY_MENU) { g_capture_key = 1; SetWindowTextW(g_btn_key[0], T(STR_PRESS_KEY)); SetFocus(h); return 0; }
            if (id == ID_KEY_ESP) { g_capture_key = 2; SetWindowTextW(g_btn_key[1], T(STR_PRESS_KEY)); SetFocus(h); return 0; }
            if (id == ID_KEY_AIM) { g_capture_key = 3; SetWindowTextW(g_btn_key[2], T(STR_PRESS_KEY)); SetFocus(h); return 0; }
            if (id == ID_KEY_PANIC) { g_capture_key = 4; SetWindowTextW(g_btn_key[3], T(STR_PRESS_KEY)); SetFocus(h); return 0; }
            {
                std::lock_guard<std::mutex> lk(g_cfg_mtx);
                auto on = [&](int cid) { return SendMessageW(GetDlgItem(h, cid), BM_GETCHECK, 0, 0) == BST_CHECKED; };
                if (id == ID_ESP) g_cfg.esp = on(ID_ESP);
                else if (id == ID_BOX) g_cfg.esp_box = on(ID_BOX);
                else if (id == ID_SKEL) g_cfg.esp_skeleton = on(ID_SKEL);
                else if (id == ID_HP) g_cfg.esp_hp = on(ID_HP);
                else if (id == ID_NAME) g_cfg.esp_name = on(ID_NAME);
                else if (id == ID_DIST) g_cfg.esp_dist = on(ID_DIST);
                else if (id == ID_SNAP) g_cfg.esp_snap = on(ID_SNAP);
                else if (id == ID_TEAM) g_cfg.esp_team = on(ID_TEAM);
                else if (id == ID_DOT) g_cfg.esp_head_dot = on(ID_DOT);
                else if (id == ID_AIM) g_cfg.aim = on(ID_AIM);
                else if (id == ID_TRIG) g_cfg.trigger = on(ID_TRIG);
                else if (id == ID_FOV_CIRCLE) g_cfg.fov_circle = on(ID_FOV_CIRCLE);
                else if (id == ID_PREDICT) g_cfg.predict = on(ID_PREDICT);
                else if (id == ID_RECOIL) g_cfg.recoil = on(ID_RECOIL);
                else if (id == ID_STREAM) { g_cfg.stream_proof = on(ID_STREAM); }
            }
            if (id == ID_STREAM) {
                Config c = cfg_snapshot();
                SetWindowDisplayAffinity(g_ovl, c.stream_proof ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
            }
        }
        if (id == ID_LANG && HIWORD(w) == CBN_SELCHANGE) {
            {
                std::lock_guard<std::mutex> lk(g_cfg_mtx);
                g_cfg.lang = (SendMessageW(GetDlgItem(h, ID_LANG), CB_GETCURSEL, 0, 0) == 0) ? LANG_JA : LANG_EN;
                g_lang = g_cfg.lang;
            }
            update_ui_text();
            save_cfg();
        }
    } else if (m == WM_HSCROLL) {
        HWND c = (HWND)l;
        if (!c) return 0;
        int v = (int)SendMessageW(c, TBM_GETPOS, 0, 0);
        int cid = GetDlgCtrlID(c);
        std::lock_guard<std::mutex> lk(g_cfg_mtx);
        if (cid == ID_FOV) g_cfg.fov = (float)v;
        else if (cid == ID_SMOOTH) g_cfg.smooth = (float)v;
        else if (cid == ID_MAX_DIST_SL) g_cfg.max_dist = (float)v;
    } else if (m == WM_KEYDOWN || m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_MBUTTONDOWN) {
        if (g_capture_key) {
            int vk = (m == WM_KEYDOWN) ? (int)w : (m == WM_LBUTTONDOWN) ? VK_LBUTTON : (m == WM_RBUTTONDOWN) ? VK_RBUTTON : VK_MBUTTON;
            {
                std::lock_guard<std::mutex> lk(g_cfg_mtx);
                if (g_capture_key == 1) g_cfg.key_menu = vk;
                else if (g_capture_key == 2) g_cfg.key_esp = vk;
                else if (g_capture_key == 3) g_cfg.key_aim = vk;
                else if (g_capture_key == 4) g_cfg.key_panic = vk;
                g_capture_key = 0;
            }
            update_ui_text();
            save_cfg();
        }
    } else if (m == WM_CLOSE) {
        ShowWindow(h, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void create_gui(HINSTANCE hi) {
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = menu_proc; wc.hInstance = hi;
    wc.lpszClassName = L"robrun_menu_dll"; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);
    g_menu = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        100, 100, 380, 580, nullptr, nullptr, hi, nullptr);

    g_tab = CreateWindowExW(0, WC_TABCONTROL, L"", WS_CHILD | WS_VISIBLE | TCS_FOCUSNEVER,
        10, 10, 340, 520, g_menu, (HMENU)ID_TAB, hi, nullptr);
    TCITEM tie{}; tie.mask = TCIF_TEXT; tie.pszText = (LPWSTR)L" ";
    TabCtrl_InsertItem(g_tab, 0, &tie);
    TabCtrl_InsertItem(g_tab, 1, &tie);
    TabCtrl_InsertItem(g_tab, 2, &tie);

    auto add_check = [&](std::vector<HWND>& pg, const wchar_t* t, int id, bool v, int x, int y) {
        HWND h = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | BS_AUTOCHECKBOX, x, y, 300, 20, g_menu, (HMENU)(INT_PTR)id, nullptr, nullptr);
        SendMessageW(h, BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0); pg.push_back(h);
    };
    auto add_label = [&](std::vector<HWND>& pg, const wchar_t* t, int id, int x, int y) {
        HWND h = CreateWindowExW(0, L"STATIC", t, WS_CHILD, x, y, 300, 20, g_menu, (HMENU)(INT_PTR)id, nullptr, nullptr);
        pg.push_back(h);
    };
    auto add_slider = [&](std::vector<HWND>& pg, int id, int lo, int hi, int pos, int x, int y) {
        HWND h = CreateWindowExW(0, L"msctls_trackbar32", nullptr, WS_CHILD | TBS_HORZ, x, y, 300, 25, g_menu, (HMENU)(INT_PTR)id, nullptr, nullptr);
        SendMessageW(h, TBM_SETRANGE, TRUE, MAKELONG(lo, hi)); SendMessageW(h, TBM_SETPOS, TRUE, pos); pg.push_back(h);
    };
    auto add_btn = [&](std::vector<HWND>& pg, const wchar_t* t, int id, int x, int y, HWND* out = nullptr) {
        HWND h = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | BS_PUSHBUTTON, x, y, 300, 28, g_menu, (HMENU)(INT_PTR)id, nullptr, nullptr);
        pg.push_back(h); if (out) *out = h;
    };

    int y = 50;
    add_check(g_page_esp, L"ESP", ID_ESP, g_cfg.esp, 20, y); y += 25;
    add_check(g_page_esp, L"Box", ID_BOX, g_cfg.esp_box, 20, y); y += 25;
    add_check(g_page_esp, L"Skeleton", ID_SKEL, g_cfg.esp_skeleton, 20, y); y += 25;
    add_check(g_page_esp, L"HP", ID_HP, g_cfg.esp_hp, 20, y); y += 25;
    add_check(g_page_esp, L"Name", ID_NAME, g_cfg.esp_name, 20, y); y += 25;
    add_check(g_page_esp, L"Dist", ID_DIST, g_cfg.esp_dist, 20, y); y += 25;
    add_check(g_page_esp, L"Snap", ID_SNAP, g_cfg.esp_snap, 20, y); y += 25;
    add_check(g_page_esp, L"Team", ID_TEAM, g_cfg.esp_team, 20, y); y += 25;
    add_check(g_page_esp, L"Dot", ID_DOT, g_cfg.esp_head_dot, 20, y); y += 35;
    add_btn(g_page_esp, L"Save", ID_SAVE, 20, y);

    y = 50;
    add_check(g_page_aim, L"Aim", ID_AIM, g_cfg.aim, 20, y); y += 25;
    add_check(g_page_aim, L"Trigger", ID_TRIG, g_cfg.trigger, 20, y); y += 25;
    add_check(g_page_aim, L"FOV Circle", ID_FOV_CIRCLE, g_cfg.fov_circle, 20, y); y += 25;
    add_check(g_page_aim, L"Prediction", ID_PREDICT, g_cfg.predict, 20, y); y += 25;
    add_check(g_page_aim, L"Recoil Control", ID_RECOIL, g_cfg.recoil, 20, y); y += 35;
    add_label(g_page_aim, L"FOV", ID_LBL_FOV, 20, y); y += 20;
    add_slider(g_page_aim, ID_FOV, 50, 800, (int)g_cfg.fov, 20, y); y += 35;
    add_label(g_page_aim, L"Smooth", ID_LBL_SMOOTH, 20, y); y += 20;
    add_slider(g_page_aim, ID_SMOOTH, 1, 100, (int)g_cfg.smooth, 20, y); y += 35;
    add_label(g_page_aim, L"Max Dist", ID_LBL_MAX_DIST, 20, y); y += 20;
    add_slider(g_page_aim, ID_MAX_DIST_SL, 50, 2000, (int)g_cfg.max_dist, 20, y); y += 35;
    add_btn(g_page_aim, L"Load", ID_LOAD, 20, y);

    y = 50;
    add_check(g_page_misc, L"Stream Proof", ID_STREAM, g_cfg.stream_proof, 20, y); y += 35;
    add_btn(g_page_misc, L"Menu Key", ID_KEY_MENU, 20, y, &g_btn_key[0]); y += 35;
    add_btn(g_page_misc, L"ESP Key", ID_KEY_ESP, 20, y, &g_btn_key[1]); y += 35;
    add_btn(g_page_misc, L"Aim Key", ID_KEY_AIM, 20, y, &g_btn_key[2]); y += 35;
    add_btn(g_page_misc, L"Panic Key", ID_KEY_PANIC, 20, y, &g_btn_key[3]); y += 35;

    add_label(g_page_misc, L"Language", ID_LBL_LANG, 20, y); y += 20;
    HWND cb = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 20, y, 300, 100, g_menu, (HMENU)ID_LANG, nullptr, nullptr);
    g_page_misc.push_back(cb);

    update_ui_text();
    switch_tab(0);
}

// ============================================================
// 10. Main Thread & DllMain
// ============================================================
static DWORD WINAPI MainThread(LPVOID hMod) {
    HINSTANCE hi = (HINSTANCE)hMod;
    g_base = (uintptr_t)GetModuleHandleW(L"GameAssembly.dll");
    if (!g_base) return 0;

    GdiplusStartupInput gsi;
    if (GdiplusStartup(&g_gdip, &gsi, nullptr) != Ok) return 0;
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_TAB_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    struct EnumCtx { DWORD pid; HWND out; };
    EnumCtx ctx{ GetCurrentProcessId(), nullptr };
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        EnumCtx* c = (EnumCtx*)lParam;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == c->pid && IsWindowVisible(hwnd) && !(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD)) {
            c->out = hwnd;
            return FALSE;
        }
        return TRUE;
    }, (LPARAM)&ctx);
    g_game = ctx.out;

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = hi; wc.lpszClassName = L"robrun_ovl_dll";
    RegisterClassExW(&wc);
    g_ovl = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, hi, nullptr);
    ShowWindow(g_ovl, SW_SHOW);

    if (!g_gfx.init()) return 0;
    load_cfg();
    create_gui(hi);
    if (cfg_snapshot().stream_proof) SetWindowDisplayAffinity(g_ovl, WDA_EXCLUDEFROMCAPTURE);

    std::thread rt([]{
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        while (g_run) {
            if (!O.static_unity_entry) {
                O.static_unity_entry = auto_scan();
                if (!O.static_unity_entry) std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }
            auto f = std::make_shared<Frame>();
            read_frame(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), *f);
            { std::lock_guard<std::mutex> lk(g_frame_mtx); g_frame = std::move(f); }
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    });

    bool key_menu = false, key_esp = false, key_panic = false;
    while (g_run) {
        if (GetAsyncKeyState(VK_END) & 0x8000) break;

        Config cfg = cfg_snapshot();

        bool nm = (GetAsyncKeyState(cfg.key_menu) & 0x8000) != 0;
        if (nm && !key_menu) {
            ShowWindow(g_menu, IsWindowVisible(g_menu) ? SW_HIDE : SW_SHOW);
            if (IsWindowVisible(g_menu)) SetForegroundWindow(g_menu);
        }
        key_menu = nm;

        bool ne = (GetAsyncKeyState(cfg.key_esp) & 0x8000) != 0;
        if (ne && !key_esp) { std::lock_guard<std::mutex> lk(g_cfg_mtx); g_cfg.esp = !g_cfg.esp; }
        key_esp = ne;

        bool np = (GetAsyncKeyState(cfg.key_panic) & 0x8000) != 0;
        if (np && !key_panic) {
            std::lock_guard<std::mutex> lk(g_cfg_mtx);
            g_cfg.esp = false; g_cfg.aim = false; g_cfg.trigger = false;
        }
        key_panic = np;

        if (!g_game || !IsWindow(g_game)) {
            EnumCtx ctx2{ GetCurrentProcessId(), nullptr };
            EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                EnumCtx* c = (EnumCtx*)lParam;
                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);
                if (pid == c->pid && IsWindowVisible(hwnd) && !(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD)) {
                    c->out = hwnd;
                    return FALSE;
                }
                return TRUE;
            }, (LPARAM)&ctx2);
            g_game = ctx2.out;
        }

        if (g_game && IsWindow(g_game) && !IsIconic(g_game)) { render(); aim_tick(); }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        Sleep(1);
    }

    g_run = false;
    rt.join();
    if (g_menu) DestroyWindow(g_menu);
    if (g_ovl) DestroyWindow(g_ovl);
    g_gfx.shutdown();
    GdiplusShutdown(g_gdip);

    // BUG FIX: FreeLibraryAndExitThread を削除。DETACH と競合しないよう自然終了させる
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        HANDLE h = CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
        if (!h) return FALSE;
        CloseHandle(h);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_run = false;
    }
    return TRUE;
}