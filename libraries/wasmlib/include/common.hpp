#pragma once

#include <type_traits>

#define EOSIO_INJECTED_MODULE_NAME "eosio_injection"

namespace ftl {

    class wasm_context;

    template<typename T>
    struct class_from_wasm {
        /**
         * by default this is just constructing an object
         * @param wasm - the wasm_interface to use
         * @return
         */
        static auto value(wasm_context &ctx) {
            return T(ctx);
        }
    };

    template<>
    struct class_from_wasm<wasm_context> {
        /**
         * Don't construct a new wasm_context, just return a reference to the existing ont
         * @param wasm
         * @return
         */
        static auto &value(wasm_context &ctx) {
            return ctx;
        }
    };

    /**
     * class to represent an in-wasm-memory array
     * it is a hint to the transcriber that the next parameter will
     * be a size (data bytes length) and that the pair are validated together
     * This triggers the template specialization of intrinsic_invoker_impl
     * @tparam T
     */
    template<typename T>
    struct array_ptr {
        explicit array_ptr(T *value) : value(value) {}

        typename std::add_lvalue_reference<T>::type operator*() const {
            return *value;
        }

        T *operator->() const noexcept {
            return value;
        }

        template<typename U>
        operator U *() const {
            return static_cast<U *>(value);
        }

        T *value;
    };

    /**
     * class to represent an in-wasm-memory char array that must be null terminated
     */
    struct null_terminated_ptr {
        explicit null_terminated_ptr(char *value) : value(value) {}

        typename std::add_lvalue_reference<char>::type operator*() const {
            return *value;
        }

        char *operator->() const noexcept {
            return value;
        }

        template<typename U>
        operator U *() const {
            return static_cast<U *>(value);
        }

        char *value;
    };

}
