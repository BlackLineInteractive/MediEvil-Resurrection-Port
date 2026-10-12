#pragma once
// Runtime for the C++ that tools/transpile.py generates from Ghidra's decompilation of main.prx.
//
// The game's code runs natively, but its data stays in a flat 32-bit guest address space: main.prx is
// loaded at address 0 (the Ghidra image base, so every DAT_/FUN_ address is unchanged), the stack and
// the heap live above it. Guest pointers are 32-bit values (GP<T>); C++ code dereferences them through
// g_mem. Nothing is emulated: there is no CPU, no registers, only memory plus the Allegrex VFPU state
// that Ghidra leaves implicit (prefixes).
#include <cmath>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

namespace psp {

constexpr uint32_t kMemSize = 0x04000000;   // 64 MB guest space
extern uint8_t* g_mem;
extern uint32_t g_sp;                        // guest stack pointer (grows down)

[[noreturn]] void Fatal(const char* fmt, ...);
void Unimplemented(uint32_t addr, const char* name);   // a function the transpiler could not convert

inline uint8_t* Host(uint32_t a) { return g_mem + a; }

template <class To, class From> inline To BitCast(const From& f) {
    static_assert(sizeof(To) == sizeof(From));
    To t; std::memcpy(&t, &f, sizeof t); return t;
}

// ── Ghidra's "undefined4": 4 bytes of unknown type. Keeps bits on float <-> int moves. ──
struct U4 {
    uint32_t v = 0;
    U4() = default;
    U4(float f) : v(BitCast<uint32_t>(f)) {}
    U4(double d) : U4(float(d)) {}
    template <class I, std::enable_if_t<std::is_integral_v<I> || std::is_enum_v<I>, int> = 0>
    U4(I i) : v(uint32_t(i)) {}
    template <class P, std::enable_if_t<std::is_same_v<decltype(P::a), uint32_t>, int> = 0>
    U4(const P& p) : v(p.a) {}   // any guest pointer
    operator uint32_t() const { return v; }
    explicit operator float() const { return BitCast<float>(v); }
    explicit operator double() const { return BitCast<float>(v); }
};
static_assert(sizeof(U4) == 4);

// ── guest pointer ──
template <class T> struct GP;
template <class T> struct GSize { static constexpr uint32_t v = sizeof(T); };
template <> struct GSize<void> { static constexpr uint32_t v = 1; };

struct code {};   // Ghidra's "code": the target of a pointer is a function

template <class... A> uint64_t ICall(uint32_t addr, A... a);   // call a guest function by address

struct CodeRef {
    uint32_t a;
    template <class... A> uint64_t operator()(A... args) const { return ICall(a, args...); }
};

template <class T> struct Deref { static T& get(uint32_t a) { return *reinterpret_cast<T*>(g_mem + a); } };

template <class T> struct GP {
    uint32_t a = 0;
    GP() = default;
    template <class I, std::enable_if_t<std::is_integral_v<I>, int> = 0> GP(I v) : a(uint32_t(v)) {}
    GP(U4 v) : a(v.v) {}
    template <class U> GP(GP<U> p) : a(p.a) {}
    operator uint32_t() const { return a; }
    explicit operator bool() const { return a != 0; }
    decltype(auto) operator*() const { return Deref<T>::get(a); }
    T* operator->() const { return reinterpret_cast<T*>(g_mem + a); }
    decltype(auto) operator[](int i) const { return Deref<T>::get(a + int32_t(i) * int32_t(GSize<T>::v)); }
    GP operator+(int i) const { return GP(a + int32_t(i) * int32_t(GSize<T>::v)); }
    GP operator-(int i) const { return GP(a - int32_t(i) * int32_t(GSize<T>::v)); }
    GP operator+(uint32_t i) const { return *this + int(i); }
    GP operator-(uint32_t i) const { return *this - int(i); }
    int operator-(GP o) const { return int(int32_t(a - o.a) / int32_t(GSize<T>::v)); }
    GP& operator+=(int i) { return *this = *this + i; }
    GP& operator-=(int i) { return *this = *this - i; }
    GP& operator++() { return *this += 1; }
    GP operator++(int) { GP t = *this; *this += 1; return t; }
    GP& operator--() { return *this -= 1; }
    GP operator--(int) { GP t = *this; *this -= 1; return t; }
};
template <> struct GP<code> {
    uint32_t a = 0;
    GP() = default;
    template <class I, std::enable_if_t<std::is_integral_v<I>, int> = 0> GP(I v) : a(uint32_t(v)) {}
    GP(U4 v) : a(v.v) {}
    template <class U> explicit GP(GP<U> p) : a(p.a) {}
    operator uint32_t() const { return a; }
    CodeRef operator*() const { return {a}; }
    template <class... A> uint64_t operator()(A... args) const { return ICall(a, args...); }
};
template <class T> struct GSize<GP<T>> { static constexpr uint32_t v = 4; };
static_assert(sizeof(GP<int>) == 4);

// ── byte vectors: Ghidra's undefined1[N] values (VFPU registers) with _A_B_ field access ──
template <int N> struct VB {
    uint8_t b[N] = {};
    float& f(int i) { return reinterpret_cast<float*>(b)[i]; }
    float f(int i) const { float r; std::memcpy(&r, b + 4 * i, 4); return r; }
};
using V8 = VB<8>; using V12 = VB<12>; using V16 = VB<16>;
template <int N> struct GSize<VB<N>> { static constexpr uint32_t v = N; };

// A reference to bytes [A, A+B) of some object: x._A_B_ in Ghidra's output.
template <int B> struct Field {
    uint8_t* p;
    template <class T> Field& operator=(const T& v) {
        if constexpr (std::is_same_v<T, Field<B>>) { std::memmove(p, v.p, B); }
        else if constexpr (std::is_floating_point_v<T>) {
            if constexpr (B == 4) { float f = float(v); std::memcpy(p, &f, 4); }
            else { double d = v; std::memcpy(p, &d, B < 8 ? B : 8); }
        } else if constexpr (sizeof(T) == B) { std::memcpy(p, &v, B); }
        else { uint64_t u = uint64_t(v); std::memcpy(p, &u, B < 8 ? B : 8); }
        return *this;
    }
    Field& operator=(const Field& o) { std::memmove(p, o.p, B); return *this; }
    template <int C> Field& operator=(const Field<C>& o) { std::memmove(p, o.p, B < C ? B : C); return *this; }
    uint64_t U() const { uint64_t u = 0; std::memcpy(&u, p, B < 8 ? B : 8); return u; }
    operator U4() const { U4 u; std::memcpy(&u.v, p, 4); return u; }
    operator float() const { float f; std::memcpy(&f, p, 4); return f; }
    template <class I, std::enable_if_t<std::is_integral_v<I>, int> = 0> operator I() const { return I(U()); }
    template <class T> operator GP<T>() const { uint32_t u; std::memcpy(&u, p, 4); return GP<T>(u); }
    template <int N> operator VB<N>() const { VB<N> v; std::memcpy(v.b, p, B < N ? B : N); return v; }
};
// *GP<VB<N>>: an array lvalue in guest memory. Reads/writes as a value, decays to its address.
template <int N> struct VRef {
    uint32_t a;
    operator VB<N>() const { VB<N> v; std::memcpy(v.b, g_mem + a, N); return v; }
    template <int M> VRef& operator=(const VB<M>& v) { std::memcpy(g_mem + a, v.b, M < N ? M : N); return *this; }
    VRef& operator=(const VRef& o) { std::memmove(g_mem + a, g_mem + o.a, N); return *this; }
    template <int M> VRef& operator=(const VRef<M>& o) { std::memmove(g_mem + a, g_mem + o.a, M < N ? M : N); return *this; }
    template <int M> VRef& operator=(const Field<M>& f) { std::memmove(g_mem + a, f.p, M < N ? M : N); return *this; }
    template <class I, std::enable_if_t<std::is_integral_v<I>, int> = 0> GP<uint8_t> operator+(I i) const { return GP<uint8_t>(a + uint32_t(i)); }
    template <class T> operator GP<T>() const { return GP<T>(a); }
    operator uint32_t() const { return a; }
    uint8_t& operator[](int i) const { return g_mem[a + i]; }
};
template <int N> struct Deref<VB<N>> { static VRef<N> get(uint32_t a) { return {a}; } };
template <class T> inline uint8_t* BytesOf(T& x) { return reinterpret_cast<uint8_t*>(&x); }
template <class T> inline uint8_t* BytesOf(const T& x) { return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(&x)); }
template <int N> inline uint8_t* BytesOf(const VRef<N>& r) { return g_mem + r.a; }
template <int N> inline uint8_t* BytesOf(VRef<N>& r) { return g_mem + r.a; }
template <int A, int B, class T> Field<B> Fld(T&& x) { return {BytesOf(x) + A}; }

