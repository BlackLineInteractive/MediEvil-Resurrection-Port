#pragma once
// gamedata.bin - the single resource archive of MediEvil: Resurrection (PSP).
// Format notes: docs/RE_NOTES.md §3.
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace med {

// Engine name hash (main.prx FUN_001be1e4): h = h*129 + (signed char)c over the file name
// with extension, no directory, starting at 0.
uint32_t NameHash(const std::string& name, uint32_t seed = 0);
// Inverse of continuing a hash with `suffix`: the hash the prefix must have so that
// NameHash(suffix, result) == full. Works because 129 is odd (invertible mod 2^32).
uint32_t NameHashStripSuffix(uint32_t full, const std::string& suffix);

// FourCC as stored in the TOC ("PSM\0", "SUBZ", ...).
inline uint32_t FourCC(const char* s) {
    uint32_t v = 0;
    for (int i = 0; i < 4 && s[i]; ++i) v |= uint32_t(uint8_t(s[i])) << (i * 8);
    return v;
}
std::string FourCCStr(uint32_t v);

struct Asset {
    uint32_t hash = 0;
    uint32_t size = 0;
    uint32_t type = 0;
    uint16_t kind = 0;   // 1 = file, 2 = group marker
    uint32_t sector = 0;
};

struct Group {
    std::string name;    // as stored, may be cut at 32 chars and repeats in the 2nd half of the table
    int index = 0;
    std::vector<Asset> assets;
    std::unordered_map<uint32_t, size_t> byHash;
};

class GameData {
public:
    ~GameData();
    bool Open(const std::string& path, std::string* err = nullptr);
    bool IsOpen() const { return f_ != nullptr; }

    const std::vector<Group>& Groups() const { return groups_; }
    std::vector<uint8_t> Read(const Asset& a) const;

    // Lookup by hash: the preferred group first, then every other group.
    const Asset* Find(uint32_t hash, int preferGroup = -1) const;
    const Asset* Find(const std::string& name, int preferGroup = -1) const { return Find(NameHash(name), preferGroup); }

    // Optional hash -> name table produced by tools/namehash.py (docs/names.tsv).
    void LoadNames(const std::string& tsvPath);
    std::string NameOf(uint32_t hash) const;

private:
    FILE* f_ = nullptr;
    std::vector<Group> groups_;
    std::unordered_map<uint32_t, std::string> names_;
};

} // namespace med
