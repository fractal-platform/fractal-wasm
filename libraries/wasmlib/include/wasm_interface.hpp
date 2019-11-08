#pragma once

#include "exceptions.hpp"
#include "types.hpp"
#include "wavm.hpp"
#include "wasm_injection.hpp"
#include "Runtime/Linker.h"
#include "Runtime/Runtime.h"
#include "IR/Module.h"
#include "Runtime/Intrinsics.h"
#include "Platform/Platform.h"
#include "WAST/WAST.h"
#include "IR/Validate.h"
#include <map>

namespace ftl {
    class wasm_context;

    using namespace IR;
    using namespace Runtime;

    struct wasm_exit {
        int32_t code = 0;
    };

    namespace webassembly {
        namespace common {
            struct root_resolver : Runtime::Resolver {
                //when validating is true; only allow "env" imports. Otherwise allow any imports. This resolver is used
                //in two cases: once by the generic validating code where we only want "env" to pass; and then second in the
                //wavm runtime where we need to allow linkage to injected functions
                root_resolver(bool validating = false) : validating(validating) {}

                bool validating;

                bool resolve(const std::string &mod_name,
                             const std::string &export_name,
                             IR::ObjectType type,
                             Runtime::ObjectInstance *&out) override {

                    //protect access to "private" injected functions; so for now just simply allow "env" since injected functions
                    //  are in a different module
                    if (validating && mod_name != "env")
                        FTL_ASSERT(false, wasm_runtime_exception,
                                   "importing from module that is not 'env': ${0}.${1}", mod_name, export_name);

                    // Try to resolve an intrinsic first.
                    if (Runtime::IntrinsicResolver::singleton.resolve(mod_name, export_name, type, out)) {
                        return true;
                    }

                    FTL_ASSERT(false, wasm_runtime_exception, "${0}.${1} unresolveable", mod_name, export_name);
                    return false;
                };
            };

            /**
             * @class wasm_interface
             *
             */
            class wasm_interface {
            public:
                wasm_interface();

                ~wasm_interface();

                //validates code -- does a WASM validation pass and checks the wasm against EOSIO specific constraints
                static void validate(const std::vector<uint8_t> &code);

                //Calls apply or error on a given code
                void apply(const sha256 &code_id, const std::vector<uint8_t> &code, wasm_context &context);

                //Immediately exits currently running wasm. UB is called when no wasm running
                void exit();

                std::vector<uint8_t> parse_initial_memory(const Module &module) {
                    std::vector<uint8_t> mem_image;

                    for (const DataSegment &data_segment : module.dataSegments) {
                        FTL_ASSERT(data_segment.baseOffset.type == InitializerExpression::Type::i32_const,
                                   wasm_runtime_exception, "");
                        FTL_ASSERT(module.memories.defs.size(), wasm_runtime_exception, "");
                        const U32 base_offset = data_segment.baseOffset.i32;
                        const Uptr memory_size = (module.memories.defs[0].type.size.min << IR::numBytesPerPageLog2);
                        if (base_offset >= memory_size || base_offset + data_segment.data.size() > memory_size)
                            FTL_THROW(wasm_runtime_exception, "WASM data segment outside of valid memory range");
                        if (base_offset + data_segment.data.size() > mem_image.size())
                            mem_image.resize(base_offset + data_segment.data.size(), 0x00);
                        memcpy(mem_image.data() + base_offset, data_segment.data.data(), data_segment.data.size());
                    }

                    return mem_image;
                }

