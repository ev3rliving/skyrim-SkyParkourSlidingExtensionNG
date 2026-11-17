#pragma once
#include <cstdint>

namespace Slide::Runtime {
constexpr std::uint32_t Pack(unsigned major, unsigned minor, unsigned patch) {
    return (major << 24) | (minor << 16) | (patch << 4);
}
constexpr bool Supported(std::uint32_t version) {
    return version==Pack(1,5,97) || version==Pack(1,6,1170) ||
        version==Pack(1,6,1179) || version==Pack(1,7,99) || version==Pack(1,7,104);
}
}
