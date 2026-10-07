#pragma once
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>
#include "tt/match.h"

// Wire format: one ENet reliable-ordered channel, one message per packet, first
// byte is the message type, all integers little-endian, floats as IEEE-754 bits.
namespace tt {

constexpr uint32_t kProtocolVersion = 1; // sent as ENet's connect data
constexpr uint16_t kDefaultPort = 7777;
constexpr uint32_t kChecksumEvery = 60;  // ticks between grid checksums in TICK

// ENet disconnect data
enum class DisconnectCode : uint32_t { None = 0, VersionMismatch = 1, ProtocolError = 2, Shutdown = 3, ServerFull = 4 };
const char* DisconnectText(uint32_t code);

class ByteWriter {
public:
    void u8(uint8_t v) { bytes.push_back(v); }
    void u16(uint16_t v);
    void u32(uint32_t v);
    void f32(float v);
    std::vector<uint8_t> bytes;
};

// Bounds-checked: reading past the end sets !ok() and returns zeros.
class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}
    uint8_t u8();
    uint16_t u16();
    uint32_t u32();
    float f32();
    bool ok() const { return ok_; }
    bool done() const { return ok_ && pos_ == size_; } // consumed exactly everything
    void fail() { ok_ = false; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
    bool ok_ = true;
};

// client -> server
struct MsgFindMatch {};
struct MsgCancelFind {};
struct MsgLeaveMatch {};
struct MsgPlaceRequest {
    uint32_t req_id;
    uint8_t slot, col, row;
    WeaponType expected; // what the client thinks is in the slot
};
using ClientMsg = std::variant<MsgFindMatch, MsgCancelFind, MsgLeaveMatch, MsgPlaceRequest>;

// server -> client
using TickEvent = std::variant<EvWeaponPlaced, EvProjSpawned, EvProjHit>;
struct MsgQueued {};
struct MsgMatchStart {
    uint32_t match_id;
    Side your_side;
    uint32_t start_tick, end_tick;
    Hand hand;
};
// sent every tick, events or not: it is also the clock
struct MsgTick {
    uint32_t tick = 0;
    bool has_checksum = false;
    uint32_t checksum = 0; // Grid::Checksum() after this tick
    std::vector<TickEvent> events;
};
struct MsgPlaceResult {
    uint32_t req_id;
    PlaceError err;
    uint32_t weapon_id;
};
struct MsgHandSlot {
    uint8_t slot;
    WeaponType type;
};
struct MsgMatchEnd {
    EndReason reason;
    Winner winner;
    uint16_t white, black;
    uint32_t end_tick;
};
using ServerMsg = std::variant<MsgQueued, MsgMatchStart, MsgTick, MsgPlaceResult, MsgHandSlot, MsgMatchEnd>;

std::vector<uint8_t> Encode(const ClientMsg& msg);
std::vector<uint8_t> Encode(const ServerMsg& msg);
// false on any malformed input (unknown type, bad enum, out-of-range cell, truncated, trailing bytes)
bool Decode(const uint8_t* data, size_t size, ClientMsg& out);
bool Decode(const uint8_t* data, size_t size, ServerMsg& out);

} // namespace tt
