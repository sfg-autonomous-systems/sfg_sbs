#include "test_fixture.hpp"

template <typename TInvocation>
size_t TestFixture::count_invocations() const
{
    return count_invocations<TInvocation>(
        [](const TInvocation &)
        {
            return true;
        });
}

template <typename TInvocation, typename TPredicate>
size_t TestFixture::count_invocations(TPredicate predicate) const
{
    size_t count = 0;

    for (const auto &invocation : m_wire.get_invocations())
    {
        if (etl::holds_alternative<TInvocation>(invocation) && predicate(etl::get<TInvocation>(invocation)))
        {
            count++;
        }
    }
    return count;
}

template <typename TInvocation>
size_t TestFixture::get_invocation_index(size_t occurrence) const
{
    const auto &invocations = m_wire.get_invocations();

    for (size_t index = 0; index < invocations.size(); index++)
    {
        if (etl::holds_alternative<TInvocation>(invocations[index]) && occurrence-- == 0)
        {
            return index;
        }
    }
    TEST_FAIL_MESSAGE("The expected bus call was not recorded.");
    return invocations.size();
}

template <typename TInvocation>
const TInvocation &TestFixture::get_invocation(size_t occurrence) const
{
    return etl::get<TInvocation>(m_wire.get_invocations()[get_invocation_index<TInvocation>(occurrence)]);
}
