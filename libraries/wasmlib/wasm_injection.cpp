#include "wasm_injection.hpp"
#include "wasm_constraints.hpp"
#include "wasm_binary_ops.hpp"
#include "exceptions.hpp"
#include "IR/Module.h"
#include "IR/Operators.h"
#include "WASM/WASM.h"

namespace ftl {
    namespace wasm_injections {
        using namespace IR;
        using namespace ftl::wasm_constraints;

        std::map<std::vector<uint16_t>, uint32_t> injector_utils::type_slots;
        std::map<std::string, uint32_t>           injector_utils::registered_injected;
        std::map<uint32_t, uint32_t>              injector_utils::injected_index_mapping;
        uint32_t                                  injector_utils::next_injected_index;


        void noop_injection_visitor::inject(Module &m) { /* just pass */ }

        void noop_injection_visitor::initializer() { /* just pass */ }

        void memories_injection_visitor::inject(Module &m) {
        }

        void memories_injection_visitor::initializer() {
        }

        void data_segments_injection_visitor::inject(Module &m) {
        }

        void data_segments_injection_visitor::initializer() {
        }

        void max_memory_injection_visitor::inject(Module &m) {
            if (m.memories.defs.size() && m.memories.defs[0].type.size.max > maximum_linear_memory / wasm_page_size)
                m.memories.defs[0].type.size.max = maximum_linear_memory / wasm_page_size;
        }

        void max_memory_injection_visitor::initializer() {}

        uint32_t instruction_counter::icnt = 0;
        uint32_t instruction_counter::tcnt = 0;
        uint32_t instruction_counter::bcnt = 0;
        std::queue<uint32_t> instruction_counter::fcnts;

    }
}
