#include "sfg_sbs/word.hpp"

namespace sfg_sbs
{
    Word::Word(std::int16_t value) : m_raw(static_cast<std::uint16_t>(value))
    {
    }

    Word::Word(std::uint16_t value) : m_raw(value)
    {
    }

    Word::Word(bool value) : m_raw(value ? 1 : 0)
    {
    }

    std::uint16_t Word::as_uint() const
    {
        return m_raw;
    }

    std::int16_t Word::as_int() const
    {
        return static_cast<std::int16_t>(m_raw);
    }

    bool Word::as_bool() const
    {
        return m_raw != 0;
    }
}
