// -*- coding: utf-8 -*-
// ============================================================================
// 枪法训练场 —— 练定位 / 练瞄准 / 练跟枪   （C++17 + SDL2 + SDL2_ttf）
// 由 sb.py（Python + pygame）一比一移植：逻辑、数值、界面布局、文案、存档格式均保持一致。
//
// 编译（源文件必须是 UTF-8）：
//   Linux / macOS : g++ -O2 -std=c++17 sb.cpp -o sb `sdl2-config --cflags --libs` -lSDL2_ttf
//   Windows MinGW : g++ -O2 -std=c++17 -finput-charset=UTF-8 sb.cpp -o sb.exe -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf
//   Windows MSVC  : cl /O2 /std:c++17 /utf-8 sb.cpp SDL2.lib SDL2main.lib SDL2_ttf.lib
//
// 操作：电脑：鼠标控制视角，左键射击，WASD 移动，ESC 暂停 / 返回，F11 全屏
//       手机：左下角移动轮盘，右下角开火键，手指拖动屏幕转视角
// 中文字体：把任意 CJK .ttf/.otf/.ttc 放在程序同目录即可（否则界面自动用英文）。
// ============================================================================
#ifdef _WIN32
#define NOMINMAX
#endif
#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

using std::string;
using std::vector;
namespace fs = std::filesystem;

// ============================ 小工具（对应 Python 内建函数） ============================
static const double PI = 3.14159265358979323846;

static inline int I(double v) {                       // Python int()：向 0 截断（并防溢出）
    if (!(v == v)) return 0;
    if (v > 1e9) return 1000000000;
    if (v < -1e9) return -1000000000;
    return (int)v;
}
static inline double radians(double d) { return d * PI / 180.0; }
static inline double degrees(double r) { return r * 180.0 / PI; }
static inline double clampd(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }
static inline int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }
static inline double pymod(double a, double m) {      // Python 的 % ：结果符号跟除数
    double r = std::fmod(a, m);
    if (r < 0) r += m;
    return r;
}
static inline int pyceil(double v) { return (int)std::ceil(v); }

static std::mt19937_64 g_rng{std::random_device{}()};
static double uniform(double a, double b) {           // random.uniform
    return a + (b - a) * std::uniform_real_distribution<double>(0.0, 1.0)(g_rng);
}
static int rnd_sign() { return (g_rng() & 1) ? 1 : -1; }   // random.choice((-1, 1))

static string vfmt(const char* f, ...) {              // Python 的 "%d %s" % (...)
    va_list ap;
    va_start(ap, f);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(nullptr, 0, f, ap);
    va_end(ap);
    string out((size_t)(n > 0 ? n : 0), '\0');
    if (n > 0) vsnprintf(&out[0], (size_t)n + 1, f, ap2);
    va_end(ap2);
    return out;
}

static vector<string> utf8_chars(const string& s) {   // Python list(text)：按字符拆分
    vector<string> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        size_t n = c < 0x80 ? 1 : ((c >> 5) == 6 ? 2 : ((c >> 4) == 14 ? 3 : ((c >> 3) == 30 ? 4 : 1)));
        out.push_back(s.substr(i, n));
        i += n;
    }
    return out;
}

static string lower_ascii(string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s;
}
static bool ends_with_any(const string& s, const vector<string>& exts) {
    string l = lower_ascii(s);
    for (auto& e : exts)
        if (l.size() >= e.size() && l.compare(l.size() - e.size(), e.size(), e) == 0) return true;
    return false;
}

// ============================ 颜色 / 矩形 ============================
struct Color {
    int r = 0, g = 0, b = 0, a = 255;
};
static inline Color rgb(int r, int g, int b) { return Color{r, g, b, 255}; }
static inline Color rgba(int r, int g, int b, int a) { return Color{r, g, b, a}; }
static inline bool same_rgb(const Color& p, const Color& q) { return p.r == q.r && p.g == q.g && p.b == q.b; }
static inline bool color_less(const Color& p, const Color& q) {
    return std::tie(p.r, p.g, p.b) < std::tie(q.r, q.g, q.b);
}
static Color lerp_color(const Color& a, const Color& b, double t) {
    return rgb(I(a.r + (b.r - a.r) * t), I(a.g + (b.g - a.g) * t), I(a.b + (b.b - a.b) * t));
}

struct Rect {                                          // pygame.Rect
    int x = 0, y = 0, w = 0, h = 0;
    Rect() = default;
    Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    int centerx() const { return x + w / 2; }
    int centery() const { return y + h / 2; }
    void set_center(int cx, int cy) { x = cx - w / 2; y = cy - h / 2; }
    void set_midtop(int cx, int ty) { x = cx - w / 2; y = ty; }
    void set_midright(int rx, int cy) { x = rx - w; y = cy - h / 2; }
    void set_bottomright(int rx, int by) { x = rx - w; y = by - h; }
    bool collidepoint(double px, double py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    Rect inflate(int dx, int dy) const { return Rect(x - dx / 2, y - dy / 2, w + dx, h + dy); }
    Rect move(int dx, int dy) const { return Rect(x + dx, y + dy, w, h); }
    Rect clip(const Rect& o) const {
        int nx = std::max(x, o.x), ny = std::max(y, o.y);
        int nr = std::min(right(), o.right()), nb = std::min(bottom(), o.bottom());
        if (nr <= nx || nb <= ny) return Rect(x, y, 0, 0);
        return Rect(nx, ny, nr - nx, nb - ny);
    }
    bool nonzero() const { return w != 0 && h != 0; }   // bool(Rect)
};

// ============================ 软件 Surface（对应 pygame.Surface） ============================
// 像素格式固定为 0xAARRGGBB。alpha=true 相当于 SRCALPHA：pygame.draw 直接写入 RGBA（不混合），
// blit 到别的 Surface 时才做 alpha 混合 —— 与 pygame 行为一致。
static inline uint32_t pack(int r, int g, int b, int a) {
    return ((uint32_t)(a & 255) << 24) | ((uint32_t)(r & 255) << 16) | ((uint32_t)(g & 255) << 8) | (uint32_t)(b & 255);
}

struct Surface {
    int w = 0, h = 0, pitch = 0;                       // pitch 以像素计
    uint32_t* px = nullptr;
    vector<uint32_t> store;
    bool alpha = false;

    Surface() = default;
    Surface(int w_, int h_, bool alpha_ = false)
        : w(std::max(0, w_)), h(std::max(0, h_)), pitch(std::max(0, w_)),
          store((size_t)std::max(0, w_) * (size_t)std::max(0, h_), alpha_ ? 0u : 0xFF000000u), alpha(alpha_) {
        px = store.data();
    }
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&&) = default;
    Surface& operator=(Surface&&) = default;

    Surface copy() const {                              // Surface.copy()
        Surface o(w, h, alpha);
        for (int y = 0; y < h; y++) std::memcpy(o.px + (size_t)y * o.pitch, px + (size_t)y * pitch, (size_t)w * 4);
        return o;
    }
    Surface sub(const Rect& r) {                        // Surface.subsurface()：共享像素的视图
        Rect c = r.clip(Rect(0, 0, w, h));
        Surface v;
        v.w = c.w; v.h = c.h; v.pitch = pitch; v.alpha = alpha;
        v.px = px + (size_t)c.y * pitch + c.x;
        return v;
    }
};

static inline uint32_t pk(const Surface& s, const Color& c) { return pack(c.r, c.g, c.b, s.alpha ? c.a : 255); }
static inline void put(Surface& s, int x, int y, uint32_t c) {
    if (x >= 0 && y >= 0 && x < s.w && y < s.h) s.px[(size_t)y * s.pitch + x] = c;
}
static inline void hspan(Surface& s, int y, int x0, int x1, uint32_t c) {   // 含两端
    if (y < 0 || y >= s.h || x0 > x1) return;
    x0 = std::max(x0, 0);
    x1 = std::min(x1, s.w - 1);
    if (x0 > x1) return;
    uint32_t* p = s.px + (size_t)y * s.pitch;
    for (int x = x0; x <= x1; x++) p[x] = c;
}
static void fill(Surface& s, const Color& col) {
    uint32_t c = pk(s, col);
    for (int y = 0; y < s.h; y++) hspan(s, y, 0, s.w - 1, c);
}

