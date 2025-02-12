#include "mcp/client.h"
#include "mcp/transport.h"
#include <flatbuffers/flatbuffers.h>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>

namespace mcp {

class ClientImpl {
public:
    explicit ClientImpl(const ClientConfig& config)
        : config_(config)
        , running_(false)
        , next_request_id_(1) {
        
        // Initialize FlatBuffers builder
        fbb_.Clear();
        fbb_.ForceDefaults(true);
    }

    ~ClientImpl() {
        if (running_) {
            running_ = false;
            if (receive_thread_.joinable()) {
                receive_thread_.join();
            }
        }
    }

    bool initialize() {
        if (!transport_) {
            return false;
        }

        // Build client capabilities
        auto roots = fb::RootsCapability(true);
        auto sampling = fb::SamplingCapability();
        
        auto capabilities = fb::CreateClientCapabilities(
            fbb_,
            fbb_.CreateVector<fb::Capability*>({}),  // experimental
            &roots,
            &sampling
        );

        // Build client info
        auto client_info = fb::Implementation(
            fbb_.CreateString(config_.client_name),
            fbb_.CreateString(config_.client_version)
        );

        // Build initialize request
        auto init_request = fb::CreateInitializeRequest(
            fbb_,
            fbb_.CreateString(config_.protocol_version),
            capabilities,
            &client_info
        );

        // Send initialize request and wait for response
        auto response = send_request_internal(init_request);
        if (!response) {
            return false;
        }

        // Start receive thread
        running_ = true;
        receive_thread_ = std::thread(&ClientImpl::receive_loop, this);

        return true;
    }

    void set_transport(std::unique_ptr<Transport> transport) {
        transport_ = std::move(transport);
        if (transport_) {
            transport_->set_message_callback(
                [this](const std::vector<uint8_t>& data) {
                    handle_message(data);
                }
            );
        }
    }

    std::unique_ptr<fb::Message> send_request(const fb::Message& request) {
        return send_request_internal(&request);
    }

    void send_notification(const fb::Message& notification) {
        if (!transport_) return;

        fbb_.Clear();
        auto offset = notification.Pack(fbb_);
        fbb_.Finish(offset);

        std::vector<uint8_t> data(fbb_.GetBufferPointer(), 
                                 fbb_.GetBufferPointer() + fbb_.GetSize());
        transport_->send(data);
    }

    void set_message_callback(MessageCallback callback) {
        message_callback_ = std::move(callback);
    }

private:
    std::unique_ptr<fb::Message> send_request_internal(const flatbuffers::Offset<void>* request) {
        if (!transport_) return nullptr;

        // Assign request ID
        uint64_t request_id = next_request_id_++;
        
        // Build and send request
        fbb_.Clear();
        auto offset = fb::CreateMessage(
            fbb_,
            fb::MessageType_Request,
            request->Union()
        );
        fbb_.Finish(offset);

        std::vector<uint8_t> data(fbb_.GetBufferPointer(), 
                                 fbb_.GetBufferPointer() + fbb_.GetSize());
        
        {
            std::unique_lock<std::mutex> lock(pending_mutex_);
            pending_requests_[request_id] = std::make_shared<ResponsePromise>();
        }

        if (!transport_->send(data)) {
            return nullptr;
        }

        // Wait for response
        std::shared_ptr<ResponsePromise> promise;
        {
            std::unique_lock<std::mutex> lock(pending_mutex_);
            promise = pending_requests_[request_id];
        }

        std::unique_lock<std::mutex> lock(promise->mutex);
        if (!promise->cv.wait_for(lock, std::chrono::seconds(30), 
            [&] { return promise->response != nullptr; })) {
            // Timeout
            return nullptr;
        }

        return std::move(promise->response);
    }

    void receive_loop() {
        std::vector<uint8_t> buffer;
        while (running_) {
            if (transport_->receive(buffer) > 0) {
                handle_message(buffer);
            }
        }
    }

    void handle_message(const std::vector<uint8_t>& data) {
        auto message = fb::GetMessage(data.data());
        if (!message) return;

        switch (message->type()) {
            case fb::MessageType_Response: {
                auto response = message->response();
                if (!response) break;

                uint64_t request_id = response->id()->id();
                std::shared_ptr<ResponsePromise> promise;
                
                {
                    std::unique_lock<std::mutex> lock(pending_mutex_);
                    auto it = pending_requests_.find(request_id);
                    if (it != pending_requests_.end()) {
                        promise = it->second;
                        pending_requests_.erase(it);
                    }
                }

                if (promise) {
                    std::unique_lock<std::mutex> lock(promise->mutex);
                    promise->response = std::make_unique<fb::Message>(*message);
                    promise->cv.notify_one();
                }
                break;
            }
            case fb::MessageType_Notification:
                if (message_callback_) {
                    message_callback_(message);
                }
                break;
            default:
                break;
        }
    }

    struct ResponsePromise {
        std::mutex mutex;
        std::condition_variable cv;
        std::unique_ptr<fb::Message> response;
    };

    ClientConfig config_;
    std::unique_ptr<Transport> transport_;
    flatbuffers::FlatBufferBuilder fbb_;
    MessageCallback message_callback_;

    std::atomic<bool> running_;
    std::thread receive_thread_;
    std::atomic<uint64_t> next_request_id_;

    std::mutex pending_mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<ResponsePromise>> pending_requests_;
};

Client::Client(const ClientConfig& config)
    : impl_(std::make_unique<ClientImpl>(config)) {
}

Client::~Client() = default;

bool Client::initialize() {
    return impl_->initialize();
}

void Client::set_transport(std::unique_ptr<Transport> transport) {
    impl_->set_transport(std::move(transport));
}

std::unique_ptr<fb::Message> Client::send_request(const fb::Message& request) {
    return impl_->send_request(request);
}

void Client::send_notification(const fb::Message& notification) {
    impl_->send_notification(notification);
}

void Client::set_message_callback(MessageCallback callback) {
    impl_->set_message_callback(std::move(callback));
}

} // namespace mcp 