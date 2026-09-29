#pragma once

#include "JsonRPC/JsonRPC.h"

#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>

namespace Ryntra::LSP {
    class JsonRpcTransport {
    public:
        explicit JsonRpcTransport(std::istream &input, std::ostream &output);

        std::optional<nlohmann::json> readMessage();

        void writeMessage(const nlohmann::json &message);

    private:
        std::istream &input;
        std::ostream &output;
    };
} // namespace Ryntra::LSP
