#pragma once

namespace sfg_sbs::utils
{
    template <typename Callable>
    class ScopeGuard
    {
    public:
        explicit ScopeGuard(Callable callable);
        ~ScopeGuard();

        ScopeGuard(const ScopeGuard &) = delete;
        ScopeGuard(ScopeGuard &&) = delete;
        ScopeGuard &operator=(const ScopeGuard &) = delete;
        ScopeGuard &operator=(ScopeGuard &&) = delete;

    private:
        Callable m_callable;
    };
}
#include "sfg_sbs/utils/scope_guard.tpp"