#pragma once

#include <memory>
#include <string>
#include <functional>
#include <vector>
#include "mcp_generated.h"

namespace mcp {

// Forward declarations
class Transport;
class ClientImpl;

/**
 * @brief Configuration for the MCP client
 */
struct ClientConfig {
    std::string protocol_version = "1.0";
    std::string client_name;
    std::string client_version;
    bool enable_logging = false;
    bool enable_sampling = true;
    bool enable_resources = true;
    bool enable_tools = true;
    bool enable_prompts = true;
};

/**
 * @brief Callback type for message handling
 */
using MessageCallback = std::function<void(const fb::Message*)>;

/**
 * @brief Main MCP client class
 */
class Client {
public:
    /**
     * @brief Create a new MCP client with the given configuration
     */
    explicit Client(const ClientConfig& config);
    ~Client();

    /**
     * @brief Initialize the client connection
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Set the transport to use for communication
     */
    void set_transport(std::unique_ptr<Transport> transport);

    /**
     * @brief Send a request and wait for response
     * @param request The request message to send
     * @return Response message or nullptr on error
     */
    std::unique_ptr<fb::Message> send_request(const fb::Message& request);

    /**
     * @brief Send a notification (no response expected)
     * @param notification The notification message to send
     */
    void send_notification(const fb::Message& notification);

    /**
     * @brief Register a callback for incoming messages
     */
    void set_message_callback(MessageCallback callback);

    /**
     * @brief Get the client capabilities
     */
    const fb::ClientCapabilities* capabilities() const;

    /**
     * @brief Get the server capabilities (available after initialization)
     */
    const fb::ServerCapabilities* server_capabilities() const;

private:
    std::unique_ptr<ClientImpl> impl_;
};

} // namespace mcp 