#pragma once
#include <array>
#include <vector>
#include "tt/grid.h"
#include "tt/weapons.h"

namespace tt {

enum class MatchPhase : uint8_t { Setup, Combat, Ended };
enum class CombatTargetKind : uint8_t { None, Weapon, Infantry, Headquarters, Count };
enum class InfantryPhase : uint8_t { Advancing, Attacking, Count };

constexpr uint16_t kBaseMaxHealth = 1200;
constexpr float kBaseRadius = 28.0f;
constexpr int kSquadSize = 3;
constexpr int kMaxInfantryPerSide = 24;
constexpr uint16_t kInfantryMaxHealth = 90;
constexpr uint16_t kInfantryDamage = 10;
constexpr uint16_t kInfantryAttackTicks = 36;
constexpr float kInfantrySpeed = 55.0f;
constexpr float kInfantryRadius = 7.0f;
constexpr float kInfantryRange = 92.0f;
constexpr float kInfantryAcquireRange = 180.0f;

struct HeadquartersState {
    uint32_t id = 0;
    Side side = Side::White;
    Vec2 pos{};
    uint16_t health = kBaseMaxHealth;
};

struct InfantryState {
    uint32_t id = 0;
    Side side = Side::White;
    Vec2 pos{};
    uint16_t health = kInfantryMaxHealth;
    CombatTargetKind target_kind = CombatTargetKind::None;
    uint32_t target_id = 0;
    InfantryPhase phase = InfantryPhase::Advancing;
    uint16_t cooldown = 0;
};

Vec2 BaseCenter(Side side);
std::array<HeadquartersState, 2> MakeHeadquarters();
// Infantry occupies continuous space, not permanent artillery cells/padding.
// Validates all three formation positions and the per-side cap atomically.
PlaceError ValidateInfantryPlacement(const Grid& grid, Side side, int col, int row,
                                    const std::vector<InfantryState>& infantry);
PlaceError ValidateArtilleryPlacement(const Grid& grid, Side side, int col, int row,
                                      const std::vector<InfantryState>& infantry);
std::array<Vec2, kSquadSize> SquadPositions(Side side, int col, int row);

// Shared server/view hash: excludes locally replayed cosmetic weapon animation.
uint32_t BattleChecksum(const Grid& grid, const std::vector<WeaponState>& weapons,
                        const std::array<HeadquartersState, 2>& bases,
                        const std::vector<InfantryState>& infantry);

} // namespace tt