// ── value conversions used by VFPU ops: integers / U4 / Fields are float bit patterns ──
inline float FB(float f) { return f; }
inline float FB(double d) { return float(d); }
inline float FB(U4 u) { return BitCast<float>(u.v); }
inline float FB(Field<4> f) { return float(f); }
template <class I, std::enable_if_t<std::is_integral_v<I>, int> = 0> inline float FB(I i) { return BitCast<float>(uint32_t(i)); }
template <int N> inline V16 VQ(const VB<N>& v) { V16 r; std::memcpy(r.b, v.b, N < 16 ? N : 16); return r; }
template <int N> inline V16 VQ(Field<N> f) { V16 r; std::memcpy(r.b, f.p, N < 16 ? N : 16); return r; }
template <int N> inline V16 VQ(const VRef<N>& r) { V16 v; std::memcpy(v.b, g_mem + r.a, N < 16 ? N : 16); return v; }
template <class T> inline float FB(GP<T> p) { return BitCast<float>(p.a); }
template <int N> inline float FB(const VRef<N>& r) { float f; std::memcpy(&f, g_mem + r.a, 4); return f; }
template <class T> inline V16 VQ(GP<T> p) { V16 v; std::memcpy(v.b, g_mem + p.a, 16); return v; }   // array argument
template <size_t N> inline V16 VQ(const uint8_t (&a)[N]) { V16 v; std::memcpy(v.b, a, N < 16 ? N : 16); return v; }
inline V16 VQ(U4 u) { V16 r; std::memcpy(r.b, &u.v, 4); return r; }
inline V16 VQ(float f) { V16 r; r.f(0) = f; return r; }
template <class I, std::enable_if_t<std::is_integral_v<I>, int> = 0> inline V16 VQ(I i) { V16 r; uint64_t u = uint64_t(i); std::memcpy(r.b, &u, 8); return r; }

