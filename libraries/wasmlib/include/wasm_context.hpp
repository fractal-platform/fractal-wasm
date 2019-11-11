#pragma once

#include "wasm_action.hpp"
#include "wasm_interface.hpp"
#include "Runtime/Runtime.h"
#include <sstream>
#include <algorithm>
#include <set>

namespace ftl {

    typedef void c_db_store(uint64_t callbackParamKey, uint64_t table, char *key, int keyLength,
                            char *value, int valueLength);

    typedef int c_db_load(uint64_t callbackParamKey, uint64_t table, char *key, int keyLength,
                          char *value, int valueLength);

    typedef int c_db_has_key(uint64_t callbackParamKey, uint64_t table, char *key, int keyLength);

    typedef void c_db_remove_key(uint64_t callbackParamKey, uint64_t table, char *key, int keyLength);

    typedef int c_db_has_table(uint64_t callbackParamKey, uint64_t table);

    typedef void c_db_remove_table(uint64_t callbackParamKey, uint64_t table);

    typedef uint64_t c_chain_current_time(uint64_t callbackParamKey);

    typedef uint64_t c_chain_current_height(uint64_t callbackParamKey);

    typedef void c_chain_current_hash(uint64_t callbackParamKey, char *simple_hash, char *full_hash);

    typedef void c_add_log(uint64_t callbackParamKey, char *topics, int topicNum, const char *data, int dataLength);

    typedef void c_transfer(uint64_t callbackParamKey, char *to, uint64_t amount);

    typedef int c_call_action(uint64_t callbackParamKey, char *to, char *action, int actionLength, uint64_t amount,
                              int storageDelegate, int userDelegate);

    typedef int c_call_result(uint64_t callbackParamKey, char *result, int resultLength);

    typedef int c_set_result(uint64_t callbackParamKey, char *result, int resultLength);

    typedef struct {
        c_db_store *cb_store;
        c_db_load *cb_load;
        c_db_has_key *cb_has_key;
        c_db_remove_key *cb_remove_key;
        c_db_has_table *cb_has_table;
        c_db_remove_table *cb_remove_table;
        c_chain_current_time *cb_current_time;
        c_chain_current_height *cb_current_height;
        c_chain_current_hash *cb_current_hash;
        c_add_log *cb_add_log;
        c_transfer *cb_transfer;
        c_call_action *cb_call_action;
        c_call_result *cb_call_result;
        c_set_result *cb_set_result;
        c_sha256 *cb_sha256;
    } Callbacks;

    class wasm_context {
        /// Constructor
    public:
        wasm_context(ftl::webassembly::common::wasm_interface &wasmif, const ftl::wasm_action &a,
                      uint8_t *fromAddrBytes, uint8_t *toAddrBytes, uint8_t *ownerAddrBytes, uint8_t *userAddrBytes,
                      uint64_t transferAmount, uint64_t *remainedGas, uint64_t stateKey, Callbacks *callbacks)
                : wasmif(wasmif), act(a),
                  from_addr_bytes(fromAddrBytes), to_addr_bytes(toAddrBytes), owner_addr_bytes(ownerAddrBytes),
                  user_addr_bytes(userAddrBytes),
                  transfer_amount(transferAmount), remained_gas(remainedGas), state_key(stateKey),
                  callbacks(callbacks),
                  recurse_depth(0) {
            _pending_console_output = std::ostringstream();
            _pending_console_output.setf(std::ios::scientific, std::ios::floatfield);
        }

        /// Execution methods:
    public:

        void exec();

        /// Console methods:
    public:

        std::ostringstream &get_console_stream() { return _pending_console_output; }

        template<typename T>
        void console_append(T val) {
            _pending_console_output << val;
        }

        template<typename T, typename ...Ts>
        void console_append(T val, Ts ...rest) {
            console_append(val);
            console_append(rest...);
        };