// ---- pygame.draw.line ----
static void line_raw(Surface& s, int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        put(s, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static bool clip_line(double& x0, double& y0, double& x1, double& y1, double xmin, double ymin, double xmax, double ymax) {
    double dx = x1 - x0, dy = y1 - y0, t0 = 0.0, t1 = 1.0;
    auto clip = [&](double p, double q) {
        if (p == 0) return q >= 0;
        double r = q / p;
        if (p < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
        else { if (r < t0) return false; if (r < t1) t1 = r; }
        return true;
    };
    if (clip(-dx, x0 - xmin) && clip(dx, xmax - x0) && clip(-dy, y0 - ymin) && clip(dy, ymax - y0)) {
        x1 = x0 + t1 * dx; y1 = y0 + t1 * dy;
        x0 = x0 + t0 * dx; y0 = y0 + t0 * dy;
        return true;
    }
    return false;
}
static void draw_line(Surface& s, const Color& col, double ax, double ay, double bx, double by, int width = 1) {
    if (width < 1) return;
    uint32_t c = pk(s, col);
    int x0 = I(ax), y0 = I(ay), x1 = I(bx), y1 = I(by);
    auto one = [&](int ox0, int oy0, int ox1, int oy1) {
        bool inside = ox0 >= 0 && oy0 >= 0 && ox1 >= 0 && oy1 >= 0 && ox0 < s.w && ox1 < s.w && oy0 < s.h && oy1 < s.h;
        if (!inside) {
            double a0 = ox0, b0 = oy0, a1 = ox1, b1 = oy1;
            if (!clip_line(a0, b0, a1, b1, 0, 0, s.w - 1, s.h - 1)) return;
            ox0 = (int)std::lround(a0); oy0 = (int)std::lround(b0);
            ox1 = (int)std::lround(a1); oy1 = (int)std::lround(b1);
        }
        line_raw(s, ox0, oy0, ox1, oy1, c);
    };
    if (width == 1) { one(x0, y0, x1, y1); return; }
    bool horiz = std::abs(x1 - x0) > std::abs(y1 - y0);
    int start = -(width - 1) / 2, end = start + width - 1;
    for (int k = start; k <= end; k++) {
        if (horiz) one(x0, y0 + k, x1, y1 + k);
        else one(x0 + k, y0, x1 + k, y1);
    }
}

// ---- pygame.draw.rect（支持 width / border_radius） ----
static int rr_inset(int i, int rad) {                   // 圆角矩形第 i 行（0 为最外一行）左右各缩进多少像素
    if (i >= rad) return 0;
    double dy = rad - (i + 0.5);
    double dx = std::sqrt(std::max(0.0, (double)rad * rad - dy * dy));
    return std::max(0, (int)std::ceil(rad - dx - 0.5));
}
static void rrect_span(int x, int w, int h, int rad, int row, int& x0, int& x1) {   // [x0, x1)
    int ins = 0;
    if (rad > 0) {
        if (row < rad) ins = rr_inset(row, rad);
        else if (row >= h - rad) ins = rr_inset(h - 1 - row, rad);
    }
    x0 = x + ins;
    x1 = x + w - ins;
}
static void draw_rect(Surface& s, const Color& col, const Rect& r, int width = 0, int radius = 0) {
    if (r.w <= 0 || r.h <= 0 || width < 0) return;
    uint32_t c = pk(s, col);
    int rad = std::max(0, std::min(radius, std::min(r.w, r.h) / 2));
    bool outline = width > 0 && width * 2 < std::min(r.w, r.h);
    int iw = r.w - 2 * width, ih = r.h - 2 * width, irad = std::max(0, rad - width);
    for (int row = 0; row < r.h; row++) {
        int y = r.y + row;
        if (y < 0 || y >= s.h) continue;
        int ox0, ox1;
        rrect_span(r.x, r.w, r.h, rad, row, ox0, ox1);
        if (!outline) { hspan(s, y, ox0, ox1 - 1, c); continue; }
        int irow = row - width;
        if (irow < 0 || irow >= ih) {
            hspan(s, y, ox0, ox1 - 1, c);
        } else {
            int ix0, ix1;
            rrect_span(r.x + width, iw, ih, irad, irow, ix0, ix1);
            hspan(s, y, ox0, ix0 - 1, c);
            hspan(s, y, ix1, ox1 - 1, c);
        }
    }
}

// ---- pygame.draw.circle（width=0 实心；width>0 圆环） ----
static void draw_circle(Surface& s, const Color& col, double cxf, double cyf, double rf, int width = 0) {
    int cx = I(cxf), cy = I(cyf), r = I(rf);
    if (r < 1 || width < 0) return;
    uint32_t c = pk(s, col);
    bool ring = width > 0 && width < r;
    long long to = (long long)r * r + r;
    int ri = r - width;
    long long ti = (long long)ri * ri + ri;
    int ylo = std::max(-r, -cy), yhi = std::min(r, s.h - 1 - cy);
    for (int dy = ylo; dy <= yhi; dy++) {
        int y = cy + dy;
        long long rem = to - (long long)dy * dy;
        if (rem < 0) continue;
        int dxo = (int)std::floor(std::sqrt((double)rem));
        if (ring) {
            long long remi = ti - (long long)dy * dy;
            if (remi >= 0) {
                int dxi = (int)std::floor(std::sqrt((double)remi));
                hspan(s, y, cx - dxo, cx - dxi - 1, c);
                hspan(s, y, cx + dxi + 1, cx + dxo, c);
                continue;
            }
        }
        hspan(s, y, cx - dxo, cx + dxo, c);
    }
}

// ---- pygame.draw.polygon ----
typedef vector<std::pair<double, double>> Pts;
static void draw_polygon(Surface& s, const Color& col, const Pts& pts, int width = 0) {
    int n = (int)pts.size();
    if (n < 3) return;
    vector<int> X(n), Y(n);
    for (int i = 0; i < n; i++) { X[i] = I(pts[i].first); Y[i] = I(pts[i].second); }
    if (width > 0) {
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            draw_line(s, col, X[i], Y[i], X[j], Y[j], width);
        }
        return;
    }
    uint32_t c = pk(s, col);
    int ymin = *std::min_element(Y.begin(), Y.end()), ymax = *std::max_element(Y.begin(), Y.end());
    ymin = std::max(ymin, 0);
    ymax = std::min(ymax, s.h - 1);
    vector<int> xs;
    for (int y = ymin; y <= ymax; y++) {
        xs.clear();
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n, y0 = Y[i], y1 = Y[j];
            if (y0 == y1) continue;
            if ((y >= y0 && y < y1) || (y >= y1 && y < y0)) {
                double t = (double)(y - y0) / (double)(y1 - y0);
                xs.push_back((int)std::floor(X[i] + t * (X[j] - X[i]) + 0.5));
            }
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2) hspan(s, y, xs[k], xs[k + 1], c);
    }
    for (int i = 0; i < n; i++) {                       // 补边，避免缺口
        int j = (i + 1) % n;
        draw_line(s, col, X[i], Y[i], X[j], Y[j], 1);
    }
}

// ---- Surface.blit（带 alpha 混合，公式与 pygame 一致） ----
static inline uint32_t blend_over(uint32_t d, uint32_t sp, bool dst_alpha) {
    int sA = (int)(sp >> 24);
    if (sA == 0) return d;
    int sR = (int)((sp >> 16) & 255), sG = (int)((sp >> 8) & 255), sB = (int)(sp & 255);
    if (sA == 255) return pack(sR, sG, sB, 255);
    int dR = (int)((d >> 16) & 255), dG = (int)((d >> 8) & 255), dB = (int)(d & 255), dA = (int)(d >> 24);
    if (dst_alpha && dA == 0) return sp;
    dR += (((sR - dR) * sA + sR) >> 8);
    dG += (((sG - dG) * sA + sG) >> 8);
    dB += (((sB - dB) * sA + sB) >> 8);
    int nA = dst_alpha ? (sA + dA - (sA * dA) / 255) : 255;
    return pack(dR, dG, dB, nA);
}
static void blit(Surface& dst, const Surface& src, int dx, int dy) {
    int x0 = std::max(0, dx), y0 = std::max(0, dy);
    int x1 = std::min(dst.w, dx + src.w), y1 = std::min(dst.h, dy + src.h);
    if (x0 >= x1 || y0 >= y1) return;
    for (int y = y0; y < y1; y++) {
        const uint32_t* sp = src.px + (size_t)(y - dy) * src.pitch + (x0 - dx);
        uint32_t* dp = dst.px + (size_t)y * dst.pitch + x0;
        if (!src.alpha) {
            std::memcpy(dp, sp, (size_t)(x1 - x0) * 4);
        } else {
            for (int x = 0; x < x1 - x0; x++) dp[x] = blend_over(dp[x], sp[x], dst.alpha);
        }
    }
}
static inline void blit(Surface& dst, const Surface& src, const Rect& r) { blit(dst, src, r.x, r.y); }

// 整屏压暗：相当于 Surface(set_alpha(a)) 填黑后 blit 到目标上
static void dim_surface(Surface& s, int a) {
    for (int y = 0; y < s.h; y++) {
        uint32_t* p = s.px + (size_t)y * s.pitch;
        for (int x = 0; x < s.w; x++) {
            uint32_t d = p[x];
            int dR = (int)((d >> 16) & 255), dG = (int)((d >> 8) & 255), dB = (int)(d & 255);
            dR += ((0 - dR) * a) >> 8;
            dG += ((0 - dG) * a) >> 8;
            dB += ((0 - dB) * a) >> 8;
            p[x] = pack(dR, dG, dB, 255);
        }
    }
}

// pygame.transform.smoothscale（双线性）
static Surface smoothscale(const Surface& src, int nw, int nh) {
    Surface o(nw, nh, src.alpha);
    if (src.w <= 0 || src.h <= 0 || nw <= 0 || nh <= 0) return o;
    for (int y = 0; y < nh; y++) {
        double fy = (y + 0.5) * src.h / nh - 0.5;
        int y0 = clampi((int)std::floor(fy), 0, src.h - 1), y1 = clampi(y0 + 1, 0, src.h - 1);
        double ty = clampd(fy - std::floor(fy), 0.0, 1.0);
        for (int x = 0; x < nw; x++) {
            double fx = (x + 0.5) * src.w / nw - 0.5;
            int x0 = clampi((int)std::floor(fx), 0, src.w - 1), x1 = clampi(x0 + 1, 0, src.w - 1);
            double tx = clampd(fx - std::floor(fx), 0.0, 1.0);
            uint32_t p00 = src.px[(size_t)y0 * src.pitch + x0], p10 = src.px[(size_t)y0 * src.pitch + x1];
            uint32_t p01 = src.px[(size_t)y1 * src.pitch + x0], p11 = src.px[(size_t)y1 * src.pitch + x1];
            int ch[4];
            for (int k = 0; k < 4; k++) {
                int sh = 8 * k;
                double a = ((p00 >> sh) & 255) * (1 - tx) + ((p10 >> sh) & 255) * tx;
                double b = ((p01 >> sh) & 255) * (1 - tx) + ((p11 >> sh) & 255) * tx;
                ch[k] = I(a * (1 - ty) + b * ty + 0.5);
            }
            o.px[(size_t)y * o.pitch + x] = pack(ch[2], ch[1], ch[0], ch[3]);
        }
    }
    return o;
}


static const char* TITLE = "枪法训练场";
static string SAVE_FILE = "aim_trainer_settings.json";   // main() 里改成「程序所在目录/aim_trainer_settings.json」
static string HERE = "./";                                // 程序所在目录（对应 Python 的脚本目录）

// ============================ 默认设置 ============================
// 键位布局：名称 -> [x, y, 大小]。按键的 x、y 是屏幕比例位置（0~1），大小是倍数；
// 步枪的 x、y 是相对默认位置的偏移比例，大小是倍数。
typedef std::array<double, 3> Vec3;
static const vector<string> LAYOUT_ORDER = {"fire", "pause", "stick", "rifle"};
static std::map<string, Vec3> make_default_layout() {
    std::map<string, Vec3> m;
    m["fire"] = {0.88, 0.66, 1.0};
    m["pause"] = {0.955, 0.10, 1.0};
    m["stick"] = {0.13, 0.70, 1.0};
    m["rifle"] = {0.0, 0.0, 1.0};
    return m;
}
static const std::map<string, Vec3> DEFAULT_LAYOUT = make_default_layout();
static const std::map<string, string> LAYOUT_NAMES = {
    {"fire", "开火键"}, {"pause", "暂停键"}, {"stick", "移动轮盘"}, {"rifle", "步枪"}};
static const std::map<string, int> BTN_BASE_R = {{"fire", 66}, {"pause", 26}, {"stick", 84}};  // 720p 下像素

static const vector<string> MODE_ORDER = {"flick", "move", "multi", "precision", "track"};

struct Config {
    double sens = 3.0;
    int duration = 60;
    bool hold_track = true, show_hud = true, show_fps = true, particles = true;
    int fps = 144, fov = 90;
    bool fullscreen = false;
    int win_idx = 1, theme = 0;
    bool grid = true;
    double ball_size = 1.0;
    std::array<int, 3> ball_color = {255, 70, 70};
    bool ball_random = false;
    bool ch_on = true;
    int ch_style = 0;
    std::array<int, 3> ch_color = {0, 255, 120};
    int ch_size = 10, ch_thick = 2, ch_gap = 4, ch_alpha = 100;
    bool ch_outline = true;
    bool show_bar = true, show_fire = true, show_rifle = true;
    double touch_sens = 1.0, touch_ratio = 1.0;
    bool move_on = true;
    double move_speed = 1.0;
    std::map<string, Vec3> layout = DEFAULT_LAYOUT;
    bool sound_on = true;
    int volume = 60;
    string lang = "auto";
    int diff = 1;
    std::map<string, int> best;
    Config() {
        for (auto& m : MODE_ORDER)
            for (int d = 0; d < 3; d++) best[vfmt("%s_%d", m.c_str(), d)] = 0;
    }
};

static const int FPS_OPTIONS[] = {30, 60, 90, 120, 144, 165, 240, 0};
static const int N_FPS_OPTIONS = 8;
static const int DURATIONS[] = {30, 60, 90, 120, 180};
static const int N_DURATIONS = 5;
static const vector<string> CH_STYLES = {"十字", "十字+圆点", "T 形", "圆点", "圆圈", "圆圈+圆点"};

struct Theme { const char* name; Color bg, grid, front; };   // 名称, 背景色, 网格色, 正前方墙网格色
static const vector<Theme> THEMES = {
    {"深邃蓝", rgb(14, 18, 32), rgb(40, 54, 88), rgb(62, 86, 140)},
    {"纯黑", rgb(5, 5, 7), rgb(46, 46, 54), rgb(80, 80, 92)},
    {"灰度", rgb(38, 40, 44), rgb(84, 88, 96), rgb(120, 126, 136)},
    {"暗绿", rgb(10, 24, 20), rgb(36, 84, 70), rgb(56, 128, 108)},
};
static const vector<Color> PALETTE = {rgb(255, 70, 70), rgb(255, 160, 40), rgb(255, 230, 60), rgb(80, 230, 110),
                                      rgb(60, 230, 230), rgb(70, 140, 255), rgb(180, 100, 255), rgb(255, 110, 200),
                                      rgb(255, 255, 255)};
struct ModeInfo { const char* key; const char* name; const char* desc; };
static const vector<ModeInfo> MODES = {
    {"flick", "定位练习", "大范围随机出现目标，练习快速甩枪与准心落点。"},
    {"move", "移动目标定位", "目标左右闪避横移，练习预判提前量与移动中定位。"},
    {"multi", "三球定位", "同时三个小球缓慢漂移，击中后立刻补一个，练习连续切换目标。"},
    {"precision", "瞄准练习", "小目标密集出现，练习微调与点射精度。"},
    {"track", "跟枪练习", "目标持续移动，按住左键让准心始终咬住目标。"},
};
static const ModeInfo& mode_info(const string& m) {
    for (auto& mi : MODES) if (m == mi.key) return mi;
    return MODES[0];
}
// 难度：名称, 小球大小倍数, 速度倍数, 得分倍数
struct Diff { const char* name; double size, speed, score; };
static const Diff DIFFS[3] = {{"简单", 1.35, 0.7, 0.6}, {"正常", 1.0, 1.0, 1.0}, {"困难", 0.7, 1.4, 1.5}};

static string bkey(const string& mode, const Config& cfg) {   // 最高分按「模式 + 难度」分别记录
    return vfmt("%s_%d", mode.c_str(), clampi(cfg.diff, 0, 2));
}
static const char* hint_of(const string& mode) {
    if (mode == "flick") return "点击鼠标左键击中小球";
    if (mode == "move") return "预判移动方向，击中横移的小球";
    if (mode == "multi") return "同时三个小球在漂移，逐个击中";
    if (mode == "precision") return "点击鼠标左键击中小球";
    return "按住鼠标左键，让准心停在移动的小球上";
}

// UI 颜色
static const Color PANEL = rgb(26, 31, 48);
static const Color BTN = rgb(42, 49, 72);
static const Color BTN_H = rgb(62, 72, 104);
static const Color ACCENT = rgb(0, 190, 255);
static const Color ACCENT_H = rgb(80, 215, 255);
static const Color TXT = rgb(235, 240, 250);
static const Color SUB = rgb(150, 162, 186);
static const Color YELLOW = rgb(255, 215, 80);
static const Color WHITE = rgb(255, 255, 255);

static double SCALE = 1.0;  // 界面缩放（随窗口高度变化）
static int sc(double v) { return I(v * SCALE); }

template <class C>
static int idx_of(const C& lst, int v, int def = 0) {
    for (size_t i = 0; i < lst.size(); i++) if ((int)lst[i] == v) return (int)i;
    return def;
}
static int idx_of_arr(const int* a, int n, int v, int def = 0) {
    for (int i = 0; i < n; i++) if (a[i] == v) return i;
    return def;
}

// ============================ 极简 JSON（读写设置文件用） ============================
struct JV {
    enum Type { NUL, BOOL, INT, FLT, STR, ARR, OBJ } t = NUL;
    bool b = false;
    long long i = 0;
    double d = 0;
    string s;
    vector<JV> a;
    vector<std::pair<string, JV>> o;
    bool is_num() const { return t == INT || t == FLT; }       // 不含 bool
    double num() const { return t == INT ? (double)i : d; }
    const JV* get(const string& k) const {
        const JV* r = nullptr;
        for (auto& kv : o) if (kv.first == k) r = &kv.second;
        return r;
    }
};

struct JParser {
    const string& s;
    size_t p = 0;
    bool ok = true;
    explicit JParser(const string& str) : s(str) {}
    void ws() { while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r')) p++; }
    bool eat(char c) { ws(); if (p < s.size() && s[p] == c) { p++; return true; } return false; }
    static void put_utf8(string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    unsigned hex4() {
        if (p + 4 > s.size()) { ok = false; return 0; }
        unsigned v = 0;
        for (int k = 0; k < 4; k++) {
            char c = s[p++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else ok = false;
        }
        return v;
    }
    string str() {
        string out;
        if (!eat('"')) { ok = false; return out; }
        while (p < s.size() && s[p] != '"') {
            char c = s[p++];
            if (c != '\\') { out += c; continue; }
            if (p >= s.size()) { ok = false; break; }
            char e = s[p++];
            switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp < 0xDC00 && p + 1 < s.size() && s[p] == '\\' && s[p + 1] == 'u') {
                        p += 2;
                        unsigned lo = hex4();
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    put_utf8(out, cp);
                    break;
                }
                default: out += e;
            }
        }
        if (p >= s.size()) ok = false; else p++;
        return out;
    }
    JV value() {
        JV v;
        ws();
        if (p >= s.size()) { ok = false; return v; }
        char c = s[p];
        if (c == '{') {
            p++;
            v.t = JV::OBJ;
            if (eat('}')) return v;
            for (;;) {
                ws();
                string k = str();
                if (!ok || !eat(':')) { ok = false; return v; }
                JV x = value();
                if (!ok) return v;
                v.o.emplace_back(k, std::move(x));
                if (eat(',')) continue;
                if (eat('}')) break;
                ok = false;
                return v;
            }
        } else if (c == '[') {
            p++;
            v.t = JV::ARR;
            if (eat(']')) return v;
            for (;;) {
                JV x = value();
                if (!ok) return v;
                v.a.push_back(std::move(x));
                if (eat(',')) continue;
                if (eat(']')) break;
                ok = false;
                return v;
            }
        } else if (c == '"') {
            v.t = JV::STR;
            v.s = str();
        } else if (s.compare(p, 4, "true") == 0) { p += 4; v.t = JV::BOOL; v.b = true; }
        else if (s.compare(p, 5, "false") == 0) { p += 5; v.t = JV::BOOL; v.b = false; }
        else if (s.compare(p, 4, "null") == 0) { p += 4; v.t = JV::NUL; }
        else {
            size_t st = p;
            bool isf = false;
            if (p < s.size() && (s[p] == '-' || s[p] == '+')) p++;
            while (p < s.size() && (std::isdigit((unsigned char)s[p]) || s[p] == '.' || s[p] == 'e' || s[p] == 'E' ||
                                    s[p] == '-' || s[p] == '+')) {
                if (s[p] == '.' || s[p] == 'e' || s[p] == 'E') isf = true;
                p++;
            }
            if (p == st) { ok = false; return v; }
            string tok = s.substr(st, p - st);
            if (isf) { v.t = JV::FLT; v.d = std::strtod(tok.c_str(), nullptr); }
            else { v.t = JV::INT; v.i = std::strtoll(tok.c_str(), nullptr, 10); }
        }
        return v;
    }
    JV parse() {
        JV v = value();
        ws();
        if (p != s.size()) ok = false;
        return v;
    }
};

// Python float repr
static string pyfloat(double v) {
    if (v != v) return "NaN";
    if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
    if (v == 0) return std::signbit(v) ? "-0.0" : "0.0";
    char buf[64];
    int p = 1;
    for (; p <= 17; p++) {
        snprintf(buf, sizeof buf, "%.*e", p - 1, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    string b = buf;
    bool neg = b[0] == '-';
    if (neg) b = b.substr(1);
    size_t ep = b.find('e');
    string mant = b.substr(0, ep);
    int ex = std::atoi(b.c_str() + ep + 1);
    string digits;
    for (char ch : mant) if (ch != '.') digits += ch;
    string out;
    if (ex >= -4 && ex < 16) {
        if (ex >= 0) {
            if ((int)digits.size() <= ex + 1) out = digits + string((size_t)(ex + 1 - (int)digits.size()), '0') + ".0";
            else out = digits.substr(0, (size_t)ex + 1) + "." + digits.substr((size_t)ex + 1);
        } else out = "0." + string((size_t)(-ex - 1), '0') + digits;
    } else {
        out = digits.substr(0, 1);
        if (digits.size() > 1) out += "." + digits.substr(1);
        char eb[16];
        snprintf(eb, sizeof eb, "e%c%02d", ex < 0 ? '-' : '+', std::abs(ex));
        out += eb;
    }
    return (neg ? "-" : "") + out;
}
static string json_str(const string& s) {
    string o = "\"";
    for (unsigned char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) o += vfmt("\\u%04x", c);
        else o += (char)c;
    }
    return o + "\"";
}

// ============================ 读 / 写设置 ============================
static Config load_config() {
    Config cfg;
    try {
        std::ifstream f(SAVE_FILE, std::ios::binary);
        if (!f) return cfg;
        std::stringstream ss;
        ss << f.rdbuf();
        string txt = ss.str();
        JParser jp(txt);
        JV root = jp.parse();
        if (!jp.ok || root.t != JV::OBJ) return cfg;

        auto set_bool = [](bool& d, const JV& v) { if (v.t == JV::BOOL) d = v.b; };
        auto set_int = [](int& d, const JV& v) { if (v.is_num()) d = (int)v.num(); };
        auto set_dbl = [](double& d, const JV& v) { if (v.is_num()) d = v.num(); };
        auto set_col = [](std::array<int, 3>& d, const JV& v) {
            if (v.t == JV::ARR && v.a.size() == 3 && v.a[0].is_num() && v.a[1].is_num() && v.a[2].is_num())
                for (int k = 0; k < 3; k++) d[(size_t)k] = (int)v.a[(size_t)k].num();
        };
        for (auto& kv : root.o) {
            const string& k = kv.first;
            const JV& v = kv.second;
            if (k == "best") {
                if (v.t == JV::OBJ)
                    for (auto& m : cfg.best) {
                        const JV* x = v.get(m.first);
                        if (x && (x->is_num() || x->t == JV::BOOL)) m.second = x->t == JV::BOOL ? (x->b ? 1 : 0) : (int)x->num();
                    }
            } else if (k == "layout") {
                if (v.t == JV::OBJ)
                    for (auto& nm : LAYOUT_ORDER) {
                        const JV* q = v.get(nm);
                        if (q && q->t == JV::ARR && q->a.size() == 3 && q->a[0].is_num() && q->a[1].is_num() && q->a[2].is_num())
                            cfg.layout[nm] = {q->a[0].num(), q->a[1].num(), q->a[2].num()};
                    }
            }
            else if (k == "sens") set_dbl(cfg.sens, v);
            else if (k == "duration") set_int(cfg.duration, v);
            else if (k == "hold_track") set_bool(cfg.hold_track, v);
            else if (k == "show_hud") set_bool(cfg.show_hud, v);
            else if (k == "show_fps") set_bool(cfg.show_fps, v);
            else if (k == "particles") set_bool(cfg.particles, v);
            else if (k == "fps") set_int(cfg.fps, v);
            else if (k == "fov") set_int(cfg.fov, v);
            else if (k == "fullscreen") set_bool(cfg.fullscreen, v);
            else if (k == "win_idx") set_int(cfg.win_idx, v);
            else if (k == "theme") set_int(cfg.theme, v);
            else if (k == "grid") set_bool(cfg.grid, v);
            else if (k == "ball_size") set_dbl(cfg.ball_size, v);
            else if (k == "ball_color") set_col(cfg.ball_color, v);
            else if (k == "ball_random") set_bool(cfg.ball_random, v);
            else if (k == "ch_on") set_bool(cfg.ch_on, v);
            else if (k == "ch_style") set_int(cfg.ch_style, v);
            else if (k == "ch_color") set_col(cfg.ch_color, v);
            else if (k == "ch_size") set_int(cfg.ch_size, v);
            else if (k == "ch_thick") set_int(cfg.ch_thick, v);
            else if (k == "ch_gap") set_int(cfg.ch_gap, v);
            else if (k == "ch_alpha") set_int(cfg.ch_alpha, v);
            else if (k == "ch_outline") set_bool(cfg.ch_outline, v);
            else if (k == "show_bar") set_bool(cfg.show_bar, v);
            else if (k == "show_fire") set_bool(cfg.show_fire, v);
            else if (k == "show_rifle") set_bool(cfg.show_rifle, v);
            else if (k == "touch_sens") set_dbl(cfg.touch_sens, v);
            else if (k == "touch_ratio") set_dbl(cfg.touch_ratio, v);
            else if (k == "move_on") set_bool(cfg.move_on, v);
            else if (k == "move_speed") set_dbl(cfg.move_speed, v);
            else if (k == "sound_on") set_bool(cfg.sound_on, v);
            else if (k == "volume") set_int(cfg.volume, v);
            else if (k == "lang") { if (v.t == JV::STR) cfg.lang = v.s; }
            else if (k == "diff") set_int(cfg.diff, v);
        }
    } catch (...) {
    }
    return cfg;
}

static void save_config(const Config& c) {
    try {
        vector<string> m;
        auto B = [](bool v) { return string(v ? "true" : "false"); };
        auto I3 = [](const std::array<int, 3>& a) {
            return vfmt("[\n    %d,\n    %d,\n    %d\n  ]", a[0], a[1], a[2]);
        };
        auto kv = [&](const char* k, const string& v) { m.push_back(vfmt("  \"%s\": ", k) + v); };
        kv("sens", pyfloat(c.sens));
        kv("duration", std::to_string(c.duration));
        kv("hold_track", B(c.hold_track));
        kv("show_hud", B(c.show_hud));
        kv("show_fps", B(c.show_fps));
        kv("particles", B(c.particles));
        kv("fps", std::to_string(c.fps));
        kv("fov", std::to_string(c.fov));
        kv("fullscreen", B(c.fullscreen));
        kv("win_idx", std::to_string(c.win_idx));
        kv("theme", std::to_string(c.theme));
        kv("grid", B(c.grid));
        kv("ball_size", pyfloat(c.ball_size));
        kv("ball_color", I3(c.ball_color));
        kv("ball_random", B(c.ball_random));
        kv("ch_on", B(c.ch_on));
        kv("ch_style", std::to_string(c.ch_style));
        kv("ch_color", I3(c.ch_color));
        kv("ch_size", std::to_string(c.ch_size));
        kv("ch_thick", std::to_string(c.ch_thick));
        kv("ch_gap", std::to_string(c.ch_gap));
        kv("ch_alpha", std::to_string(c.ch_alpha));
        kv("ch_outline", B(c.ch_outline));
        kv("show_bar", B(c.show_bar));
        kv("show_fire", B(c.show_fire));
        kv("show_rifle", B(c.show_rifle));
        kv("touch_sens", pyfloat(c.touch_sens));
        kv("touch_ratio", pyfloat(c.touch_ratio));
        kv("move_on", B(c.move_on));
        kv("move_speed", pyfloat(c.move_speed));
        {
            string lay = "{\n";
            for (size_t i = 0; i < LAYOUT_ORDER.size(); i++) {
                const Vec3& q = c.layout.at(LAYOUT_ORDER[i]);
                lay += vfmt("    \"%s\": [\n      %s,\n      %s,\n      %s\n    ]", LAYOUT_ORDER[i].c_str(),
                            pyfloat(q[0]).c_str(), pyfloat(q[1]).c_str(), pyfloat(q[2]).c_str());
                lay += i + 1 < LAYOUT_ORDER.size() ? ",\n" : "\n";
            }
            lay += "  }";
            kv("layout", lay);
        }
        kv("sound_on", B(c.sound_on));
        kv("volume", std::to_string(c.volume));
        kv("lang", json_str(c.lang));
        kv("diff", std::to_string(c.diff));
        {
            string b = "{\n";
            vector<string> keys;
            for (auto& mo : MODE_ORDER) for (int d = 0; d < 3; d++) keys.push_back(vfmt("%s_%d", mo.c_str(), d));
            for (size_t i = 0; i < keys.size(); i++) {
                auto it = c.best.find(keys[i]);
                b += vfmt("    \"%s\": %d", keys[i].c_str(), it == c.best.end() ? 0 : it->second);
                b += i + 1 < keys.size() ? ",\n" : "\n";
            }
            b += "  }";
            kv("best", b);
        }
        string out = "{\n";
        for (size_t i = 0; i < m.size(); i++) out += m[i] + (i + 1 < m.size() ? ",\n" : "\n");
        out += "}";
        std::ofstream f(SAVE_FILE, std::ios::binary | std::ios::trunc);
        if (f) f << out;
    } catch (...) {
    }
}


// ============================ 多语言 ============================
// (简体, 繁體, English)  —— 代码里的中文原文就是 key
struct TrRow { const char* zh; const char* tw; const char* en; };
static const TrRow TABLE[] = {
    {"黄明康", "黃明康", "Huang Mingkang"},
    {"枪法训练场", "槍法訓練場", "Aim Trainer"},
    {"定位 · 瞄准 · 跟枪", "定位 · 瞄準 · 跟槍", "Flicking · Precision · Tracking"},
    {"开始", "開始", "Start"},
    {"设置", "設定", "Settings"},
    {"开发人员", "開發人員", "Developer"},
    {"退出", "退出", "Quit"},
    {"选择训练模式", "選擇訓練模式", "Select Mode"},
    {"返回", "返回", "Back"},
    {"定位练习", "定位練習", "Positioning Drill"},
    {"瞄准练习", "瞄準練習", "Precision Drill"},
    {"跟枪练习", "跟槍練習", "Tracking Drill"},
    {"大范围随机出现目标，练习快速甩枪与准心落点。", "大範圍隨機出現目標，練習快速甩槍與準心落點。", "Targets appear across a wide area. Train fast flicks and crosshair placement."},
    {"小目标密集出现，练习微调与点射精度。", "小目標密集出現，練習微調與點射精度。", "Small targets in a tight cluster. Train fine adjustments and click accuracy."},
    {"目标持续移动，按住左键让准心始终咬住目标。", "目標持續移動，按住左鍵讓準心始終咬住目標。", "The target keeps moving. Hold left click and keep your crosshair on it."},
    {"点击鼠标左键击中小球", "點擊滑鼠左鍵擊中小球", "Click the left mouse button to hit the balls"},
    {"按住鼠标左键，让准心停在移动的小球上", "按住滑鼠左鍵，讓準心停在移動的小球上", "Hold left click and keep the crosshair on the ball"},
    {"最高分：%d", "最高分：%d", "Best: %d"},
    {"已暂停", "已暫停", "Paused"},
    {"继续", "繼續", "Resume"},
    {"重新开始", "重新開始", "Restart"},
    {"返回主页", "返回主頁", "Main Menu"},
    {"训练结束", "訓練結束", "Finished"},
    {"得分", "得分", "Score"},
    {"新纪录！", "新紀錄！", "New record!"},
    {"追踪时长", "追蹤時長", "Time on target"},
    {"追踪率", "追蹤率", "Tracking"},
    {"训练时长", "訓練時長", "Duration"},
    {"命中数", "命中數", "Hits"},
    {"射击数", "射擊數", "Shots"},
    {"命中率", "命中率", "Accuracy"},
    {"平均击杀用时", "平均擊殺用時", "Avg. time per kill"},
    {"最高连击", "最高連擊", "Best combo"},
    {"%.1f 秒", "%.1f 秒", "%.1f s"},
    {"%.2f 秒", "%.2f 秒", "%.2f s"},
    {"%d 秒", "%d 秒", "%d s"},
    {"再来一次", "再來一次", "Play Again"},
    {"模式选择", "模式選擇", "Modes"},
    {"游戏名称：%s", "遊戲名稱：%s", "Game: %s"},
    {"版本：%s", "版本：%s", "Version: %s"},
    {"技术：%s", "技術：%s", "Built with: %s"},
    {"通用", "一般", "General"},
    {"画面", "畫面", "Display"},
    {"小球", "小球", "Ball"},
    {"准心", "準心", "Crosshair"},
    {"声音", "聲音", "Sound"},
    {"语言", "語言", "Language"},
    {"鼠标灵敏度", "滑鼠靈敏度", "Mouse sensitivity"},
    {"游戏时长", "遊戲時長", "Round length"},
    {"跟枪需按住左键", "跟槍需按住左鍵", "Hold click to track"},
    {"显示得分 / 命中率", "顯示得分 / 命中率", "Show score / accuracy"},
    {"显示 FPS", "顯示 FPS", "Show FPS"},
    {"命中粒子特效", "命中粒子特效", "Hit particles"},
    {"小提示", "小提示", "Tips"},
    {"· 灵敏度越高，视角转动越快", "· 靈敏度越高，視角轉動越快", "· Higher sensitivity turns faster"},
    {"· 新手建议先用较低灵敏度，", "· 新手建議先用較低靈敏度，", "· Beginners: start low and raise"},
    {"  准度稳定后再逐步提高", "  準度穩定後再逐步提高", "  it once your aim is steady"},
    {"· 跟枪时关闭「需按住左键」", "· 跟槍時關閉「需按住左鍵」", "· Turn off hold-to-track to"},
    {"  可以只练准心移动", "  可以只練準心移動", "  practice crosshair movement"},
    {"· 游戏中按 ESC 可暂停", "· 遊戲中按 ESC 可暫停", "· Press ESC in game to pause"},
    {"FPS 帧率上限", "FPS 幀率上限", "FPS limit"},
    {"不限制", "不限制", "Unlimited"},
    {"视野 FOV", "視野 FOV", "Field of view"},
    {"全屏（F11）", "全螢幕（F11）", "Fullscreen (F11)"},
    {"窗口大小", "視窗大小", "Window size"},
    {"场景主题", "場景主題", "Theme"},
    {"显示场景网格", "顯示場景網格", "Show grid"},
    {"· FPS 越高，画面越流畅", "· FPS 越高，畫面越流暢", "· Higher FPS = smoother"},
    {"· FOV 越大，视野越广", "· FOV 越大，視野越廣", "· Larger FOV = wider view"},
    {"· 修改窗口大小后立即生效", "· 修改視窗大小後立即生效", "· Window size applies instantly"},
    {"小球大小", "小球大小", "Ball size"},
    {"随机颜色", "隨機顏色", "Random colors"},
    {"预设颜色", "預設顏色", "Presets"},
    {"红色 R", "紅色 R", "Red"},
    {"绿色 G", "綠色 G", "Green"},
    {"蓝色 B", "藍色 B", "Blue"},
    {"预览", "預覽", "Preview"},
    {"显示准心", "顯示準心", "Show crosshair"},
    {"准心样式", "準心樣式", "Style"},
    {"准心大小", "準心大小", "Size"},
    {"准心粗细", "準心粗細", "Thickness"},
    {"中心间隙", "中心間隙", "Gap"},
    {"不透明度", "不透明度", "Opacity"},
    {"黑色描边", "黑色描邊", "Outline"},
    {"准心已关闭", "準心已關閉", "Crosshair off"},
    {"十字", "十字", "Cross"},
    {"十字+圆点", "十字+圓點", "Cross + dot"},
    {"T 形", "T 形", "T-shape"},
    {"圆点", "圓點", "Dot"},
    {"圆圈", "圓圈", "Circle"},
    {"圆圈+圆点", "圓圈+圓點", "Circle + dot"},
    {"深邃蓝", "深邃藍", "Deep blue"},
    {"纯黑", "純黑", "Black"},
    {"灰度", "灰階", "Gray"},
    {"暗绿", "暗綠", "Dark green"},
    {"命中音效", "命中音效", "Hit sound"},
    {"音量", "音量", "Volume"},
    {"试听音效", "試聽音效", "Test sound"},
    {"未检测到可用的音频设备", "未偵測到可用的音訊裝置", "No audio device found"},
    {"恢复默认设置", "恢復預設設定", "Reset defaults"},
    {"保存并返回", "儲存並返回", "Save & Back"},
    {"得分 %d", "得分 %d", "Score %d"},
    {"追踪率 %.0f%%", "追蹤率 %.0f%%", "Tracking %.0f%%"},
    {"命中率 %.0f%%", "命中率 %.0f%%", "Accuracy %.0f%%"},
    {"连击 x%d", "連擊 x%d", "Combo x%d"},
    {"开", "開", "On"},
    {"关", "關", "Off"},
    {"界面", "介面", "HUD"},
    {"追踪", "追蹤", "Track"},
    {"时间", "時間", "Time"},
    {"开火", "開火", "FIRE"},
    {"显示抬头指示器", "顯示抬頭指示器", "Show HUD bar"},
    {"显示步枪模型", "顯示步槍模型", "Show rifle model"},
    {"步枪大小", "步槍大小", "Rifle size"},
    {"步枪已隐藏", "步槍已隱藏", "Rifle hidden"},
    {"· 抬头指示器：左边命中数，", "· 抬頭指示器：左邊命中數，", "· HUD: hits on the left,"},
    {"  右边剩余时间", "  右邊剩餘時間", "  time left on the right"},
    {"· 开火指示器：命中亮绿灯，", "· 開火指示器：命中亮綠燈，", "· Fire light: green = hit,"},
    {"  未命中亮红灯", "  未命中亮紅燈", "  red = miss"},
    {"开火键", "開火鍵", "Fire button"},
    {"暂停键", "暫停鍵", "Pause button"},
    {"步枪", "步槍", "Rifle"},
    {"大小", "大小", "Size"},
    {"重置此项", "重置此項", "Reset this"},
    {"全部重置", "全部重置", "Reset all"},
    {"完成", "完成", "Done"},
    {"拖动按键或步枪调整位置；选中后可调大小", "拖動按鍵或步槍調整位置；選中後可調大小", "Drag buttons or the rifle to move; select to resize"},
    {"显示开火键", "顯示開火鍵", "Show fire button"},
    {"步枪 左右位置", "步槍 左右位置", "Rifle X offset"},
    {"步枪 上下位置", "步槍 上下位置", "Rifle Y offset"},
    {"调整键位 / 位置与大小", "調整鍵位 / 位置與大小", "Edit layout (position & size)"},
    {"· 在「界面」里可调键位", "· 在「介面」裡可調鍵位", "· Edit layout under HUD tab"},
    {"· 手机上手指拖屏幕转视角", "· 手機上手指拖螢幕轉視角", "· On phones, drag to look"},
    {"· 手机上点开火键射击", "· 手機上點開火鍵射擊", "· On phones, tap FIRE to shoot"},
    {"移动目标定位", "移動目標定位", "Moving Targets"},
    {"目标左右闪避横移，练习预判提前量与移动中定位。", "目標左右閃避橫移，練習預判提前量與移動中定位。", "Targets strafe and dodge. Train lead prediction and aiming while moving."},
    {"预判移动方向，击中横移的小球", "預判移動方向，擊中橫移的小球", "Lead your shots and hit the strafing balls"},
    {"移动轮盘", "移動輪盤", "Move stick"},
    {"触屏", "觸屏", "Touch"},
    {"滑屏灵敏度", "滑屏靈敏度", "Swipe sensitivity"},
    {"滑屏垂直比例", "滑屏垂直比例", "Swipe vertical ratio"},
    {"启用移动（轮盘 / WASD）", "啟用移動（輪盤 / WASD）", "Enable movement (stick / WASD)"},
    {"移动速度", "移動速度", "Move speed"},
    {"· 滑屏灵敏度：手指拖屏转视角的快慢", "· 滑屏靈敏度：手指拖屏轉視角的快慢", "· Swipe sens: how fast a drag turns"},
    {"· 左下轮盘控制人物前后左右移动", "· 左下輪盤控制人物前後左右移動", "· Left stick moves your character"},
    {"· 电脑上用 WASD 移动", "· 電腦上用 WASD 移動", "· On PC use WASD to move"},
    {"· 轮盘位置 / 大小可在「界面」里调", "· 輪盤位置 / 大小可在「介面」裡調", "· Edit stick under HUD tab"},
    {"· 暂停菜单里也能实时调灵敏度", "· 暫停選單裡也能即時調靈敏度", "· Tweak sens live in the pause menu"},
    {"三球定位", "三球定位", "3-Ball Drill"},
    {"同时三个小球缓慢漂移，击中后立刻补一个，练习连续切换目标。", "同時三個小球緩慢漂移，擊中後立刻補一個，練習連續切換目標。", "Three balls drift around. Hit one and another appears. Train target switching."},
    {"同时三个小球在漂移，逐个击中", "同時三個小球在漂移，逐個擊中", "Three balls are drifting - hit them one by one"},
    {"简单", "簡單", "Easy"},
    {"正常", "正常", "Normal"},
    {"困难", "困難", "Hard"},
    {"难度", "難度", "Difficulty"},
};

struct TrMaps {
    std::unordered_map<string, string> tw, en;
    TrMaps() {
        for (auto& r : TABLE) { tw[r.zh] = r.tw; en[r.zh] = r.en; }
    }
};
static const TrMaps& tr_maps() {
    static TrMaps m;
    return m;
}
static const std::map<string, string> LANG_NAMES = {{"zh", "简体中文"}, {"tw", "繁體中文"}, {"en", "English"}};
static string LANG = "zh";
static bool CJK_OK = false;

static string T(const string& s) {
    if (LANG == "zh") return s;
    const auto& m = LANG == "tw" ? tr_maps().tw : tr_maps().en;
    auto it = m.find(s);
    return it == m.end() ? s : it->second;
}

static string current_lang(const Config& cfg) {          // 没有中文字体时一律用英文，避免出现方块
    if (!CJK_OK) return "en";
    const string& lang = cfg.lang;
    return (lang == "zh" || lang == "tw" || lang == "en") ? lang : "zh";
}

// ============================ 字体与文字 ============================
static string FONT_FILE;      // 空串 = Python 里的 None（没有可用的中文字体）
static string LATIN_FONT;     // 没有中文字体时用来显示英文的系统字体（pygame 自带的默认字体在 C++ 里没有）
static const vector<string> FONT_PATHS = {
    "C:/Windows/Fonts/msyh.ttc", "C:/Windows/Fonts/msyh.ttf", "C:/Windows/Fonts/msjh.ttc",
    "C:/Windows/Fonts/simhei.ttf", "C:/Windows/Fonts/simsun.ttc", "C:/Windows/Fonts/Deng.ttf",
    "/System/Library/Fonts/PingFang.ttc", "/System/Library/Fonts/STHeiti Medium.ttc",
    "/System/Library/Fonts/STHeiti Light.ttc", "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/Supplemental/Songti.ttc", "/Library/Fonts/Arial Unicode.ttf",
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    "/system/fonts/NotoSansCJK-Regular.ttc", "/system/fonts/DroidSansFallback.ttf",
};
static const vector<string> FONT_NAMES = {
    "microsoftyahei", "msyh", "simhei", "pingfangsc", "heitisc", "stheitimedium",
    "notosanscjksc", "notosanscjk", "notosanssc", "wenquanyimicrohei", "droidsansfallback",
    "arialunicodems", "simsun", "sourcehansanssc", "sarasa"};
static const vector<string> FONT_KEYS = {
    "yahei", "msyh", "msjh", "hei", "song", "kai", "ming", "fang", "cjk", "noto", "wqy",
    "wenquan", "droid", "sarasa", "unifont", "source", "han", "pingfang", "hiragino",
    "arialuni", "gothic", "jhenghei", "deng", "simsun", "cn", "sc", "tc", "hk", "gb"};
static const vector<string> LATIN_FONT_PATHS = {
    "C:/Windows/Fonts/arialbd.ttf", "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/segoeui.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/System/Library/Fonts/Helvetica.ttc", "/Library/Fonts/Arial.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
    "/system/fonts/Roboto-Regular.ttf", "/system/fonts/DroidSans.ttf",
};
static std::unordered_map<int, TTF_Font*> g_fonts;
static std::unordered_map<string, std::shared_ptr<Surface>> g_txt_cache;

// 把 SDL_ttf 渲染出的文字转成 Surface（带每像素 alpha，相当于 font.render(text, True, color)）
static Surface render_text(TTF_Font* font, const string& text, const Color& c) {
    int fh = font ? std::max(1, TTF_FontHeight(font)) : 1;
    if (!font || text.empty()) return Surface(1, fh, true);
    SDL_Color fg{(Uint8)c.r, (Uint8)c.g, (Uint8)c.b, 255};
    SDL_Surface* ts = TTF_RenderUTF8_Blended(font, text.c_str(), fg);
    if (!ts) return Surface(1, fh, true);
    SDL_Surface* cv = SDL_ConvertSurfaceFormat(ts, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(ts);
    if (!cv) return Surface(1, fh, true);
    Surface o(cv->w, cv->h, true);
    for (int y = 0; y < cv->h; y++)
        std::memcpy(o.px + (size_t)y * o.pitch, (const Uint8*)cv->pixels + (size_t)y * cv->pitch, (size_t)cv->w * 4);
    SDL_FreeSurface(cv);
    return o;
}

static bool font_has_cjk(const string& path) {          // 真正渲染几个汉字：缺字的字体会把它们都画成同一个方块
    TTF_Font* font = TTF_OpenFont(path.c_str(), 32);
    if (!font) return false;
    typedef std::pair<std::pair<int, int>, vector<uint32_t>> Sig;
    vector<Sig> sigs;
    const char* chars[] = {"汉", "字", "體", "枪"};
    for (const char* ch : chars) {
        Surface surf = render_text(font, ch, rgb(255, 255, 255));
        if (surf.w < 4) { TTF_CloseFont(font); return false; }
        Sig sg;
        sg.first = {surf.w, surf.h};
        sg.second.assign(surf.px, surf.px + (size_t)surf.w * surf.h);
        sigs.push_back(std::move(sg));
    }
    TTF_CloseFont(font);
    for (size_t i = 0; i < sigs.size(); i++)
        for (size_t j = i + 1; j < sigs.size(); j++)
            if (sigs[i] == sigs[j]) return false;
    return true;
}

static string env_or(const char* k, const char* d) {
    const char* v = std::getenv(k);
    return v && *v ? string(v) : string(d);
}
static vector<string> font_dirs() {
    string home = env_or("HOME", "");
    if (home.empty()) home = env_or("USERPROFILE", "");
    vector<string> dirs = {env_or("WINDIR", "C:/Windows") + "/Fonts", "/System/Library/Fonts", "/Library/Fonts",
                           home + "/Library/Fonts", "/usr/share/fonts", "/usr/local/share/fonts", home + "/.fonts",
                           home + "/.local/share/fonts", "/system/fonts"};
    if (!env_or("LOCALAPPDATA", "").empty()) dirs.push_back(env_or("LOCALAPPDATA", "") + "/Microsoft/Windows/Fonts");
    vector<string> out;
    for (auto& d : dirs) {
        std::error_code ec;
        if (fs::is_directory(d, ec)) out.push_back(d);
    }
    return out;
}
static const vector<string> FONT_EXTS = {".ttf", ".otf", ".ttc"};

// os.walk：按目录逐个收集字体文件，累计超过 3000 个就停
static void walk_fonts(const fs::path& root, vector<string>& files, bool& done) {
    if (done) return;
    std::error_code ec;
    vector<fs::path> subdirs;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end;
         it.increment(ec)) {
        std::error_code e2;
        if (it->is_symlink(e2)) {
            if (it->is_regular_file(e2) && ends_with_any(it->path().filename().string(), FONT_EXTS))
                files.push_back(it->path().string());
        } else if (it->is_directory(e2)) subdirs.push_back(it->path());
        else if (ends_with_any(it->path().filename().string(), FONT_EXTS)) files.push_back(it->path().string());
    }
    if (files.size() > 3000) { done = true; return; }
    for (auto& sd : subdirs) {
        walk_fonts(sd, files, done);
        if (done) return;
    }
}
static vector<string> scan_font_files() {
    vector<string> files;
    for (auto& d : font_dirs()) {
        bool done = false;
        walk_fonts(d, files, done);
    }
    return files;
}
static string match_font(const string& name, const vector<string>& files) {   // pygame.font.match_font 的近似实现
    for (auto& f : files) {
        string stem = lower_ascii(fs::path(f).stem().string()), norm;
        for (char c : stem) if (std::isalnum((unsigned char)c)) norm += c;
        if (norm == name) return f;
    }
    return "";
}

static string find_cjk_font() {                           // 按顺序找一个真正能显示中文的字体；找不到返回空串（界面自动改用英文）
    std::set<string> tried;
    auto ok = [&](const string& p) {
        std::error_code ec;
        if (p.empty() || tried.count(p) || !fs::is_regular_file(p, ec)) return false;
        tried.insert(p);
        return font_has_cjk(p);
    };
    {   // 1) 程序同目录下自己放的字体（最优先）
        std::error_code ec;
        vector<string> names;
        for (fs::directory_iterator it(HERE, ec), end; !ec && it != end; it.increment(ec))
            names.push_back(it->path().filename().string());
        std::sort(names.begin(), names.end());
        for (auto& fn : names)
            if (ends_with_any(fn, FONT_EXTS) && ok(HERE + fn)) return HERE + fn;
    }
    for (auto& p : FONT_PATHS)                            // 2) 常见系统字体路径
        if (ok(p)) return p;
    vector<string> files = scan_font_files();             // 3) 按名字查   4) 扫描系统字体目录
    for (auto& n : FONT_NAMES) {
        string p = match_font(n, files);
        if (ok(p)) return p;
    }
    vector<string> hinted, rest;
    std::set<string> hs;
    for (auto& f : files) {
        string base = lower_ascii(fs::path(f).filename().string());
        bool hit = false;
        for (auto& k : FONT_KEYS) if (base.find(k) != string::npos) { hit = true; break; }
        if (hit) { hinted.push_back(f); hs.insert(f); }
    }
    for (auto& f : files) if (!hs.count(f)) rest.push_back(f);
    if (rest.size() > 150) rest.resize(150);
    for (auto& p : hinted) if (ok(p)) return p;
    for (auto& p : rest) if (ok(p)) return p;
    return "";
}
static string find_latin_font() {
    std::error_code ec;
    for (fs::directory_iterator it(HERE, ec), end; !ec && it != end; it.increment(ec)) {
        string fn = it->path().filename().string();
        if (ends_with_any(fn, FONT_EXTS)) return HERE + fn;
    }
    for (auto& p : LATIN_FONT_PATHS)
        if (fs::is_regular_file(p, ec)) return p;
    return "";
}

static TTF_Font* get_font(int px) {
    auto it = g_fonts.find(px);
    if (it != g_fonts.end()) return it->second;
    int size = !FONT_FILE.empty() ? px : I(px * 1.25);
    const string& path = !FONT_FILE.empty() ? FONT_FILE : LATIN_FONT;
    TTF_Font* f = path.empty() ? nullptr : TTF_OpenFont(path.c_str(), size);
    g_fonts[px] = f;
    return f;
}
static std::shared_ptr<Surface> render(const string& text, int px, const Color& c) {
    string key = text + '\x01' + std::to_string(px) + ',' + std::to_string(c.r) + ',' + std::to_string(c.g) + ',' +
                 std::to_string(c.b);
    auto it = g_txt_cache.find(key);
    if (it != g_txt_cache.end()) return it->second;
    if (g_txt_cache.size() > 500) g_txt_cache.clear();
    auto img = std::make_shared<Surface>(render_text(get_font(px), text, c));
    g_txt_cache[key] = img;
    return img;
}
static int font_text_width(int px, const string& text) {   // font.size(text)[0]
    TTF_Font* f = get_font(px);
    int w = 0, h = 0;
    if (f && TTF_SizeUTF8(f, text.c_str(), &w, &h) == 0) return w;
    return (int)utf8_chars(text).size() * px / 2;
}

enum Anchor { A_CENTER, A_MIDLEFT, A_MIDRIGHT };
static Rect draw_text(Surface& s, const string& text, double size, const Color& color, double x, double y,
                      Anchor anchor = A_CENTER, bool shadow = false) {
    int px = std::max(8, I(size * SCALE));
    auto img = render(text, px, color);
    Rect r(0, 0, img->w, img->h);
    int ix = I(x), iy = I(y);
    if (anchor == A_CENTER) r.set_center(ix, iy);
    else if (anchor == A_MIDLEFT) { r.x = ix; r.y = iy - r.h / 2; }
    else r.set_midright(ix, iy);
    if (shadow) {
        int d = std::max(1, px / 16);
        blit(s, *render(text, px, rgb(0, 0, 0)), r.x + d, r.y + d);
    }
    blit(s, *img, r);
    return r;
}

static void draw_wrapped(Surface& s, const string& text, double size, const Color& color, const Rect& rect,
                         double gap = 1.4) {
    int px = std::max(8, I(size * SCALE));
    bool has_space = text.find(' ') != string::npos;
    string sep = has_space ? " " : "";
    vector<string> words;
    if (has_space) {                                       // text.split(" ")
        size_t st = 0;
        for (;;) {
            size_t p = text.find(' ', st);
            if (p == string::npos) { words.push_back(text.substr(st)); break; }
            words.push_back(text.substr(st, p - st));
            st = p + 1;
        }
    } else words = utf8_chars(text);
    vector<string> lines;
    string cur;
    for (auto& w : words) {
        string trial = !cur.empty() ? cur + sep + w : w;
        if (!cur.empty() && font_text_width(px, trial) > rect.w) { lines.push_back(cur); cur = w; }
        else cur = trial;
    }
    lines.push_back(cur);
    int y = rect.y;
    for (auto& ln : lines) {
        blit(s, *render(ln, px, color), rect.x, y);
        y += I(px * gap);
    }
}

// ============================ 声音 ============================
struct Sound {
    bool ok = false;
    int freq = 44100, ch = 1;
    struct Snd { vector<int16_t> data; double vol = 1.0; };
    std::map<string, Snd> snd;
    struct Voice { const Snd* s; size_t pos; };
    vector<Voice> voices;                                  // pygame 默认 8 个声道
    SDL_AudioDeviceID dev = 0;

    static void SDLCALL callback(void* ud, Uint8* stream, int len) {
        static_cast<Sound*>(ud)->mix((int16_t*)stream, len / 2);
    }
    void mix(int16_t* out, int n) {
        for (int i = 0; i < n; i++) {
            int acc = 0;
            for (auto& v : voices)
                if (v.pos < v.s->data.size()) acc += (int)(v.s->data[v.pos++] * v.s->vol);
            out[i] = (int16_t)clampi(acc, -32768, 32767);
        }
        voices.erase(std::remove_if(voices.begin(), voices.end(), [](const Voice& v) { return v.pos >= v.s->data.size(); }),
                     voices.end());
    }
    Sound() {
        if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
        SDL_AudioSpec want, have;
        SDL_zero(want);
        SDL_zero(have);
        want.freq = 44100;
        want.format = AUDIO_S16SYS;                        // pygame.mixer.pre_init(44100, -16, 1, 512)
        want.channels = 1;
        want.samples = 512;
        want.callback = &Sound::callback;
        want.userdata = this;
        dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (!dev) return;
        if (have.format != AUDIO_S16SYS) { SDL_CloseAudioDevice(dev); dev = 0; return; }
        freq = have.freq;
        ch = have.channels;
        snd["hit"].data = tone({{1046, 0.6}, {1568, 0.4}}, 90);
        snd["miss"].data = tone({{170, 1.0}}, 80, 0.6);
        snd["end"].data = tone({{659, 0.5}, {880, 0.5}}, 420);
        ok = true;
        SDL_PauseAudioDevice(dev, 0);
    }
    ~Sound() { if (dev) SDL_CloseAudioDevice(dev); }
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    vector<int16_t> tone(const vector<std::pair<double, double>>& parts, int ms, double vol = 0.8) {
        int n = I(freq * (double)ms / 1000.0);
        vector<int16_t> buf;
        for (int i = 0; i < n; i++) {
            double t = (double)i / freq;
            double e1 = 1.0 - (double)i / n;
            double env = e1 * e1 * std::min(1.0, i / (freq * 0.002));
            double v = 0;
            for (auto& pa : parts) v += pa.second * std::sin(2 * PI * pa.first * t);
            int val = I(32767 * 0.5 * vol * env * v);
            for (int c = 0; c < ch; c++) buf.push_back((int16_t)val);
        }
        return buf;
    }
    void play(const string& name, const Config& cfg) {
        if (!ok || !cfg.sound_on) return;
        auto it = snd.find(name);
        if (it == snd.end()) return;
        SDL_LockAudioDevice(dev);
        it->second.vol = cfg.volume / 100.0;
        if (voices.size() < 8) voices.push_back({&it->second, 0});
        SDL_UnlockAudioDevice(dev);
    }
};


// ============================ 3D 相机 / 房间 ============================
struct Camera {
    static constexpr double NEAR_Z = 0.3;
    double yaw = 0.0, pitch = 0.0;
    double px = 0.0, pz = 0.0;                            // 人物在场景里的位置（地面 x / z）
    int w = 0, h = 0;
    double f = 0, cx = 0, cy = 0;
    double cyw = 1, syw = 0, cp = 1, sp = 0;

    Camera() { update_proj(1280, 720, 90); prep(); }
    void update_proj(int w_, int h_, double fov) {
        w = w_; h = h_;
        f = (w / 2.0) / std::tan(radians(fov) / 2);
        cx = w / 2.0; cy = h / 2.0;
    }
    void prep() {
        double yw = radians(yaw), pt = radians(pitch);
        cyw = std::cos(yw); syw = std::sin(yw);
        cp = std::cos(pt); sp = std::sin(pt);
    }
    Vec3 to_cam(double x, double y, double z) const {
        x -= px;
        z -= pz;
        double x1 = x * cyw - z * syw;
        double z1 = x * syw + z * cyw;
        return {x1, y * cp - z1 * sp, y * sp + z1 * cp};
    }
    std::optional<Vec3> project(double x, double y, double z) const {
        Vec3 c = to_cam(x, y, z);
        if (c[2] < NEAR_Z) return std::nullopt;
        double k = f / c[2];
        return Vec3{cx + c[0] * k, cy - c[1] * k, c[2]};
    }
    Vec3 forward() const { return {cp * syw, sp, cp * cyw}; }
};

struct Seg { Vec3 a, b; };
static void build_room(vector<Seg>& lines, vector<Seg>& front) {
    const int X = 60, Y = 18, Z = 60, st = 6;
    auto V = [](int x, int y, int z) { return Vec3{(double)x, (double)y, (double)z}; };
    for (int x = -X; x <= X; x += st) {
        front.push_back({V(x, -Y, Z), V(x, Y, Z)});
        lines.push_back({V(x, -Y, -Z), V(x, Y, -Z)});
        lines.push_back({V(x, -Y, -Z), V(x, -Y, Z)});
        lines.push_back({V(x, Y, -Z), V(x, Y, Z)});
    }
    for (int y = -Y; y <= Y; y += st) {
        front.push_back({V(-X, y, Z), V(X, y, Z)});
        lines.push_back({V(-X, y, -Z), V(X, y, -Z)});
        lines.push_back({V(-X, y, -Z), V(-X, y, Z)});
        lines.push_back({V(X, y, -Z), V(X, y, Z)});
    }
    for (int z = -Z; z <= Z; z += st) {
        lines.push_back({V(-X, -Y, z), V(-X, Y, z)});
        lines.push_back({V(X, -Y, z), V(X, Y, z)});
        lines.push_back({V(-X, -Y, z), V(X, -Y, z)});
        lines.push_back({V(-X, Y, z), V(X, Y, z)});
    }
}
static vector<Seg> ROOM_LINES, ROOM_FRONT;

static void draw_lines_3d(Surface& s, const Camera& cam, const vector<Seg>& lines, const Color& color) {
    double n = Camera::NEAR_Z, f = cam.f, cx = cam.cx, cy = cam.cy;
    for (auto& sg : lines) {
        Vec3 a = cam.to_cam(sg.a[0], sg.a[1], sg.a[2]);
        Vec3 b = cam.to_cam(sg.b[0], sg.b[1], sg.b[2]);
        double ax = a[0], ay = a[1], az = a[2], bx = b[0], by = b[1], bz = b[2];
        if (az < n) {
            if (bz < n) continue;
            double t = (n - az) / (bz - az);
            ax += (bx - ax) * t;
            ay += (by - ay) * t;
            az = n;
        } else if (bz < n) {
            double t = (n - bz) / (az - bz);
            bx += (ax - bx) * t;
            by += (ay - by) * t;
            bz = n;
        }
        double ka = f / az, kb = f / bz;
        draw_line(s, color, cx + ax * ka, cy - ay * ka, cx + bx * kb, cy - by * kb, 1);
    }
}
static void draw_room(Surface& s, const Camera& cam, const Color& color, const Color& front_color) {
    draw_lines_3d(s, cam, ROOM_LINES, color);
    draw_lines_3d(s, cam, ROOM_FRONT, front_color);
}

static void draw_ball(Surface& s, double cx, double cy, double rf, const Color& color) {
    int r = I(rf);
    if (r < 1 || r > 4000) return;
    if (r < 4) { draw_circle(s, color, I(cx), I(cy), r); return; }
    Color edge = lerp_color(rgb(0, 0, 0), color, 0.45);
    Color hi = lerp_color(color, WHITE, 0.65);
    int steps = std::max(5, std::min(12, r / 3));
    for (int i = 0; i < steps; i++) {
        double t = (double)i / (steps - 1);
        int rad = std::max(1, I(r * (1 - 0.8 * t)));
        Color col = t < 0.55 ? lerp_color(edge, color, t / 0.55) : lerp_color(color, hi, (t - 0.55) / 0.45);
        double off = r * 0.30 * t;
        draw_circle(s, col, I(cx - off), I(cy - off), rad);
    }
}

// ============================ 准心 ============================
static std::shared_ptr<Surface> crosshair_surface(const Config& cfg) {
    static std::array<int, 9> ckey;
    static bool has = false;
    static std::shared_ptr<Surface> cached;
    std::array<int, 9> key = {cfg.ch_style, cfg.ch_color[0], cfg.ch_color[1], cfg.ch_color[2], cfg.ch_size,
                              cfg.ch_thick, cfg.ch_gap, cfg.ch_alpha, cfg.ch_outline ? 1 : 0};
    if (has && ckey == key) return cached;
    int style = cfg.ch_style, size = cfg.ch_size, th = cfg.ch_thick, gap = cfg.ch_gap;
    int ext = size + gap + th + 6;
    int d = ext * 2 + 1;
    auto surf = std::make_shared<Surface>(d, d, true);
    int c = ext;
    int a = I(255.0 * cfg.ch_alpha / 100);
    Color col = rgba(cfg.ch_color[0], cfg.ch_color[1], cfg.ch_color[2], a);
    Color out = rgba(0, 0, 0, a);
    int h = th / 2;
    vector<Rect> bars;
    if (style == 0 || style == 1 || style == 2) {          // 十字类
        bars.push_back(Rect(c + gap, c - h, size, th));        // 右
        bars.push_back(Rect(c - gap - size, c - h, size, th)); // 左
        bars.push_back(Rect(c - h, c + gap, th, size));        // 下
        if (style != 2) bars.push_back(Rect(c - h, c - gap - size, th, size));   // 上（T 形没有）
    }
    vector<std::pair<int, int>> circles;                   // (半径, 线宽)  线宽 0 表示实心
    if (style == 1) circles.push_back({std::max(1, th), 0});
    else if (style == 3) circles.push_back({std::max(1, size / 3), 0});
    else if (style == 4 || style == 5) {
        circles.push_back({size, th});
        if (style == 5) circles.push_back({std::max(1, th), 0});
    }
    if (cfg.ch_outline) {
        for (auto& b : bars) draw_rect(*surf, out, b.inflate(2, 2));
        for (auto& cw : circles) draw_circle(*surf, out, c, c, cw.first + 1, cw.second == 0 ? 0 : cw.second + 2);
    }
    for (auto& b : bars) draw_rect(*surf, col, b);
    for (auto& cw : circles) draw_circle(*surf, col, c, c, cw.first, cw.second);
    ckey = key;
    has = true;
    cached = surf;
    return surf;
}

// ============================ 第一人称步枪 ============================
// 用几个长方体在“枪自己的坐标系”里搭出步枪（x 右、y 下、z 向前），再用透视投影画出来。
// 因为枪管沿 z 轴向前，所以枪口自然指向准心——就是第一人称握枪的样子。
struct GunBox { double x0, x1, y0, y1, z0, z1; Color col; };
static vector<GunBox> rifle_boxes() {
    Color DK = rgb(40, 44, 54), MD = rgb(72, 79, 94), MT = rgb(112, 121, 140);
    Color SK = rgb(214, 170, 140), SL = rgb(52, 74, 62);
    Color GL = rgb(66, 124, 186), DOT = rgb(255, 70, 70);
    return {
        {-0.030, 0.030, 0.055, 0.135, 0.20, 0.34, SL},      // 右前臂 / 袖子
        {-0.020, 0.020, -0.028, 0.060, 0.22, 0.42, MD},     // 枪托
        {-0.030, 0.030, -0.042, 0.046, 0.38, 0.66, MD},     // 机匣
        {-0.016, 0.016, -0.052, -0.042, 0.38, 0.66, DK},    // 上导轨
        {-0.016, 0.016, 0.046, 0.115, 0.40, 0.45, DK},      // 握把
        {-0.022, 0.022, 0.050, 0.112, 0.34, 0.44, SK},      // 右手
        {-0.014, 0.014, 0.046, 0.140, 0.54, 0.60, DK},      // 弹匣
        {-0.027, 0.027, -0.030, 0.036, 0.66, 0.90, MT},     // 护木
        {-0.031, 0.031, 0.034, 0.080, 0.70, 0.84, SK},      // 左手
        {-0.008, 0.008, -0.008, 0.008, 0.90, 1.10, DK},     // 枪管
        {-0.013, 0.013, -0.013, 0.013, 1.10, 1.16, MT},     // 枪口制退器
        {-0.003, 0.003, -0.044, -0.008, 0.96, 0.975, DK},   // 准星
        {-0.018, 0.018, -0.086, -0.052, 0.46, 0.54, DK},    // 红点镜
        {-0.013, 0.013, -0.081, -0.060, 0.535, 0.540, GL},  // 镜片
        {-0.0025, 0.0025, -0.072, -0.067, 0.534, 0.536, DOT},// 红点
    };
}
static const vector<GunBox> GUN_BOXES = rifle_boxes();
static const int BOX_FACES[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 3, 7, 4}, {1, 2, 6, 5}, {0, 1, 5, 4}, {3, 2, 6, 7}};
static const double LIGHT[3] = {0.30, -0.90, -0.30};

static Color shade(const Color& c, double k) {
    return rgb(std::min(255, I(c.r * k)), std::min(255, I(c.g * k)), std::min(255, I(c.b * k)));
}

static void draw_star(Surface& s, double x, double y, double R, const Color& col, int spikes = 8, double rot = 0.0) {
    Pts pts;
    for (int i = 0; i < spikes * 2; i++) {
        double r = (i % 2 == 0) ? R : R * 0.38;
        double a = rot + i * PI / spikes;
        pts.push_back({x + std::cos(a) * r, y + std::sin(a) * r});
    }
    draw_polygon(s, col, pts);
}

// 在 w×h 的画面里画第一人称步枪。lay = [x偏移比例, y偏移比例, 大小倍数]。
// 返回步枪占的矩形（布局编辑时用来点选）。
static std::optional<Rect> draw_fp_rifle(Surface& s, int w, int h, const Vec3& lay, double kick = 0.0,
                                         double sway_x = 0.0, double sway_y = 0.0, double t = 0.0, bool flash = false) {
    double cx = w / 2.0, cy = h / 2.0;
    double fg = h * 0.92;                                  // 枪专用焦距：只跟屏幕高度有关，不受 FOV / 宽高比影响
    double size = lay[2];
    double shx = lay[0] * w, shy = lay[1] * h;
    double lim = 44 * SCALE + 1e-6;
    double yaw = clampd(sway_x / lim, -1.0, 1.0) * 0.05;
    double pit = -clampd(sway_y / lim, -1.0, 1.0) * 0.05 + kick * 0.06;
    double cyw = std::cos(yaw), syw = std::sin(yaw), cp = std::cos(pit), sp = std::sin(pit);
    const double pv[3] = {0.0, 0.05, 0.40};
    double tx = 0.135, ty = 0.100 + kick * 0.008 + std::sin(t * 2.0) * 0.0012, tz = 0.14 - kick * 0.05;

    auto xf = [&](double x, double y, double z) -> Vec3 {
        x -= pv[0]; y -= pv[1]; z -= pv[2];
        double x1 = x * cyw + z * syw, z1 = -x * syw + z * cyw;
        double y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
        return {x1 + pv[0] + tx, y2 + pv[1] + ty, std::max(0.07, z2 + pv[2] + tz)};
    };
    auto scr = [&](const Vec3& v) -> std::pair<double, double> {
        double k = fg / v[2];
        return {cx + (v[0] * k) * size + shx, cy + (v[1] * k) * size + shy};
    };

    struct Face { double depth; Pts pts; Color col; };
    vector<Face> faces;
    for (auto& bx : GUN_BOXES) {
        double raw[8][3] = {{bx.x0, bx.y0, bx.z0}, {bx.x1, bx.y0, bx.z0}, {bx.x1, bx.y1, bx.z0}, {bx.x0, bx.y1, bx.z0},
                            {bx.x0, bx.y0, bx.z1}, {bx.x1, bx.y0, bx.z1}, {bx.x1, bx.y1, bx.z1}, {bx.x0, bx.y1, bx.z1}};
        Vec3 V[8];
        for (int i = 0; i < 8; i++) V[i] = xf(raw[i][0], raw[i][1], raw[i][2]);
        double bc[3] = {0, 0, 0};
        for (int i = 0; i < 8; i++) for (int k = 0; k < 3; k++) bc[k] += V[i][(size_t)k];
        for (int k = 0; k < 3; k++) bc[k] /= 8.0;
        for (auto& f : BOX_FACES) {
            const Vec3 &p0 = V[f[0]], &p1 = V[f[1]], &p2 = V[f[2]];
            double u[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
            double v[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
            double n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
            double c[3] = {0, 0, 0};
            for (int i = 0; i < 4; i++) for (int k = 0; k < 3; k++) c[k] += V[f[i]][(size_t)k];
            for (int k = 0; k < 3; k++) c[k] /= 4.0;
            if (n[0] * (c[0] - bc[0]) + n[1] * (c[1] - bc[1]) + n[2] * (c[2] - bc[2]) < 0) {
                n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2];
            }
            if (n[0] * c[0] + n[1] * c[1] + n[2] * c[2] >= 0) continue;     // 背对镜头的面不画
            double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (nl == 0.0) nl = 1.0;
            double lit = std::max(0.0, (n[0] * LIGHT[0] + n[1] * LIGHT[1] + n[2] * LIGHT[2]) / nl / 0.99);
            Face fc;
            fc.depth = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
            for (int i = 0; i < 4; i++) fc.pts.push_back(scr(V[f[i]]));
            fc.col = shade(bx.col, 0.55 + 0.75 * lit);
            faces.push_back(std::move(fc));
        }
    }
    std::stable_sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.depth > b.depth; });
    double x_lo = 1e9, y_lo = 1e9, x_hi = -1e9, y_hi = -1e9;
    for (auto& fc : faces) {
        draw_polygon(s, fc.col, fc.pts);
        draw_polygon(s, shade(fc.col, 0.55), fc.pts, 1);
        for (auto& p : fc.pts) {
            x_lo = std::min(x_lo, p.first); x_hi = std::max(x_hi, p.first);
            y_lo = std::min(y_lo, p.second); y_hi = std::max(y_hi, p.second);
        }
    }
    if (flash) {                                           // 枪口火焰
        Vec3 m = xf(0.0, 0.0, 1.17);
        auto mp = scr(m);
        double R = 0.095 * fg / m[2] * size * uniform(0.85, 1.1);
        double rot = uniform(0, PI);
        draw_star(s, mp.first, mp.second, R, rgb(255, 150, 40), 8, rot);
        draw_star(s, mp.first, mp.second, R * 0.7, rgb(255, 205, 80), 8, rot + 0.2);
        draw_star(s, mp.first, mp.second, R * 0.4, rgb(255, 245, 200), 8, rot + 0.4);
    }
    if (x_lo > x_hi) return std::nullopt;
    Rect r(I(x_lo), I(y_lo), I(x_hi - x_lo), I(y_hi - y_lo));
    return r.clip(Rect(0, 0, w, h));
}

// 半透明圆角面板（带缓存）
static std::shared_ptr<Surface> panel_surface(int w, int h, int radius, const Color& rgbaCol) {
    static std::map<std::array<int, 7>, std::shared_ptr<Surface>> cache;
    std::array<int, 7> key = {w, h, radius, rgbaCol.r, rgbaCol.g, rgbaCol.b, rgbaCol.a};
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    if (cache.size() > 20) cache.clear();
    auto surf = std::make_shared<Surface>(w, h, true);
    draw_rect(*surf, rgbaCol, Rect(0, 0, w, h), 0, radius);
    cache[key] = surf;
    return surf;
}

// ============================ 数学小工具 ============================
static Vec3 dir_angles(double yaw, double pitch) {
    double y = radians(yaw), p = radians(pitch);
    return {std::cos(p) * std::sin(y), std::sin(p), std::cos(p) * std::cos(y)};
}
static double ang(const Vec3& p, const Vec3& q) {
    double lp = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    double lq = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
    double c = (p[0] * q[0] + p[1] * q[1] + p[2] * q[2]) / (lp * lq + 1e-9);
    return degrees(std::acos(clampd(c, -1.0, 1.0)));
}

// ============================ 手机相关：平台检测 / 按键几何 ============================
static bool is_android() {
#ifdef __ANDROID__
    return true;
#else
    return false;
#endif
}
static vector<string> active_buttons(const Config& cfg, bool touch, bool force = false) {   // 后面的画在上层
    vector<string> names;
    if ((touch || force) && cfg.move_on) names.push_back("stick");   // 轮盘放最底层，其它按键盖在上面
    if (cfg.show_fire || force) names.push_back("fire");
    if (touch || force) names.push_back("pause");
    return names;
}
struct BtnGeom { double x, y, r; };
static BtnGeom btn_geom(const Config& cfg, const string& name, int W, int H) {
    const Vec3& l = cfg.layout.at(name);
    return {l[0] * W, l[1] * H, sc(BTN_BASE_R.at(name)) * l[2]};
}


// ============================ 游戏本体 ============================
struct Item { double z, x, y, r; Color col; };             // 画小球时用的排序项（远的先画）
static void sort_items_far_first(vector<Item>& items) {
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.z != b.z) return a.z > b.z;
        if (a.x != b.x) return a.x > b.x;
        if (a.y != b.y) return a.y > b.y;
        if (a.r != b.r) return a.r > b.r;
        return color_less(b.col, a.col);
    });
}

