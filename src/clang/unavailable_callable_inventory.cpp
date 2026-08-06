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

} // namespace codesplit::analysis
