#include "sfg_sbs/utils/scope_guard.hpp"

namespace sfg_sbs::utils
{
    template <typename Callable>
    ScopeGuard<Callable>::ScopeGuard(Callable callable) : m_callable(std::move(callable))
    {
    }

    template <typename Callable>
    ScopeGuard<Callable>::~ScopeGuard()
    {
        m_callable();
    }
}