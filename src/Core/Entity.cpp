#include "Core/Entity.h"

#include <cstring>
#include <fstream>
#include <sstream>

namespace med {
namespace {

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// "KEY = VALUE !" -> (KEY, VALUE); the trailing " !" terminates list-like values in the game data.
bool SplitProp(const std::string& s, std::string& key, std::string& value) {
    size_t eq = s.find('=');
    if (eq == std::string::npos) return false;
    key = Trim(s.substr(0, eq));
    value = Trim(s.substr(eq + 1));
    if (!value.empty() && value.back() == '!') value = Trim(value.substr(0, value.size() - 1));
    return true;
}

} // namespace

const PropBlock* PropBlock::Block(const std::string& n) const {
    for (const auto& b : blocks)
        if (b.name == n) return &b;
    return nullptr;
}

std::optional<std::string> PropBlock::Get(const std::string& key) const {
    std::optional<std::string> found;
    for (const auto& [kind, text] : strings) {
        std::string k, v;
        if (kind == 5 && SplitProp(text, k, v) && k == key) found = v;
    }
    return found;
}

bool TemplateDB::Load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    EntityTemplate* cur = nullptr;
    std::vector<PropBlock*> stack;
    while (std::getline(in, line)) {
        if (line.rfind("template ", 0) == 0) {
            std::istringstream ss(line.substr(9));
            EntityTemplate t;
            std::string base, size;
            ss >> t.name >> base >> size;
            if (base.rfind("base=", 0) == 0) t.base = base.substr(5);
            if (size.rfind("size=", 0) == 0) t.size = uint32_t(std::stoul(size.substr(5), nullptr, 16));
            cur = &(templates_[t.name] = t);
            stack.clear();
            continue;
        }
        std::string s = Trim(line);
        if (!cur || s.empty()) continue;
        if (s[0] == '{') {
            std::string name = Trim(s.substr(1));
            if (stack.empty()) {
                cur->defaults.name = name;
                stack.push_back(&cur->defaults);
            } else {
                stack.back()->blocks.push_back({name, {}, {}});
                stack.push_back(&stack.back()->blocks.back());
            }
        } else if (s[0] == '}') {
            if (!stack.empty()) stack.pop_back();
        } else if (!stack.empty() && s.size() > 2 && s[1] == ' ') {
            stack.back()->strings.emplace_back(s[0] - '0', s.substr(2));
        }
    }
    return !templates_.empty();
}

const EntityTemplate* TemplateDB::Find(const std::string& name) const {
    auto it = templates_.find(name);
    return it == templates_.end() ? nullptr : &it->second;
}

std::optional<std::string> Entity::Get(const std::string& key, const std::string& component) const {
    const PropBlock* inst = component.empty() ? &props : props.Block(component);
    if (inst)
        if (auto v = inst->Get(key)) return v;
    if (tmpl) {
        const PropBlock* def = component.empty() ? &tmpl->defaults : tmpl->defaults.Block(component);
        if (def) return def->Get(key);
    }
    return std::nullopt;
}

glm::vec3 Entity::Position() const {
    glm::vec3 p(0);
    if (auto v = Get("POS")) sscanf(v->c_str(), "%f %f %f", &p.x, &p.y, &p.z);
    return p;
}

glm::quat Entity::Rotation() const {
    float x = 0, y = 0, z = 0, w = 1;
    if (auto v = Get("ROT")) sscanf(v->c_str(), "%f %f %f %f", &x, &y, &z, &w);
    return glm::quat(w, x, y, z);
}

std::string Entity::Uid() const {
    return Get("UID").value_or(std::string());
}

static std::string BaseName(std::string r) {
    size_t slash = r.find_last_of('/');
    if (slash != std::string::npos) r = r.substr(slash + 1);
    size_t dot = r.find('.');
    return dot == std::string::npos ? r : r.substr(0, dot);
}

std::string Entity::ResourceBaseName() const {
    auto res = Get("RES", "corporeal");
    return res ? BaseName(*res) : std::string();
}

std::string Entity::AnimBaseName() const {
    auto anims = Get("ANIMS", "corporeal");
    if (!anims) return ResourceBaseName();
    std::string b = BaseName(*anims);
    const std::string pkg = "_apkg";
    if (b.size() > pkg.size() && b.compare(b.size() - pkg.size(), pkg.size(), pkg) == 0) b.resize(b.size() - pkg.size());
    return b;
}

bool LoadEntities(const std::vector<uint8_t>& data, const TemplateDB* db, std::vector<Entity>& out) {
    if (data.size() < 12 || memcmp(data.data(), "ENT0", 4) != 0) return false;
    const uint8_t* base = data.data() + 8;
    size_t size = data.size() - 8;
    auto u32 = [&](size_t o) {
        uint32_t v = 0;
        if (o + 4 <= size) memcpy(&v, base + o, 4);
        return v;
    };
    auto str = [&](size_t o) {
        if (o >= size) return std::string();
        const void* e = memchr(base + o, 0, size - o);
        return std::string(reinterpret_cast<const char*>(base + o),
                           e ? static_cast<const uint8_t*>(e) - (base + o) : size - o);
    };

    uint32_t count = u32(0);
    for (uint32_t i = 0; i < count; ++i) {
        size_t p = u32(4 + i * 4);
        Entity e;
        e.recordName = str(u32(p + 4));
        e.className = e.recordName.substr(0, e.recordName.find(':'));
        e.props.name = e.className;
        p += 8;
        std::vector<PropBlock*> stack{&e.props};
        // FUN_001db478 walk: ends when the block depth drops below zero
        for (int guard = 0; guard < 100000 && p + 4 <= size; ++guard) {
            uint32_t k = u32(p);
            if (k == 1 || k == 2 || k == 5) {
                stack.back()->strings.emplace_back(int(k), str(u32(p + 4)));
                p += 8;
            } else if (k == 3) {
                stack.back()->blocks.push_back({str(u32(p + 4)), {}, {}});
                stack.push_back(&stack.back()->blocks.back());
                p += 8;
            } else if (k == 4) {
                p += 4;
                stack.pop_back();
                if (stack.empty()) break;
            } else {
                p += 4;
            }
        }
        if (db) e.tmpl = db->Find(e.className);
        out.push_back(std::move(e));
    }
    return true;
}

} // namespace med