// Scalar VFPU result: converts to float by value and to integers by bit pattern.
struct VS {
    float f;
    operator float() const { return f; }
    operator U4() const { return U4(f); }
    template <class I, std::enable_if_t<std::is_integral_v<I>, int> = 0> operator I() const { return I(BitCast<uint32_t>(f)); }
    template <class T> operator GP<T>() const { return GP<T>(BitCast<uint32_t>(f)); }
};
inline V16 VQ(VS s) { return VQ(s.f); }

// ── VFPU prefixes (allegrex.sinc encodings) ──
// s/t lane: bit4 neg, bit3 abs, bit2 constant, bits0-1 swizzle (constants 0,1,2,1/2 when bit2).
// d lane:   bit2 write mask, bits0-1 1 = none, 2 = saturate [0,1], 4 = saturate [-1,1].
struct Vfpu { int s[4] = {0, 1, 2, 3}, t[4] = {0, 1, 2, 3}, d[4] = {1, 1, 1, 1}; bool ps = false, pt = false, pd = false; };
extern Vfpu g_vfpu;
inline U4 vpfxs(int a, int b, int c, int d) { g_vfpu.s[0] = a; g_vfpu.s[1] = b; g_vfpu.s[2] = c; g_vfpu.s[3] = d; g_vfpu.ps = true; return {}; }
inline U4 vpfxt(int a, int b, int c, int d) { g_vfpu.t[0] = a; g_vfpu.t[1] = b; g_vfpu.t[2] = c; g_vfpu.t[3] = d; g_vfpu.pt = true; return {}; }
inline U4 vpfxd(int a, int b, int c, int d) { g_vfpu.d[0] = a; g_vfpu.d[1] = b; g_vfpu.d[2] = c; g_vfpu.d[3] = d; g_vfpu.pd = true; return {}; }
inline float PfxLane(const V16& v, int code) {
    static const float k[4] = {0.f, 1.f, 2.f, 0.5f};
    float x = (code & 4) ? k[code & 3] : v.f(code & 3);
    if (code & 8) x = std::fabs(x);
    if (code & 16) x = -x;
    return x;
}
inline V16 PfxS(const V16& v) { if (!g_vfpu.ps) return v; V16 r; for (int i = 0; i < 4; i++) r.f(i) = PfxLane(v, g_vfpu.s[i]); return r; }
inline V16 PfxT(const V16& v) { if (!g_vfpu.pt) return v; V16 r; for (int i = 0; i < 4; i++) r.f(i) = PfxLane(v, g_vfpu.t[i]); return r; }
inline V16 PfxD(V16 v, int n) {
    if (g_vfpu.pd)
        for (int i = 0; i < n; i++) {
            int m = g_vfpu.d[i] & 3;
            if (m == 2) v.f(i) = std::fmin(std::fmax(v.f(i), 0.f), 1.f);
            else if (m == 0) v.f(i) = std::fmin(std::fmax(v.f(i), -1.f), 1.f);   // export 4 & 3 == 0
            // the write mask (bit 2) cannot be honoured: Ghidra returns the whole register as a value
        }
    g_vfpu.ps = g_vfpu.pt = g_vfpu.pd = false;
    return v;
}
template <int N> inline VB<N> Cut(const V16& v) { VB<N> r; std::memcpy(r.b, v.b, N); return r; }

float VfpuConst(int i);

#define PSP_VOP1(name, N, expr) \
    template <class A> inline VB<N * 4> name(const A& a_) { V16 a = PfxS(VQ(a_)), r; \
        for (int i = 0; i < N; i++) { float x = a.f(i); r.f(i) = (expr); } return Cut<N * 4>(PfxD(r, N)); }
#define PSP_VOP2(name, N, expr) \
    template <class A, class B> inline VB<N * 4> name(const A& a_, const B& b_) { V16 a = PfxS(VQ(a_)), b = PfxT(VQ(b_)), r; \
        for (int i = 0; i < N; i++) { float x = a.f(i), y = b.f(i); r.f(i) = (expr); } return Cut<N * 4>(PfxD(r, N)); }
