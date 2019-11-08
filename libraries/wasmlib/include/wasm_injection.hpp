#pragma once

#include "wasm_gas_table.hpp"
#include "wasm_binary_ops.hpp"
#include "wasm_constraints.hpp"
#include "common.hpp"
#include "exceptions.hpp"
#include "IR/Module.h"
#include "IR/Operators.h"
#include "WASM/WASM.h"
#include <type_traits>
#include <string>
#include <vector>
#include <map>
#include <queue>

namespace ftl {
    namespace wasm_injections {
        using namespace ftl;
        using namespace IR;
        // helper functions for injection

        struct injector_utils {
            static std::map<std::vector<uint16_t>, uint32_t> type_slots;
            static std::map<std::string, uint32_t> registered_injected;
            static std::map<uint32_t, uint32_t> injected_index_mapping;
            static uint32_t next_injected_index;

            static void init(Module &mod) {
                type_slots.clear();
                registered_injected.clear();
                injected_index_mapping.clear();
                build_type_slots(mod);
                next_injected_index = 0;
            }

            static void build_type_slots(Module &mod) {
                // add the module types to the type_slots map
                for (size_t i = 0; i < mod.types.size(); i++) {
                    std::vector<uint16_t> type_slot_list = {static_cast<uint16_t>(mod.types[i]->ret)};
                    for (auto param : mod.types[i]->parameters)
                        type_slot_list.push_back(static_cast<uint16_t>(param));
                    type_slots.emplace(type_slot_list, i);
                }
            }

            template<ResultType Result, ValueType... Params>
            static void add_type_slot(Module &mod) {
                if (type_slots.find({FromResultType<Result>::value, FromValueType<Params>::value...}) ==
                    type_slots.end()) {
                    type_slots.emplace(
                            std::vector<uint16_t>{FromResultType<Result>::value, FromValueType<Params>::value...},
                            mod.types.size());
                    mod.types.push_back(FunctionType::get(Result, {Params...}));
                }
            }

            // get the next available index that is greater than the last exported function
            static void get_next_indices(Module &module, int &next_function_index, int &next_actual_index) {
                int exports = 0;
                for (auto exp : module.exports)
                    if (exp.kind == IR::ObjectKind::function)
                        exports++;

                next_function_index =
                        module.functions.imports.size() + module.functions.defs.size() + registered_injected.size();
                next_actual_index = next_injected_index++;
            }

            template<ResultType Result, ValueType... Params>
            static void add_import(Module &module, const char *func_name, int32_t &index) {
                if (module.functions.imports.size() == 0 ||
                    registered_injected.find(func_name) == registered_injected.end()) {
                    add_type_slot<Result, Params...>(module);
                    const uint32_t func_type_index = type_slots[{FromResultType<Result>::value,
                                                                 FromValueType<Params>::value...}];
                    int actual_index;
                    get_next_indices(module, index, actual_index);
                    registered_injected.emplace(func_name, index);
                    decltype(module.functions.imports) new_import = {{{func_type_index}, EOSIO_INJECTED_MODULE_NAME, std::move(
                            func_name)}};
                    // prepend to the head of the imports
                    module.functions.imports.insert(
                            module.functions.imports.begin() + (registered_injected.size() - 1), new_import.begin(),
                            new_import.end());
                    injected_index_mapping.emplace(index, actual_index);

                    // shift all exported functions by 1
                    for (size_t i = 0; i < module.exports.size(); i++) {
                        if (module.exports[i].kind == IR::ObjectKind::function) {
                            module.exports[i].index++;
                        }
                    }

                    // update the start function
                    if (module.startFunctionIndex != UINTPTR_MAX) {
                        module.startFunctionIndex++;
                    }

                    // shift all table entries for call indirect
                    for (TableSegment &ts : module.tableSegments) {
                        for (auto &idx : ts.indices)
                            idx++;
                    }
                } else {
                    index = registered_injected[func_name];
                }
            }
        };

        struct noop_injection_visitor {
            static void inject(IR::Module &m);

            static void initializer();
        };

        struct memories_injection_visitor {
            static void inject(IR::Module &m);

            static void initializer();
        };

        struct data_segments_injection_visitor {
            static void inject(IR::Module &m);

            static void initializer();
        };

        struct tables_injection_visitor {
            static void inject(IR::Module &m);

            static void initializer();
        };

