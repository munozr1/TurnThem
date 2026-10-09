#include "tt/protocol.h"
#include <cstring>
#include <limits>
#include <math.h>

namespace tt {

static_assert(std::numeric_limits<float>::is_iec559, "floats go on the wire as IEEE-754 bits");

namespace {

enum MsgType : uint8_t {
    kFindMatch = 0x01,
    kCancelFind = 0x02,
    kPlaceRequest = 0x03,
    kLeaveMatch = 0x04,
    kQueued = 0x81,
    kMatchStart = 0x82,
    kTick = 0x83,
    kPlaceResult = 0x84,
    kHandSlot = 0x85,
    kMatchEnd = 0x86,
};

enum EventType : uint8_t { kWeaponPlaced = 1, kProjSpawn = 2, kProjHit = 3 };

// readers that reject out-of-range enums and cells
bool ReadSide(ByteReader& in, Side& side) {
    const uint8_t v = in.u8();
    side = (Side)v;
    return v <= 1;
}
bool ReadWeaponType(ByteReader& in, WeaponType& type) {
    const uint8_t v = in.u8();
    type = (WeaponType)v;
    return v < (uint8_t)WeaponType::Count;
}
bool ReadCell(ByteReader& in, uint8_t& col, uint8_t& row) {
    col = in.u8();
    row = in.u8();
    return Grid::InBounds(col, row);
}
bool ReadVec(ByteReader& in, Vec2& v) {
    v.x = in.f32();
    v.y = in.f32();
    return isfinite(v.x) && isfinite(v.y);
}
void WriteVec(ByteWriter& out, Vec2 v) {
    out.f32(v.x);
    out.f32(v.y);
}

} // namespace

const char* DisconnectText(uint32_t code) {
    switch ((DisconnectCode)code) {
        case DisconnectCode::None: return "disconnected";
        case DisconnectCode::VersionMismatch: return "game version doesn't match the server";
        case DisconnectCode::ProtocolError: return "protocol error";
        case DisconnectCode::Shutdown: return "server shut down";
        case DisconnectCode::ServerFull: return "server is full";
    }
    return "disconnected";
}

void ByteWriter::u16(uint16_t v) {
    u8((uint8_t)v);
    u8((uint8_t)(v >> 8));
}

void ByteWriter::u32(uint32_t v) {
    for (int i = 0; i < 4; i++) u8((uint8_t)(v >> (8 * i)));
}

void ByteWriter::f32(float v) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);
    u32(bits);
}

uint8_t ByteReader::u8() {
    if (!ok_ || pos_ >= size_) {
        ok_ = false;
        return 0;
    }
    return data_[pos_++];
}

uint16_t ByteReader::u16() {
    const uint16_t lo = u8();
    return (uint16_t)(lo | (u8() << 8));
}

uint32_t ByteReader::u32() {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= (uint32_t)u8() << (8 * i);
    return v;
}

float ByteReader::f32() {
    const uint32_t bits = u32();
    float v;
    memcpy(&v, &bits, sizeof v);
    return v;
}

std::vector<uint8_t> Encode(const ClientMsg& msg) {
    ByteWriter out;
    std::visit([&](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, MsgFindMatch>) {
            out.u8(kFindMatch);
        } else if constexpr (std::is_same_v<T, MsgCancelFind>) {
            out.u8(kCancelFind);
        } else if constexpr (std::is_same_v<T, MsgLeaveMatch>) {
            out.u8(kLeaveMatch);
        } else {
            out.u8(kPlaceRequest);
            out.u32(m.req_id);
            out.u8(m.slot);
            out.u8(m.col);
            out.u8(m.row);
            out.u8((uint8_t)m.expected);
        }
    }, msg);
    return std::move(out.bytes);
}

std::vector<uint8_t> Encode(const ServerMsg& msg) {
    ByteWriter out;
    std::visit([&](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, MsgQueued>) {
            out.u8(kQueued);
        } else if constexpr (std::is_same_v<T, MsgMatchStart>) {
            out.u8(kMatchStart);
            out.u32(m.match_id);
            out.u8((uint8_t)m.your_side);
            out.u32(m.start_tick);
            out.u32(m.end_tick);
            for (WeaponType card : m.hand) out.u8((uint8_t)card);
        } else if constexpr (std::is_same_v<T, MsgTick>) {
            out.u8(kTick);
            out.u32(m.tick);
            out.u8(m.has_checksum ? 1 : 0);
            if (m.has_checksum) out.u32(m.checksum);
            out.u16((uint16_t)m.events.size());
            for (const TickEvent& event : m.events) {
                std::visit([&](const auto& e) {
                    using E = std::decay_t<decltype(e)>;
                    if constexpr (std::is_same_v<E, EvWeaponPlaced>) {
                        out.u8(kWeaponPlaced);
                        out.u32(e.weapon_id);
                        out.u8((uint8_t)e.type);
                        out.u8((uint8_t)e.side);
                        out.u8(e.col);
                        out.u8(e.row);
                    } else if constexpr (std::is_same_v<E, EvProjSpawned>) {
                        out.u8(kProjSpawn);
                        out.u32(e.proj_id);
                        out.u32(e.weapon_id);
                        WriteVec(out, e.pos);
                        WriteVec(out, e.vel);
                    } else {
                        out.u8(kProjHit);
                        out.u32(e.proj_id);
                        out.u8(e.col);
                        out.u8(e.row);
                        WriteVec(out, e.point);
                    }
                }, event);
            }
        } else if constexpr (std::is_same_v<T, MsgPlaceResult>) {
            out.u8(kPlaceResult);
            out.u32(m.req_id);
            out.u8((uint8_t)m.err);
            out.u32(m.weapon_id);
        } else if constexpr (std::is_same_v<T, MsgHandSlot>) {
            out.u8(kHandSlot);
            out.u8(m.slot);
            out.u8((uint8_t)m.type);
        } else {
            out.u8(kMatchEnd);
            out.u8((uint8_t)m.reason);
            out.u8((uint8_t)m.winner);
            out.u16(m.white);
            out.u16(m.black);
            out.u32(m.end_tick);
        }
    }, msg);
    return std::move(out.bytes);
}

