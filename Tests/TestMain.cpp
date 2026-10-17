#include "TestFramework.h"

int main()
{
    using namespace chimedes::test;

    int failedTests = 0;

    for (const auto& test : registry())
    {
        const auto before = failureCount();
        test.body();
        const bool passed = failureCount() == before;

        std::cout << (passed ? "  ok     " : "  FALLA  ") << test.name << "\n";

        if (! passed)
            ++failedTests;
    }

    std::cout << "\n" << registry().size() - static_cast<std::size_t> (failedTests) << " de "
              << registry().size() << " pruebas pasaron\n";

    return failedTests == 0 ? 0 : 1;
}
