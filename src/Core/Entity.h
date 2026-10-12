#pragma once
// Level entities (ENTS) and the entity/component templates compiled into main.prx.
//
//   FUN_001d95ac  CEntityLoader::LoadENTS   ENT0: u32 count, u32 recordOffset[count]
//   FUN_001db478  EntityRecord_Relocate     token stream: u32 kind [u32 offset]
//                                           1/2/5 string, 3 begin block, 4 end block (ends below depth 0)
//   FUN_001d975c  CEntityManager::Create    class = record name up to ':' (hashed), UID from "UID="
//   FUN_001d7f58  CEntityTemplate ctor      static initialisers register 454 templates: name, token
//                                           stream with defaults (absolute pointers), base type, size
// Templates are extracted to text by tools/entity_templates.py (docs/entity_templates.txt).
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace med {

// One block of the token tree: string tokens and nested blocks. "KEY=VALUE" strings are properties,
// bare strings (kind 1) list the components of an entity.
struct PropBlock {
    std::string name;
    std::vector<std::pair<int, std::string>> strings;   // (token kind, text)
    std::vector<PropBlock> blocks;

    const PropBlock* Block(const std::string& n) const;
    // Value of KEY in "KEY=VALUE" / "KEY = VALUE !" strings of this block (last one wins).
    std::optional<std::string> Get(const std::string& key) const;
};

struct EntityTemplate {
    std::string name, base;
    uint32_t size = 0;
    PropBlock defaults;   // the template's top-level block (name == template name)
};

class TemplateDB {
public:
    bool Load(const std::string& path);
    const EntityTemplate* Find(const std::string& name) const;
    size_t Size() const { return templates_.size(); }

private:
    std::map<std::string, EntityTemplate> templates_;
};

struct Entity {
    std::string recordName;   // "zombie", "danheighttracker:danheighttracker"
    std::string className;    // up to ':'
    PropBlock props;          // instance values (top-level strings + component blocks)
    const EntityTemplate* tmpl = nullptr;

    // Instance value first, then the template default. `component` empty = entity level.
    std::optional<std::string> Get(const std::string& key, const std::string& component = "") const;
    glm::vec3 Position() const;
    glm::quat Rotation() const;     // ROT=x y z w
    std::string Uid() const;
    // corporeal RES: "exporteddata/objects/x/x.group" -> "x" (empty if the entity has no model)
    std::string ResourceBaseName() const;
};

// ENTS asset -> entities (templates are attached when `db` is given).
bool LoadEntities(const std::vector<uint8_t>& data, const TemplateDB* db, std::vector<Entity>& out);

} // namespace med