struct Target {
    double x = 0, y = 0, z = 0, r = 0, born = 0;
    Color color;
    double vx = 0, vy = 0, vz = 0, timer = 0;
    Target() = default;
    Target(double x_, double y_, double z_, double r_, double born_, const Color& c)
        : x(x_), y(y_), z(z_), r(r_), born(born_), color(c) {}
};
struct Particle {
    double x, y, z, vx, vy, vz, born;
    Color color;
    double life;
};

struct Game {
    string mode;
    Config& cfg;
    Sound& sound;
    Diff dif;
    Camera cam;
    double countdown = 3.0;
    bool running = false, finished = false;
    double t = 0.0, time_left;
    vector<Target> targets;
    vector<Particle> particles;
    double score = 0.0;
    int hits = 0, shots = 0, combo = 0, max_combo = 0;
    double ttk_sum = 0.0, track_on = 0.0;
    bool hot = false;
    double hitmark = 0.0;
    double kick = 0.0;                 // 步枪后坐力 0~1
    double flash = 0.0;                // 枪口火焰剩余时间
    double auto_t = 0.0;               // 跟枪模式按住左键时的连发计时
    double sway_x = 0.0, sway_y = 0.0; // 枪身随视角转动的惯性摇摆
    double fire_t = 0.0;               // 开火指示灯剩余时间
    Color fire_col = rgb(80, 230, 110);
    bool firing = false;
    bool touch_mode = false;           // 触屏模式：显示屏幕按键
    bool edit = false;                 // 键位布局编辑预览
    std::set<string> pressed;          // 当前被按住的屏幕按键
    std::optional<Rect> rifle_bbox;
    std::pair<double, double> move_in{0.0, 0.0};   // 移动输入：(左右, 前后)，-1~1
    double vel[2] = {0.0, 0.0};        // 人物当前速度（世界坐标）
    std::pair<double, double> stick{0.0, 0.0};     // 轮盘旋钮偏移（画图用）
    double bob = 0.0;                  // 走路时枪身起伏相位
    // 跟枪模式
    double anc[2] = {0, 0};
    double t_yaw = 0, t_pitch = 0, t_vel[2] = {0, 0}, t_goal[2] = {0, 0}, t_next = 0;