bool Decode(const uint8_t* data, size_t size, ClientMsg& out) {
    ByteReader in(data, size);
    switch (in.u8()) {
        case kFindMatch: out = MsgFindMatch{}; break;
        case kCancelFind: out = MsgCancelFind{}; break;
        case kLeaveMatch: out = MsgLeaveMatch{}; break;
        case kPlaceRequest: {
            MsgPlaceRequest m;
            m.req_id = in.u32();
            m.slot = in.u8();
            m.col = in.u8(); // the simulation reports bad cells/slots as PlaceErrors
            m.row = in.u8();
            if (!ReadWeaponType(in, m.expected)) return false;
            out = m;
            break;
        }
        default: return false;
    }
    return in.done();
}

bool Decode(const uint8_t* data, size_t size, ServerMsg& out) {
    ByteReader in(data, size);
    switch (in.u8()) {
        case kQueued: out = MsgQueued{}; break;
        case kMatchStart: {
            MsgMatchStart m;
            m.match_id = in.u32();
            if (!ReadSide(in, m.your_side)) return false;
            m.start_tick = in.u32();
            m.end_tick = in.u32();
            for (WeaponType& card : m.hand)
                if (!ReadWeaponType(in, card)) return false;
            if (m.end_tick < m.start_tick) return false;
            out = m;
            break;
        }
        case kTick: {
            MsgTick m;
            m.tick = in.u32();
            const uint8_t flags = in.u8();
            if (flags > 1) return false;
            m.has_checksum = flags & 1;
            if (m.has_checksum) m.checksum = in.u32();
            const uint16_t count = in.u16();
            if (count > size) return false; // every event is at least one byte
            m.events.reserve(count);
            for (uint16_t i = 0; i < count && in.ok(); i++) {
                switch (in.u8()) {
                    case kWeaponPlaced: {
                        EvWeaponPlaced e;
                        e.weapon_id = in.u32();
                        if (!ReadWeaponType(in, e.type) || !ReadSide(in, e.side) || !ReadCell(in, e.col, e.row)) return false;
                        m.events.push_back(e);
                        break;
                    }
                    case kProjSpawn: {
                        EvProjSpawned e;
                        e.proj_id = in.u32();
                        e.weapon_id = in.u32();
                        if (!ReadVec(in, e.pos) || !ReadVec(in, e.vel)) return false;
                        m.events.push_back(e);
                        break;
                    }
                    case kProjHit: {
                        EvProjHit e;
                        e.proj_id = in.u32();
                        if (!ReadCell(in, e.col, e.row) || !ReadVec(in, e.point)) return false;
                        m.events.push_back(e);
                        break;
                    }
                    default: return false;
                }
            }
            out = std::move(m);
            break;
        }
        case kPlaceResult: {
            MsgPlaceResult m;
            m.req_id = in.u32();
            const uint8_t err = in.u8();
            if (err >= (uint8_t)PlaceError::Count) return false;
            m.err = (PlaceError)err;
            m.weapon_id = in.u32();
            out = m;
            break;
        }
        case kHandSlot: {
            MsgHandSlot m;
            m.slot = in.u8();
            if (m.slot >= kHandSize || !ReadWeaponType(in, m.type)) return false;
            out = m;
            break;
        }
        case kMatchEnd: {
            MsgMatchEnd m;
            const uint8_t reason = in.u8(), winner = in.u8();
            if (reason >= (uint8_t)EndReason::Count || winner >= (uint8_t)Winner::Count) return false;
            m.reason = (EndReason)reason;
            m.winner = (Winner)winner;
            m.white = in.u16();
            m.black = in.u16();
            m.end_tick = in.u32();
            out = m;
            break;
        }
        default: return false;
    }
    return in.done();
}

} // namespace tt