                std::unique_ptr<ftl::wasm_instantiated_module> &
                get_instantiated_module(const sha256 &code_id,
                                        const bytes &code) {
                    auto it = instantiation_cache.find(code_id);
                    if (it == instantiation_cache.end()) {
                        IR::Module module;
                        try {
                            Serialization::MemoryInputStream stream((const U8 *) code.data(), code.size());
                            WASM::serialize(stream, module);
                            module.userSections.clear();
                        } catch (const Serialization::FatalSerializationException &e) {
                            FTL_ASSERT(false, wasm_serialization_exception, e.message.c_str());
                        } catch (const IR::ValidationException &e) {
                            FTL_ASSERT(false, wasm_serialization_exception, e.message.c_str());
                        }

                        wasm_injections::wasm_binary_injection injector(module);
                        injector.inject();

                        std::vector<U8> bytes;
                        try {
                            Serialization::ArrayOutputStream outstream;
                            WASM::serialize(outstream, module);
                            bytes = outstream.getBytes();
                        } catch (const Serialization::FatalSerializationException &e) {
                            FTL_ASSERT(false, wasm_serialization_exception, e.message.c_str());
                        } catch (const IR::ValidationException &e) {
                            FTL_ASSERT(false, wasm_serialization_exception, e.message.c_str());
                        }
                        it = instantiation_cache.emplace(code_id, runtime_interface->instantiate_module(
                                (const char *) bytes.data(), bytes.size(), parse_initial_memory(module))).first;
                    }
                    return it->second;
                }

            private:
                std::unique_ptr<ftl::wavm_runtime> runtime_interface;
                std::map<sha256, std::unique_ptr<ftl::wasm_instantiated_module>> instantiation_cache;
            };

        }
    }
}

#define _REGISTER_INTRINSIC_EXPLICIT(CLS, MOD, METHOD, WASM_SIG, NAME, SIG)\
   _REGISTER_WAVM_INTRINSIC(CLS, MOD, METHOD, WASM_SIG, NAME, SIG)\

#define _REGISTER_INTRINSIC4(CLS, MOD, METHOD, WASM_SIG, NAME, SIG)\
   _REGISTER_INTRINSIC_EXPLICIT(CLS, MOD, METHOD, WASM_SIG, NAME, SIG )

#define _REGISTER_INTRINSIC3(CLS, MOD, METHOD, WASM_SIG, NAME)\
   _REGISTER_INTRINSIC_EXPLICIT(CLS, MOD, METHOD, WASM_SIG, NAME, decltype(&CLS::METHOD) )

#define _REGISTER_INTRINSIC2(CLS, MOD, METHOD, WASM_SIG)\
   _REGISTER_INTRINSIC_EXPLICIT(CLS, MOD, METHOD, WASM_SIG, BOOST_PP_STRINGIZE(METHOD), decltype(&CLS::METHOD) )

#define _REGISTER_INTRINSIC1(CLS, MOD, METHOD)\
   static_assert(false, "Cannot register " BOOST_PP_STRINGIZE(CLS) ":" BOOST_PP_STRINGIZE(METHOD) " without a signature");

#define _REGISTER_INTRINSIC0(CLS, MOD, METHOD)\
   static_assert(false, "Cannot register " BOOST_PP_STRINGIZE(CLS) ":<unknown> without a method name and signature");

#define _UNWRAP_SEQ(...) __VA_ARGS__

#define _EXPAND_ARGS(CLS, MOD, INFO)\
   ( CLS, MOD, _UNWRAP_SEQ INFO )

#define _REGISTER_INTRINSIC(R, CLS, INFO)\
   BOOST_PP_CAT(BOOST_PP_OVERLOAD(_REGISTER_INTRINSIC, _UNWRAP_SEQ INFO) _EXPAND_ARGS(CLS, "env", INFO), BOOST_PP_EMPTY())

#define REGISTER_INTRINSICS(CLS, MEMBERS)\
   BOOST_PP_SEQ_FOR_EACH(_REGISTER_INTRINSIC, CLS, _WRAPPED_SEQ(MEMBERS))

#define _REGISTER_INJECTED_INTRINSIC(R, CLS, INFO)\
   BOOST_PP_CAT(BOOST_PP_OVERLOAD(_REGISTER_INTRINSIC, _UNWRAP_SEQ INFO) _EXPAND_ARGS(CLS, EOSIO_INJECTED_MODULE_NAME, INFO), BOOST_PP_EMPTY())

#define REGISTER_INJECTED_INTRINSICS(CLS, MEMBERS)\
   BOOST_PP_SEQ_FOR_EACH(_REGISTER_INJECTED_INTRINSIC, CLS, _WRAPPED_SEQ(MEMBERS))