    Game(const string& mode_, Config& cfg_, Sound& snd, int W, int H)
        : mode(mode_), cfg(cfg_), sound(snd), dif(DIFFS[clampi(cfg_.diff, 0, 2)]), time_left((double)cfg_.duration) {
        cam.update_proj(W, H, cfg.fov);
    }

    // ---------- 生成目标 ----------
    Color pick_color() {
        if (cfg.ball_random) return PALETTE[(size_t)(g_rng() % (PALETTE.size() - 1))];
        return rgb(cfg.ball_color[0], cfg.ball_color[1], cfg.ball_color[2]);
    }
    Vec3 rel(const Target& tg) const {                     // 目标相对人物的坐标
        return {tg.x - cam.px, tg.y, tg.z - cam.pz};
    }
    void spawn() {
        double size = cfg.ball_size * dif.size;
        double px = cam.px, pz = cam.pz;
        double r;
        Vec3 pos = {0, 0, 0};
        if (mode == "flick" || mode == "move" || mode == "multi") {
            r = (mode == "flick" ? 1.1 : 1.0) * size;
            for (int it = 0; it < 60; it++) {
                double dist = mode == "flick" ? uniform(20, 30) : uniform(17, 28);
                double yw = uniform(-70, 70);
                double pt = uniform(-25, 25);
                Vec3 d = dir_angles(yw, pt);
                pos = {d[0] * dist, d[1] * dist, d[2] * dist};
                double sep = std::max(22.0, degrees(r / dist) * 2 + 10);
                bool all_ok = true;
                for (auto& tg : targets)
                    if (!(ang(pos, rel(tg)) > sep)) { all_ok = false; break; }
                if (all_ok && ang(pos, cam.forward()) > std::max(15.0, sep - 5)) break;
            }
        } else {                                           // precision
            r = 0.55 * size;
            for (int it = 0; it < 60; it++) {
                double yw = uniform(-17, 17);
                double pt = uniform(-9, 9);
                Vec3 d = dir_angles(yw, pt);
                pos = {d[0] * 25, d[1] * 25, d[2] * 25};
                double sep = degrees(r / 25) * 2.3 + 1.0;
                bool all_ok = true;
                for (auto& tg : targets)
                    if (!(ang(pos, rel(tg)) > sep)) { all_ok = false; break; }
                if (all_ok) break;
            }
        }
        Target tg(pos[0] + px, pos[1], pos[2] + pz, r, t, pick_color());
        if (mode == "move") {                              // 横向闪避：垂直于视线方向来回移动
            double n = std::hypot(pos[0], pos[2]) + 1e-6;
            double sp = uniform(5.0, 10.0) * rnd_sign() * dif.speed;
            tg.vx = -pos[2] / n * sp;
            tg.vz = pos[0] / n * sp;
            tg.vy = uniform(-1.2, 1.2);
            tg.timer = uniform(0.5, 1.4);
        } else if (mode == "multi") {                      // 缓慢漂移：朝随机方向平滑移动，隔一会儿换方向
            double a = uniform(0, 2 * PI);
            double sp = uniform(3.0, 6.0) * dif.speed;
            tg.vx = std::cos(a) * sp;
            tg.vz = std::sin(a) * sp;
            tg.vy = uniform(-1.0, 1.0);
            tg.timer = uniform(1.5, 3.0);
        }
        targets.push_back(tg);
    }

