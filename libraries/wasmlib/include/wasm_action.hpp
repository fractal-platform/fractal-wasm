#pragma once

#include "name.hpp"
#include "types.hpp"
#include "exceptions.hpp"

namespace ftl {

    struct wasm_action {
        struct name name;
        sha256 code_id;
        bytes code;
        bytes data;

        wasm_action() {}

        wasm_action(const struct name &name, const bytes &code, const bytes &data)
                : name(name), code(code), data(data) {
            code_id = hash(code);
        }
    };

}

