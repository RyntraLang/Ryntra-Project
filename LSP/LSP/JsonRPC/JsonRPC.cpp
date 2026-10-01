#include "JsonRPC/JsonRPC.h"

#include <utility>

namespace Ryntra::LSP {
    JsonRPCException::JsonRPCException(JsonRPCError error) : error(std::move(error)), description(this->error.message) {
    }

    const JsonRPCError &JsonRPCException::getError() const noexcept {
        return error;
    }

    const char *JsonRPCException::what() const noexcept {
        return description.c_str();
    }

    nlohmann::json serialize(const JsonRPCRequest &request) {
        return {
            {"jsonrpc", kJsonRpcVersion},
            {"id", request.id},
            {"method", request.method},
            {"params", request.params},
        };
    }

    nlohmann::json serialize(const JsonRPCNotification &notification) {
        return {
            {"jsonrpc", kJsonRpcVersion},
            {"method", notification.method},
            {"params", notification.params},
        };
    }

    nlohmann::json serialize(const JsonRPCResponse &response) {
        return {
            {"jsonrpc", kJsonRpcVersion},
            {"id", response.id},
            {"result", response.result},
        };
    }

    nlohmann::json serialize(const JsonRPCError &error) {
        nlohmann::json result = {
            {"code", static_cast<std::int32_t>(error.code)},
            {"message", error.message},
        };

        if (!error.data.is_null()) {
            result["data"] = error.data;
        }

        return result;
    }

    nlohmann::json serialize(const JsonRPCErrorResponse &response) {
        return {
            {"jsonrpc", kJsonRpcVersion},
            {"id", response.id},
            {"error", serialize(response.error)},
        };
    }

    nlohmann::json serialize(const JsonRPCMessage &message) {
        return std::visit([](const auto &value) { return serialize(value); }, message);
    }

    JsonRPCMessage parseMessage(const nlohmann::json &message) {
        if (!message.is_object()) {
            throw JsonRPCException({JsonRPCErrorCode::InvalidRequest, "A JSON-RPC message must be a JSON object."});
        }

        if (!message.contains("jsonrpc") || !message.at("jsonrpc").is_string() || message.at("jsonrpc").get<std::string>() != kJsonRpcVersion) {
            throw JsonRPCException({JsonRPCErrorCode::InvalidRequest, "Missing or unsupported JSON-RPC version."});
        }

        const bool hasId = message.contains("id");
        const bool hasMethod = message.contains("method");

        if (hasMethod) {
            if (!message.at("method").is_string()) {
                throw JsonRPCException({JsonRPCErrorCode::InvalidRequest, "The 'method' field must be a string."});
            }

            const std::string method = message.at("method").get<std::string>();
            const nlohmann::json params = message.contains("params") ? message.at("params") : nlohmann::json::object();

            if (hasId) {
                return JsonRPCRequest{method, params, message.at("id")};
            }

            return JsonRPCNotification{method, params};
        }

        if (!hasId) {
            throw JsonRPCException({JsonRPCErrorCode::InvalidRequest, "A response must contain an 'id' field."});
        }

        const nlohmann::json id = message.at("id");
        const bool hasResult = message.contains("result");
        const bool hasError = message.contains("error");

        if (hasResult == hasError) {
            throw JsonRPCException({JsonRPCErrorCode::InvalidRequest, "A response must contain exactly one of 'result' or 'error'."});
        }

        if (hasError) {
            const nlohmann::json &rawError = message.at("error");

            if (!rawError.is_object() || !rawError.contains("code") || !rawError.at("code").is_number_integer() || !rawError.contains("message") || !rawError.at("message").is_string()) {
                throw JsonRPCException({JsonRPCErrorCode::InvalidRequest, "The 'error' field is malformed."});
            }

            JsonRPCError error;
            error.code = static_cast<JsonRPCErrorCode>(rawError.at("code").get<std::int32_t>());
            error.message = rawError.at("message").get<std::string>();
            error.data = rawError.contains("data") ? rawError.at("data") : nlohmann::json(nullptr);

            return JsonRPCErrorResponse{id, error};
        }

        return JsonRPCResponse{id, message.at("result")};
    }
} // namespace Ryntra::LSP