    // 移动目标定位：目标左右闪避、随机变向，碰到边界 / 太近就反弹
    void move_targets(double dt) {
        for (auto& tg : targets) {
            tg.timer -= dt;
            if (tg.timer <= 0) {
                if (mode == "move") {                      // 突然反向闪避
                    double k = -uniform(0.8, 1.25);
                    tg.vx *= k;
                    tg.vz *= k;
                    tg.vy = uniform(-1.5, 1.5);
                    tg.timer = uniform(0.4, 1.5);
                } else {                                   // 换一个漂移方向
                    double a = uniform(0, 2 * PI);
                    double sp = uniform(3.0, 6.0) * dif.speed;
                    tg.vx = std::cos(a) * sp;
                    tg.vz = std::sin(a) * sp;
                    tg.vy = uniform(-1.0, 1.0);
                    tg.timer = uniform(1.5, 3.0);
                }
            }
            tg.x += tg.vx * dt;
            tg.y += tg.vy * dt;
            tg.z += tg.vz * dt;
            if (std::abs(tg.x) > 52) { tg.x = std::copysign(52.0, tg.x); tg.vx = -tg.vx; }
            if (std::abs(tg.z) > 52) { tg.z = std::copysign(52.0, tg.z); tg.vz = -tg.vz; }
            if (std::abs(tg.y) > 12) { tg.y = std::copysign(12.0, tg.y); tg.vy = -tg.vy; }
            double rx = tg.x - cam.px, rz = tg.z - cam.pz;
            double yaw_rel = degrees(std::atan2(rx, rz));
            if (std::abs(yaw_rel) > 78 || std::hypot(rx, rz) < 11) {   // 跑到身后 / 贴脸：掉头
                tg.vx = -tg.vx;
                tg.vz = -tg.vz;
                tg.x += tg.vx * dt * 2;
                tg.z += tg.vz * dt * 2;
            }
        }
    }

    void begin() {
        running = true;
        if (mode == "track") {
            anc[0] = cam.px;
            anc[1] = cam.pz;
            t_yaw = uniform(-20, 20);
            t_pitch = uniform(-8, 8);
            t_vel[0] = t_vel[1] = 0.0;
            t_goal[0] = t_goal[1] = 0.0;
            t_next = 0.0;
            targets.push_back(Target(0, 0, 25, 1.5 * cfg.ball_size * dif.size, 0, pick_color()));
            move_track(0);
        } else {
            for (int i = 0; i < (mode == "flick" ? 2 : 3); i++) spawn();
        }
    }

    // ---------- 跟枪目标运动 ----------
    void move_track(double dt) {
        t_next -= dt;
        if (t_next <= 0) {
            double a = uniform(0, 2 * PI);
            double sp = uniform(15, 38) * dif.speed;
            t_goal[0] = std::cos(a) * sp;
            t_goal[1] = std::sin(a) * sp * 0.6;
            t_next = uniform(0.4, 1.4);
        }
        double k = std::min(1.0, dt * 5);
        for (int i = 0; i < 2; i++) t_vel[i] += (t_goal[i] - t_vel[i]) * k;
        t_yaw += t_vel[0] * dt;
        t_pitch += t_vel[1] * dt;
        if (std::abs(t_yaw) > 55) {
            t_yaw = std::copysign(55.0, t_yaw);
            t_vel[0] *= -1;
            t_goal[0] *= -1;
        }
        if (std::abs(t_pitch) > 22) {
            t_pitch = std::copysign(22.0, t_pitch);
            t_vel[1] *= -1;
            t_goal[1] *= -1;
        }
        Vec3 d = dir_angles(t_yaw, t_pitch);
        Target& tg = targets[0];
        k = std::min(1.0, dt * 0.8);                       // 目标活动中心慢慢跟着人物走
        anc[0] += (cam.px - anc[0]) * k;
        anc[1] += (cam.pz - anc[1]) * k;
        tg.x = anc[0] + d[0] * 25;
        tg.y = d[1] * 25;
        tg.z = anc[1] + d[2] * 25;
    }

    std::optional<double> ray_hit(const Target& tg) const {
        Vec3 f = cam.forward();
        Vec3 r = rel(tg);
        double proj = r[0] * f[0] + r[1] * f[1] + r[2] * f[2];
        if (proj <= 0) return std::nullopt;
        double d2 = r[0] * r[0] + r[1] * r[1] + r[2] * r[2] - proj * proj;
        if (d2 <= tg.r * tg.r) return proj;
        return std::nullopt;
    }

    // ---------- 输入 ----------
    void on_click() {
        if (!running || mode == "track") return;
        int best = -1;
        double bt = 1e9;
        for (size_t i = 0; i < targets.size(); i++) {
            auto p = ray_hit(targets[i]);
            if (p && *p < bt) { best = (int)i; bt = *p; }
        }
        shots += 1;
        kick = 1.0; flash = 0.06; fire_t = 0.14;           // 步枪后坐力 / 枪口火焰 / 开火灯
        if (best < 0) {
            fire_col = rgb(255, 90, 80);
            combo = 0;
            sound.play("miss", cfg);
            return;
        }
        fire_col = rgb(80, 230, 110);
        Target hit = targets[(size_t)best];
        double ttk = t - hit.born;
        hits += 1;
        combo += 1;
        max_combo = std::max(max_combo, combo);
        int pts = 100 + std::min(combo, 20) * 5;
        if (mode == "flick" || mode == "move" || mode == "multi") pts += std::max(0, I(100 * (2.0 - ttk) / 2.0));
        score += pts * dif.score;
        ttk_sum += ttk;
        hitmark = 0.15;
        sound.play("hit", cfg);
        burst(hit);
        targets.erase(targets.begin() + best);
        spawn();
    }

    void burst(const Target& tg) {
        if (!cfg.particles) return;
        for (int i = 0; i < 14; i++) {
            double a = uniform(0, 2 * PI), b = uniform(-1, 1);
            double sr = std::sqrt(1 - b * b);
            double sp = uniform(3, 9);
            double life = uniform(0.15, 0.35);
            particles.push_back({tg.x, tg.y, tg.z, sr * std::cos(a) * sp, b * sp, sr * std::sin(a) * sp, t, tg.color, life});
        }
    }

    // ---------- 更新 ----------
    void update(double dt, double dx, double dy, bool firing_) {
        // 手指滑屏：滑屏灵敏度独立可调（touch_sens、touch_ratio）；电脑用鼠标灵敏度
        double kx, ky;
        if (touch_mode) {
            kx = 0.066 * cfg.touch_sens;
            ky = kx * cfg.touch_ratio;
        } else {
            kx = ky = 0.022 * cfg.sens;
        }
        double yaw_d = dx * kx, pit_d = -dy * ky;
        cam.yaw = pymod(cam.yaw + yaw_d, 360.0);
        cam.pitch = clampd(cam.pitch + pit_d, -89.0, 89.0);
        // ---- 人物移动：轮盘 / WASD，带一点加减速，走路时枪身轻微起伏 ----
        double spd = 0.0;
        if (cfg.move_on) {
            double mxi = move_in.first, myi = move_in.second;
            double n = std::hypot(mxi, myi);
            if (n > 1.0) { mxi /= n; myi /= n; }
            double sp = 14.0 * cfg.move_speed;
            double yw = radians(cam.yaw);
            double sy_ = std::sin(yw), cy_ = std::cos(yw);
            double k = std::min(1.0, dt * 14);
            vel[0] += ((myi * sy_ + mxi * cy_) * sp - vel[0]) * k;
            vel[1] += ((myi * cy_ - mxi * sy_) * sp - vel[1]) * k;
            cam.px = clampd(cam.px + vel[0] * dt, -30.0, 30.0);
            cam.pz = clampd(cam.pz + vel[1] * dt, -30.0, 30.0);
            spd = std::hypot(vel[0], vel[1]);
            bob += spd * dt * 0.5;
        } else {
            vel[0] = vel[1] = 0.0;
        }
        double wk = std::min(1.0, spd / 10.0);
        double bob_x = std::cos(bob) * 3.0 * SCALE * wk;
        double bob_y = std::abs(std::sin(bob)) * 5.0 * SCALE * wk;
        // 步枪后坐力 / 枪口火焰 / 开火灯 / 枪身惯性摇摆
        firing = firing_;
        kick = std::max(0.0, kick - dt * 9);
        flash = std::max(0.0, flash - dt);
        fire_t = std::max(0.0, fire_t - dt);
        if (running && mode == "track" && firing_) {       // 跟枪时按住左键 = 连发
            auto_t -= dt;
            if (auto_t <= 0) {
                auto_t = 0.09;
                kick = 1.0;
                flash = 0.05;
            }
        } else {
            auto_t = 0.0;
        }
        double lim = 44 * SCALE, f = std::min(1.0, dt * 10);
        sway_x += (clampd(-yaw_d * 5 * SCALE, -lim, lim) + bob_x - sway_x) * f;
        sway_y += (clampd(pit_d * 5 * SCALE, -lim, lim) + bob_y - sway_y) * f;
        hitmark = std::max(0.0, hitmark - dt);
        {
            vector<Particle> keep;
            for (auto& p : particles) if (t - p.born < 0.45) keep.push_back(p);
            particles.swap(keep);
        }
        if (!running) {
            countdown -= dt;
            if (countdown <= 0) begin();
            return;
        }
        t += dt;
        time_left -= dt;
        if (mode == "move" || mode == "multi") move_targets(dt);
        if (mode == "track") {
            move_track(dt);
            bool on = ray_hit(targets[0]).has_value();
            bool fire = firing_ || !cfg.hold_track;
            hot = on && fire;
            if (hot) {
                track_on += dt;
                score += dt * 100 * dif.score;
            }
        }
        if (time_left <= 0) {
            time_left = 0.0;
            finished = true;
        }
    }