#define PSP_VFAM1(name, expr) PSP_VOP1(name##_p, 2, expr) PSP_VOP1(name##_t, 3, expr) PSP_VOP1(name##_q, 4, expr)
#define PSP_VFAM2(name, expr) PSP_VOP2(name##_p, 2, expr) PSP_VOP2(name##_t, 3, expr) PSP_VOP2(name##_q, 4, expr)

constexpr float kHalfPi = 1.5707963267948966f;
PSP_VFAM1(vmov, x) PSP_VFAM1(vneg, -x) PSP_VFAM1(vabs, std::fabs(x))
PSP_VFAM1(vsin, std::sin(x * kHalfPi)) PSP_VFAM1(vcos, std::cos(x * kHalfPi))
PSP_VFAM1(vrcp, 1.f / x) PSP_VFAM1(vrsq, 1.f / std::sqrt(x)) PSP_VFAM1(vsqrt, std::sqrt(x))
#define PSP_VSCALE(name, N, expr) \
    template <class A> inline VB<N * 4> name(const A& a_, int sc = 0) { V16 a = PfxS(VQ(a_)), r; \
        for (int i = 0; i < N; i++) { float x = a.f(i); r.f(i) = (expr); } return Cut<N * 4>(PfxD(r, N)); }
#define PSP_VSCALEF(name, expr) PSP_VSCALE(name##_p, 2, expr) PSP_VSCALE(name##_t, 3, expr) PSP_VSCALE(name##_q, 4, expr)
PSP_VSCALEF(vf2iz, BitCast<float>(int32_t(std::trunc(std::ldexp(x, sc)))))
PSP_VSCALEF(vf2in, BitCast<float>(int32_t(std::nearbyint(std::ldexp(x, sc)))))
PSP_VSCALEF(vi2f, std::ldexp(float(BitCast<int32_t>(x)), -sc))
PSP_VFAM1(vlog2, std::log2(x)) PSP_VFAM1(vexp2, std::exp2(x))
PSP_VFAM2(vadd, x + y) PSP_VFAM2(vsub, x - y) PSP_VFAM2(vmul, x * y) PSP_VFAM2(vdiv, x / y)
PSP_VFAM2(vmin, std::fmin(x, y)) PSP_VFAM2(vmax, std::fmax(x, y))

// scalar forms take/return single floats
#define PSP_VS1(name, expr) template <class A> inline VS name##_s(const A& a_) { V16 a = PfxS(VQ(FB(a_))), r; float x = a.f(0); r.f(0) = (expr); return {PfxD(r, 1).f(0)}; }
#define PSP_VS2(name, expr) template <class A, class B> inline VS name##_s(const A& a_, const B& b_) { V16 a = PfxS(VQ(FB(a_))), b = PfxT(VQ(FB(b_))), r; float x = a.f(0), y = b.f(0); r.f(0) = (expr); return {PfxD(r, 1).f(0)}; }
PSP_VS1(vmov, x) PSP_VS1(vneg, -x) PSP_VS1(vabs, std::fabs(x)) PSP_VS1(vsin, std::sin(x * kHalfPi)) PSP_VS1(vcos, std::cos(x * kHalfPi))
PSP_VS1(vrcp, 1.f / x) PSP_VS1(vrsq, 1.f / std::sqrt(x)) PSP_VS1(vsqrt, std::sqrt(x)) PSP_VS1(vlog2, std::log2(x)) PSP_VS1(vexp2, std::exp2(x))
template <class A> inline VS vf2iz_s(const A& a, int sc = 0) { return {BitCast<float>(int32_t(std::trunc(std::ldexp(FB(a), sc))))}; }
template <class A> inline VS vi2f_s(const A& a, int sc = 0) { return {std::ldexp(float(BitCast<int32_t>(FB(a))), -sc)}; }
PSP_VS2(vadd, x + y) PSP_VS2(vsub, x - y) PSP_VS2(vmul, x * y) PSP_VS2(vdiv, x / y) PSP_VS2(vmin, std::fmin(x, y)) PSP_VS2(vmax, std::fmax(x, y))

inline V16 vzero_q() { return Cut<16>(PfxD(V16{}, 4)); }
inline V12 vzero_t() { return Cut<12>(PfxD(V16{}, 3)); }
inline V8 vzero_p() { return Cut<8>(PfxD(V16{}, 2)); }
inline VS vzero_s() { return {PfxD(V16{}, 1).f(0)}; }
inline V16 Ones() { V16 r; for (int i = 0; i < 4; i++) r.f(i) = 1.f; return r; }
inline V16 vone_q() { return Cut<16>(PfxD(Ones(), 4)); }
inline V12 vone_t() { return Cut<12>(PfxD(Ones(), 3)); }
inline V8 vone_p() { return Cut<8>(PfxD(Ones(), 2)); }
inline VS vone_s() { return {PfxD(Ones(), 1).f(0)}; }
inline V16 CstV(int i) { V16 r; for (int k = 0; k < 4; k++) r.f(k) = VfpuConst(i); return r; }
inline V16 vcst_q(int i) { return Cut<16>(PfxD(CstV(i), 4)); }
inline V12 vcst_t(int i) { return Cut<12>(PfxD(CstV(i), 3)); }
inline V8 vcst_p(int i) { return Cut<8>(PfxD(CstV(i), 2)); }
inline VS vcst_s(int i) { return {PfxD(CstV(i), 1).f(0)}; }

