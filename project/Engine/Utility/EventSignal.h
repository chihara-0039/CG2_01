#pragma once
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

// メインスレッド用Observer。購読の寿命と通知元の寿命を独立させる。
template <class Event> class EventSignal {
    struct Storage {
        size_t next = 0;
        std::map<size_t, std::function<void(const Event &)>> callbacks;
    };

  public:
    class Connection {
      public:
        Connection() = default;
        Connection(const Connection &) = delete;
        Connection &operator=(const Connection &) = delete;
        Connection(Connection &&other) noexcept : storage_(std::move(other.storage_)), id_(other.id_) {}
        // 古い購読を解除してから、新しい購読の所有権を受け取る。
        Connection &operator=(Connection &&other) noexcept {
            if (this != &other) {
                Disconnect();
                storage_ = std::move(other.storage_);
                id_ = other.id_;
            }
            return *this;
        }
        ~Connection() { Disconnect(); }
        // 通知元が先に消えていても安全に解除する。
        void Disconnect() {
            if (auto storage = storage_.lock()) {
                storage->callbacks.erase(id_);
            }
            storage_.reset();
        }

      private:
        friend class EventSignal;
        Connection(std::weak_ptr<Storage> storage, size_t id) : storage_(std::move(storage)), id_(id) {}
        std::weak_ptr<Storage> storage_;
        size_t id_ = 0;
    };
    EventSignal() = default;
    EventSignal(const EventSignal &) = delete;
    EventSignal &operator=(const EventSignal &) = delete;
    // Connectionを保持している間だけ通知を受け取る。
    Connection Subscribe(std::function<void(const Event &)> callback) {
        const auto id = ++storage_->next;
        storage_->callbacks.emplace(id, std::move(callback));
        return Connection(storage_, id);
    }
    // 通知中の購読解除を許可し、通知中に追加された購読は次回から呼ぶ。
    void Publish(const Event &event) const {
        const auto storage = storage_;
        std::vector<size_t> ids;
        for (const auto &[id, callback] : storage->callbacks) {
            ids.push_back(id);
        }
        for (const auto id : ids) {
            const auto it = storage->callbacks.find(id);
            if (it != storage->callbacks.end()) {
                const auto callback = it->second;
                if (callback) {
                    callback(event);
                }
            }
        }
    }

  private:
    std::shared_ptr<Storage> storage_ = std::make_shared<Storage>();
};