    // ---------- 绘制 ----------
    void draw(Surface& s, double fps) {
        const Theme& th = THEMES[(size_t)std::min(cfg.theme, (int)THEMES.size() - 1)];
        fill(s, th.bg);
        cam.prep();
        if (cfg.grid) draw_room(s, cam, th.grid, th.front);
        // 粒子
        for (auto& p : particles) {
            double age = t - p.born;
            auto pr = cam.project(p.x + p.vx * age, p.y + p.vy * age, p.z + p.vz * age);
            if (pr) {
                double r = p.life * (1 - age / 0.45) * cam.f / (*pr)[2];
                draw_circle(s, p.color, I((*pr)[0]), I((*pr)[1]), std::max(1, I(r)));
            }
        }
        // 小球（远的先画）
        vector<Item> items;
        for (auto& tg : targets) {
            auto pr = cam.project(tg.x, tg.y, tg.z);
            if (pr) {
                Color col = tg.color;
                if (mode == "track" && hot) col = lerp_color(col, WHITE, 0.5);
                items.push_back({(*pr)[2], (*pr)[0], (*pr)[1], tg.r * cam.f / (*pr)[2], col});
            }
        }
        sort_items_far_first(items);
        for (auto& it : items) draw_ball(s, it.x, it.y, it.r, it.col);
        // 准心
        int cx = I(cam.cx), cy = I(cam.cy);
        if (cfg.ch_on) {
            auto cs = crosshair_surface(cfg);
            blit(s, *cs, cx - cs->w / 2, cy - cs->h / 2);
        }
        if (hitmark > 0) {
            int o = sc(7), L = sc(8);
            const int sg[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
            for (auto& q : sg)
                draw_line(s, WHITE, cx + q[0] * o, cy + q[1] * o, cx + q[0] * (o + L), cy + q[1] * (o + L), 2);
        }
        if (cfg.show_rifle || edit) draw_rifle(s);
        draw_hud(s, fps);
    }

    void draw_hud(Surface& s, double fps) {
        int W = cam.w, H = cam.h;
        if (cfg.show_bar) draw_bar(s);
        else                                                // 关掉抬头指示器时，保留原来的顶部大号倒计时
            draw_text(s, std::to_string(pyceil(time_left)), 54, WHITE, W / 2, sc(44), A_CENTER, true);
        draw_buttons(s);
        if (cfg.show_hud) {
            draw_text(s, vfmt(T("得分 %d").c_str(), I(score)), 32, WHITE, sc(28), sc(40), A_MIDLEFT, true);
            if (mode == "track") {
                double rate = track_on / std::max(0.001, t) * 100;
                draw_text(s, vfmt(T("追踪率 %.0f%%").c_str(), rate), 32, WHITE, W - sc(28), sc(40), A_MIDRIGHT, true);
            } else {
                double acc = shots ? (double)hits / shots * 100 : 0.0;
                draw_text(s, vfmt(T("命中率 %.0f%%").c_str(), acc), 32, WHITE, W - sc(28), sc(40), A_MIDRIGHT, true);
                if (combo > 1)
                    draw_text(s, vfmt(T("连击 x%d").c_str(), combo), 28, YELLOW, sc(28), sc(82), A_MIDLEFT, true);
            }
        }
        if (cfg.show_fps)
            draw_text(s, vfmt("FPS %d", I(fps)), 20, rgb(200, 210, 230), sc(20), H - sc(24), A_MIDLEFT, true);
        if (!running) {
            int n = pyceil(countdown);
            draw_text(s, std::to_string(std::max(n, 1)), 140, WHITE, W / 2, I(H * 0.30), A_CENTER, true);
            draw_text(s, T(hint_of(mode)), 30, YELLOW, W / 2, I(H * 0.30) + sc(110), A_CENTER, true);
        }
    }

    // ---------- 抬头指示器：左边命中数，右边剩余时间 ----------
    void draw_bar(Surface& s) {
        int W = cam.w;
        int w = sc(400), h = sc(58);
        Rect r(0, 0, w, h);
        r.set_midtop(W / 2, sc(12));
        Color line = rgb(74, 88, 128);
        blit(s, *panel_surface(w, h, h / 2, rgba(12, 16, 28, 175)), r.x, r.y);
        draw_rect(s, line, r, 2, h / 2);
        draw_line(s, line, r.centerx(), r.y + sc(12), r.centerx(), r.bottom() - sc(12), 2);
        int lx = r.x + w / 4, rx = r.x + w * 3 / 4;
        string label, val;
        if (mode == "track") {                             // 跟枪没有“个数”，改成追踪秒数
            label = T("追踪");
            val = vfmt("%.1fs", track_on);
        } else {
            label = T("命中数");
            val = std::to_string(hits);
        }
        draw_text(s, label, 17, SUB, lx, r.y + sc(15));
        draw_text(s, val, 32, WHITE, lx, r.y + sc(40));
        int left = std::max(0, pyceil(time_left));
        Color col = left <= 5 ? rgb(255, 90, 80) : (left <= 10 ? YELLOW : WHITE);
        draw_text(s, T("时间"), 17, SUB, rx, r.y + sc(15));
        draw_text(s, vfmt("%d:%02d", left / 60, left % 60), 32, col, rx, r.y + sc(40));
    }

    // ---------- 屏幕按键：大号开火键（命中绿 / 未命中红 / 跟枪按住绿或橙）、移动轮盘、暂停 ----------
    void draw_buttons(Surface& s) {
        for (auto& name : active_buttons(cfg, touch_mode, edit)) draw_button(s, name);
    }

    void draw_button(Surface& s, const string& name) {
        BtnGeom bg = btn_geom(cfg, name, cam.w, cam.h);
        bool is_pressed = pressed.count(name) > 0;
        double rf = bg.r;
        if (is_pressed) rf *= 0.94;
        int r = std::max(8, I(rf));
        int cx = I(bg.x), cy = I(bg.y);
        int c = r + 6;
        Surface surf(c * 2, c * 2, true);
        if (name == "fire") {
            bool held = running && mode == "track" && is_pressed;
            bool on = is_pressed || fire_t > 0;
            Color col;
            if (held) col = hot ? rgb(80, 230, 110) : rgb(255, 190, 60);
            else if (on) col = fire_col;
            else col = rgb(120, 140, 190);
            draw_circle(surf, rgba(col.r, col.g, col.b, on ? 130 : 60), c, c, r);
            draw_circle(surf, rgba(col.r, col.g, col.b, on ? 255 : 200), c, c, r, std::max(3, r / 14));
            draw_circle(surf, rgba(col.r, col.g, col.b, on ? 230 : 130), c, c, I(r * 0.26), std::max(2, r / 22));
            for (int k = 0; k < 4; k++) {                  // 中心十字刻线
                double a = k * PI / 2;
                draw_line(surf, rgba(col.r, col.g, col.b, on ? 230 : 150),
                          c + std::cos(a) * r * 0.40, c + std::sin(a) * r * 0.40,
                          c + std::cos(a) * r * 0.62, c + std::sin(a) * r * 0.62, std::max(2, r / 20));
            }
            blit(s, surf, cx - c, cy - c);
            draw_text(s, T("开火"), std::max(10, I(r / SCALE * 0.30)), on ? WHITE : rgb(210, 220, 240), cx,
                      cy + I(r * 0.80), A_CENTER, true);
        } else if (name == "stick") {
            double sx = stick.first, sy = stick.second;
            bool act = std::abs(sx) + std::abs(sy) > 0.01;
            Color col = act ? ACCENT : rgb(150, 162, 186);
            draw_circle(surf, rgba(20, 26, 44, 110), c, c, r);
            draw_circle(surf, rgba(col.r, col.g, col.b, 220), c, c, r, std::max(2, r / 20));
            draw_circle(surf, rgba(col.r, col.g, col.b, 90), c, c, I(r * 0.62), 1);
            const int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};   // 四个方向小三角
            for (auto& dn : dirs) {
                int dxn = dn[0], dyn = dn[1];
                double ax = c + dxn * r * 0.82, ay = c + dyn * r * 0.82;
                double q = r * 0.09;
                Pts pts;
                if (dxn == 0) pts = {{ax, ay + dyn * q}, {ax - q, ay - dyn * q}, {ax + q, ay - dyn * q}};
                else pts = {{ax + dxn * q, ay}, {ax - dxn * q, ay - q}, {ax - dxn * q, ay + q}};
                draw_polygon(surf, rgba(col.r, col.g, col.b, 200), pts);
            }
            int kr = I(r * 0.36);
            int kx_ = c + I(sx * r * 0.64), ky_ = c + I(sy * r * 0.64);
            draw_circle(surf, rgba(col.r, col.g, col.b, act ? 150 : 90), kx_, ky_, kr);
            draw_circle(surf, rgba(col.r, col.g, col.b, 255), kx_, ky_, kr, std::max(2, r / 22));
            blit(s, surf, cx - c, cy - c);
        } else if (name == "pause") {
            Color col = rgb(230, 235, 250);
            draw_circle(surf, rgba(20, 26, 44, 150), c, c, r);
            draw_circle(surf, rgba(col.r, col.g, col.b, 220), c, c, r, std::max(2, r / 10));
            int bw = std::max(3, I(r * 0.20)), bh = I(r * 0.9);
            for (int dx_ : {-1, 1})
                draw_rect(surf, rgba(col.r, col.g, col.b, 240), Rect(c + dx_ * I(r * 0.30) - bw / 2, c - bh / 2, bw, bh), 0, 2);
            blit(s, surf, cx - c, cy - c);
        }
    }

    // ---------- 第一人称步枪：开火后坐 + 枪口火焰 + 随视角惯性摇摆 ----------
    void draw_rifle(Surface& s) {
        rifle_bbox = draw_fp_rifle(s, cam.w, cam.h, cfg.layout.at("rifle"), kick, sway_x, sway_y, running ? t : 0.0,
                                   flash > 0);
    }
};


// ============================ 事件 / 时钟（对应 pygame.event / pygame.time.Clock） ============================
enum EvType { E_QUIT, E_KEYDOWN, E_MOUSEDOWN, E_MOUSEUP, E_MOUSEMOTION, E_FINGERDOWN, E_FINGERMOTION, E_FINGERUP, E_FOCUSLOST };
struct Ev {
    EvType type = E_QUIT;
    int key = 0, button = 0, x = 0, y = 0;
    bool touch = false;                                    // 触屏合成的鼠标事件
    long long finger = 0;
    double fx = 0, fy = 0;                                 // 手指位置（0~1）
};

struct Clock {
    uint64_t last = 0;
    uint64_t fps_tick = 0;
    int fps_count = 0;
    double fps = 0.0;
    // 返回距上一次 tick 的毫秒数；framerate>0 时限制帧率
    double tick(int framerate) {
        double freq = (double)SDL_GetPerformanceFrequency();
        uint64_t now = SDL_GetPerformanceCounter();
        if (last == 0) { last = now; fps_tick = now; }
        if (framerate > 0) {
            double target = 1000.0 / framerate;
            for (;;) {
                double el = (double)(now - last) * 1000.0 / freq;
                if (el >= target) break;
                if (target - el > 2.0) SDL_Delay((Uint32)(target - el - 1.0));
                now = SDL_GetPerformanceCounter();
            }
        }
        double ms = (double)(now - last) * 1000.0 / freq;
        last = now;
        if (++fps_count % 10 == 0) {                       // 与 pygame 一样：每 10 帧更新一次 FPS
            double span = (double)(now - fps_tick) * 1000.0 / freq;
            if (span > 0) fps = 10000.0 / span;
            fps_tick = now;
        }
        return ms;
    }
    double get_fps() const { return fps; }
};

// ============================ 界面控件（立即模式） ============================
struct UI {
    int mx = 0, my = 0;
    bool down = false, clicked = false;
    string active;
    bool has_active = false;

    void begin(const vector<Ev>& events, int mouse_x, int mouse_y, bool left_down) {
        mx = mouse_x; my = mouse_y;
        down = left_down;
        clicked = false;
        for (auto& e : events) if (e.type == E_MOUSEDOWN && e.button == 1) clicked = true;
        if (!down) has_active = false;
    }
    bool hover(const Rect& rect) const { return rect.collidepoint(mx, my); }

    bool button(Surface& s, const Rect& rect, const string& text, int size = 30, bool primary = false) {
        bool h = hover(rect);
        Color col, tc;
        if (primary) { col = h ? ACCENT_H : ACCENT; tc = rgb(8, 14, 26); }
        else { col = h ? BTN_H : BTN; tc = TXT; }
        draw_rect(s, col, rect, 0, sc(12));
        draw_text(s, text, size, tc, rect.centerx(), rect.centery());
        return h && clicked;
    }

    // int_mode=true 相当于 Python 里 step 为 int（返回整数值）；fmt 用 printf 风格
    double slider(Surface& s, const Rect& rect, const string& label, double val, double lo, double hi, double step,
                  const char* fmt, bool int_mode) {
        draw_text(s, label, 22, TXT, rect.x, rect.centery(), A_MIDLEFT);
        Rect track(I(rect.x + rect.w * 0.42), rect.centery() - sc(4), I(rect.w * 0.40), sc(8));
        Rect hit = track.inflate(sc(24), sc(34));
        if (clicked && hit.collidepoint(mx, my)) { active = label; has_active = true; }
        if (has_active && active == label && down) {
            double t = clampd((mx - track.x) / (double)std::max(1, track.w), 0.0, 1.0);
            double v = std::nearbyint((lo + t * (hi - lo)) / step) * step;
            val = std::max(lo, std::min(hi, v));
            val = int_mode ? (double)(int)val : std::round(val * 1e4) / 1e4;
        }
        double t = (val - lo) / (hi - lo);
        draw_rect(s, BTN, track, 0, sc(4));
        Rect fl = track;
        fl.w = I(track.w * t);
        draw_rect(s, ACCENT, fl, 0, sc(4));
        draw_circle(s, WHITE, track.x + I(track.w * t), track.centery(), sc(10));
        draw_text(s, int_mode ? vfmt(fmt, (int)val) : vfmt(fmt, val), 22, SUB, rect.right(), rect.centery(), A_MIDRIGHT);
        return val;
    }
    double slider_f(Surface& s, const Rect& rect, const string& label, double val, double lo, double hi, double step,
                    const char* fmt = "%.1f") {
        return slider(s, rect, label, val, lo, hi, step, fmt, false);
    }
    int slider_i(Surface& s, const Rect& rect, const string& label, int val, int lo, int hi, int step, const char* fmt) {
        return (int)slider(s, rect, label, val, lo, hi, step, fmt, true);
    }

    bool toggle(Surface& s, const Rect& rect, const string& label, bool val) {
        draw_text(s, label, 22, TXT, rect.x, rect.centery(), A_MIDLEFT);
        Rect sw(0, 0, sc(64), sc(30));
        sw.set_midright(rect.right(), rect.centery());
        draw_rect(s, val ? ACCENT : BTN, sw, 0, sc(15));
        int kx = val ? sw.right() - sc(15) : sw.x + sc(15);
        draw_circle(s, WHITE, kx, sw.centery(), sc(11));
        draw_text(s, val ? T("开") : T("关"), 20, SUB, sw.x - sc(12), rect.centery(), A_MIDRIGHT);
        if (clicked && hover(rect)) val = !val;
        return val;
    }

    int choice(Surface& s, const Rect& rect, const string& label, int idx, const vector<string>& options) {
        draw_text(s, label, 22, TXT, rect.x, rect.centery(), A_MIDLEFT);
        Rect box(0, 0, I(rect.w * 0.46), sc(38));
        box.set_midright(rect.right(), rect.centery());
        draw_rect(s, BTN, box, 0, sc(10));
        Rect left(box.x, box.y, sc(40), box.h);
        Rect right(box.right() - sc(40), box.y, sc(40), box.h);
        int a = sc(7);
        const Rect* rs[2] = {&left, &right};
        const int ds[2] = {-1, 1};
        for (int k = 0; k < 2; k++) {
            const Rect& r_ = *rs[k];
            int d = ds[k];
            if (hover(r_)) draw_rect(s, BTN_H, r_, 0, sc(10));
            int cx = r_.centerx(), cy = r_.centery();
            draw_polygon(s, TXT, {{(double)(cx + d * a), (double)cy}, {(double)(cx - d * a), (double)(cy - a)},
                                  {(double)(cx - d * a), (double)(cy + a)}});
        }
        int n = (int)options.size();
        if (n > 0) draw_text(s, options[(size_t)clampi(idx, 0, n - 1)], 22, TXT, box.centerx(), box.centery());
        if (clicked && n > 0) {
            if (left.collidepoint(mx, my)) idx = ((idx - 1) % n + n) % n;
            else if (right.collidepoint(mx, my)) idx = (idx + 1) % n;
        }
        return idx;
    }

    std::optional<Color> swatches(Surface& s, const Rect& rect, const vector<Color>& colors, const std::array<int, 3>& cur) {
        int n = (int)colors.size(), gap = sc(8);
        int size = std::min(sc(34), (rect.w - gap * (n - 1)) / n);
        std::optional<Color> picked;
        for (int i = 0; i < n; i++) {
            const Color& c = colors[(size_t)i];
            Rect r_(rect.x + i * (size + gap), rect.centery() - size / 2, size, size);
            draw_rect(s, c, r_, 0, sc(8));
            if (cur[0] == c.r && cur[1] == c.g && cur[2] == c.b)
                draw_rect(s, WHITE, r_.inflate(sc(8), sc(8)), 3, sc(10));
            if (clicked && r_.collidepoint(mx, my)) picked = c;
        }
        return picked;
    }
};

// ============================ 应用 ============================
struct Ptr {                                                // 按住的手指 / 鼠标
    enum Role { LOOK, STICK, BTNP } role = LOOK;
    string name;
    double x = 0, y = 0;
};
struct BgBall { double x, y, z, r; Color col; };
struct Result {
    string mode, name;
    int score = 0;
    bool is_new = false;
    int best = 0;
    struct Row { string label, fmt; bool is_int; double v; };
    vector<Row> rows;
    int diff = 1;
};

struct App {
    Config cfg;
    string cap_lang;
    bool cap_set = false;
    string caption;
    std::unique_ptr<Sound> sound_p;
    Sound& sound;
    UI ui;
    bool touch_mode;                                       // 手机：屏幕按键 + 手指拖动转视角
    bool finger_ok = false;                                // 是否收到了真正的多点触控事件
    std::map<string, Ptr> ptrs;                            // 当前按住的手指 / 鼠标
    double look[2] = {0.0, 0.0};                           // 手指拖动累计的视角增量（像素）
    bool pause_req = false;
    bool back_to_pause = false;                            // 设置是从暂停菜单打开的吗
    double stick[2] = {0.0, 0.0};                          // 移动轮盘当前偏移（-1~1，屏幕坐标：y 向下为正）
    std::unique_ptr<Game> lay_game;
    string lay_sel = "fire";
    bool lay_drag_on = false;
    double lay_drag[4] = {0, 0, 0, 0};
    Clock clock;
    int dw = 1920, dh = 1080;
    vector<std::pair<int, int>> win_sizes;
    Camera cam_bg;
    vector<BgBall> bg_balls;
    string state = "menu";
    int tab = 0;
    std::unique_ptr<Game> game;
    std::unique_ptr<Surface> snap;
    Result result;
    double t = 0.0, dt = 0.016;
    bool has_applied = false;
    bool applied_full = false;
    int applied_idx = 0;
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex = nullptr;
    Surface screen;
    int W = 0, H = 0;
    int rel_x = 0, rel_y = 0;                              // pygame.mouse.get_rel() 的累计量

    App() : cfg(load_config()), sound_p(new Sound()), sound(*sound_p), touch_mode(is_android()) {
        set_lang();
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { dw = dm.w; dh = dm.h; }
        const std::pair<int, int> sizes[] = {{1024, 576}, {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}};
        for (auto& z : sizes) if (z.first <= dw && z.second <= dh - 40) win_sizes.push_back(z);
        if (win_sizes.empty()) win_sizes.push_back({1024, 576});
        for (int i = 0; i < 12; i++) {
            double yw = uniform(0, 360);
            double pt = uniform(-20, 20);
            Vec3 d = dir_angles(yw, pt);
            double r = uniform(1.0, 2.0);
            Color col = PALETTE[(size_t)(g_rng() % (PALETTE.size() - 1))];
            bg_balls.push_back({d[0] * 28, d[1] * 28, d[2] * 28, r, col});
        }
        apply_display();
    }

    // ---------- 语言 ----------
    void set_caption(const string& c) {
        caption = c;
        if (win) SDL_SetWindowTitle(win, caption.c_str());
    }
    void set_lang() {
        LANG = current_lang(cfg);
        if (!cap_set || cap_lang != LANG) {
            cap_set = true;
            cap_lang = LANG;
            set_caption(T(TITLE));
        }
    }
    vector<string> avail_langs() const {
        if (CJK_OK) return {"zh", "tw", "en"};
        return {"en"};
    }

