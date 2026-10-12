/// @file template_check.cpp
/// @brief This file's only purpose is to force the compiler to instantiate the
///        library's templates to see whether they compile without errors.
#include <cstddef>
#include <cstdint>

#include "sfg_sbs/sbs.hpp"

namespace sfg_sbs
{
    class DummyWire
    {
    public:
        void beginTransmission(std::uint8_t)
        {
        }

        std::uint8_t endTransmission(bool = true)
        {
            return 0;
        }

        int read()
        {
            return -1;
        }

        void write(std::uint8_t)
        {
        }

        void write(const std::uint8_t *, size_t)
        {
        }

        size_t requestFrom(std::uint8_t, size_t, bool = true)
        {
            return 0;
        }

        int available()
        {
            return 0;
        }
    };

    DummyWire dummy_wire;
    Host<DummyWire> dummy_host(dummy_wire);
    Battery<DummyWire> dummy_battery(dummy_wire, nullptr, nullptr);
}