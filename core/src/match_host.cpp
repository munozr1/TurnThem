#include "tt/match_host.h"

namespace tt {

MatchHost::MatchHost(uint32_t match_id, const MatchConfig& config) : id_(match_id), match_(config) {
    for (Side side : {Side::White, Side::Black}) {
        Send(side, MsgMatchStart{id_, side, match_.start_tick(), match_.end_tick(), match_.hand(side)});
    }
}

void MatchHost::OnClientMessage(Side side, const ClientMsg& msg) {
    if (auto* place = std::get_if<MsgPlaceRequest>(&msg)) {
        match_.SubmitPlace(side, place->req_id, place->slot, place->col, place->row, place->expected);
    } else if (std::holds_alternative<MsgLeaveMatch>(msg)) {
        match_.Forfeit(side);
    }
}

void MatchHost::Forfeit(Side side) {
    match_.Forfeit(side);
}

void MatchHost::Send(Side side, const ServerMsg& msg) {
    outbox_[(int)side].push_back(Encode(msg));
}

void MatchHost::Step() {
    if (match_.ended()) return;
    const uint32_t tick = match_.tick();
    events_.clear();
    match_.Step(events_);

    MsgTick shared;
    shared.tick = tick;
    if (tick % kChecksumEvery == 0) {
        shared.has_checksum = true;
        shared.checksum = match_.grid().Checksum();
    }
    std::vector<std::pair<Side, ServerMsg>> private_msgs;
    const EvMatchEnded* ended = nullptr;
    for (const Event& event : events_) {
        if (auto* e = std::get_if<EvWeaponPlaced>(&event)) shared.events.push_back(*e);
        else if (auto* e = std::get_if<EvProjSpawned>(&event)) shared.events.push_back(*e);
        else if (auto* e = std::get_if<EvProjHit>(&event)) shared.events.push_back(*e);
        else if (auto* e = std::get_if<EvPlaceResult>(&event)) private_msgs.push_back({e->to, MsgPlaceResult{e->req_id, e->err, e->weapon_id}});
        else if (auto* e = std::get_if<EvHandSlot>(&event)) private_msgs.push_back({e->to, MsgHandSlot{e->slot, e->type}});
        else ended = &std::get<EvMatchEnded>(event);
    }

    for (Side side : {Side::White, Side::Black}) Send(side, shared);
    for (const auto& [side, msg] : private_msgs) Send(side, msg);
    if (ended) {
        const MsgMatchEnd end{ended->reason, ended->winner, ended->white, ended->black, tick};
        for (Side side : {Side::White, Side::Black}) Send(side, end);
    }
}

std::vector<MatchHost::Packet> MatchHost::TakeOutbox(Side side) {
    std::vector<Packet> out;
    out.swap(outbox_[(int)side]);
    return out;
}

} // namespace tt
