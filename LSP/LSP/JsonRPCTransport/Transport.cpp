#include "JsonRPCTransport/Transport.h"

#include <cctype>
#include <string_view>

namespace Ryntra::LSP {
    namespace {
        std::string_view trim(std::string_view text) {
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
                text.remove_prefix(1);
            }

            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
                text.remove_suffix(1);
            }

            return text;
        }
    } // anonymous namespace

    JsonRpcTransport::JsonRpcTransport(std::istream &input, std::ostream &output) : input(input), output(output) {
    }

    std::optional<nlohmann::json> JsonRpcTransport::readMessage() {
        constexpr std::string_view prefix = "Content-Length:";

        std::size_t contentLength = 0;
        bool hasContentLength = false;
        std::string line;

        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty()) {
                break;
            }

            if (line.starts_with(prefix)) {
                const std::string_view value = trim(std::string_view(line).substr(prefix.size()));

                try {
                    contentLength = std::stoull(std::string(value));
                    hasContentLength = true;
                } catch (const std::exception &) {
                    throw JsonRPCException({JsonRPCErrorCode::ParseError, "Malformed 'Content-Length' header."});
                }
            }
        }

        if (!hasContentLength) {
            return std::nullopt;
        }

        std::string body(contentLength, '\0');

        if (contentLength > 0) {
            input.read(body.data(), static_cast<std::streamsize>(contentLength));

            if (input.gcount() != static_cast<std::streamsize>(contentLength)) {
                return std::nullopt;
            }
        }

        try {
            return nlohmann::json::parse(body);
        } catch (const nlohmann::json::parse_error &error) {
            throw JsonRPCException({JsonRPCErrorCode::ParseError, error.what()});
        }
    }

    void JsonRpcTransport::writeMessage(const nlohmann::json &message) {
        const std::string body = message.dump();

        output << "Content-Length: " << body.size() << "\r\n\r\n";
        output.write(body.data(), static_cast<std::streamsize>(body.size()));
        output.flush();
    }
} // namespace Ryntra::LSP
