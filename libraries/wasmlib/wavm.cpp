#include "wavm.hpp"
#include "wasm_constraints.hpp"
#include "wasm_injection.hpp"
#include "wasm_context.hpp"
#include "exceptions.hpp"
#include "IR/Module.h"
#include "Platform/Platform.h"
#include "WAST/WAST.h"
#include "IR/Operators.h"
#include "IR/Validate.h"
#include "Runtime/Linker.h"
#include "Runtime/Intrinsics.h"

#include <mutex>

using namespace IR;
using namespace Runtime;

namespace ftl {

    running_instance_context the_running_instance_context;

    wasm_instantiated_module::wasm_instantiated_module(ModuleInstance *instance, std::unique_ptr<Module> module,
                                                       std::vector<uint8_t> initial_mem) :
            _initial_memory(initial_mem),
            _instance(instance),
            _module(std::move(module)) {}

    void wasm_instantiated_module::apply(wasm_context &context) {
        std::vector<Value> args = {Value(uint64_t(context.act.name))};

        call("apply", args, context);
    }

    void
    wasm_instantiated_module::call(const string &entry_point, const std::vector<Value> &args, wasm_context &context) {

        FunctionInstance *call = asFunctionNullable(getInstanceExport(_instance, entry_point));
        if (!call)
            return;

        FTL_ASSERT(getFunctionType(call)->parameters.size() == args.size(), wasm_runtime_exception, "");

        //The memory instance is reused across all wavm_instantiated_modules, but for wasm instances
        // that didn't declare "memory", getDefaultMemory() won't see it
        MemoryInstance *default_mem = getDefaultMemory(_instance);
        if (default_mem) {
            //reset memory resizes the sandbox'ed memory to the module's init memory size and then
            // (effectively) memzeros it all
            resetMemory(default_mem, _module->memories.defs[0].type);

            char *memstart = &memoryRef<char>(getDefaultMemory(_instance), 0);
            memcpy(memstart, _initial_memory.data(), _initial_memory.size());
        }

        the_running_instance_context.memory = default_mem;
        the_running_instance_context.apply_ctx = &context;
        context.memory = default_mem;

        resetGlobalInstances(_instance);
        runInstanceStartFunc(_instance);
        Runtime::invokeFunction(call, args);
    }


    wavm_runtime::runtime_guard::runtime_guard() {
        // TODO clean this up
        //check_wasm_opcode_dispositions();
        Runtime::init();
    }

    wavm_runtime::runtime_guard::~runtime_guard() {
        Runtime::freeUnreferencedObjects({});
    }

    static std::weak_ptr<wavm_runtime::runtime_guard> __runtime_guard_ptr;
    static std::mutex __runtime_guard_lock;

    wavm_runtime::wavm_runtime() {
        std::lock_guard<std::mutex> l(__runtime_guard_lock);
        if (__runtime_guard_ptr.use_count() == 0) {
            _runtime_guard = std::make_shared<runtime_guard>();
            __runtime_guard_ptr = _runtime_guard;
        } else {
            _runtime_guard = __runtime_guard_ptr.lock();
        }
    }

    wavm_runtime::~wavm_runtime() {
    }

    std::unique_ptr<wasm_instantiated_module>
    wavm_runtime::instantiate_module(const char *code_bytes, size_t code_size,
                                     std::vector<uint8_t> initial_memory) {
        std::unique_ptr<Module> module = std::make_unique<Module>();
        try {
            Serialization::MemoryInputStream stream((const U8 *) code_bytes, code_size);
            WASM::serialize(stream, *module);
        } catch (const Serialization::FatalSerializationException &e) {
            FTL_ASSERT(false, wasm_serialization_exception, e.message.c_str());
        } catch (const IR::ValidationException &e) {
            FTL_ASSERT(false, wasm_serialization_exception, e.message.c_str());
        }

        webassembly::common::root_resolver resolver;
        LinkResult link_result = linkModule(*module, resolver);
        ModuleInstance *instance = instantiateModule(*module, std::move(link_result.resolvedImports));
        FTL_ASSERT(instance != nullptr, wasm_runtime_exception, "Fail to Instantiate WAVM Module");

        return std::make_unique<wasm_instantiated_module>(instance, std::move(module), initial_memory);
    }

    void wavm_runtime::immediately_exit_currently_running_module() {
#ifdef _WIN32
        throw wasm_exit();
#else
        Platform::immediately_exit();
#endif
    }

}
