#pragma once

#include <cstddef>
#include <cstdint>

namespace sfg_sbs
{
    /// @brief Stores the raw wire word.
    class Word
    {
    public:
        /// @brief Wire size in bytes of a word transfer.
        static constexpr size_t s_wire_size = 2;

        /// @brief Constructs a default word holding 0.
        Word() = default;

        /// @brief Constructs a word holding the given value.
        /// @param value The value to hold.
        Word(std::int16_t value);

        /// @brief Constructs a word holding the given value.
        /// @param value The value to hold.
        Word(std::uint16_t value);

        /// @brief Constructs a word holding the given value.
        /// @param value The value to hold.
        Word(bool value);

        /// @brief The word interpreted as an unsigned value.
        /// @returns The raw value as an unsigned integer.
        std::uint16_t as_uint() const;

        /// @brief The word interpreted as a signed value.
        /// @returns The raw value as a signed integer.
        std::int16_t as_int() const;

        /// @brief The word interpreted as a Boolean.
        /// @returns True when the raw value is non-zero, false when it is zero.
        bool as_bool() const;

    private:
        std::uint16_t m_raw = 0;
    };
}