    // ---------- 显示 ----------
    void apply_display() {
        int idx = clampi(cfg.win_idx, 0, (int)win_sizes.size() - 1);
        if (tex) { SDL_DestroyTexture(tex); tex = nullptr; }
        if (ren) { SDL_DestroyRenderer(ren); ren = nullptr; }
        if (win) { SDL_DestroyWindow(win); win = nullptr; }
        bool full = cfg.fullscreen || is_android();
        Uint32 flags = SDL_WINDOW_SHOWN | (full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0u);
        int ww = full ? dw : win_sizes[(size_t)idx].first, wh = full ? dh : win_sizes[(size_t)idx].second;
        win = SDL_CreateWindow(caption.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, flags);
        if (!win) { std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); std::exit(1); }
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
        if (!ren) { std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError()); std::exit(1); }
        SDL_GetRendererOutputSize(ren, &W, &H);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
        screen = Surface(W, H, false);
        has_applied = true;
        applied_full = cfg.fullscreen;
        applied_idx = idx;
        SCALE = H / 720.0;
        cam_bg.update_proj(W, H, 90);
    }
    void flip() {                                          // pygame.display.flip()
        SDL_UpdateTexture(tex, nullptr, screen.px, screen.pitch * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    }
    void set_mouse_capture(bool on) {
        if (touch_mode) return;
        SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE);
        SDL_SetWindowGrab(win, on ? SDL_TRUE : SDL_FALSE);
        SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
        if (on) { rel_x = rel_y = 0; }
    }
    void get_rel(double& dx, double& dy) { dx = rel_x; dy = rel_y; rel_x = rel_y = 0; }

    void make_snap() {
        snap.reset(new Surface(screen.copy()));
        dim_surface(*snap, 150);
    }

    void draw_bg() {
        const Theme& th = THEMES[(size_t)std::min(cfg.theme, (int)THEMES.size() - 1)];
        Surface& s = screen;
        fill(s, th.bg);
        cam_bg.yaw = pymod(t * 5, 360.0);
        cam_bg.pitch = 8 * std::sin(t * 0.4);
        cam_bg.prep();
        draw_room(s, cam_bg, th.grid, th.front);
        vector<Item> items;
        for (auto& b : bg_balls) {
            auto pr = cam_bg.project(b.x, b.y, b.z);
            if (pr) items.push_back({(*pr)[2], (*pr)[0], (*pr)[1], b.r * cam_bg.f / (*pr)[2], b.col});
        }
        sort_items_far_first(items);
        for (auto& it : items) draw_ball(s, it.x, it.y, it.r, it.col);
    }

    // ---------- 事件 ----------
    vector<Ev> pump_events() {
        vector<Ev> out;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            Ev ev;
            switch (e.type) {
                case SDL_QUIT: ev.type = E_QUIT; out.push_back(ev); break;
                case SDL_KEYDOWN:
                    if (e.key.repeat) break;
                    ev.type = E_KEYDOWN; ev.key = (int)e.key.keysym.sym; out.push_back(ev); break;
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                    ev.type = e.type == SDL_MOUSEBUTTONDOWN ? E_MOUSEDOWN : E_MOUSEUP;
                    ev.button = e.button.button; ev.x = e.button.x; ev.y = e.button.y;
                    ev.touch = e.button.which == SDL_TOUCH_MOUSEID;
                    out.push_back(ev);
                    break;
                case SDL_MOUSEMOTION:
                    ev.type = E_MOUSEMOTION; ev.x = e.motion.x; ev.y = e.motion.y;
                    ev.touch = e.motion.which == SDL_TOUCH_MOUSEID;
                    if (!ev.touch) { rel_x += e.motion.xrel; rel_y += e.motion.yrel; }
                    out.push_back(ev);
                    break;
                case SDL_FINGERDOWN:
                case SDL_FINGERMOTION:
                case SDL_FINGERUP:
                    ev.type = e.type == SDL_FINGERDOWN ? E_FINGERDOWN : (e.type == SDL_FINGERMOTION ? E_FINGERMOTION : E_FINGERUP);
                    ev.finger = (long long)e.tfinger.fingerId; ev.fx = e.tfinger.x; ev.fy = e.tfinger.y;
                    out.push_back(ev);
                    break;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) { ev.type = E_FOCUSLOST; out.push_back(ev); }
                    break;
                default: break;
            }
        }
        return out;
    }

    // ---------- 主循环 ----------
    void run() { for (;;) frame(); }

    void frame() {
        int fps = cfg.fps;
        dt = std::min(clock.tick(fps > 0 ? fps : 0) / 1000.0, 0.05);
        t += dt;
        set_lang();
        vector<Ev> events = pump_events();
        for (auto& e : events) {
            if (e.type == E_QUIT) quit();
            if (e.type == E_FINGERDOWN || e.type == E_FINGERMOTION) touch_mode = finger_ok = true;
            else if ((e.type == E_MOUSEDOWN || e.type == E_MOUSEMOTION) && e.touch) touch_mode = true;
            if (e.type == E_KEYDOWN && e.key == SDLK_F11 && state != "playing") cfg.fullscreen = !cfg.fullscreen;
        }
        int mx = 0, my = 0;
        Uint32 mb = SDL_GetMouseState(&mx, &my);
        ui.begin(events, mx, my, (mb & SDL_BUTTON_LMASK) != 0);
        int idx = clampi(cfg.win_idx, 0, (int)win_sizes.size() - 1);
        if (!has_applied || cfg.fullscreen != applied_full || idx != applied_idx) apply_display();
        if (state == "menu") st_menu(events);
        else if (state == "modes") st_modes(events);
        else if (state == "playing") st_playing(events);
        else if (state == "paused") st_paused(events);
        else if (state == "result") st_result(events);
        else if (state == "about") st_about(events);
        else if (state == "settings") st_settings(events);
        else if (state == "layout") st_layout(events);
        flip();
    }

    [[noreturn]] void quit() {
        save_config(cfg);
        if (tex) SDL_DestroyTexture(tex);
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        sound_p.reset();
        for (auto& f : g_fonts) if (f.second) TTF_CloseFont(f.second);
        TTF_Quit();
        SDL_Quit();
        std::exit(0);
    }

    bool esc(const vector<Ev>& events) const {
        for (auto& e : events)
            if (e.type == E_KEYDOWN && (e.key == SDLK_ESCAPE || e.key == SDLK_AC_BACK)) return true;
        return false;
    }

    // ---------- 主页 ----------
    void st_menu(const vector<Ev>&) {
        Surface& s = screen;
        draw_bg();
        draw_text(s, T(TITLE), 92, WHITE, W / 2, I(H * 0.20), A_CENTER, true);
        draw_text(s, T("定位 · 瞄准 · 跟枪"), 28, ACCENT, W / 2, I(H * 0.20) + sc(72), A_CENTER, true);
        const std::pair<const char*, const char*> items[] = {{"开始", "modes"}, {"设置", "settings"}, {"开发人员", "about"}, {"退出", ""}};
        for (int i = 0; i < 4; i++) {
            Rect r(0, 0, sc(340), sc(62));
            r.set_center(W / 2, I(H * 0.42) + i * sc(80));
            if (ui.button(s, r, T(items[i].first), 32, i == 0)) {
                if (string(items[i].second).empty()) quit();
                state = items[i].second;
            }
        }
        if (CJK_OK) {                                      // 右下角语言切换
            vector<string> langs = avail_langs();
            Rect lb(0, 0, sc(190), sc(46));
            lb.set_bottomright(W - sc(24), H - sc(24));
            if (ui.button(s, lb, LANG_NAMES.at(LANG), 24)) {
                int cur = 0;
                for (size_t i = 0; i < langs.size(); i++) if (langs[i] == LANG) cur = (int)i;
                cfg.lang = langs[(size_t)((cur + 1) % (int)langs.size())];
            }
        } else {
            draw_text(s, "No Chinese font found - showing English.", 20, YELLOW, W / 2, H - sc(52), A_CENTER, true);
            draw_text(s, "Put any CJK .ttf/.otf font file (e.g. NotoSansSC) next to this program to enable Chinese.",
                      18, SUB, W / 2, H - sc(26), A_CENTER, true);
        }
    }

    // ---------- 模式选择 ----------
    void draw_mode_icon(const string& m, int cx, int cy) {
        Surface& s = screen;
        Color col = rgb(cfg.ball_color[0], cfg.ball_color[1], cfg.ball_color[2]);
        if (m == "flick") {
            draw_line(s, SUB, cx - sc(46), cy + sc(20), cx + sc(46), cy - sc(20), 2);
            draw_ball(s, cx - sc(70), cy + sc(24), sc(20), col);
            draw_ball(s, cx + sc(70), cy - sc(24), sc(20), col);
        } else if (m == "move") {
            for (int i = 0; i < 3; i++) {                  // 残影 + 箭头：表示横移
                Color c2 = lerp_color(PANEL, col, 0.25 + 0.25 * i);
                draw_ball(s, cx - sc(60) + i * sc(30), cy - sc(8), sc(12 + 3 * i), c2);
            }
            draw_ball(s, cx + sc(36), cy - sc(8), sc(22), col);
            int y0 = cy + sc(32);
            draw_line(s, SUB, cx - sc(70), y0, cx + sc(70), y0, 2);
            draw_polygon(s, SUB, {{(double)(cx + sc(78)), (double)y0}, {(double)(cx + sc(64)), (double)(y0 - sc(7))},
                                  {(double)(cx + sc(64)), (double)(y0 + sc(7))}});
        } else if (m == "multi") {
            const int dd[3][3] = {{-50, 18, 1}, {46, 14, -1}, {0, -28, 1}};
            for (auto& q : dd) {
                int dx = q[0], dy = q[1], ax = q[2];
                draw_ball(s, cx + sc(dx), cy + sc(dy), sc(16), col);
                int x1 = cx + sc(dx) + ax * sc(26), y1 = cy + sc(dy);
                draw_line(s, SUB, cx + sc(dx) + ax * sc(18), y1, x1 + ax * sc(10), y1, 2);
            }
        } else if (m == "precision") {
            const int dd[3][2] = {{-44, 14}, {40, 18}, {0, -26}};
            for (auto& q : dd) draw_ball(s, cx + sc(q[0]), cy + sc(q[1]), sc(14), col);
            draw_circle(s, WHITE, cx, cy, sc(5), 2);
        } else {
            for (int i = 0; i < 4; i++) {
                Color c = lerp_color(PANEL, col, 0.2 + 0.2 * i);
                draw_ball(s, cx - sc(66) + i * sc(24), cy + sc(14) - i * sc(8), sc(10 + 4 * i), c);
            }
            draw_ball(s, cx + sc(34), cy - sc(16), sc(26), col);
        }
    }

    void st_modes(const vector<Ev>& events) {
        Surface& s = screen;
        if (esc(events)) { state = "menu"; return; }
        draw_bg();
        draw_text(s, T("选择训练模式"), 52, WHITE, W / 2, I(H * 0.11), A_CENTER, true);
        int n_m = (int)MODE_ORDER.size(), gap = sc(24), ch = sc(380);
        int cw = std::min(sc(320), (W - sc(60) - (n_m - 1) * gap) / n_m);
        int x0 = (W - (n_m * cw + (n_m - 1) * gap)) / 2, y = I(H * 0.20);
        for (int i = 0; i < n_m; i++) {
            const string& m = MODE_ORDER[(size_t)i];
            Rect r(x0 + i * (cw + gap), y, cw, ch);
            bool hov = ui.hover(r);
            draw_rect(s, hov ? BTN_H : PANEL, r, 0, sc(18));
            draw_rect(s, hov ? ACCENT : rgb(60, 70, 100), r, 2, sc(18));
            const ModeInfo& mi = mode_info(m);
            draw_mode_icon(m, r.centerx(), r.y + sc(95));
            draw_text(s, T(mi.name), cw >= sc(310) ? 38 : (cw >= sc(260) ? 30 : 26), TXT, r.centerx(), r.y + sc(190));
            draw_wrapped(s, T(mi.desc), cw >= sc(310) ? 22 : 20, SUB, Rect(r.x + sc(20), r.y + sc(228), r.w - sc(40), sc(100)));
            draw_text(s, vfmt(T("最高分：%d").c_str(), cfg.best[bkey(m, cfg)]), 24, YELLOW, r.centerx(), r.bottom() - sc(34));
            if (ui.clicked && hov) { start_game(m); return; }
        }
        int dw_ = sc(150), dh_ = sc(46);                   // 难度：简单 / 正常 / 困难（所有模式通用）
        int dy_ = y + ch + sc(34);
        int dx0 = W / 2 - (3 * dw_ + 2 * sc(14)) / 2;
        draw_text(s, T("难度"), 26, SUB, dx0 - sc(14), dy_, A_MIDRIGHT);
        for (int i = 0; i < 3; i++) {
            if (ui.button(s, Rect(dx0 + i * (dw_ + sc(14)), dy_ - dh_ / 2, dw_, dh_), T(DIFFS[i].name), 24, i == cfg.diff))
                cfg.diff = i;
        }
        Rect b(0, 0, sc(240), sc(56));
        b.set_center(W / 2, y + ch + sc(112));
        if (ui.button(s, b, T("返回"), 28)) state = "menu";
    }

    // ---------- 游戏流程 ----------
    void start_game(const string& mode) {
        game.reset(new Game(mode, cfg, sound, W, H));
        ptrs.clear();
        look[0] = look[1] = 0.0;
        stick[0] = stick[1] = 0.0;
        state = "playing";
        set_mouse_capture(true);
    }

    // ---- 手指 / 触屏指针 ----
    void set_stick(double x, double y) {
        BtnGeom g = btn_geom(cfg, "stick", W, H);
        double dx = (x - g.x) / g.r, dy = (y - g.y) / g.r;
        double n = std::hypot(dx, dy);
        if (n > 1.0) { dx /= n; dy /= n; n = 1.0; }
        if (n < 0.12) { stick[0] = stick[1] = 0.0; }
        else { stick[0] = dx; stick[1] = dy; }
    }
    void ptr_down(const string& pid, double x, double y) {
        Game* g = game.get();
        vector<string> names = active_buttons(cfg, true);
        for (auto it = names.rbegin(); it != names.rend(); ++it) {
            const string& name = *it;
            BtnGeom bg = btn_geom(cfg, name, W, H);
            double reach = name == "stick" ? 1.5 : 1.2;   // 判定范围比外观大一圈，更好按
            if (std::hypot(x - bg.x, y - bg.y) <= bg.r * reach) {
                if (name == "stick") {
                    Ptr p; p.role = Ptr::STICK;
                    ptrs[pid] = p;
                    set_stick(x, y);
                    return;
                }
                Ptr p; p.role = Ptr::BTNP; p.name = name;
                ptrs[pid] = p;
                if (name == "fire") g->on_click();
                else if (name == "pause") pause_req = true;
                return;
            }
        }
        Ptr p; p.role = Ptr::LOOK; p.x = x; p.y = y;
        ptrs[pid] = p;
    }
    void ptr_move(const string& pid, double x, double y) {
        auto it = ptrs.find(pid);
        if (it == ptrs.end()) return;
        Ptr& p = it->second;
        if (p.role == Ptr::LOOK) {
            look[0] += x - p.x;
            look[1] += y - p.y;
            p.x = x; p.y = y;
        } else if (p.role == Ptr::STICK) {
            set_stick(x, y);
        }
    }
    void ptr_up(const string& pid) {
        auto it = ptrs.find(pid);
        if (it == ptrs.end()) return;
        Ptr::Role role = it->second.role;
        ptrs.erase(it);
        if (role == Ptr::STICK) stick[0] = stick[1] = 0.0;
    }

    void st_playing(const vector<Ev>& events) {
        Game* g = game.get();
        g->touch_mode = touch_mode;
        pause_req = false;
        for (auto& e : events) {
            if ((e.type == E_KEYDOWN && (e.key == SDLK_ESCAPE || e.key == SDLK_AC_BACK)) || e.type == E_FOCUSLOST) {
                pause();
                return;
            }
            if (e.type == E_FINGERDOWN) ptr_down("f" + std::to_string(e.finger), e.fx * W, e.fy * H);
            else if (e.type == E_FINGERMOTION) ptr_move("f" + std::to_string(e.finger), e.fx * W, e.fy * H);
            else if (e.type == E_FINGERUP) ptr_up("f" + std::to_string(e.finger));
            else if (e.type == E_MOUSEDOWN && e.button == 1) {
                if (e.touch) {                              // 触屏合成的鼠标事件：有真多点触控时忽略
                    if (!finger_ok) ptr_down("m", e.x, e.y);
                } else if (touch_mode) ptr_down("m", e.x, e.y);
                else g->on_click();
            } else if (e.type == E_MOUSEMOTION) {
                if (ptrs.count("m") && (!e.touch || !finger_ok)) ptr_move("m", e.x, e.y);
            } else if (e.type == E_MOUSEUP && e.button == 1) ptr_up("m");
        }
        if (pause_req) { pause(); return; }
        double dx, dy;
        bool firing;
        std::set<string> pressed;
        if (touch_mode) {
            dx = look[0]; dy = look[1];
            look[0] = look[1] = 0.0;
            for (auto& kv : ptrs) if (kv.second.role == Ptr::BTNP) pressed.insert(kv.second.name);
            firing = pressed.count("fire") > 0;
        } else {
            get_rel(dx, dy);
            firing = (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
            if (firing) pressed.insert("fire");
        }
        g->pressed = pressed;
        if (cfg.move_on) {                                  // 移动输入：手机用轮盘，电脑用 WASD
            if (touch_mode) {
                g->move_in = {stick[0], -stick[1]};
            } else {
                const Uint8* k = SDL_GetKeyboardState(nullptr);
                g->move_in = {(double)k[SDL_SCANCODE_D] - (double)k[SDL_SCANCODE_A],
                              (double)k[SDL_SCANCODE_W] - (double)k[SDL_SCANCODE_S]};
            }
        } else {
            g->move_in = {0.0, 0.0};
        }
        g->stick = touch_mode ? std::make_pair(stick[0], stick[1]) : std::make_pair(0.0, 0.0);
        g->update(dt, dx, dy, firing);
        g->draw(screen, clock.get_fps());
        if (g->finished) finish();
    }

    void pause() {
        ptrs.clear();
        look[0] = look[1] = 0.0;
        stick[0] = stick[1] = 0.0;
        game->move_in = {0.0, 0.0};
        game->draw(screen, clock.get_fps());
        make_snap();
        set_mouse_capture(false);
        state = "paused";
    }

    void st_paused(const vector<Ev>& events) {
        Surface& s = screen;
        if (esc(events)) {
            set_mouse_capture(true);
            ptrs.clear();
            state = "playing";
            return;
        }
        blit(s, *snap, 0, 0);
        draw_text(s, T("已暂停"), 70, WHITE, W / 2, I(H * 0.22), A_CENTER, true);
        int sw_ = std::min(sc(520), W - sc(40));
        int sy0 = I(H * 0.40) + 4 * sc(78) + sc(6);
        if (touch_mode) {
            cfg.touch_sens = ui.slider_f(s, Rect((W - sw_) / 2, sy0, sw_, sc(44)), T("滑屏灵敏度"), cfg.touch_sens, 0.1, 5.0, 0.1, "%.1fx");
        } else {
            cfg.sens = ui.slider_f(s, Rect((W - sw_) / 2, sy0, sw_, sc(44)), T("鼠标灵敏度"), cfg.sens, 0.1, 10.0, 0.1);
        }
        const char* labels[4] = {"继续", "重新开始", "设置", "返回主页"};
        for (int i = 0; i < 4; i++) {
            Rect r(0, 0, sc(320), sc(60));
            r.set_center(W / 2, I(H * 0.40) + i * sc(78));
            if (ui.button(s, r, T(labels[i]), 30, i == 0)) {
                if (i == 0) {
                    set_mouse_capture(true);
                    ptrs.clear();
                    state = "playing";
                } else if (i == 1) {
                    start_game(game->mode);
                } else if (i == 2) {
                    back_to_pause = true;                  // 设置里点返回 / ESC 会回到暂停菜单
                    state = "settings";
                } else {
                    state = "menu";
                }
                return;
            }
        }
    }

    void finish() {
        Game* g = game.get();
        ptrs.clear();
        stick[0] = stick[1] = 0.0;
        set_mouse_capture(false);
        int score = I(g->score);
        string bk = bkey(g->mode, cfg);
        bool is_new = score > cfg.best[bk];
        if (is_new) cfg.best[bk] = score;
        int dur = cfg.duration;
        Result r;
        if (g->mode == "track") {
            r.rows = {{"追踪时长", "%.1f 秒", false, g->track_on},
                      {"追踪率", "%.1f%%", false, g->track_on / std::max(1, dur) * 100},
                      {"训练时长", "%d 秒", true, (double)dur}};
        } else {
            double acc = g->shots ? (double)g->hits / g->shots * 100 : 0.0;
            double avg = g->hits ? g->ttk_sum / g->hits : 0.0;
            r.rows = {{"命中数", "%d", true, (double)g->hits}, {"射击数", "%d", true, (double)g->shots},
                      {"命中率", "%.1f%%", false, acc}, {"平均击杀用时", "%.2f 秒", false, avg},
                      {"最高连击", "%d", true, (double)g->max_combo}, {"训练时长", "%d 秒", true, (double)dur}};
        }
        r.mode = g->mode;
        r.name = mode_info(g->mode).name;
        r.score = score;
        r.is_new = is_new;
        r.best = cfg.best[bk];
        r.diff = cfg.diff;
        result = r;
        save_config(cfg);
        sound.play("end", cfg);
        g->draw(screen, clock.get_fps());
        make_snap();
        state = "result";
    }

    void st_result(const vector<Ev>& events) {
        Surface& s = screen;
        if (esc(events)) { state = "modes"; return; }
        blit(s, *snap, 0, 0);
        const Result& r = result;
        Rect panel(0, 0, std::min(sc(780), W - sc(40)), std::min(sc(600), H - sc(30)));
        panel.set_center(W / 2, H / 2);
        draw_rect(s, PANEL, panel, 0, sc(20));
        draw_text(s, vfmt("%s · %s · %s", T(r.name).c_str(), T(DIFFS[clampi(r.diff, 0, 2)].name).c_str(), T("训练结束").c_str()),
                  36, TXT, panel.centerx(), panel.y + sc(46));
        draw_text(s, std::to_string(r.score), 96, ACCENT, panel.centerx(), panel.y + sc(140));
        draw_text(s, T("得分"), 24, SUB, panel.centerx(), panel.y + sc(200));
        if (r.is_new) draw_text(s, T("新纪录！"), 32, YELLOW, panel.centerx(), panel.y + sc(244));
        else draw_text(s, vfmt(T("最高分：%d").c_str(), r.best), 24, SUB, panel.centerx(), panel.y + sc(244));
        int colw = (panel.w - sc(140)) / 2;
        for (size_t i = 0; i < r.rows.size(); i++) {
            const Result::Row& row = r.rows[i];
            int x0 = panel.x + sc(70) + (int)(i % 2) * colw;
            int y = panel.y + sc(300) + (int)(i / 2) * sc(44);
            draw_text(s, T(row.label), 24, SUB, x0, y, A_MIDLEFT);
            string f = T(row.fmt);
            string val = row.is_int ? vfmt(f.c_str(), (int)row.v) : vfmt(f.c_str(), row.v);
            draw_text(s, val, 26, TXT, x0 + colw - sc(30), y, A_MIDRIGHT);
        }
        int bw = (panel.w - sc(100)) / 3;
        const char* txts[3] = {"再来一次", "模式选择", "返回主页"};
        for (int i = 0; i < 3; i++) {
            Rect b(panel.x + sc(30) + i * (bw + sc(20)), panel.bottom() - sc(80), bw, sc(52));
            if (ui.button(s, b, T(txts[i]), 26, i == 0)) {
                if (i == 0) start_game(r.mode);
                else state = i == 1 ? "modes" : "menu";
                return;
            }
        }
    }

    // ---------- 开发人员 ----------
    void st_about(const vector<Ev>& events) {
        Surface& s = screen;
        if (esc(events)) { state = "menu"; return; }
        draw_bg();
        Rect panel(0, 0, std::min(sc(720), W - sc(40)), sc(470));
        panel.set_center(W / 2, H / 2);
        draw_rect(s, PANEL, panel, 0, sc(20));
        draw_text(s, T("开发人员"), 40, SUB, panel.centerx(), panel.y + sc(60));
        draw_text(s, "Claude", 72, ACCENT, panel.centerx(), panel.y + sc(135));
        draw_text(s, T("黄明康"), 72, ACCENT, panel.centerx(), panel.y + sc(215));
        string lines[3] = {vfmt(T("游戏名称：%s").c_str(), T(TITLE).c_str()), vfmt(T("版本：%s").c_str(), "1.0"),
                           vfmt(T("技术：%s").c_str(), "C++ + SDL2")};
        for (int i = 0; i < 3; i++) draw_text(s, lines[i], 24, TXT, panel.centerx(), panel.y + sc(293) + i * sc(34));
        Rect b(0, 0, sc(240), sc(54));
        b.set_center(panel.centerx(), panel.bottom() - sc(50));
        if (ui.button(s, b, T("返回"), 28, true)) state = "menu";
    }

    // ---------- 设置 ----------
    // ---------- 键位布局编辑：拖动按键 / 步枪，选中后调大小 ----------
    void open_layout() {
        lay_game.reset(new Game("flick", cfg, sound, W, H));
        Game* g = lay_game.get();
        g->running = true;
        g->countdown = 0.0;
        g->time_left = 60.0;
        g->touch_mode = true;
        g->edit = true;
        lay_sel = "fire";
        lay_drag_on = false;
        state = "layout";
    }

    void st_layout(const vector<Ev>& events) {
        Surface& s = screen;
        Game* g = lay_game.get();
        auto& lay = cfg.layout;
        if (esc(events)) { save_config(cfg); state = "settings"; return; }
        g->cam.update_proj(W, H, cfg.fov);
        g->draw(s, 0);
        vector<string> names = active_buttons(cfg, true, true);
        // 控制面板
        Rect P(0, 0, std::min(sc(780), W - sc(30)), sc(142));
        P.set_midtop(W / 2, sc(78));
        blit(s, *panel_surface(P.w, P.h, sc(16), rgba(12, 16, 28, 225)), P.x, P.y);
        // 点选 / 拖动
        if (ui.clicked && !P.collidepoint(ui.mx, ui.my)) {
            string pick;
            bool has = false;
            for (auto it = names.rbegin(); it != names.rend(); ++it) {
                BtnGeom bg = btn_geom(cfg, *it, W, H);
                if (std::hypot(ui.mx - bg.x, ui.my - bg.y) <= bg.r * 1.15) { pick = *it; has = true; break; }
            }
            if (!has && g->rifle_bbox && g->rifle_bbox->nonzero() && g->rifle_bbox->collidepoint(ui.mx, ui.my)) {
                pick = "rifle";
                has = true;
            }
            if (has) {
                lay_sel = pick;
                lay_drag_on = true;
                lay_drag[0] = ui.mx; lay_drag[1] = ui.my;
                lay_drag[2] = lay[pick][0]; lay_drag[3] = lay[pick][1];
            }
        }
        if (lay_drag_on && ui.down) {
            double nx = lay_drag[2] + (ui.mx - lay_drag[0]) / W, ny = lay_drag[3] + (ui.my - lay_drag[1]) / H;
            Vec3& q = lay[lay_sel];
            if (lay_sel == "rifle") {
                q[0] = clampd(nx, -0.45, 0.45);
                q[1] = clampd(ny, -0.45, 0.45);
            } else {
                q[0] = clampd(nx, 0.03, 0.97);
                q[1] = clampd(ny, 0.05, 0.95);
            }
        } else if (!ui.down) {
            lay_drag_on = false;
        }
        if (std::find(names.begin(), names.end(), lay_sel) == names.end() && lay_sel != "rifle") lay_sel = "fire";
        const string sel = lay_sel;
        // 名称标签 + 选中框
        for (auto& n : names) {
            BtnGeom bg = btn_geom(cfg, n, W, H);
            draw_text(s, T(LAYOUT_NAMES.at(n)), 18, n == sel ? YELLOW : SUB, bg.x, bg.y - bg.r - sc(16), A_CENTER, true);
            if (n == sel) draw_circle(s, YELLOW, I(bg.x), I(bg.y), I(bg.r * 1.15), 3);
        }
        if (g->rifle_bbox) {
            const Rect& bb = *g->rifle_bbox;
            if (sel == "rifle") draw_rect(s, YELLOW, bb.inflate(sc(10), sc(10)), 3, sc(8));
            draw_text(s, T(LAYOUT_NAMES.at("rifle")), 18, sel == "rifle" ? YELLOW : SUB, bb.centerx(), bb.y - sc(12), A_CENTER, true);
        }
        draw_text(s, T("拖动按键或步枪调整位置；选中后可调大小"), 20, SUB, P.centerx(), P.y + sc(20));
        double hi = sel == "rifle" ? 1.8 : 2.2;
        lay[sel][2] = ui.slider_f(s, Rect(P.x + sc(24), P.y + sc(34), P.w - sc(48), sc(40)),
                                  T(LAYOUT_NAMES.at(sel)) + " " + T("大小"), lay[sel][2], 0.5, hi, 0.05, "%.2fx");
        int bw = (P.w - sc(24) * 2 - sc(14) * 2) / 3, bh = sc(40);
        int by = P.bottom() - bh - sc(12);
        const char* txts[3] = {"重置此项", "全部重置", "完成"};
        for (int i = 0; i < 3; i++) {
            Rect r(P.x + sc(24) + i * (bw + sc(14)), by, bw, bh);
            if (ui.button(s, r, T(txts[i]), 22, i == 2)) {
                if (i == 0) lay[sel] = DEFAULT_LAYOUT.at(sel);
                else if (i == 1) cfg.layout = DEFAULT_LAYOUT;
                else { save_config(cfg); state = "settings"; return; }
            }
        }
    }

    void leave_settings() {
        save_config(cfg);
        if (back_to_pause && game) {
            back_to_pause = false;
            game->cam.update_proj(W, H, cfg.fov);          // 视野 / 窗口可能改了
            if (snap && (snap->w != W || snap->h != H)) snap.reset(new Surface(smoothscale(*snap, W, H)));
            state = "paused";
        } else {
            state = "menu";
        }
    }

    void reset_cfg() {
        auto best = cfg.best;
        cfg = Config();
        cfg.best = best;
    }

    void st_settings(const vector<Ev>& events) {
        Surface& s = screen;
        if (esc(events)) { leave_settings(); return; }
        draw_bg();
        Rect panel(0, 0, std::min(sc(1060), W - sc(40)), H - sc(30));
        panel.set_center(W / 2, H / 2);
        draw_rect(s, PANEL, panel, 0, sc(20));
        draw_text(s, T("设置"), 40, TXT, panel.centerx(), panel.y + sc(40));
        const char* tabs[7] = {"通用", "画面", "小球", "准心", "界面", "触屏", "声音"};
        double tw = (panel.w - sc(60)) / 7.0;
        for (int i = 0; i < 7; i++) {
            Rect r(I(panel.x + sc(30) + i * tw), panel.y + sc(78), I(tw - sc(10)), sc(44));
            if (ui.button(s, r, T(tabs[i]), 22, i == tab)) tab = i;
        }
        Rect content(panel.x + sc(40), panel.y + sc(140), panel.w - sc(80), panel.h - sc(230));
        int rh = sc(54);
        int lw = I(content.w * 0.56);
        int rx = content.x + I(content.w * 0.62);
        int rw = content.right() - rx;
        auto row = [&](int i) { return Rect(content.x, content.y + i * rh, lw, rh - sc(6)); };
        const Theme& th = THEMES[(size_t)std::min(cfg.theme, (int)THEMES.size() - 1)];

        if (tab == 0) {
            vector<string> langs = avail_langs();
            int cur = 0;
            for (size_t i = 0; i < langs.size(); i++) if (langs[i] == LANG) { cur = (int)i; break; }
            vector<string> lnames;
            for (auto& l : langs) lnames.push_back(LANG_NAMES.at(l));
            int li = ui.choice(s, row(0), T("语言"), cur, lnames);
            cfg.lang = langs[(size_t)li];
            cfg.sens = ui.slider_f(s, row(1), T("鼠标灵敏度"), cfg.sens, 0.1, 10.0, 0.1);
            vector<string> dnames;
            for (int d : DURATIONS) dnames.push_back(vfmt(T("%d 秒").c_str(), d));
            int i = ui.choice(s, row(2), T("游戏时长"), idx_of_arr(DURATIONS, N_DURATIONS, cfg.duration, 1), dnames);
            cfg.duration = DURATIONS[i];
            cfg.hold_track = ui.toggle(s, row(3), T("跟枪需按住左键"), cfg.hold_track);
            cfg.show_hud = ui.toggle(s, row(4), T("显示得分 / 命中率"), cfg.show_hud);
            cfg.show_fps = ui.toggle(s, row(5), T("显示 FPS"), cfg.show_fps);
            cfg.particles = ui.toggle(s, row(6), T("命中粒子特效"), cfg.particles);
            draw_text(s, T("小提示"), 26, ACCENT, rx, content.y + sc(10), A_MIDLEFT);
            const char* tips[8] = {"· 灵敏度越高，视角转动越快", "· 新手建议先用较低灵敏度，", "  准度稳定后再逐步提高",
                                   "· 跟枪时关闭「需按住左键」", "  可以只练准心移动", "· 游戏中按 ESC 可暂停",
                                   "· 手机上手指拖屏幕转视角", "· 手机上点开火键射击"};
            for (int k = 0; k < 8; k++) draw_text(s, T(tips[k]), 22, SUB, rx, content.y + sc(52) + k * sc(34), A_MIDLEFT);
            if (!CJK_OK) {
                const char* en[4] = {"No Chinese font found.", "Copy a CJK .ttf/.otf font", "next to this program and",
                                     "restart to enable Chinese."};
                for (int k = 0; k < 4; k++) draw_text(s, en[k], 20, YELLOW, rx, content.y + sc(290) + k * sc(28), A_MIDLEFT);
            }
        } else if (tab == 1) {
            vector<string> fnames;
            for (int f : FPS_OPTIONS) fnames.push_back(f == 0 ? T("不限制") : std::to_string(f));
            int i = ui.choice(s, row(0), T("FPS 帧率上限"), idx_of_arr(FPS_OPTIONS, N_FPS_OPTIONS, cfg.fps, 4), fnames);
            cfg.fps = FPS_OPTIONS[i];
            cfg.fov = ui.slider_i(s, row(1), T("视野 FOV"), cfg.fov, 60, 120, 1, "%d°");
            cfg.fullscreen = ui.toggle(s, row(2), T("全屏（F11）"), cfg.fullscreen);
            vector<string> wnames;
            for (auto& z : win_sizes) wnames.push_back(vfmt("%dx%d", z.first, z.second));
            cfg.win_idx = ui.choice(s, row(3), T("窗口大小"), clampi(cfg.win_idx, 0, (int)win_sizes.size() - 1), wnames);
            vector<string> tnames;
            for (auto& t_ : THEMES) tnames.push_back(T(t_.name));
            cfg.theme = ui.choice(s, row(4), T("场景主题"), std::min(cfg.theme, (int)THEMES.size() - 1), tnames);
            cfg.grid = ui.toggle(s, row(5), T("显示场景网格"), cfg.grid);
            draw_text(s, T("小提示"), 26, ACCENT, rx, content.y + sc(10), A_MIDLEFT);
            const char* tips[3] = {"· FPS 越高，画面越流畅", "· FOV 越大，视野越广", "· 修改窗口大小后立即生效"};
            for (int k = 0; k < 3; k++) draw_text(s, T(tips[k]), 22, SUB, rx, content.y + sc(52) + k * sc(34), A_MIDLEFT);
        } else if (tab == 2) {
            cfg.ball_size = ui.slider_f(s, row(0), T("小球大小"), cfg.ball_size, 0.3, 3.0, 0.1, "%.1fx");
            cfg.ball_random = ui.toggle(s, row(1), T("随机颜色"), cfg.ball_random);
            Rect r2 = row(2);
            draw_text(s, T("预设颜色"), 22, TXT, r2.x, r2.centery(), A_MIDLEFT);
            auto pick = ui.swatches(s, Rect(r2.x + I(r2.w * 0.42), r2.y, I(r2.w * 0.58), r2.h), PALETTE, cfg.ball_color);
            if (pick) cfg.ball_color = {pick->r, pick->g, pick->b};
            const string cn[3] = {T("红色 R"), T("绿色 G"), T("蓝色 B")};
            for (int k = 0; k < 3; k++)
                cfg.ball_color[(size_t)k] = ui.slider_i(s, row(3 + k), cn[k], cfg.ball_color[(size_t)k], 0, 255, 1, "%d");
            Rect box(rx, content.y, rw, std::min(content.h, sc(330)));
            draw_rect(s, th.bg, box, 0, sc(14));
            draw_rect(s, th.front, box, 2, sc(14));
            draw_text(s, T("预览"), 22, SUB, box.x + sc(16), box.y + sc(20), A_MIDLEFT);
            double rad = std::min(box.h * 0.38, sc(34) * cfg.ball_size);
            if (cfg.ball_random) {
                int k = 0;
                for (size_t pi = 0; pi < PALETTE.size() - 1; pi += 3, k++)   // PALETTE[:-1][::3]
                    draw_ball(s, box.centerx() + (k - 1) * rad * 2.4, box.centery(), rad * 0.8, PALETTE[pi]);
            } else {
                draw_ball(s, box.centerx(), box.centery(), rad, rgb(cfg.ball_color[0], cfg.ball_color[1], cfg.ball_color[2]));
            }
        } else if (tab == 3) {
            cfg.ch_on = ui.toggle(s, row(0), T("显示准心"), cfg.ch_on);
            vector<string> cnames;
            for (auto& c : CH_STYLES) cnames.push_back(T(c));
            cfg.ch_style = ui.choice(s, row(1), T("准心样式"), std::min(cfg.ch_style, (int)CH_STYLES.size() - 1), cnames);
            cfg.ch_size = ui.slider_i(s, row(2), T("准心大小"), cfg.ch_size, 2, 40, 1, "%d");
            cfg.ch_thick = ui.slider_i(s, row(3), T("准心粗细"), cfg.ch_thick, 1, 10, 1, "%d");
            cfg.ch_gap = ui.slider_i(s, row(4), T("中心间隙"), cfg.ch_gap, 0, 20, 1, "%d");
            cfg.ch_alpha = ui.slider_i(s, row(5), T("不透明度"), cfg.ch_alpha, 10, 100, 5, "%d%%");
            cfg.ch_outline = ui.toggle(s, row(6), T("黑色描边"), cfg.ch_outline);
            Rect box(rx, content.y, rw, sc(170));
            draw_rect(s, th.bg, box, 0, sc(14));
            draw_rect(s, th.front, box, 2, sc(14));
            draw_ball(s, box.centerx() + sc(90), box.centery() - sc(30), sc(16), rgb(cfg.ball_color[0], cfg.ball_color[1], cfg.ball_color[2]));
            if (cfg.ch_on) {
                auto cs = crosshair_surface(cfg);
                blit(s, *cs, box.centerx() - cs->w / 2, box.centery() - cs->h / 2);
            } else {
                draw_text(s, T("准心已关闭"), 22, SUB, box.centerx(), box.centery());
            }
            draw_text(s, T("预览"), 20, SUB, box.x + sc(14), box.y + sc(18), A_MIDLEFT);
            Rect sr(rx, content.y + sc(182), rw, sc(44));
            draw_text(s, T("预设颜色"), 20, SUB, rx, sr.y - sc(2), A_MIDLEFT);
            auto pick = ui.swatches(s, Rect(rx, sr.y + sc(10), rw, sc(40)), PALETTE, cfg.ch_color);
            if (pick) cfg.ch_color = {pick->r, pick->g, pick->b};
            const string cn[3] = {T("红色 R"), T("绿色 G"), T("蓝色 B")};
            for (int k = 0; k < 3; k++) {
                Rect rr(rx, content.y + sc(240) + k * sc(46), rw, sc(40));
                cfg.ch_color[(size_t)k] = ui.slider_i(s, rr, cn[k], cfg.ch_color[(size_t)k], 0, 255, 1, "%d");
            }
        } else if (tab == 4) {                              // 界面：抬头指示器 / 开火键 / 步枪 / 键位布局
            Vec3& lay = cfg.layout["rifle"];
            cfg.show_bar = ui.toggle(s, row(0), T("显示抬头指示器"), cfg.show_bar);
            cfg.show_fire = ui.toggle(s, row(1), T("显示开火键"), cfg.show_fire);
            cfg.show_rifle = ui.toggle(s, row(2), T("显示步枪模型"), cfg.show_rifle);
            lay[2] = ui.slider_f(s, row(3), T("步枪大小"), lay[2], 0.5, 1.8, 0.05, "%.2fx");
            lay[0] = ui.slider_f(s, row(4), T("步枪 左右位置"), lay[0], -0.3, 0.3, 0.01, "%+.2f");
            lay[1] = ui.slider_f(s, row(5), T("步枪 上下位置"), lay[1], -0.3, 0.3, 0.01, "%+.2f");
            Rect r7 = row(6);
            if (ui.button(s, Rect(r7.x, r7.y, r7.w, r7.h), T("调整键位 / 位置与大小"), 24, true)) {
                open_layout();
                return;
            }
            Rect box(rx, content.y, rw, std::min(I(rw * 9.0 / 16.0), sc(250)));
            draw_rect(s, th.bg, box, 0, sc(14));
            Surface sub = s.sub(box);
            if (cfg.show_rifle) draw_fp_rifle(sub, box.w, box.h, lay);
            else draw_text(s, T("步枪已隐藏"), 22, SUB, box.centerx(), box.centery());
            if (cfg.ch_on) {
                auto cs = crosshair_surface(cfg);
                blit(sub, *cs, box.w / 2 - cs->w / 2, box.h / 2 - cs->h / 2);
            }
            draw_rect(s, th.front, box, 2, sc(14));
            draw_text(s, T("预览"), 20, SUB, box.x + sc(14), box.y + sc(18), A_MIDLEFT);
            const char* tips[5] = {"· 抬头指示器：左边命中数，", "  右边剩余时间", "· 开火指示器：命中亮绿灯，",
                                   "  未命中亮红灯", "· 在「界面」里可调键位"};
            for (int k = 0; k < 5; k++) draw_text(s, T(tips[k]), 22, SUB, rx, box.bottom() + sc(26) + k * sc(32), A_MIDLEFT);
        } else if (tab == 5) {                              // 触屏：滑屏灵敏度 / 移动轮盘
            cfg.touch_sens = ui.slider_f(s, row(0), T("滑屏灵敏度"), cfg.touch_sens, 0.1, 5.0, 0.1, "%.1fx");
            cfg.touch_ratio = ui.slider_f(s, row(1), T("滑屏垂直比例"), cfg.touch_ratio, 0.3, 2.0, 0.1, "%.1fx");
            cfg.move_on = ui.toggle(s, row(2), T("启用移动（轮盘 / WASD）"), cfg.move_on);
            cfg.move_speed = ui.slider_f(s, row(3), T("移动速度"), cfg.move_speed, 0.3, 3.0, 0.1, "%.1fx");
            draw_text(s, T("小提示"), 26, ACCENT, rx, content.y + sc(10), A_MIDLEFT);
            const char* tips[5] = {"· 滑屏灵敏度：手指拖屏转视角的快慢", "· 左下轮盘控制人物前后左右移动", "· 电脑上用 WASD 移动",
                                   "· 轮盘位置 / 大小可在「界面」里调", "· 暂停菜单里也能实时调灵敏度"};
            for (int k = 0; k < 5; k++) draw_text(s, T(tips[k]), 20, SUB, rx, content.y + sc(52) + k * sc(32), A_MIDLEFT);
        } else {
            cfg.sound_on = ui.toggle(s, row(0), T("命中音效"), cfg.sound_on);
            cfg.volume = ui.slider_i(s, row(1), T("音量"), cfg.volume, 0, 100, 5, "%d%%");
            Rect r2 = row(2);
            if (ui.button(s, Rect(r2.x, r2.y + sc(4), sc(170), sc(44)), T("试听音效"), 24)) sound.play("hit", cfg);
            if (!sound.ok) draw_text(s, T("未检测到可用的音频设备"), 22, YELLOW, r2.x, r2.y + sc(80), A_MIDLEFT);
        }

        int by = panel.bottom() - sc(50);
        Rect b1(0, 0, sc(240), sc(50));
        b1.set_center(panel.x + sc(40) + sc(120), by);
        Rect b2(0, 0, sc(240), sc(50));
        b2.set_center(panel.right() - sc(40) - sc(120), by);
        if (ui.button(s, b1, T("恢复默认设置"), 24)) reset_cfg();
        if (ui.button(s, b2, T("保存并返回"), 26, true)) leave_settings();
    }
};

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return 1;
    }
    if (char* base = SDL_GetBasePath()) {
        HERE = base;
        SDL_free(base);
    }
#if defined(__IPHONEOS__)
    if (char* pref = SDL_GetPrefPath("AimTrainer", "AimTrainer")) {
        SAVE_FILE = string(pref) + "aim_trainer_settings.json";
        SDL_free(pref);
    } else {
        std::fprintf(stderr, "SDL_GetPrefPath failed: %s\n", SDL_GetError());
    }
#else
    SAVE_FILE = HERE + "aim_trainer_settings.json";
#endif
    build_room(ROOM_LINES, ROOM_FRONT);
    FONT_FILE = find_cjk_font();
    CJK_OK = !FONT_FILE.empty();
    if (!CJK_OK) LATIN_FONT = find_latin_font();
    if (CJK_OK) std::printf("Chinese font: %s\n", FONT_FILE.c_str());
    else std::printf("No Chinese font found - using English. Put a CJK .ttf/.otf next to this program.\n");
    App app;
    app.run();
    return 0;
}
