#include "codesplit/analysis/callable_inventory.hpp"

#include <iostream>
#include <string>

int main() {
    const auto result = codesplit::analysis::inventory_callables("build", "source.cpp", 1024);
    const auto has_expected_error =
        result.error == "Clang LibTooling support is not available in this build.";
    const auto compilation_has_expected_error = result.compilation.error == result.error;

    if (!has_expected_error) {
        std::cerr << "FAILED: fallback should explain that LibTooling is unavailable\n";
    }
    if (!compilation_has_expected_error) {
        std::cerr << "FAILED: fallback should expose the compilation status\n";
    }

    return has_expected_error && compilation_has_expected_error ? 0 : 1;
}