template <int N, class A, class B> inline VS VDot(const A& a_, const B& b_) {
    V16 a = PfxS(VQ(a_)), b = PfxT(VQ(b_)); float s = 0; for (int i = 0; i < N; i++) s += a.f(i) * b.f(i);
    V16 r; r.f(0) = s; return {PfxD(r, 1).f(0)};
}
template <class A, class B> VS vdot_p(const A& a, const B& b) { return VDot<2>(a, b); }
template <class A, class B> VS vdot_t(const A& a, const B& b) { return VDot<3>(a, b); }
template <class A, class B> VS vdot_q(const A& a, const B& b) { return VDot<4>(a, b); }
template <class A, class B> VS vhdp_q(const A& a_, const B& b_) {   // homogeneous dot: a.w treated as 1
    V16 a = PfxS(VQ(a_)), b = PfxT(VQ(b_)); float s = a.f(0) * b.f(0) + a.f(1) * b.f(1) + a.f(2) * b.f(2) + b.f(3);
    V16 r; r.f(0) = s; return {PfxD(r, 1).f(0)};
}
template <class A> VS vfad_t(const A& a_) { V16 a = PfxS(VQ(a_)); V16 r; r.f(0) = a.f(0) + a.f(1) + a.f(2); return {PfxD(r, 1).f(0)}; }
template <class A> VS vfad_q(const A& a_) { V16 a = PfxS(VQ(a_)); V16 r; r.f(0) = a.f(0) + a.f(1) + a.f(2) + a.f(3); return {PfxD(r, 1).f(0)}; }
template <class A, class S> VB<16> vscl_q(const A& a_, const S& s) { V16 a = PfxS(VQ(a_)), r; float k = FB(s); for (int i = 0; i < 4; i++) r.f(i) = a.f(i) * k; return PfxD(r, 4); }
template <class A, class S> VB<12> vscl_t(const A& a_, const S& s) { V16 a = PfxS(VQ(a_)), r; float k = FB(s); for (int i = 0; i < 3; i++) r.f(i) = a.f(i) * k; return Cut<12>(PfxD(r, 3)); }
template <class A, class S> VB<8> vscl_p(const A& a_, const S& s) { V16 a = PfxS(VQ(a_)), r; float k = FB(s); for (int i = 0; i < 2; i++) r.f(i) = a.f(i) * k; return Cut<8>(PfxD(r, 2)); }
template <class A, class B> V12 vcrsp_t(const A& a_, const B& b_) {
    V16 a = PfxS(VQ(a_)), b = PfxT(VQ(b_)), r;
    r.f(0) = a.f(1) * b.f(2) - a.f(2) * b.f(1); r.f(1) = a.f(2) * b.f(0) - a.f(0) * b.f(2); r.f(2) = a.f(0) * b.f(1) - a.f(1) * b.f(0);
    return Cut<12>(PfxD(r, 3));
}
template <class A, class B> V16 vqmul_q(const A& a_, const B& b_) {   // quaternion product
    V16 a = PfxS(VQ(a_)), b = PfxT(VQ(b_)), r;
    float ax = a.f(0), ay = a.f(1), az = a.f(2), aw = a.f(3), bx = b.f(0), by = b.f(1), bz = b.f(2), bw = b.f(3);
    r.f(0) = ax * bw + ay * bz - az * by + aw * bx; r.f(1) = -ax * bz + ay * bw + az * bx + aw * by;
    r.f(2) = ax * by - ay * bx + az * bw + aw * bz; r.f(3) = -ax * bx - ay * by - az * bz + aw * bw;
    return PfxD(r, 4);
}
// vcmp: returns the CC register. Bits 0..3 per lane, bit 4 = any, bit 5 = all.
uint32_t VCmp(int op, const V16& a, const V16& b, int n);
template <class A, class B> uint32_t vcmp_s(int op, const A& a, const B& b) { return VCmp(op, PfxS(VQ(FB(a))), PfxT(VQ(FB(b))), 1); }
template <class A, class B> uint32_t vcmp_p(int op, const A& a, const B& b) { return VCmp(op, PfxS(VQ(a)), PfxT(VQ(b)), 2); }
template <class A, class B> uint32_t vcmp_t(int op, const A& a, const B& b) { return VCmp(op, PfxS(VQ(a)), PfxT(VQ(b)), 3); }
template <class A, class B> uint32_t vcmp_q(int op, const A& a, const B& b) { return VCmp(op, PfxS(VQ(a)), PfxT(VQ(b)), 4); }

