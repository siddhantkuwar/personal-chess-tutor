#pragma once

#include <atomic>
#include <memory>
#include <utility>

namespace pct {

class CancellationToken {
  public:
    CancellationToken() = default;

    [[nodiscard]] bool stop_requested() const noexcept {
        return state_ && state_->load(std::memory_order_acquire);
    }

  private:
    explicit CancellationToken(std::shared_ptr<std::atomic_bool> state)
        : state_(std::move(state)) {}

    std::shared_ptr<std::atomic_bool> state_;

    friend class CancellationSource;
};

class CancellationSource {
  public:
    CancellationSource() : state_(std::make_shared<std::atomic_bool>(false)) {}

    [[nodiscard]] CancellationToken get_token() const noexcept {
        return CancellationToken(state_);
    }

    bool request_stop() noexcept {
        return !state_->exchange(true, std::memory_order_acq_rel);
    }

    [[nodiscard]] bool stop_requested() const noexcept {
        return state_->load(std::memory_order_acquire);
    }

  private:
    std::shared_ptr<std::atomic_bool> state_;
};

} // namespace pct
