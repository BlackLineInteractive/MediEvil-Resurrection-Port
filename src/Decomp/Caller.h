#pragma once
// Direct calls between generated functions: adapts Ghidra's argument list to the callee's parameters,
// and lets a caller use the value of a function Ghidra declared void (it reads v0 anyway).
#include "Decomp/Runtime.h"

namespace psp {

template <auto F> struct Caller {
    template <class... A> auto operator()(A&&... a) const {
        using R = decltype(CallAdapt(F, std::forward<A>(a)...));
        if constexpr (std::is_void_v<R>) { CallAdapt(F, std::forward<A>(a)...); return RV(); }
        else return CallAdapt(F, std::forward<A>(a)...);
    }
};

} // namespace psp
