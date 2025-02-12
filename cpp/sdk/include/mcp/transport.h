#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "mcp_generated.h"

namespace mcp {

/**
 * @brief Abstract base class for MCP transports
 */
class Transport {
public:
    virtual ~Transport() = default;

    /**
     * @brief Initialize the transport
     * @return true if initialization was successful
     */
    virtual bool initialize() = 0;

    /**
     * @brief Send raw data over the transport
     * @param data The data to send
     * @return true if send was successful
     */
    virtual bool send(const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Receive raw data from the transport
     * @param data Buffer to store received data
     * @return Number of bytes received, or -1 on error
     */
    virtual int receive(std::vector<uint8_t>& data) = 0;

    /**
     * @brief Set callback for received messages
     */
    using MessageCallback = std::function<void(const std::vector<uint8_t>&)>;
    virtual void set_message_callback(MessageCallback callback) = 0;

    /**
     * @brief Close the transport connection
     */
    virtual void close() = 0;

    /**
     * @brief Check if transport is connected
     */
    virtual bool is_connected() const = 0;
};

/**
 * @brief Create a transport instance based on the given configuration
 */
std::unique_ptr<Transport> create_transport(const fb::Transport* config);

/**
 * @brief Create a stdio transport
 */
std::unique_ptr<Transport> create_stdio_transport(const fb::StdioTransport* config);

/**
 * @brief Create an SSE transport
 */
std::unique_ptr<Transport> create_sse_transport(const fb::SSETransport* config);

} // namespace mcp 