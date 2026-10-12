#pragma once
// Level zones (LAND asset), ported from main.prx.
//
//   LND0 { PTL0* portals, ZNE0* zones }
//   ZNE0 { NAME "01_danscrypt003", LGT1 lighting, ZDT0 bounds, WND0 wind, RZN0 }
//
//   FUN_001f3920  CZone::Load      LGT1 -> zone record (+0x60 light set, see ZoneLighting)
//   FUN_00216a58  SetLightSet      light set -> fog / ambient / two directional lights (static time of day)
//   FUN_0021757c  BuildLightList   enabled directional lights -> GE lights 0..n-1
//   FUN_00216cb4  EmitLights       sceGuLight(i, GU_DIRECTIONAL, GU_DIFFUSE, dir) + diffuse colour,
//                                  sceGuAmbient(world ambient for sub-zones, object ambient otherwise)
//   FUN_001f7038  sub-zone draw    + up to 4-n LIT0 point lights, sceGuEnable(GU_LIGHTING)
//
// The GE runs with sceGuColorMaterial(ambient|diffuse|specular) (FUN_002686a8), so the baked vertex
// colour is the material and the final colour is
//     clamp(ambient * vcol + sum(lightColour * vcol * max(dot(N, dir), 0))) * texture
// There is no colour doubling (TFUNC is always written without the 2x bit).
#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace med {

struct ZoneLighting {
    glm::vec3 fogColor{0};        // 0..255
    float fogNear = 0, fogFar = 0;
    glm::vec3 worldAmbient{1};    // sceGuAmbient for sub-zone geometry
    glm::vec3 objectAmbient{1};   // sceGuAmbient for models (FUN_002189c0 without holder flag 0x1000)
    glm::vec3 lightColor[2]{};
    glm::vec3 lightDir[2]{};      // GE directional light vector: points towards the light
    bool lightOn[2]{};
};

struct Zone {
    std::string name;
    int index = -1;               // ZDT0 byte 7
    glm::vec3 bmin{0}, bmax{0};   // ZDT0: min, size
    ZoneLighting light;
};

bool LoadLand(const std::vector<uint8_t>& data, std::vector<Zone>& out);

// Zone whose box overlaps [bmin, bmax] the most (sub-zones and zones share their bounds), or the zone
// containing / closest to the box centre. -1 when there are no zones.
int FindZone(const std::vector<Zone>& zones, glm::vec3 bmin, glm::vec3 bmax);

} // namespace med
