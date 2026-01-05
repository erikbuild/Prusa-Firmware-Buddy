#include "requests_read.hpp"
#include "requests_read_multi.hpp"

namespace buddy::openprinttag {

void opt_usage_example() {
    const auto tag_opt = ToolTag::for_tool_assigned(VirtualToolIndex::from_raw(0));
    if (!tag_opt) {
        return;
    }

    const auto tag = *tag_opt;

    ReadFieldRequest<MainField::material_name> material_name { tag };
    ReadFieldRequest<MainField::density> density { tag };

    material_name.issue();
    density.issue();

    while (!density.finished()) {
        // Wait for the request group to finish
        // We could possibly give the request group a semaphore or something as well
    }

    const auto mat = material_name.result();
    printf("%.*s", mat->size(), mat->data());
}

__attribute__((__used__)) void opt_usage_example_2() {
    const auto tag_opt = ToolTag::for_tool_assigned(VirtualToolIndex::from_raw(0));
    if (!tag_opt) {
        return;
    }

    MultiReadFieldRequest<MainField::material_name, MainField::density> read { *tag_opt };

    while (!read.finished()) {
        // Wait for the request group to finish
        // We could possibly give the request group a semaphore or something as well
    }

    const auto mat = read.result<MainField::material_name>();
    // printf("%.*s", mat->size(), mat->data());
}

} // namespace buddy::openprinttag
