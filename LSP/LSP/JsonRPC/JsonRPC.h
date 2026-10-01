#pragma once

#include <cstdint>
#include <exception>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>

namespace Ryntra::LSP {
    inline const std::string kJsonRpcVersion = "2.0";

    enum class JsonRPCErrorCode : std::int32_t {
        // Errors defined by the JSON-RPC 2.0 specification.
        ParseError = -32700,
        InvalidRequest = -32600,
        MethodNotFound = -32601,
        InvalidParams = -32602,
        InternalError = -32603,
        // Errors defined by the LSP specification.
        ServerNotInitialized = -32002,
        UnknownErrorCode = -32001,
        RequestFailed = -32803,
        ServerCancelled = -32802,
        ContentModified = -32801,
        RequestCancelled = -32800,
    };

    struct JsonRPCRequest {
        std::string method;
        nlohmann::json params = nlohmann::json::object();
        nlohmann::json id;
    };

    struct JsonRPCNotification {
        std::string method;
        nlohmann::json params = nlohmann::json::object();
    };

    struct JsonRPCResponse {
        nlohmann::json id;
        nlohmann::json result;
    };

    struct JsonRPCError {
        JsonRPCErrorCode code = JsonRPCErrorCode::InternalError;
        std::string message;
        nlohmann::json data = nullptr;
    };

    struct JsonRPCErrorResponse {
        nlohmann::json id;
        JsonRPCError error;
    };

    using JsonRPCMessage = std::variant<JsonRPCRequest, JsonRPCNotification, JsonRPCResponse, JsonRPCErrorResponse>;

    class JsonRPCException : public std::exception {
    public:
        explicit JsonRPCException(JsonRPCError error);

        [[nodiscard]] const JsonRPCError &getError() const noexcept;

        [[nodiscard]] const char *what() const noexcept override;

    private:
        JsonRPCError error;
        std::string description;
    };

    nlohmann::json serialize(const JsonRPCRequest &request);

    nlohmann::json serialize(const JsonRPCNotification &notification);

    nlohmann::json serialize(const JsonRPCResponse &response);

    nlohmann::json serialize(const JsonRPCError &error);

    nlohmann::json serialize(const JsonRPCErrorResponse &response);

    nlohmann::json serialize(const JsonRPCMessage &message);

    JsonRPCMessage parseMessage(const nlohmann::json &message);
} // namespace Ryntra::LSP
