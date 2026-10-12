// Smoke test for the translated main.prx: boots guest memory and runs a few of Dan's functions.
#include "Decomp/Memory.h"
#include "decls.h"

#include <cstdio>

using namespace psp;

int main(int argc, char** argv) {
    const char* prx = argc > 1 ? argv[1] : "extracted/PSP_GAME/USRDIR/prx/main.prx";
    if (!Boot(prx)) { std::fprintf(stderr, "cannot load %s\n", prx); return 1; }
    std::printf("static constructors run: %d\n", RunStaticCtors());

    // danbrain speed curve FUN_0008ce24(stick, scale, brain): brain+0xc0c caps the result
    uint32_t brain = kHeapBase;
    *GP<float>(brain + 0xc0c) = 1.0f;
    std::printf("FUN_0008ce24 stick -> speed fraction\n");
    for (float s : {0.0f, 0.3f, 0.5f, 0.84f, 0.9f, 1.0f})
        std::printf("  %.2f -> %.4f\n", s, game::f_0008ce24(s, 1.0f, int(brain)));

    game::f_0020f284();   // engine init (FUN_001e77dc) step: acos table at 0x3de210 used by the stick code
    // pad stick filter FUN_001ea594(pad + 0xc4): raw axes at +6/+7 floats, result x,y at +0/+1, mag +3
    uint32_t pad = kHeapBase + 0x2000;
    std::printf("FUN_001ea594 raw stick -> filtered (x, y, magnitude)\n");
    for (auto [x, y] : {std::pair{0.1f, 0.0f}, {0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {0.7f, 0.7f}}) {
        GP<float> p(pad);
        p[6] = x; p[7] = y;
        game::f_001ea594(p);
        std::printf("  (%.2f, %.2f) -> (%.3f, %.3f) |%.3f| angle %.3f\n", x, y, float(p[0]), float(p[1]), float(p[3]), float(p[2]));
    }
    return 0;
}
