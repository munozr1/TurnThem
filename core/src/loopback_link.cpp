#include "tt/loopback_link.h"
#include <algorithm>
#include <math.h>
#include "tt/bot.h"

namespace tt {

LoopbackLink::LoopbackLink(const LoopbackOptions& options) : options_(options), rng_(options.seed) {}

double LoopbackLink::DeliveryTime(double& last) {
    const double jitter = options_.jitter_s * (double)(rng_() % 1000) / 1000.0;
    last = std::max(last, now_ + options_.lag_s + jitter); // reliable-ordered: never overtake
    return last;
}

void LoopbackLink::Send(const ClientMsg& msg) {
    up_.push_back({DeliveryTime(last_up_), Encode(msg)});
}

bool LoopbackLink::Receive(ServerMsg& msg) {
    if (inbox_.empty()) return false;
    msg = std::move(inbox_.front());
    inbox_.pop_front();
    return true;
}

void LoopbackLink::BotAct() {
    const Match& match = host_->match();
    const Side bot = Opponent(options_.human);
    if (match.tick() < bot_next_tick_ || match.tick() < match.start_tick()) return;
    const uint8_t slot = (uint8_t)(rng_() % kHandSize);
    if (match.elixir(bot) < CardCostPoints(match.hand(bot)[slot])) {
        bot_next_tick_ = match.tick() + 20; // saving up
        return;
    }
    bot_next_tick_ = match.tick() + 30 + (uint32_t)(rng_() % 90); // then a 0.5-2 s pause
    const auto choice = ChooseBotPlacement(match.grid(), bot, rng_);
    if (!choice) return;
    host_->OnClientMessage(bot, MsgPlaceRequest{bot_req_id_++, slot, choice->col, choice->row, match.hand(bot)[slot]});
}

void LoopbackLink::Pump(double now_s) {
    now_ = now_s;

    // client -> "server"
    while (!up_.empty() && up_.front().at <= now_) {
        ClientMsg msg;
        if (Decode(up_.front().bytes.data(), up_.front().bytes.size(), msg)) {
            if (std::holds_alternative<MsgFindMatch>(msg) && (!host_ || host_->Finished())) {
                MatchConfig config = options_.config;
                config.seed = rng_();
                host_ = std::make_unique<MatchHost>(1, config);
                match_start_ = now_;
                steps_ = 0;
                bot_next_tick_ = 0;
            } else if (host_) {
                host_->OnClientMessage(options_.human, msg);
            }
        }
        up_.pop_front();
    }

    // step the match in real time
    if (host_) {
        const uint64_t due = (uint64_t)floor((now_ - match_start_) * kTickRate) + 1;
        int budget = kMaxStepsPerPump;
        while (steps_ < due && budget-- > 0 && !host_->Finished()) {
            if (options_.bot_opponent) BotAct();
            host_->Step();
            steps_++;
        }
        if (steps_ < due) { // fell behind: let the match run slow instead of fast-forwarding
            match_start_ = now_ - (double)steps_ / kTickRate;
        }
        for (auto& packet : host_->TakeOutbox(options_.human)) down_.push_back({DeliveryTime(last_down_), std::move(packet)});
        host_->TakeOutbox(Opponent(options_.human)); // the bot reads the match directly
    }

    // "server" -> client
    while (!down_.empty() && down_.front().at <= now_) {
        ServerMsg msg;
        if (Decode(down_.front().bytes.data(), down_.front().bytes.size(), msg)) inbox_.push_back(std::move(msg));
        down_.pop_front();
    }
}

} // namespace tt
