#include "Core/GameData.h"

#include <cstring>
#include <fstream>
#include <sstream>

namespace med {

static constexpr uint32_t kSector = 2048;

static uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }
static uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }

uint32_t NameHash(const std::string& name) {
    uint32_t h = 0;
    for (char c : name) h = h * 129u + uint32_t(int32_t(int8_t(c)));
    return h;
}

std::string FourCCStr(uint32_t v) {
    std::string s;
    for (int i = 0; i < 4; ++i) {
        char c = char(v >> (i * 8));
        if (c) s += c;
    }
    return s;
}

GameData::~GameData() {
    if (f_) fclose(f_);
}

bool GameData::Open(const std::string& path, std::string* err) {
    auto fail = [&](const char* m) { if (err) *err = m; return false; };
    f_ = fopen(path.c_str(), "rb");
    if (!f_) return fail("cannot open gamedata.bin");

    std::vector<uint8_t> hdr(kSector);
    if (fread(hdr.data(), 1, kSector, f_) != kSector || memcmp(hdr.data(), "TOCS", 4) != 0)
        return fail("not a TOCS archive");

    // TOCS: 16-byte header, then {char name[32]; u32 tocSize; u32 tocSector} until an empty name.
    for (size_t pos = 16; pos + 40 <= hdr.size() && hdr[pos]; pos += 40) {
        Group g;
        g.name.assign(reinterpret_cast<const char*>(&hdr[pos]), strnlen(reinterpret_cast<const char*>(&hdr[pos]), 32));
        g.index = int(groups_.size());
        uint32_t tocSize = rd32(&hdr[pos + 32]), tocSector = rd32(&hdr[pos + 36]);
        if (tocSize) {
            std::vector<uint8_t> toc(tocSize);
            fseek(f_, long(tocSector) * kSector, SEEK_SET);
            if (fread(toc.data(), 1, tocSize, f_) != tocSize || memcmp(toc.data(), "TOC0", 4) != 0)
                return fail("bad TOC0");
            // TOC0: nBlocks x 8B {u24 sector, u8 flags, u16 asset, u16 nextCopy},
            //       nAssets x 20B {hash, size, type, u16 kind, u16 block, u16 bstLeft, u16 bstRight}
            uint32_t nBlocks = rd32(&toc[4]), nAssets = rd32(&toc[8]);
            const uint8_t* blocks = &toc[16];
            const uint8_t* recs = blocks + nBlocks * 8;
            for (uint32_t i = 0; i < nAssets; ++i) {
                const uint8_t* r = recs + i * 20;
                Asset a;
                a.hash = rd32(r);
                a.size = rd32(r + 4);
                a.type = rd32(r + 8);
                a.kind = rd16(r + 12);
                a.sector = rd32(blocks + rd16(r + 14) * 8) & 0xFFFFFF;
                g.byHash.emplace(a.hash, g.assets.size());
                g.assets.push_back(a);
            }
        }
        groups_.push_back(std::move(g));
    }
    return true;
}

std::vector<uint8_t> GameData::Read(const Asset& a) const {
    std::vector<uint8_t> out(a.size);
    fseek(f_, long(a.sector) * kSector, SEEK_SET);
    out.resize(fread(out.data(), 1, a.size, f_));
    return out;
}

const Asset* GameData::Find(uint32_t hash, int preferGroup) const {
    auto look = [&](const Group& g) -> const Asset* {
        auto it = g.byHash.find(hash);
        return it == g.byHash.end() || g.assets[it->second].kind != 1 ? nullptr : &g.assets[it->second];
    };
    if (preferGroup >= 0 && preferGroup < int(groups_.size()))
        if (auto* a = look(groups_[preferGroup])) return a;
    for (const auto& g : groups_)
        if (auto* a = look(g)) return a;
    return nullptr;
}

void GameData::LoadNames(const std::string& tsvPath) {
    std::ifstream in(tsvPath);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string h, type, name;
        if (!std::getline(ss, h, '\t') || !std::getline(ss, type, '\t') || !std::getline(ss, name)) continue;
        if (h == "hash") continue;
        names_[uint32_t(std::stoul(h, nullptr, 16))] = name;
    }
}

std::string GameData::NameOf(uint32_t hash) const {
    auto it = names_.find(hash);
    if (it != names_.end()) return it->second;
    char buf[16];
    snprintf(buf, sizeof buf, "%08x", hash);
    return buf;
}

} // namespace med