// matrices are passed as 64-byte values (4 columns of 4 floats, column major like the VFPU)
using M64 = VB<64>;
template <int N> struct GSize<VB<N>>;
template <class M, class V> V16 VTfm(const M& m_, const V& v_, int n, bool homog) {
    M64 m; std::memcpy(m.b, &m_, sizeof(M) < 64 ? sizeof(M) : 64);
    V16 v = VQ(v_), r;
    for (int i = 0; i < n; i++) {
        float s = 0;
        for (int k = 0; k < n; k++) s += m.f(k * 4 + i) * ((homog && k == n - 1) ? 1.f : v.f(k));
        r.f(i) = s;
    }
    g_vfpu.ps = g_vfpu.pt = g_vfpu.pd = false;
    return r;
}
template <class M, class V> V16 vtfm4_q(const M& m, const V& v) { return VTfm(m, v, 4, false); }
template <class M, class V> V12 vtfm3_t(const M& m, const V& v) { return Cut<12>(VTfm(m, v, 3, false)); }
template <class M, class V> V16 vhtfm4_q(const M& m, const V& v) { return VTfm(m, v, 4, true); }
template <class M, class V> V12 vhtfm3_t(const M& m, const V& v) { return Cut<12>(VTfm(m, v, 3, true)); }
template <class A, class B> M64 vmmul_q(const A& a_, const B& b_) {
    M64 a, b, r; std::memcpy(a.b, &a_, 64); std::memcpy(b.b, &b_, 64);
    for (int c = 0; c < 4; c++) for (int i = 0; i < 4; i++) { float s = 0; for (int k = 0; k < 4; k++) s += a.f(i * 4 + k) * b.f(c * 4 + k); r.f(c * 4 + i) = s; }
    g_vfpu.ps = g_vfpu.pt = g_vfpu.pd = false; return r;
}
inline M64 vmidt_q() { M64 r; for (int i = 0; i < 4; i++) r.f(i * 5) = 1.f; return r; }
inline M64 vmzero_q() { return M64{}; }


// ── arithmetic on Field<4>/VS: float when the other side is floating, else integer bits ──
template <class X> struct IsBits : std::false_type {};
template <> struct IsBits<Field<4>> : std::true_type {};
template <> struct IsBits<Field<2>> : std::true_type {};
template <> struct IsBits<Field<1>> : std::true_type {};
template <> struct IsBits<Field<8>> : std::true_type {};
template <> struct IsBits<VS> : std::true_type {};
template <class X> inline auto BitsNum(const X& x, bool asFloat) {
    if constexpr (std::is_same_v<X, VS>) return x.f;
    else return x;
}
#define PSP_BITS_OP(op) \
    template <class X, class T, std::enable_if_t<IsBits<X>::value && std::is_arithmetic_v<T>, int> = 0> \
    inline auto operator op(const X& x, T t) { if constexpr (std::is_floating_point_v<T> || std::is_same_v<X, VS>) return float(x) op t; \
        else if constexpr (std::is_same_v<X, Field<8>>) return uint64_t(x) op t; else return uint32_t(x) op t; } \
    template <class X, class T, std::enable_if_t<IsBits<X>::value && std::is_arithmetic_v<T>, int> = 0> \
    inline auto operator op(T t, const X& x) { if constexpr (std::is_floating_point_v<T> || std::is_same_v<X, VS>) return t op float(x); \
        else if constexpr (std::is_same_v<X, Field<8>>) return t op uint64_t(x); else return t op uint32_t(x); }
PSP_BITS_OP(+) PSP_BITS_OP(-) PSP_BITS_OP(*) PSP_BITS_OP(/) PSP_BITS_OP(<) PSP_BITS_OP(>) PSP_BITS_OP(<=) PSP_BITS_OP(>=)
PSP_BITS_OP(==) PSP_BITS_OP(!=)
#define PSP_BITS_IOP(op) \
    template <class X, class T, std::enable_if_t<IsBits<X>::value && std::is_integral_v<T>, int> = 0> \
    inline auto operator op(const X& x, T t) { return uint64_t(x) op t; } \
    template <class X, class T, std::enable_if_t<IsBits<X>::value && std::is_integral_v<T>, int> = 0> \
    inline auto operator op(T t, const X& x) { return t op uint64_t(x); }
PSP_BITS_IOP(&) PSP_BITS_IOP(|) PSP_BITS_IOP(^) PSP_BITS_IOP(<<) PSP_BITS_IOP(>>)
template <class X, std::enable_if_t<IsBits<X>::value, int> = 0> inline float operator-(const X& x) { return -float(x); }

