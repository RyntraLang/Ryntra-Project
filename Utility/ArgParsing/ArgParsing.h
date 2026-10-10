#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

/*
ArgParser parser = ArgParser(argc, argv);
auto inputList = parser.option<std::vector<std::string>>("input", "i")
                       .help("Input Source")
                       .required();
 **/

namespace Ryntra::Utility {
    template <typename T>
    struct isVector : std::false_type {};

    template <typename T, typename Alloca>
    struct isVector<std::vector<T, Alloca>> : std::true_type {};

    template <typename T>
    inline constexpr bool isVectorValue = isVector<T>::value;

    template <typename T>
    using vectorValueType = typename T::value_type;

    template <typename T>
    T parseValue(const std::string &str);

    template <>
    inline std::string parseValue<std::string>(const std::string &str) {
        return str;
    }

    template <>
    inline int parseValue<int>(const std::string &str) {
        return std::stoi(str);
    }

    template <>
    inline double parseValue<double>(const std::string &str) {
        return std::stod(str);
    }

    template <>
    inline bool parseValue<bool>(const std::string &str) {
        return str == "true" || str == "1" || str == "yes";
    }

    class ArgumentBase {
    public:
        ArgumentBase(std::string name, std::string help)
            : name_(std::move(name)), help_(std::move(help)) {}

        virtual ~ArgumentBase() = default;
        virtual bool parse(const std::string &value) = 0;
        virtual bool isRequired() const { return false; }
        virtual bool isSet() const { return isSet_; }
        virtual bool isFlag() const { return false; }
        virtual std::string help() const { return help_; }
        virtual std::string name() const { return name_; }
        virtual std::string typeName() const { return "value"; }

        ArgumentBase &alias(const std::string &al) {
            aliases_.push_back(al);
            return *this;
        }

        ArgumentBase &help(const std::string &hlp) {
            help_ = hlp;
            return *this;
        }

        ArgumentBase &required(bool req = true) {
            required_ = req;
            return *this;
        }

        ArgumentBase &shorten(bool shrt) {
            shorten_ = shrt;
            return *this;
        }

        const std::vector<std::string> &aliases() const {
            return aliases_;
        }

        bool shorten() const {
            return shorten_;
        }

    protected:
        std::string name_;
        std::string help_;
        std::vector<std::string> aliases_;
        bool required_ = false;
        bool isSet_ = false;
        bool shorten_ = true;
    };

    class FlagArgument : public ArgumentBase {
    public:
        FlagArgument(std::string name, std::string help)
            : ArgumentBase(std::move(name), std::move(help)) {}

        bool parse(const std::string &) override {
            isSet_ = true;
            value_ = true;
            return true;
        }

        bool isFlag() const override { return true; }
        bool get() const { return value_; }
        std::string typeName() const override { return "flag"; }

    private:
        bool value_ = false;
    };

    template <typename T>
    class OptionArgument : public ArgumentBase {
    public:
        OptionArgument(std::string name, std::string help)
            : ArgumentBase(std::move(name), std::move(help)) {}

        bool parse(const std::string &value) override {
            try {
                if constexpr (isVectorValue<T>) {
                    value_.push_back(parseValue<vectorValueType<T>>(value));
                } else {
                    value_ = parseValue<T>(value);
                }
                isSet_ = true;
                return true;
            } catch (const std::exception &) {
                return false;
            }
        }

        const T &get() const { return value_; }

        std::string typeName() const override {
            if constexpr (isVectorValue<T>) {
                return "list";
            } else if constexpr (std::is_same_v<T, bool>) {
                return "bool";
            } else if constexpr (std::is_same_v<T, int>) {
                return "int";
            } else if constexpr (std::is_same_v<T, double>) {
                return "double";
            } else {
                return "string";
            }
        }

    private:
        T value_{};
    };

    template <typename T>
    class OptionProxy {
    public:
        OptionProxy(OptionArgument<T> *arg) : arg_(arg) {}

        OptionProxy &alias(const std::string &a) {
            arg_->alias(a);
            return *this;
        }

        OptionProxy &help(const std::string &h) {
            arg_->help(h);
            return *this;
        }

        OptionProxy &required(bool r = true) {
            arg_->required(r);
            return *this;
        }

        OptionProxy &shorten(bool s = true) {
            arg_->shorten(s);
            return *this;
        }

        T get() const { return arg_->get(); }
        operator T() const { return arg_->get(); }

        T value_or(const T &default_val) const {
            return arg_->isSet() ? arg_->get() : default_val;
        }

        bool isSet() const { return arg_->isSet(); }

    private:
        OptionArgument<T> *arg_;
    };

    class FlagProxy {
    public:
        FlagProxy(FlagArgument *arg) : arg_(arg) {}

        FlagProxy &alias(const std::string &a) {
            arg_->alias(a);
            return *this;
        }

        FlagProxy &help(const std::string &h) {
            arg_->help(h);
            return *this;
        }

        FlagProxy &required(bool r = true) {
            arg_->required(r);
            return *this;
        }

        FlagProxy &shorten(bool s = true) {
            arg_->shorten(s);
            return *this;
        }

