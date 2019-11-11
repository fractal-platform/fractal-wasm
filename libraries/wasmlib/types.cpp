#include "types.hpp"
#include "wasm_context.hpp"

namespace ftl {
    sha256 hash(const bytes &input) {
        sha256 hash;
        g_sha256((char *)&input[0], input.size(), (char *)&hash._hash[0]);
        return hash;
    }
}