// ── 128-bit integer view of byte vectors (Ghidra moves lanes with shifts) ──
template <int N> inline VB<N> operator<<(const VB<N>& v, int bits) {
    VB<N> r; for (int i = N - 1; i >= 0; i--) { int src = i - bits / 8; r.b[i] = src >= 0 ? v.b[src] : 0; } return r;
}
template <int N> inline VB<N> operator>>(const VB<N>& v, int bits) {
    VB<N> r; for (int i = 0; i < N; i++) { int src = i + bits / 8; r.b[i] = src < N ? v.b[src] : 0; } return r;
}
template <int N> inline VB<N> operator&(const VB<N>& a, const VB<N>& b) { VB<N> r; for (int i = 0; i < N; i++) r.b[i] = a.b[i] & b.b[i]; return r; }
template <int N> inline VB<N> operator|(const VB<N>& a, const VB<N>& b) { VB<N> r; for (int i = 0; i < N; i++) r.b[i] = a.b[i] | b.b[i]; return r; }
template <int N, class I, std::enable_if_t<std::is_integral_v<I>, int> = 0> inline uint32_t operator+(const VB<N>& v, I i) { uint32_t u; std::memcpy(&u, v.b, 4); return u + uint32_t(i); }
template <int N, class I, std::enable_if_t<std::is_integral_v<I>, int> = 0> inline uint64_t operator&(const VB<N>& v, I i) { uint64_t u = 0; std::memcpy(&u, v.b, N < 8 ? N : 8); return u & uint64_t(i); }

// ── Ghidra helpers ──
template <class T> inline auto ABS(T x) { return x < 0 ? -x : x; }
inline float SQRT(float x) { return std::sqrt(x); }
inline double SQRT(double x) { return std::sqrt(x); }
template <class T> inline T SQRT(T x) { return T(std::sqrt(double(x))); }
inline float ROUND(float x) { return std::nearbyint(x); }
inline float FLOOR(float x) { return std::floor(x); }
inline int LZCOUNT(uint32_t x) { return x ? __builtin_clz(x) : 32; }
inline int POPCOUNT(uint32_t x) { return __builtin_popcount(x); }
inline void trap(...) {}
inline void halt_baddata() { Fatal("halt_baddata"); }
inline void halt_unimplemented() { Fatal("halt_unimplemented"); }

template <class T> inline uint64_t UB(const T& v) {   // raw bits of any small value
    if constexpr (std::is_floating_point_v<T>) return BitCast<uint32_t>(float(v));
    else if constexpr (std::is_same_v<T, bool>) return v ? 1u : 0u;
    else if constexpr (std::is_integral_v<T>) return uint64_t(std::make_unsigned_t<T>(v));
    else { uint64_t u = 0; std::memcpy(&u, &v, sizeof(T) < 8 ? sizeof(T) : 8); return u; }
}
template <int B> inline uint64_t UB(const Field<B>& f) { return f.U(); }
#define PSP_CONCAT(a, b) \
    template <class H, class L> inline uint64_t CONCAT##a##b(const H& h, const L& l) { \
        uint64_t lo = UB(l) & ((b) >= 8 ? ~0ull : ((1ull << ((b) * 8)) - 1)); return (UB(h) << ((b) * 8)) | lo; }
PSP_CONCAT(1, 1) PSP_CONCAT(1, 2) PSP_CONCAT(1, 3) PSP_CONCAT(1, 4) PSP_CONCAT(2, 1) PSP_CONCAT(2, 2) PSP_CONCAT(3, 1)
PSP_CONCAT(4, 4) PSP_CONCAT(2, 4) PSP_CONCAT(4, 2) PSP_CONCAT(1, 7) PSP_CONCAT(3, 4) PSP_CONCAT(1, 6) PSP_CONCAT(1, 5)
template <class T> inline uint64_t SUBN(const T& v, int off) { return UB(v) >> (off * 8); }
#define SUB41(v, o) uint8_t(::psp::SUBN(v, o))
#define SUB42(v, o) uint16_t(::psp::SUBN(v, o))
#define SUB81(v, o) uint8_t(::psp::SUBN(v, o))
#define SUB82(v, o) uint16_t(::psp::SUBN(v, o))
#define SUB84(v, o) uint32_t(::psp::SUBN(v, o))
#define SUB21(v, o) uint8_t(::psp::SUBN(v, o))
#define SUB164(v, o) ::psp::U4(uint32_t(::psp::SubV(v, o)))
template <class T> inline uint32_t SubV(const T& v, int o) { uint32_t r; std::memcpy(&r, reinterpret_cast<const uint8_t*>(&v) + o, 4); return r; }
template <class T> inline uint64_t ZEXT(const T& v) { return UB(v); }
#define ZEXT14(v) uint32_t(::psp::UB(v) & 0xff)
#define ZEXT24(v) uint32_t(::psp::UB(v) & 0xffff)
#define ZEXT48(v) uint64_t(::psp::UB(v) & 0xffffffffull)
#define SEXT48(v) uint64_t(int64_t(int32_t(::psp::UB(v))))
template <int N, class T> inline VB<N> ZextV(const T& v) { VB<N> r; uint64_t u = UB(v); std::memcpy(r.b, &u, N < 8 ? N : 8); return r; }
#define ZEXT412(v) ::psp::ZextV<12>(v)
#define ZEXT416(v) ::psp::ZextV<16>(v)
#define ZEXT812(v) ::psp::ZextV<12>(v)
#define ZEXT816(v) ::psp::ZextV<16>(v)
#define ZEXT1216(v) ::psp::ZextV<16>(v)
inline bool CARRY4(uint32_t a, uint32_t b) { return uint32_t(a + b) < a; }
inline bool SCARRY4(int32_t a, int32_t b) { int32_t r; return __builtin_add_overflow(a, b, &r); }
inline bool SBORROW4(int32_t a, int32_t b) { int32_t r; return __builtin_sub_overflow(a, b, &r); }

