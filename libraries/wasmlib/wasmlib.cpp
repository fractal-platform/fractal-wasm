
#include "wasm_action.hpp"
#include "types.hpp"
#include "wasm_interface.hpp"
#include "wasm_context.hpp"
#include <stdio.h>
#include <sstream>
#include <algorithm>
#include <set>

extern "C" {

int execute(uint8_t *codeBytes, int codeLength,
            uint8_t *actionBytes, int actionLength,
            uint8_t *fromAddrBytes, uint8_t *toAddrBytes, uint8_t *ownerAddrBytes, uint8_t *userAddrBytes,
            uint64_t transferAmount, uint64_t *remainedGas, uint64_t stateKey, ftl::Callbacks *callbacks) {

    // set global method
    ftl::g_sha256 = callbacks->cb_sha256;

    try {
        ftl::webassembly::common::wasm_interface wasmif;

        // code
        ftl::bytes code_bytes;
        for (int i = 0; i < codeLength; i++) {
            code_bytes.push_back(codeBytes[i]);
        }

        // action name
        uint64_t action_name;
        memcpy(&action_name, actionBytes, sizeof(uint64_t));

        // action
        ftl::bytes action_bytes;
        for (int i = sizeof(uint64_t); i < actionLength; i++) {
            action_bytes.push_back(actionBytes[i]);
        }

        auto act = ftl::wasm_action(ftl::name(action_name), code_bytes, action_bytes);

        ftl::wasm_context ctx(wasmif, act, fromAddrBytes, toAddrBytes, ownerAddrBytes, userAddrBytes, transferAmount, remainedGas, stateKey, callbacks);
        ctx.exec();
    }
    catch (const ftl::exception &e) {
        std::cout << "err: " << e.name() << ": " << e.what() << std::endl;
        return e.code();
    }

    return 0;
}

}