        bool get() const { return arg_->get(); }
        operator bool() const { return arg_->get(); }
        bool isSet() const { return arg_->isSet(); }

    private:
        FlagArgument *arg_;
    };

    class ArgParser {
    public:
        ArgParser(int argc, char *argv[]) {
            for (int i = 1; i < argc; ++i) {
                args_.emplace_back(argv[i]);
            }
        }

        FlagProxy flag(const std::string &name) {
            auto arg = std::make_unique<FlagArgument>(name, "");
            auto *ptr = arg.get();
            registerArgument(std::move(arg));
            return FlagProxy(ptr);
        }

        template <typename T>
        OptionProxy<T> option(const std::string &name) {
            auto arg = std::make_unique<OptionArgument<T>>(name, "");
            auto *ptr = arg.get();
            registerArgument(std::move(arg));
            return OptionProxy<T>(ptr);
        }

        ArgumentBase *find(const std::string &name) {
            for (auto &arg : arguments_) {
                if (arg->name() == name)
                    return arg.get();
                for (const auto &a : arg->aliases()) {
                    if (a == name)
                        return arg.get();
                }
            }
            return nullptr;
        }

        bool parse() {
            for (size_t i = 0; i < args_.size(); ++i) {
                const auto &token = args_[i];

                if (token.size() >= 2 && token[0] == '-' && token[1] == '-') {
                    std::string key = token;
                    std::string value;
                    bool has_inline_value = false;

                    if (auto pos = token.find('='); pos != std::string::npos) {
                        key = token.substr(0, pos);
                        value = token.substr(pos + 1);
                        has_inline_value = true;
                    }

                    auto *arg = find(key);
                    if (!arg) {
                        std::cerr << "Unknown option: " << key << "\n";
                        return false;
                    }

                    if (arg->isFlag()) {
                        arg->parse("");
                    } else {
                        if (!has_inline_value) {
                            if (i + 1 >= args_.size()) {
                                std::cerr << "Missing value for " << key << "\n";
                                return false;
                            }
                            value = args_[++i];
                        }
                        if (!arg->parse(value)) {
                            std::cerr << "Invalid value for " << key << ": " << value << "\n";
                            return false;
                        }
                    }
                } else if (token.size() >= 2 && token[0] == '-') {
                    if (token.size() > 2) {
                        bool all_flags = true;
                        for (size_t j = 1; j < token.size(); ++j) {
                            std::string short_name = std::string("-") + token[j];
                            auto *arg = find(short_name);
                            if (!arg || !arg->isFlag()) {
                                all_flags = false;
                                break;
                            }
                        }
                        if (all_flags) {
                            for (size_t j = 1; j < token.size(); ++j) {
                                std::string short_name = std::string("-") + token[j];
                                find(short_name)->parse("");
                            }
                            continue;
                        } else {
                            std::string key = token.substr(0, 2);
                            std::string value = token.substr(2);
                            auto *arg = find(key);
                            if (!arg) {
                                std::cerr << "Unknown option: " << key << "\n";
                                return false;
                            }
                            if (arg->isFlag()) {
                                arg->parse("");
                            } else if (!arg->parse(value)) {
                                std::cerr << "Invalid value for " << key << ": " << value << "\n";
                                return false;
                            }
                            continue;
                        }
                    }

                    auto *arg = find(token);
                    if (!arg) {
                        std::cerr << "Unknown option: " << token << "\n";
                        return false;
                    }

                    if (arg->isFlag()) {
                        arg->parse("");
                    } else {
                        if (i + 1 >= args_.size()) {
                            std::cerr << "Missing value for " << token << "\n";
                            return false;
                        }
                        std::string value = args_[++i];
                        if (!arg->parse(value)) {
                            std::cerr << "Invalid value for " << token << ": " << value << "\n";
                            return false;
                        }
                    }
                } else {
                    positional_.push_back(token);
                }
            }

            return validateRequired();
        }

        void printHelp(const std::string &prog = "program") const {
            std::cout << "Usage: " << prog << " [options]\n\n";
            std::cout << "Options:\n";
            for (const auto &arg : arguments_) {
                std::string names = arg->name();
                for (const auto &a : arg->aliases()) {
                    names += ", " + a;
                }
                if (arg->isRequired())
                    names += " (required)";
                std::cout << "  " << names;
                if (!arg->isFlag()) {
                    std::cout << " <" << arg->typeName() << ">";
                }
                std::cout << "\n      " << arg->help() << "\n";
            }
        }

        const std::vector<std::string> &positional() const { return positional_; }

    private:
        void registerArgument(std::unique_ptr<ArgumentBase> arg) {
            if (find(arg->name())) {
                throw std::runtime_error("Duplicate argument: " + arg->name());
            }
            arguments_.push_back(std::move(arg));
        }

        bool validateRequired() const {
            for (const auto &arg : arguments_) {
                if (arg->isRequired() && !arg->isSet()) {
                    std::cerr << "Missing required argument: " << arg->name() << "\n";
                    return false;
                }
            }
            return true;
        }

        std::vector<std::string> args_;
        std::vector<std::unique_ptr<ArgumentBase>> arguments_;
        std::vector<std::string> positional_;
    };
} // namespace Ryntra::Utility