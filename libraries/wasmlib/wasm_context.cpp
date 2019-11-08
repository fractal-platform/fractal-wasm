#include "wasm_context.hpp"
#include "wasm_interface.hpp"
#include "exceptions.hpp"

namespace ftl {
    void wasm_context::exec() {
        try {
            get_wasm_interface().apply(act.code_id, act.code, *this);
        } catch (exception &e) {
            std::string console = _pending_console_output.str();
            if (!console.empty()) {
                std::cout << "PENDING CONSOLE OUTPUT BEGIN =====================" << std::endl;
                std::cout << console << std::endl;
                std::cout << "PENDING CONSOLE OUTPUT END   =====================" << std::endl;
            }
            throw;
        }

        std::string console = _pending_console_output.str();
        if (!console.empty()) {
            std::cout << "CONSOLE OUTPUT BEGIN =====================" << std::endl;
            std::cout << console << std::endl;
            std::cout << "CONSOLE OUTPUT END   =====================" << std::endl;
        } else {
            std::cout << "empty console log" << std::endl;
        }
    }
}