uint32_t GStr(const char* s);   // interns a C string literal in guest memory
void builtin_strncpy(uint32_t dst, const char* src, int n);
inline void builtin_strncpy(GP<char> dst, const char* src, int n) { builtin_strncpy(dst.a, src, n); }

// ── stack frames: locals whose address is taken live in guest memory ──
struct Frame {
    uint32_t saved, base;
    explicit Frame(uint32_t size) : saved(g_sp), base(g_sp - 0x40) { g_sp = base - ((size + 15) & ~15u); }
    ~Frame() { g_sp = saved; }
};

// ── calls by address (virtual methods, function pointers): EABI register images ──
struct CallRegs { uint32_t i[8] = {}; float f[8] = {}; int ni = 0, nf = 0; };
struct CallRet { uint64_t v = 0; float f = 0; };
using Thunk = CallRet (*)(const CallRegs&);
Thunk FindThunk(uint32_t addr);
template <class A> inline void PushArg(CallRegs& r, const A& a) {
    if constexpr (std::is_floating_point_v<A>) { if (r.nf < 8) r.f[r.nf++] = float(a); }
    else if constexpr (std::is_same_v<A, VS>) { if (r.nf < 8) r.f[r.nf++] = a.f; }
    else if constexpr (std::is_same_v<A, uint64_t> || std::is_same_v<A, int64_t>) {   // EABI: 64-bit in one register
        if (r.ni < 8) r.i[r.ni++] = uint32_t(a);
    } else { if (r.ni < 8) r.i[r.ni++] = uint32_t(UB(a)); }
}
template <class... A> uint64_t ICall(uint32_t addr, A... a) {
    CallRegs r; (PushArg(r, a), ...);
    Thunk t = FindThunk(addr);
    if (!t) { Unimplemented(addr, "icall"); return 0; }
    return t(r).v;
}
// Arguments by the callee's parameter types, from a register image.
template <class T> inline T ArgFrom(const CallRegs& r, int& ni, int& nf) {
    if constexpr (std::is_floating_point_v<T>) return T(nf < 8 ? r.f[nf++] : 0.f);
    else { uint32_t v = ni < 8 ? r.i[ni++] : 0; if constexpr (std::is_integral_v<T>) return T(v); else return T(v); }
}

// Direct calls: Ghidra sometimes passes more or fewer arguments than the callee declares.
template <class P, class A> inline P Conv(const A& a) {
    if constexpr (std::is_same_v<P, A>) return a;
    else if constexpr (std::is_floating_point_v<P>) {
        if constexpr (std::is_arithmetic_v<A>) return P(a); else return P(FB(a));
    } else if constexpr (std::is_integral_v<P>) {
        if constexpr (std::is_floating_point_v<A>) return P(BitCast<uint32_t>(float(a)));   // float bits in an int register
        else if constexpr (std::is_arithmetic_v<A>) return P(a);
        else return P(UB(a));
    } else if constexpr (std::is_constructible_v<P, A>) return P(a);
    else return P(uint32_t(UB(a)));
}
template <class R, class... P, class... A, size_t... I>
inline R CallAdaptImpl(R (*f)(P...), std::index_sequence<I...>, A&&... a) {
    auto tup = std::forward_as_tuple(std::forward<A>(a)...);
    return f([&]() -> std::tuple_element_t<I, std::tuple<P...>> {
        using PT = std::tuple_element_t<I, std::tuple<P...>>;
        if constexpr (I < sizeof...(A)) return Conv<PT>(std::get<I < sizeof...(A) ? I : 0>(tup)); else return PT{};
    }()...);
}
template <class R, class... P, class... A> inline R CallAdapt(R (*f)(P...), A&&... a) {
    return CallAdaptImpl(f, std::index_sequence_for<P...>{}, std::forward<A>(a)...);
}

// (__bc_T)x in Ghidra's output: a p-code CAST between float and integer, i.e. the same bits reread.
template <class T> struct BC {
    T v;
    template <class A> BC(const A& a) { uint64_t u = UB(a); std::memcpy(&v, &u, sizeof(T)); }
    operator T() const { return v; }
};

// Return values of unknown use: converts to float or integer by bit pattern like a register would.
struct RV {
    uint64_t v;
    template <class T> RV(const T& t) : v(UB(t)) {}
    RV() : v(0) {}
    operator U4() const { return U4(uint32_t(v)); }
    explicit operator float() const { return BitCast<float>(uint32_t(v)); }
    template <class I, std::enable_if_t<std::is_integral_v<I>, int> = 0> operator I() const { return I(v); }
    template <class T> operator GP<T>() const { return GP<T>(uint32_t(v)); }
};

} // namespace psp