        /// Database methods:
    public:
        void db_store(uint64_t table, const char *key, size_t key_size, const char *buffer, size_t buffer_size) {
            callbacks->cb_store(state_key, table, (char *) key, key_size, (char *) buffer,
                                buffer_size);
        }

        int db_load(uint64_t table, const char *key, size_t key_size, char *buffer, size_t buffer_size) {
            return callbacks->cb_load(state_key, table, (char *) key, key_size, buffer,
                                      buffer_size);
        }

        int db_has_key(uint64_t table, const char *key, size_t key_size) {
            return callbacks->cb_has_key(state_key, table, (char *) key, key_size);
        }

        void db_remove_key(uint64_t table, const char *key, size_t key_size) {
            callbacks->cb_remove_key(state_key, table, (char *) key, key_size);
        }

        int db_has_table(uint64_t table) {
            return callbacks->cb_has_table(state_key, table);
        }

        void db_remove_table(uint64_t table) {
            callbacks->cb_remove_table(state_key, table);
        }

        uint64_t chain_current_time() {
            return callbacks->cb_current_time(state_key);
        }

        uint64_t chain_current_height() {
            return callbacks->cb_current_height(state_key);
        }

        void chain_current_hash(sha256 &simple_hash, sha256 &full_hash) {
            callbacks->cb_current_hash(state_key, simple_hash.data(), full_hash.data());
        }

        void log0(const char *data, size_t data_size, const sha256 &name) {
            char topics[32] = {0};
            memcpy(topics, name.data(), 32);
            callbacks->cb_add_log(state_key, &(topics[0]), 1, data, data_size);
        }

        void log1(const char *data, size_t data_size, const sha256 &name, const sha256 &param1) {
            char topics[64] = {0};
            memcpy(topics, name.data(), 32);
            memcpy(&(topics[32]), param1.data(), 32);
            callbacks->cb_add_log(state_key, &(topics[0]), 2, data, data_size);
        }

        void log2(const char *data, size_t data_size, const sha256 &name, const sha256 &param1, const sha256 &param2) {
            char topics[96] = {0};
            memcpy(topics, name.data(), 32);
            memcpy(&(topics[32]), param1.data(), 32);
            memcpy(&(topics[64]), param2.data(), 32);
            callbacks->cb_add_log(state_key, &(topics[0]), 3, data, data_size);
        }

        void transfer(const char *to, uint64_t amount) {
            callbacks->cb_transfer(state_key, (char *) to, amount);
        }

        int call_action(const char *to, const char *action, size_t action_size, uint64_t amount, int storage_delegate,
                        int user_delegate) {
            int ret = callbacks->cb_call_action(state_key, (char *) to, (char *) action, action_size, amount,
                                                storage_delegate, user_delegate);
            return ret;
        }

        int call_result(char *result, size_t result_size) {
            return callbacks->cb_call_result(state_key, result, result_size);
        }

        int set_result(char *result, size_t result_size) {
            return callbacks->cb_set_result(state_key, result, result_size);
        }

    public:
        void use_gas(int64_t gas) {
            if (*(this->remained_gas) < gas) {
                FTL_THROW(wasm_runtime_exception, "gas limit exceeded");
            }
            *(this->remained_gas) -= gas;
        }

        /// Misc methods:
    public:
        ftl::webassembly::common::wasm_interface &get_wasm_interface() {
            return wasmif;
        }

        /// Fields:
    public:

        ftl::webassembly::common::wasm_interface &wasmif;
        const ftl::wasm_action &act; ///< message being applied

        uint8_t *from_addr_bytes;
        uint8_t *to_addr_bytes;
        uint8_t *owner_addr_bytes;
        uint8_t *user_addr_bytes;
        uint64_t state_key;
        uint64_t transfer_amount;
        uint64_t *remained_gas;
        Callbacks *callbacks;

        uint32_t recurse_depth; ///< how deep inline actions can recurse

        Runtime::MemoryInstance *memory;

    private:

        std::ostringstream _pending_console_output;
    };

    using apply_handler = std::function<void(wasm_context &)>;

}

