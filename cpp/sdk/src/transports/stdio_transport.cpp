#include "mcp/transport.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

namespace mcp {

class StdioTransport : public Transport {
public:
    explicit StdioTransport(const fb::StdioTransport* config) 
        : buffer_size_(config ? config->buffer_size() : 8192)
        , running_(false) {
        
        #ifdef _WIN32
        // Set binary mode for Windows
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
        #endif
    }

    ~StdioTransport() override {
        close();
    }

    bool initialize() override {
        if (running_) return true;

        running_ = true;
        receive_thread_ = std::thread(&StdioTransport::receive_loop, this);
        return true;
    }

    bool send(const std::vector<uint8_t>& data) override {
        if (!running_) return false;

        std::lock_guard<std::mutex> lock(write_mutex_);
        
        // Write message length
        uint32_t length = static_cast<uint32_t>(data.size());
        if (fwrite(&length, sizeof(length), 1, stdout) != 1) {
            return false;
        }

        // Write message data
        if (fwrite(data.data(), 1, data.size(), stdout) != data.size()) {
            return false;
        }

        fflush(stdout);
        return true;
    }

    int receive(std::vector<uint8_t>& data) override {
        // Not used directly - we use the callback mechanism instead
        return -1;
    }

    void set_message_callback(MessageCallback callback) override {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        message_callback_ = std::move(callback);
    }

    void close() override {
        if (running_) {
            running_ = false;
            if (receive_thread_.joinable()) {
                receive_thread_.join();
            }
        }
    }

    bool is_connected() const override {
        return running_;
    }

private:
    void receive_loop() {
        std::vector<uint8_t> buffer(buffer_size_);
        
        while (running_) {
            // Read message length
            uint32_t length;
            if (fread(&length, sizeof(length), 1, stdin) != 1) {
                break;
            }

            // Ensure buffer is large enough
            if (length > buffer.size()) {
                buffer.resize(length);
            }

            // Read message data
            if (fread(buffer.data(), 1, length, stdin) != length) {
                break;
            }

            // Call message callback if set
            MessageCallback callback;
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                callback = message_callback_;
            }

            if (callback) {
                std::vector<uint8_t> message_data(buffer.begin(), buffer.begin() + length);
                callback(message_data);
            }
        }

        running_ = false;
    }

    const size_t buffer_size_;
    std::atomic<bool> running_;
    std::thread receive_thread_;
    std::mutex write_mutex_;
    std::mutex callback_mutex_;
    MessageCallback message_callback_;
};

std::unique_ptr<Transport> create_stdio_transport(const fb::StdioTransport* config) {
    return std::make_unique<StdioTransport>(config);
}

} // namespace mcp 