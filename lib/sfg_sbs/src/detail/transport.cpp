#include "sfg_sbs/detail/transport.hpp"

namespace sfg_sbs::detail
{
    bool holds_expected_type(const Value &value, const CommandRuntimeTraits &traits)
    {
        if (traits.m_protocol == SmbusProtocol::Block)
        {
            return etl::get_if<Block>(&value) != nullptr;
        }
        return etl::get_if<Word>(&value) != nullptr;
    }
}