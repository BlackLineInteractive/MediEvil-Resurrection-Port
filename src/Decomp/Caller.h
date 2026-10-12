#pragma once
// Direct calls between generated functions: adapts Ghidra's argument list to the callee's parameters,
// and lets a caller use the value of a function Ghidra declared void (it reads v0 anyway).
#include "Decomp/Runtime.h"

namespace psp {

// Ring buffer of the last guest functions entered, printed on a crash.
extern uint32_t g_trace[256];
extern uint32_t g_tracePos;

template <auto F, uint32_t Addr = 0> struct Caller {
    template <class... A> auto operator()(A&&... a) const {
        g_trace[g_tracePos++ & 255] = Addr;
        using R = decltype(CallAdapt(F, std::forward<A>(a)...));
        if constexpr (std::is_void_v<R>) { CallAdapt(F, std::forward<A>(a)...); return RV(); }
        else return CallAdapt(F, std::forward<A>(a)...);
    }
};

} // namespace psp