        struct globals_injection_visitor {
            static void inject(IR::Module &m);

            static void initializer();
        };

        struct max_memory_injection_visitor {
            static void inject(IR::Module &m);

            static void initializer();
        };

        struct blacklist_injection_visitor {
            static void inject(IR::Module &m);

            static void initializer();
        };

        using wasm_validate_func = std::function<void(IR::Module &)>;


        // just pass
        struct no_injections_injectors {
            static void inject(IR::Module &m) {}
        };

        // simple mutator that doesn't actually mutate anything
        // used to verify that a given instruction is valid for execution on our platform
        // injector 'interface' :
        //          `kills` -> should this injector kill the original instruction
        //          `post`  -> should this injector inject after the original instruction
        struct pass_injector {
            static constexpr bool kills = false;
            static constexpr bool post = false;

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
            }
        };

        struct instruction_counter {
            static constexpr bool kills = false;
            static constexpr bool post = false;

            static void init() {
                icnt = 0;
                tcnt = 0;
                bcnt = 0;
                while (!fcnts.empty())
                    fcnts.pop();
            }

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                icnt++;
                tcnt++;
            }

            static uint32_t icnt; /* instructions so far */
            static uint32_t tcnt; /* total instructions */
            static uint32_t bcnt; /* total instructions from block types */
            static std::queue<uint32_t> fcnts;
        };

        struct check_gas_injection {
            static constexpr bool kills = false;
            static constexpr bool post = false;

            static void init() {
                idx = 0;
                chkgas_idx = 0;
            }

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                auto mapped_index = injector_utils::injected_index_mapping.find(chkgas_idx);

                wasm_ops::op_types<>::call_t chktm;
                chktm.field = mapped_index->second;
                chktm.pack(arg.new_code);
            }

            static int32_t idx;
            static int32_t chkgas_idx;
        };

        struct fix_call_index {
            static constexpr bool kills = false;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                wasm_ops::op_types<>::call_t *call_inst = reinterpret_cast<wasm_ops::op_types<>::call_t *>(inst);
                auto mapped_index = injector_utils::injected_index_mapping.find(call_inst->field);

                if (mapped_index != injector_utils::injected_index_mapping.end()) {
                    call_inst->field = mapped_index->second;
                } else {
                    call_inst->field += injector_utils::registered_injected.size();
                }
            }

        };

        // float injections
        constexpr const char *inject_which_op(uint16_t opcode) {
            switch (opcode) {
                case wasm_ops::f32_add_code:
                    return u8"_eosio_f32_add";
                case wasm_ops::f32_sub_code:
                    return u8"_eosio_f32_sub";
                case wasm_ops::f32_mul_code:
                    return u8"_eosio_f32_mul";
                case wasm_ops::f32_div_code:
                    return u8"_eosio_f32_div";
                case wasm_ops::f32_min_code:
                    return u8"_eosio_f32_min";
                case wasm_ops::f32_max_code:
                    return u8"_eosio_f32_max";
                case wasm_ops::f32_copysign_code:
                    return u8"_eosio_f32_copysign";
                case wasm_ops::f32_abs_code:
                    return u8"_eosio_f32_abs";
                case wasm_ops::f32_neg_code:
                    return u8"_eosio_f32_neg";
                case wasm_ops::f32_sqrt_code:
                    return u8"_eosio_f32_sqrt";
                case wasm_ops::f32_ceil_code:
                    return u8"_eosio_f32_ceil";
                case wasm_ops::f32_floor_code:
                    return u8"_eosio_f32_floor";
                case wasm_ops::f32_trunc_code:
                    return u8"_eosio_f32_trunc";
                case wasm_ops::f32_nearest_code:
                    return u8"_eosio_f32_nearest";
                case wasm_ops::f32_eq_code:
                    return u8"_eosio_f32_eq";
                case wasm_ops::f32_ne_code:
                    return u8"_eosio_f32_ne";
                case wasm_ops::f32_lt_code:
                    return u8"_eosio_f32_lt";
                case wasm_ops::f32_le_code:
                    return u8"_eosio_f32_le";
                case wasm_ops::f32_gt_code:
                    return u8"_eosio_f32_gt";
                case wasm_ops::f32_ge_code:
                    return u8"_eosio_f32_ge";
                case wasm_ops::f64_add_code:
                    return u8"_eosio_f64_add";
                case wasm_ops::f64_sub_code:
                    return u8"_eosio_f64_sub";
                case wasm_ops::f64_mul_code:
                    return u8"_eosio_f64_mul";
                case wasm_ops::f64_div_code:
                    return u8"_eosio_f64_div";
                case wasm_ops::f64_min_code:
                    return u8"_eosio_f64_min";
                case wasm_ops::f64_max_code:
                    return u8"_eosio_f64_max";
                case wasm_ops::f64_copysign_code:
                    return u8"_eosio_f64_copysign";
                case wasm_ops::f64_abs_code:
                    return u8"_eosio_f64_abs";
                case wasm_ops::f64_neg_code:
                    return u8"_eosio_f64_neg";
                case wasm_ops::f64_sqrt_code:
                    return u8"_eosio_f64_sqrt";
                case wasm_ops::f64_ceil_code:
                    return u8"_eosio_f64_ceil";
                case wasm_ops::f64_floor_code:
                    return u8"_eosio_f64_floor";
                case wasm_ops::f64_trunc_code:
                    return u8"_eosio_f64_trunc";
                case wasm_ops::f64_nearest_code:
                    return u8"_eosio_f64_nearest";
                case wasm_ops::f64_eq_code:
                    return u8"_eosio_f64_eq";
                case wasm_ops::f64_ne_code:
                    return u8"_eosio_f64_ne";
                case wasm_ops::f64_lt_code:
                    return u8"_eosio_f64_lt";
                case wasm_ops::f64_le_code:
                    return u8"_eosio_f64_le";
                case wasm_ops::f64_gt_code:
                    return u8"_eosio_f64_gt";
                case wasm_ops::f64_ge_code:
                    return u8"_eosio_f64_ge";
                case wasm_ops::f64_promote_f32_code:
                    return u8"_eosio_f32_promote";
                case wasm_ops::f32_demote_f64_code:
                    return u8"_eosio_f64_demote";
                case wasm_ops::i32_trunc_u_f32_code:
                    return u8"_eosio_f32_trunc_i32u";
                case wasm_ops::i32_trunc_s_f32_code:
                    return u8"_eosio_f32_trunc_i32s";
                case wasm_ops::i32_trunc_u_f64_code:
                    return u8"_eosio_f64_trunc_i32u";
                case wasm_ops::i32_trunc_s_f64_code:
                    return u8"_eosio_f64_trunc_i32s";
                case wasm_ops::i64_trunc_u_f32_code:
                    return u8"_eosio_f32_trunc_i64u";
                case wasm_ops::i64_trunc_s_f32_code:
                    return u8"_eosio_f32_trunc_i64s";
                case wasm_ops::i64_trunc_u_f64_code:
                    return u8"_eosio_f64_trunc_i64u";
                case wasm_ops::i64_trunc_s_f64_code:
                    return u8"_eosio_f64_trunc_i64s";
                case wasm_ops::f32_convert_s_i32_code:
                    return u8"_eosio_i32_to_f32";
                case wasm_ops::f32_convert_u_i32_code:
                    return u8"_eosio_ui32_to_f32";
                case wasm_ops::f32_convert_s_i64_code:
                    return u8"_eosio_i64_f32";
                case wasm_ops::f32_convert_u_i64_code:
                    return u8"_eosio_ui64_to_f32";
                case wasm_ops::f64_convert_s_i32_code:
                    return u8"_eosio_i32_to_f64";
                case wasm_ops::f64_convert_u_i32_code:
                    return u8"_eosio_ui32_to_f64";
                case wasm_ops::f64_convert_s_i64_code:
                    return u8"_eosio_i64_to_f64";
                case wasm_ops::f64_convert_u_i64_code:
                    return u8"_eosio_ui64_to_f64";

                default:
                    FTL_THROW(wasm_runtime_exception,
                              "Error, unknown opcode in injection ${0}", opcode);
            }
        }

        template<uint16_t Opcode>
        struct f32_binop_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::f32, ValueType::f32, ValueType::f32>(*(arg.module),
                                                                                            inject_which_op(Opcode),
                                                                                            idx);
                wasm_ops::op_types<>::call_t f32op;
                f32op.field = idx;
                f32op.pack(arg.new_code);
            }
        };

        template<uint16_t Opcode>
        struct f32_unop_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::f32, ValueType::f32>(*(arg.module), inject_which_op(Opcode),
                                                                            idx);
                wasm_ops::op_types<>::call_t f32op;
                f32op.field = idx;
                f32op.pack(arg.new_code);
            }
        };

        template<uint16_t Opcode>
        struct f32_relop_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::i32, ValueType::f32, ValueType::f32>(*(arg.module),
                                                                                            inject_which_op(Opcode),
                                                                                            idx);
                wasm_ops::op_types<>::call_t f32op;
                f32op.field = idx;
                f32op.pack(arg.new_code);
            }
        };

        template<uint16_t Opcode>
        struct f64_binop_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::f64, ValueType::f64, ValueType::f64>(*(arg.module),
                                                                                            inject_which_op(Opcode),
                                                                                            idx);
                wasm_ops::op_types<>::call_t f64op;
                f64op.field = idx;
                f64op.pack(arg.new_code);
            }
        };

        template<uint16_t Opcode>
        struct f64_unop_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::f64, ValueType::f64>(*(arg.module), inject_which_op(Opcode),
                                                                            idx);
                wasm_ops::op_types<>::call_t f64op;
                f64op.field = idx;
                f64op.pack(arg.new_code);
            }
        };

        template<uint16_t Opcode>
        struct f64_relop_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::i32, ValueType::f64, ValueType::f64>(*(arg.module),
                                                                                            inject_which_op(Opcode),
                                                                                            idx);
                wasm_ops::op_types<>::call_t f64op;
                f64op.field = idx;
                f64op.pack(arg.new_code);
            }
        };

        template<uint16_t Opcode>
        struct f32_trunc_i32_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::i32, ValueType::f32>(*(arg.module), inject_which_op(Opcode),
                                                                            idx);
                wasm_ops::op_types<>::call_t f32op;
                f32op.field = idx;
                f32op.pack(arg.new_code);
            }
        };

        template<uint16_t Opcode>
        struct f32_trunc_i64_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::i64, ValueType::f32>(*(arg.module), inject_which_op(Opcode),
                                                                            idx);
                wasm_ops::op_types<>::call_t f32op;
                f32op.field = idx;
                f32op.pack(arg.new_code);
            }
        };

        template<uint16_t Opcode>
        struct f64_trunc_i32_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::i32, ValueType::f64>(*(arg.module), inject_which_op(Opcode),
                                                                            idx);
                wasm_ops::op_types<>::call_t f32op;
                f32op.field = idx;
                f32op.pack(arg.new_code);
            }
        };

        template<uint16_t Opcode>
        struct f64_trunc_i64_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::i64, ValueType::f64>(*(arg.module), inject_which_op(Opcode),
                                                                            idx);
                wasm_ops::op_types<>::call_t f32op;
                f32op.field = idx;
                f32op.pack(arg.new_code);
            }
        };

        template<uint64_t Opcode>
        struct i32_convert_f32_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::f32, ValueType::i32>(*(arg.module), inject_which_op(Opcode),
                                                                            idx);
                wasm_ops::op_types<>::call_t f32op;
                f32op.field = idx;
                f32op.pack(arg.new_code);
            }
        };

        template<uint64_t Opcode>
        struct i64_convert_f32_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::f32, ValueType::i64>(*(arg.module), inject_which_op(Opcode),
                                                                            idx);
                wasm_ops::op_types<>::call_t f32op;
                f32op.field = idx;
                f32op.pack(arg.new_code);
            }
        };

        template<uint64_t Opcode>
        struct i32_convert_f64_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::f64, ValueType::i32>(*(arg.module), inject_which_op(Opcode),
                                                                            idx);
                wasm_ops::op_types<>::call_t f64op;
                f64op.field = idx;
                f64op.pack(arg.new_code);
            }
        };

        template<uint64_t Opcode>
        struct i64_convert_f64_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::f64, ValueType::i64>(*(arg.module), inject_which_op(Opcode),
                                                                            idx);
                wasm_ops::op_types<>::call_t f64op;
                f64op.field = idx;
                f64op.pack(arg.new_code);
            }
        };

        struct f32_promote_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::f64, ValueType::f32>(*(arg.module), u8"_eosio_f32_promote",
                                                                            idx);
                wasm_ops::op_types<>::call_t f32promote;
                f32promote.field = idx;
                f32promote.pack(arg.new_code);
            }
        };

        struct f64_demote_injector {
            static constexpr bool kills = true;
            static constexpr bool post = false;

            static void init() {}

            static void accept(wasm_ops::instr *inst, wasm_ops::visitor_arg &arg) {
                int32_t idx;
                injector_utils::add_import<ResultType::f32, ValueType::f64>(*(arg.module), u8"_eosio_f64_demote",
                                                                            idx);
                wasm_ops::op_types<>::call_t f32promote;
                f32promote.field = idx;
                f32promote.pack(arg.new_code);
            }
        };

        struct pre_op_injectors : wasm_ops::op_types<pass_injector> {
            // float binops
            using f32_add_t         = wasm_ops::f32_add<f32_binop_injector<wasm_ops::f32_add_code>>;
            using f32_sub_t         = wasm_ops::f32_sub<f32_binop_injector<wasm_ops::f32_sub_code>>;
            using f32_div_t         = wasm_ops::f32_div<f32_binop_injector<wasm_ops::f32_div_code>>;
            using f32_mul_t         = wasm_ops::f32_mul<f32_binop_injector<wasm_ops::f32_mul_code>>;
            using f32_min_t         = wasm_ops::f32_min<f32_binop_injector<wasm_ops::f32_min_code>>;
            using f32_max_t         = wasm_ops::f32_max<f32_binop_injector<wasm_ops::f32_max_code>>;
            using f32_copysign_t    = wasm_ops::f32_copysign<f32_binop_injector<wasm_ops::f32_copysign_code>>;
            // float unops
            using f32_abs_t         = wasm_ops::f32_abs<f32_unop_injector<wasm_ops::f32_abs_code>>;
            using f32_neg_t         = wasm_ops::f32_neg<f32_unop_injector<wasm_ops::f32_neg_code>>;
            using f32_sqrt_t        = wasm_ops::f32_sqrt<f32_unop_injector<wasm_ops::f32_sqrt_code>>;
            using f32_floor_t       = wasm_ops::f32_floor<f32_unop_injector<wasm_ops::f32_floor_code>>;
            using f32_ceil_t        = wasm_ops::f32_ceil<f32_unop_injector<wasm_ops::f32_ceil_code>>;
            using f32_trunc_t       = wasm_ops::f32_trunc<f32_unop_injector<wasm_ops::f32_trunc_code>>;
            using f32_nearest_t     = wasm_ops::f32_nearest<f32_unop_injector<wasm_ops::f32_nearest_code>>;
            // float relops
            using f32_eq_t          = wasm_ops::f32_eq<f32_relop_injector<wasm_ops::f32_eq_code>>;
            using f32_ne_t          = wasm_ops::f32_ne<f32_relop_injector<wasm_ops::f32_ne_code>>;
            using f32_lt_t          = wasm_ops::f32_lt<f32_relop_injector<wasm_ops::f32_lt_code>>;
            using f32_le_t          = wasm_ops::f32_le<f32_relop_injector<wasm_ops::f32_le_code>>;
            using f32_gt_t          = wasm_ops::f32_gt<f32_relop_injector<wasm_ops::f32_gt_code>>;
            using f32_ge_t          = wasm_ops::f32_ge<f32_relop_injector<wasm_ops::f32_ge_code>>;

            // float binops
            using f64_add_t         = wasm_ops::f64_add<f64_binop_injector<wasm_ops::f64_add_code>>;
            using f64_sub_t         = wasm_ops::f64_sub<f64_binop_injector<wasm_ops::f64_sub_code>>;
            using f64_div_t         = wasm_ops::f64_div<f64_binop_injector<wasm_ops::f64_div_code>>;
            using f64_mul_t         = wasm_ops::f64_mul<f64_binop_injector<wasm_ops::f64_mul_code>>;
            using f64_min_t         = wasm_ops::f64_min<f64_binop_injector<wasm_ops::f64_min_code>>;
            using f64_max_t         = wasm_ops::f64_max<f64_binop_injector<wasm_ops::f64_max_code>>;
            using f64_copysign_t    = wasm_ops::f64_copysign<f64_binop_injector<wasm_ops::f64_copysign_code>>;
            // float unops
            using f64_abs_t         = wasm_ops::f64_abs<f64_unop_injector<wasm_ops::f64_abs_code>>;
            using f64_neg_t         = wasm_ops::f64_neg<f64_unop_injector<wasm_ops::f64_neg_code>>;
            using f64_sqrt_t        = wasm_ops::f64_sqrt<f64_unop_injector<wasm_ops::f64_sqrt_code>>;
            using f64_floor_t       = wasm_ops::f64_floor<f64_unop_injector<wasm_ops::f64_floor_code>>;
            using f64_ceil_t        = wasm_ops::f64_ceil<f64_unop_injector<wasm_ops::f64_ceil_code>>;
            using f64_trunc_t       = wasm_ops::f64_trunc<f64_unop_injector<wasm_ops::f64_trunc_code>>;
            using f64_nearest_t     = wasm_ops::f64_nearest<f64_unop_injector<wasm_ops::f64_nearest_code>>;
            // float relops
            using f64_eq_t          = wasm_ops::f64_eq<f64_relop_injector<wasm_ops::f64_eq_code>>;
            using f64_ne_t          = wasm_ops::f64_ne<f64_relop_injector<wasm_ops::f64_ne_code>>;
            using f64_lt_t          = wasm_ops::f64_lt<f64_relop_injector<wasm_ops::f64_lt_code>>;
            using f64_le_t          = wasm_ops::f64_le<f64_relop_injector<wasm_ops::f64_le_code>>;
            using f64_gt_t          = wasm_ops::f64_gt<f64_relop_injector<wasm_ops::f64_gt_code>>;
            using f64_ge_t          = wasm_ops::f64_ge<f64_relop_injector<wasm_ops::f64_ge_code>>;


            using f64_promote_f32_t = wasm_ops::f64_promote_f32<f32_promote_injector>;
            using f32_demote_f64_t  = wasm_ops::f32_demote_f64<f64_demote_injector>;


            using i32_trunc_s_f32_t = wasm_ops::i32_trunc_s_f32<f32_trunc_i32_injector<wasm_ops::i32_trunc_s_f32_code>>;
            using i32_trunc_u_f32_t = wasm_ops::i32_trunc_u_f32<f32_trunc_i32_injector<wasm_ops::i32_trunc_u_f32_code>>;
            using i32_trunc_s_f64_t = wasm_ops::i32_trunc_s_f64<f64_trunc_i32_injector<wasm_ops::i32_trunc_s_f64_code>>;
            using i32_trunc_u_f64_t = wasm_ops::i32_trunc_u_f64<f64_trunc_i32_injector<wasm_ops::i32_trunc_u_f64_code>>;
            using i64_trunc_s_f32_t = wasm_ops::i64_trunc_s_f32<f32_trunc_i64_injector<wasm_ops::i64_trunc_s_f32_code>>;
            using i64_trunc_u_f32_t = wasm_ops::i64_trunc_u_f32<f32_trunc_i64_injector<wasm_ops::i64_trunc_u_f32_code>>;
            using i64_trunc_s_f64_t = wasm_ops::i64_trunc_s_f64<f64_trunc_i64_injector<wasm_ops::i64_trunc_s_f64_code>>;
            using i64_trunc_u_f64_t = wasm_ops::i64_trunc_u_f64<f64_trunc_i64_injector<wasm_ops::i64_trunc_u_f64_code>>;

            using f32_convert_s_i32 = wasm_ops::f32_convert_s_i32<i32_convert_f32_injector<wasm_ops::f32_convert_s_i32_code>>;
            using f32_convert_s_i64 = wasm_ops::f32_convert_s_i64<i64_convert_f32_injector<wasm_ops::f32_convert_s_i64_code>>;
            using f32_convert_u_i32 = wasm_ops::f32_convert_u_i32<i32_convert_f32_injector<wasm_ops::f32_convert_u_i32_code>>;
            using f32_convert_u_i64 = wasm_ops::f32_convert_u_i64<i64_convert_f32_injector<wasm_ops::f32_convert_u_i64_code>>;
            using f64_convert_s_i32 = wasm_ops::f64_convert_s_i32<i32_convert_f64_injector<wasm_ops::f64_convert_s_i32_code>>;
            using f64_convert_s_i64 = wasm_ops::f64_convert_s_i64<i64_convert_f64_injector<wasm_ops::f64_convert_s_i64_code>>;
            using f64_convert_u_i32 = wasm_ops::f64_convert_u_i32<i32_convert_f64_injector<wasm_ops::f64_convert_u_i32_code>>;
            using f64_convert_u_i64 = wasm_ops::f64_convert_u_i64<i64_convert_f64_injector<wasm_ops::f64_convert_u_i64_code>>;
        }; // pre_op_injectors

        struct usegas_op_injectors : wasm_ops::op_types<pass_injector> {
        };

        struct post_op_injectors : wasm_ops::op_types<pass_injector> {
            using call_t   = wasm_ops::call<fix_call_index>;
        };

        template<typename ... Visitors>
        struct module_injectors {
            static void inject(IR::Module &m) {
                for (auto injector : {Visitors::inject...}) {
                    injector(m);
                }
            }

            static void init() {
                // place initial values for static fields of injectors here
                for (auto initializer : {Visitors::initializer...}) {
                    initializer();
                }
            }
        };

        // inherit from this class and define your own injectors
        class wasm_binary_injection {
            using standard_module_injectors = module_injectors<max_memory_injection_visitor>;

        public:
            wasm_binary_injection(IR::Module &mod) : _module(&mod) {
                _module_injectors.init();
                // initialize static fields of injectors
                injector_utils::init(mod);
            }

            void inject() {
                _module_injectors.inject(*_module);

                // inject use_gas first
                int32_t use_gas_idx = 0;
                injector_utils::add_import<ResultType::none, ValueType::i64>(*_module, u8"use_gas",
                                                                             use_gas_idx);
                // use gas
                for (auto &fd : _module->functions.defs) {
                    wasm_ops::EOSIO_OperatorDecoderStream <usegas_op_injectors> usegas_decoder(fd.code);
                    wasm_ops::instruction_stream usegas_code(fd.code.size() * 2);

                    wasm_ops::op_types<>::i64_const_t const_inst;
                    wasm_ops::op_types<>::call_t call_usegas;
                    call_usegas.field = use_gas_idx;

                    wasm_ops::instruction_stream tmp_stream(0);
                    int64_t gas = 0;
                    while (usegas_decoder) {
                        auto op = usegas_decoder.decodeOp();
                        uint16_t code = op->get_code();
                        gas += gas_table[code];
                        switch (code) {
                            case wasm_ops::end_code:
                            case wasm_ops::br_code:
                            case wasm_ops::br_table_code:
                            case wasm_ops::br_if_code:
                            case wasm_ops::if__code:
                            case wasm_ops::else__code:
                            case wasm_ops::return__code:
                            case wasm_ops::loop_code:
                                // call use_gas
                                if (gas > 0) {
                                    const_inst.field = gas;
                                    const_inst.pack(&usegas_code);
                                    call_usegas.pack(&usegas_code);
                                    gas = 0;
                                }

                                // add orignal ops
                                usegas_code.set(tmp_stream.idx, (const char *) (&tmp_stream.data[0]));
                                tmp_stream.idx = 0;
                                op->pack(&usegas_code);
                                break;
                            default:
                                op->pack(&tmp_stream);
                                break;
                        }
                    }
                    fd.code = usegas_code.get();
                }

                for (auto &fd : _module->functions.defs) {
                    wasm_ops::EOSIO_OperatorDecoderStream <pre_op_injectors> pre_decoder(fd.code);
                    wasm_ops::instruction_stream pre_code(fd.code.size() * 2);

                    while (pre_decoder) {
                        auto op = pre_decoder.decodeOp();
                        if (op->is_post()) {
                            op->pack(&pre_code);
                            op->visit({_module, &pre_code, &fd, pre_decoder.index()});
                        } else {
                            op->visit({_module, &pre_code, &fd, pre_decoder.index()});
                            if (!(op->is_kill()))
                                op->pack(&pre_code);
                        }
                    }
                    fd.code = pre_code.get();
                }

                for (auto &fd : _module->functions.defs) {
                    wasm_ops::EOSIO_OperatorDecoderStream <post_op_injectors> post_decoder(fd.code);
                    wasm_ops::instruction_stream post_code(fd.code.size() * 2);

                    while (post_decoder) {
                        auto op = post_decoder.decodeOp();
                        if (op->is_post()) {
                            op->pack(&post_code);
                            op->visit({_module, &post_code, &fd, post_decoder.index()});
                        } else {
                            op->visit({_module, &post_code, &fd, post_decoder.index()});
                            if (!(op->is_kill()))
                                op->pack(&post_code);
                        }
                    }
                    fd.code = post_code.get();
                }
            }

        private:
            IR::Module *_module;
            static std::string op_string;
            static standard_module_injectors _module_injectors;
        };

    }
}
