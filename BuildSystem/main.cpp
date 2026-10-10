#include <iostream>
#include <chrono>
#include <format>

int main() {
    auto now = std::chrono::system_clock::now();
    std::cout << std::format("Hello Stranger! Now is: {:%Y-%m-%d %H:%M:%S}",
        std::chrono::zoned_time{std::chrono::current_zone(), now}
    );
    return 0;
}