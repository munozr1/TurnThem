#include "tt/bot_client.h"
#include "tt/bot.h"

namespace tt {

BotClient::BotClient(std::unique_ptr<Link> link, uint64_t seed, double place_every_s)
    : link_(std::move(link)), rng_(seed), place_every_s_(place_every_s) {}

bool BotClient::finished() const {
    return view_.result().has_value() || link_->status() == LinkStatus::Closed;
}

void BotClient::Leave() {
    link_->Send(MsgLeaveMatch{});
}

void BotClient::Pump(double now_s) {
    link_->Pump(now_s);
    if (!asked_ && link_->status() == LinkStatus::Connected) {
        link_->Send(MsgFindMatch{});
        asked_ = true;
    }
    ServerMsg msg;
    while (link_->Receive(msg)) view_.OnMessage(msg, now_s);
    view_.Update(now_s);
    if (view_.TakeRejection()) rejected_++;

    if (view_.phase() != MatchView::Phase::Running || now_s < next_place_) return;
    next_place_ = now_s + place_every_s_;
    const auto choice = ChooseBotPlacement(view_.grid(), view_.side(), rng_, [&](int col, int row) {
        return view_.CheckLocal(col, row) == PlaceError::Ok;
    });
    if (!choice) return;
    const uint8_t first = (uint8_t)(rng_() % kHandSize);
    for (uint8_t i = 0; i < kHandSize; i++) {
        if (auto request = view_.RequestPlace((uint8_t)((first + i) % kHandSize), choice->col, choice->row)) {
            link_->Send(*request);
            placed_++;
            return;
        }
    }
}

} // namespace tt
