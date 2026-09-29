#include "Protocol/Initialize.h"

#include <utility>

namespace Ryntra::LSP::Protocol {
    InitializeParams parseInitializeParams(const nlohmann::json &params) {
        InitializeParams result;

        if (!params.is_object()) {
            return result;
        }

        if (params.contains("processId") && params.at("processId").is_number_integer()) {
            result.processId = params.at("processId").get<std::int32_t>();
        }

        if (params.contains("rootUri") && params.at("rootUri").is_string()) {
            result.rootUri = params.at("rootUri").get<std::string>();
        }

        if (params.contains("clientInfo") && params.at("clientInfo").is_object()) {
            const nlohmann::json &clientInfo = params.at("clientInfo");

            if (clientInfo.contains("name") && clientInfo.at("name").is_string()) {
                ClientInfo parsed;
                parsed.name = clientInfo.at("name").get<std::string>();

                if (clientInfo.contains("version") && clientInfo.at("version").is_string()) {
                    parsed.version = clientInfo.at("version").get<std::string>();
                }

                result.clientInfo = std::move(parsed);
            }
        }

        if (params.contains("initializationOptions")) {
            result.initializationOptions = params.at("initializationOptions");
        }

        return result;
    }

    nlohmann::json serializeInitializeResult(const InitializeResult &result) {
        nlohmann::json capabilities = {
            {"textDocumentSync", static_cast<std::int32_t>(result.capabilities.textDocumentSync)},
            {"hoverProvider", result.capabilities.hoverProvider},
            {"definitionProvider", result.capabilities.definitionProvider},
        };

        nlohmann::json serverInfo = {
            {"name", result.serverInfo.name},
        };

        if (result.serverInfo.version.has_value()) {
            serverInfo["version"] = *result.serverInfo.version;
        }

        return {
            {"capabilities", std::move(capabilities)},
            {"serverInfo", std::move(serverInfo)},
        };
    }
} // namespace Ryntra::LSP::Protocol
