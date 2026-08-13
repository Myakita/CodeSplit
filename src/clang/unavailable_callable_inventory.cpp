#include "codesplit/analysis/callable_inventory.hpp"

namespace codesplit::analysis {

CallableInventoryResult inventory_callables(const std::filesystem::path&,
                                            const std::filesystem::path&, std::uintmax_t) {
    constexpr auto unavailable = "Clang LibTooling support is not available in this build.";
    return {
        .compilation = {.error = unavailable},
        .error = unavailable,
    };
}

CallableInventoryResult inventory_callables(const CompilationCommand&, const std::filesystem::path&,
                                            const std::filesystem::path&, std::uintmax_t) {
    return {.compilation = {.error = "Clang LibTooling support is not available in this build."},
            .error = "Clang LibTooling support is not available in this build."};
}

} // namespace codesplit::analysis
