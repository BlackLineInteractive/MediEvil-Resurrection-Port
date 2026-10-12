#pragma once
// Asset chunk stream: char tag[4]; u32 size; payload        (leaf)
//                     char tag[4]; u32 0; u32 size; children (container)
#include <cstdint>
#include <cstring>
#include <functional>

namespace med {

struct Chunk {
    uint32_t tag = 0;
    const uint8_t* data = nullptr;
    uint32_t size = 0;
    bool container = false;

    bool Is(const char* t) const { return memcmp(&tag, t, 4) == 0; }
    template <class T> T At(size_t off) const {
        T v{};
        if (off + sizeof(T) <= size) memcpy(&v, data + off, sizeof(T));
        return v;
    }
};

inline bool IsTagByte(uint8_t c) { return (c >= '0' && c <= 'z') || c == ' ' || c == '%' || c == '_'; }

// Calls fn for each chunk in [p, p+n). Stops at anything that does not look like a chunk.
inline void ForEachChunk(const uint8_t* p, size_t n, const std::function<void(const Chunk&)>& fn) {
    size_t off = 0;
    while (off + 8 <= n) {
        const uint8_t* c = p + off;
        if (!IsTagByte(c[0]) || !IsTagByte(c[1]) || !IsTagByte(c[2]) || !IsTagByte(c[3])) return;
        Chunk ch;
        memcpy(&ch.tag, c, 4);
        uint32_t a, b = 0;
        memcpy(&a, c + 4, 4);
        if (off + 12 <= n) memcpy(&b, c + 8, 4);
        bool container = a == 0 && b && off + 12 + b <= n && off + 16 <= n &&
                         IsTagByte(c[12]) && IsTagByte(c[13]) && IsTagByte(c[14]) && IsTagByte(c[15]);
        if (container) {
            ch.container = true;
            ch.data = c + 12;
            ch.size = b;
            off += 12 + size_t(b);
        } else {
            if (off + 8 + size_t(a) > n) return;
            ch.data = c + 8;
            ch.size = a;
            off += 8 + size_t(a);
        }
        fn(ch);
    }
}

inline void ForEachChunk(const Chunk& parent, const std::function<void(const Chunk&)>& fn) {
    ForEachChunk(parent.data, parent.size, fn);
}

} // namespace med
