#include "mcp/client.h"
#include "mcp/transport.h"
#include <iostream>
#include <string>

int main() {
    // Configure the client
    mcp::ClientConfig config;
    config.client_name = "SimpleClient";
    config.client_version = "1.0.0";
    config.enable_logging = true;

    // Create client instance
    mcp::Client client(config);

    // Create and set stdio transport
    auto transport = mcp::create_stdio_transport(nullptr);
    client.set_transport(std::move(transport));

    // Initialize the client
    if (!client.initialize()) {
        std::cerr << "Failed to initialize client" << std::endl;
        return 1;
    }

    // Set up message callback
    client.set_message_callback([](const mcp::fb::Message* message) {
        if (message->type() == mcp::fb::MessageType_Notification) {
            std::cout << "Received notification" << std::endl;
        }
    });

    // Build and send a simple request
    flatbuffers::FlatBufferBuilder fbb;
    
    // Create a request to list available tools
    auto request = mcp::fb::CreateMessage(
        fbb,
        mcp::fb::MessageType_Request,
        mcp::fb::CreateRequest(
            fbb,
            fbb.CreateString("2.0"),
            mcp::fb::CreateRequestId(fbb, 1),
            fbb.CreateString("list_tools"),
            fbb.CreateString("{}") // Empty params
        ).Union()
    );
    fbb.Finish(request);

    // Send request and wait for response
    auto response = client.send_request(*mcp::fb::GetMessage(fbb.GetBufferPointer()));
    if (response) {
        if (response->type() == mcp::fb::MessageType_Response) {
            auto result = response->response()->result();
            if (result) {
                std::cout << "Received response: " << result->str() << std::endl;
            }
        } else if (response->type() == mcp::fb::MessageType_Error) {
            std::cerr << "Received error: " << response->error()->message()->str() << std::endl;
        }
    } else {
        std::cerr << "No response received" << std::endl;
    }

    return 0;
} 