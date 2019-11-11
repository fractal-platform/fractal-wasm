#include "wasm_interface.hpp"
#include "wasm_context.hpp"
#include "exceptions.hpp"
#include "wasm_validation.hpp"
#include "wasm_injection.hpp"
#include "wavm.hpp"
#include "Runtime/Runtime.h"
#include <softfloat.hpp>
#include <compiler_builtins.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <fstream>
#include <iostream>
#include <string.h>

namespace ftl {
    using namespace webassembly;
    using namespace webassembly::common;

    wasm_interface::wasm_interface() {
        runtime_interface = std::make_unique<wavm_runtime>();
    }

    wasm_interface::~wasm_interface() {}

    void wasm_interface::validate(const bytes &code) {
        Module module;
        try {
            Serialization::MemoryInputStream stream((U8 *) code.data(), code.size());
            WASM::serialize(stream, module);
        } catch (const Serialization::FatalSerializationException &e) {
            FTL_ASSERT(false, wasm_serialization_exception, e.message.c_str());
        } catch (const IR::ValidationException &e) {
            FTL_ASSERT(false, wasm_serialization_exception, e.message.c_str());
        }

        wasm_validations::wasm_binary_validation validator(module);
        validator.validate();

        root_resolver resolver(true);
        LinkResult link_result = linkModule(module, resolver);

        //there are a couple opportunties for improvement here--
        //Easy: Cache the Module created here so it can be reused for instantiaion
        //Hard: Kick off instantiation in a separate thread at this location
    }

    void wasm_interface::apply(const sha256 &code_id, const bytes &code, wasm_context &context) {
        get_instantiated_module(code_id, code)->apply(context);
    }

    void wasm_interface::exit() {
        runtime_interface->immediately_exit_currently_running_module();
    }

#if defined(assert)
#undef assert
#endif

    class softfloat_api {
    public:
        softfloat_api(wasm_context &ctx) : context(ctx) {}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

        // float binops
        float _eosio_f32_add(float a, float b) {
            float32_t ret = f32_add(to_softfloat32(a), to_softfloat32(b));
            return *reinterpret_cast<float *>(&ret);
        }

        float _eosio_f32_sub(float a, float b) {
            float32_t ret = f32_sub(to_softfloat32(a), to_softfloat32(b));
            return *reinterpret_cast<float *>(&ret);
        }

        float _eosio_f32_div(float a, float b) {
            float32_t ret = f32_div(to_softfloat32(a), to_softfloat32(b));
            return *reinterpret_cast<float *>(&ret);
        }

        float _eosio_f32_mul(float a, float b) {
            float32_t ret = f32_mul(to_softfloat32(a), to_softfloat32(b));
            return *reinterpret_cast<float *>(&ret);
        }

#pragma GCC diagnostic pop

        float _eosio_f32_min(float af, float bf) {
            float32_t a = to_softfloat32(af);
            float32_t b = to_softfloat32(bf);
            if (is_nan(a)) {
                return af;
            }
            if (is_nan(b)) {
                return bf;
            }
            if (f32_sign_bit(a) != f32_sign_bit(b)) {
                return f32_sign_bit(a) ? af : bf;
            }
            return f32_lt(a, b) ? af : bf;
        }

        float _eosio_f32_max(float af, float bf) {
            float32_t a = to_softfloat32(af);
            float32_t b = to_softfloat32(bf);
            if (is_nan(a)) {
                return af;
            }
            if (is_nan(b)) {
                return bf;
            }
            if (f32_sign_bit(a) != f32_sign_bit(b)) {
                return f32_sign_bit(a) ? bf : af;
            }
            return f32_lt(a, b) ? bf : af;
        }

        float _eosio_f32_copysign(float af, float bf) {
            float32_t a = to_softfloat32(af);
            float32_t b = to_softfloat32(bf);
            //uint32_t sign_of_a = a.v >> 31;
            uint32_t sign_of_b = b.v >> 31;
            a.v &= ~(1 << 31);             // clear the sign bit
            a.v = a.v | (sign_of_b << 31); // add the sign of b
            return from_softfloat32(a);
        }

        // float unops
        float _eosio_f32_abs(float af) {
            float32_t a = to_softfloat32(af);
            a.v &= ~(1 << 31);
            return from_softfloat32(a);
        }

        float _eosio_f32_neg(float af) {
            float32_t a = to_softfloat32(af);
            uint32_t sign = a.v >> 31;
            a.v &= ~(1 << 31);
            a.v |= (!sign << 31);
            return from_softfloat32(a);
        }

        float _eosio_f32_sqrt(float a) {
            float32_t ret = f32_sqrt(to_softfloat32(a));
            return from_softfloat32(ret);
        }

        // ceil, floor, trunc and nearest are lifted from libc
        float _eosio_f32_ceil(float af) {
            float32_t a = to_softfloat32(af);
            int e = (int) (a.v >> 23 & 0xFF) - 0X7F;
            uint32_t m;
            if (e >= 23)
                return af;
            if (e >= 0) {
                m = 0x007FFFFF >> e;
                if ((a.v & m) == 0)
                    return af;
                if (a.v >> 31 == 0)
                    a.v += m;
                a.v &= ~m;
            } else {
                if (a.v >> 31)
                    a.v = 0x80000000; // return -0.0f
                else if (a.v << 1)
                    a.v = 0x3F800000; // return 1.0f
            }

            return from_softfloat32(a);
        }

        float _eosio_f32_floor(float af) {
            float32_t a = to_softfloat32(af);
            int e = (int) (a.v >> 23 & 0xFF) - 0X7F;
            uint32_t m;
            if (e >= 23)
                return af;
            if (e >= 0) {
                m = 0x007FFFFF >> e;
                if ((a.v & m) == 0)
                    return af;
                if (a.v >> 31)
                    a.v += m;
                a.v &= ~m;
            } else {
                if (a.v >> 31 == 0)
                    a.v = 0;
                else if (a.v << 1)
                    a.v = 0xBF800000; // return -1.0f
            }
            return from_softfloat32(a);
        }

        float _eosio_f32_trunc(float af) {
            float32_t a = to_softfloat32(af);
            int e = (int) (a.v >> 23 & 0xff) - 0x7f + 9;
            uint32_t m;
            if (e >= 23 + 9)
                return af;
            if (e < 9)
                e = 1;
            m = -1U >> e;
            if ((a.v & m) == 0)
                return af;
            a.v &= ~m;
            return from_softfloat32(a);
        }

        float _eosio_f32_nearest(float af) {
            float32_t a = to_softfloat32(af);
            int e = a.v >> 23 & 0xff;
            int s = a.v >> 31;
            float32_t y;
            if (e >= 0x7f + 23)
                return af;
            if (s)
                y = f32_add(f32_sub(a, float32_t{inv_float_eps}), float32_t{inv_float_eps});
            else
                y = f32_sub(f32_add(a, float32_t{inv_float_eps}), float32_t{inv_float_eps});
            if (f32_eq(y, {0}))
                return s ? -0.0f : 0.0f;
            return from_softfloat32(y);
        }

        // float relops
        bool _eosio_f32_eq(float a, float b) { return f32_eq(to_softfloat32(a), to_softfloat32(b)); }

        bool _eosio_f32_ne(float a, float b) { return !f32_eq(to_softfloat32(a), to_softfloat32(b)); }

        bool _eosio_f32_lt(float a, float b) { return f32_lt(to_softfloat32(a), to_softfloat32(b)); }

        bool _eosio_f32_le(float a, float b) { return f32_le(to_softfloat32(a), to_softfloat32(b)); }

        bool _eosio_f32_gt(float af, float bf) {
            float32_t a = to_softfloat32(af);
            float32_t b = to_softfloat32(bf);
            if (is_nan(a))
                return false;
            if (is_nan(b))
                return false;
            return !f32_le(a, b);
        }

        bool _eosio_f32_ge(float af, float bf) {
            float32_t a = to_softfloat32(af);
            float32_t b = to_softfloat32(bf);
            if (is_nan(a))
                return false;
            if (is_nan(b))
                return false;
            return !f32_lt(a, b);
        }

        // double binops
        double _eosio_f64_add(double a, double b) {
            float64_t ret = f64_add(to_softfloat64(a), to_softfloat64(b));
            return from_softfloat64(ret);
        }

        double _eosio_f64_sub(double a, double b) {
            float64_t ret = f64_sub(to_softfloat64(a), to_softfloat64(b));
            return from_softfloat64(ret);
        }

        double _eosio_f64_div(double a, double b) {
            float64_t ret = f64_div(to_softfloat64(a), to_softfloat64(b));
            return from_softfloat64(ret);
        }

        double _eosio_f64_mul(double a, double b) {
            float64_t ret = f64_mul(to_softfloat64(a), to_softfloat64(b));
            return from_softfloat64(ret);
        }

        double _eosio_f64_min(double af, double bf) {
            float64_t a = to_softfloat64(af);
            float64_t b = to_softfloat64(bf);
            if (is_nan(a))
                return af;
            if (is_nan(b))
                return bf;
            if (f64_sign_bit(a) != f64_sign_bit(b))
                return f64_sign_bit(a) ? af : bf;
            return f64_lt(a, b) ? af : bf;
        }

        double _eosio_f64_max(double af, double bf) {
            float64_t a = to_softfloat64(af);
            float64_t b = to_softfloat64(bf);
            if (is_nan(a))
                return af;
            if (is_nan(b))
                return bf;
            if (f64_sign_bit(a) != f64_sign_bit(b))
                return f64_sign_bit(a) ? bf : af;
            return f64_lt(a, b) ? bf : af;
        }

        double _eosio_f64_copysign(double af, double bf) {
            float64_t a = to_softfloat64(af);
            float64_t b = to_softfloat64(bf);
            //uint64_t sign_of_a = a.v >> 63;
            uint64_t sign_of_b = b.v >> 63;
            a.v &= ~(uint64_t(1) << 63);             // clear the sign bit
            a.v = a.v | (sign_of_b << 63); // add the sign of b
            return from_softfloat64(a);
        }

        // double unops
        double _eosio_f64_abs(double af) {
            float64_t a = to_softfloat64(af);
            a.v &= ~(uint64_t(1) << 63);
            return from_softfloat64(a);
        }

        double _eosio_f64_neg(double af) {
            float64_t a = to_softfloat64(af);
            uint64_t sign = a.v >> 63;
            a.v &= ~(uint64_t(1) << 63);
            a.v |= (uint64_t(!sign) << 63);
            return from_softfloat64(a);
        }

        double _eosio_f64_sqrt(double a) {
            float64_t ret = f64_sqrt(to_softfloat64(a));
            return from_softfloat64(ret);
        }

        // ceil, floor, trunc and nearest are lifted from libc
        double _eosio_f64_ceil(double af) {
            float64_t a = to_softfloat64(af);
            float64_t ret;
            int e = a.v >> 52 & 0x7ff;
            float64_t y;
            if (e >= 0x3ff + 52 || f64_eq(a, {0}))
                return af;
            /* y = int(x) - x, where int(x) is an integer neighbor of x */
            if (a.v >> 63)
                y = f64_sub(f64_add(f64_sub(a, float64_t{inv_double_eps}), float64_t{inv_double_eps}), a);
            else
                y = f64_sub(f64_sub(f64_add(a, float64_t{inv_double_eps}), float64_t{inv_double_eps}), a);
            /* special case because of non-nearest rounding modes */
            if (e <= 0x3ff - 1) {
                return a.v >> 63 ? -0.0
                                 : 1.0; //float64_t{0x8000000000000000} : float64_t{0xBE99999A3F800000}; //either -0.0 or 1
            }
            if (f64_lt(y, to_softfloat64(0))) {
                ret = f64_add(f64_add(a, y), to_softfloat64(1)); // 0xBE99999A3F800000 } ); // plus 1
                return from_softfloat64(ret);
            }
            ret = f64_add(a, y);
            return from_softfloat64(ret);
        }

        double _eosio_f64_floor(double af) {
            float64_t a = to_softfloat64(af);
            float64_t ret;
            int e = a.v >> 52 & 0x7FF;
            float64_t y;
            //double de = 1 / DBL_EPSILON;
            if (a.v == 0x8000000000000000) {
                return af;
            }
            if (e >= 0x3FF + 52 || a.v == 0) {
                return af;
            }
            if (a.v >> 63)
                y = f64_sub(f64_add(f64_sub(a, float64_t{inv_double_eps}), float64_t{inv_double_eps}), a);
            else
                y = f64_sub(f64_sub(f64_add(a, float64_t{inv_double_eps}), float64_t{inv_double_eps}), a);
            if (e <= 0x3FF - 1) {
                return a.v >> 63 ? -1.0 : 0.0; //float64_t{0xBFF0000000000000} : float64_t{0}; // -1 or 0
            }
            if (!f64_le(y, float64_t{0})) {
                ret = f64_sub(f64_add(a, y), to_softfloat64(1.0));
                return from_softfloat64(ret);
            }
            ret = f64_add(a, y);
            return from_softfloat64(ret);
        }

        double _eosio_f64_trunc(double af) {
            float64_t a = to_softfloat64(af);
            int e = (int) (a.v >> 52 & 0x7ff) - 0x3ff + 12;
            uint64_t m;
            if (e >= 52 + 12)
                return af;
            if (e < 12)
                e = 1;
            m = -1ULL >> e;
            if ((a.v & m) == 0)
                return af;
            a.v &= ~m;
            return from_softfloat64(a);
        }

        double _eosio_f64_nearest(double af) {
            float64_t a = to_softfloat64(af);
            int e = (a.v >> 52 & 0x7FF);
            int s = a.v >> 63;
            float64_t y;
            if (e >= 0x3FF + 52)
                return af;
            if (s)
                y = f64_add(f64_sub(a, float64_t{inv_double_eps}), float64_t{inv_double_eps});
            else
                y = f64_sub(f64_add(a, float64_t{inv_double_eps}), float64_t{inv_double_eps});
            if (f64_eq(y, float64_t{0}))
                return s ? -0.0 : 0.0;
            return from_softfloat64(y);
        }

        // double relops
        bool _eosio_f64_eq(double a, double b) { return f64_eq(to_softfloat64(a), to_softfloat64(b)); }

        bool _eosio_f64_ne(double a, double b) { return !f64_eq(to_softfloat64(a), to_softfloat64(b)); }

        bool _eosio_f64_lt(double a, double b) { return f64_lt(to_softfloat64(a), to_softfloat64(b)); }

        bool _eosio_f64_le(double a, double b) { return f64_le(to_softfloat64(a), to_softfloat64(b)); }

        bool _eosio_f64_gt(double af, double bf) {
            float64_t a = to_softfloat64(af);
            float64_t b = to_softfloat64(bf);
            if (is_nan(a))
                return false;
            if (is_nan(b))
                return false;
            return !f64_le(a, b);
        }

        bool _eosio_f64_ge(double af, double bf) {
            float64_t a = to_softfloat64(af);
            float64_t b = to_softfloat64(bf);
            if (is_nan(a))
                return false;
            if (is_nan(b))
                return false;
            return !f64_lt(a, b);
        }

        // float and double conversions
        double _eosio_f32_promote(float a) {
            return from_softfloat64(f32_to_f64(to_softfloat32(a)));
        }

        float _eosio_f64_demote(double a) {
            return from_softfloat32(f64_to_f32(to_softfloat64(a)));
        }

        int32_t _eosio_f32_trunc_i32s(float af) {
            float32_t a = to_softfloat32(af);
            if (_eosio_f32_ge(af, 2147483648.0f) || _eosio_f32_lt(af, -2147483648.0f))
                FTL_THROW(wasm_runtime_exception, "Error, f32.convert_s/i32 overflow");

            if (is_nan(a))
                FTL_THROW(wasm_runtime_exception, "Error, f32.convert_s/i32 unrepresentable");
            return f32_to_i32(to_softfloat32(_eosio_f32_trunc(af)), 0, false);
        }

        int32_t _eosio_f64_trunc_i32s(double af) {
            float64_t a = to_softfloat64(af);
            if (_eosio_f64_ge(af, 2147483648.0) || _eosio_f64_lt(af, -2147483648.0))
                FTL_THROW(wasm_runtime_exception, "Error, f64.convert_s/i32 overflow");
            if (is_nan(a))
                FTL_THROW(wasm_runtime_exception, "Error, f64.convert_s/i32 unrepresentable");
            return f64_to_i32(to_softfloat64(_eosio_f64_trunc(af)), 0, false);
        }

        uint32_t _eosio_f32_trunc_i32u(float af) {
            float32_t a = to_softfloat32(af);
            if (_eosio_f32_ge(af, 4294967296.0f) || _eosio_f32_le(af, -1.0f))
                FTL_THROW(wasm_runtime_exception, "Error, f32.convert_u/i32 overflow");
            if (is_nan(a))
                FTL_THROW(wasm_runtime_exception, "Error, f32.convert_u/i32 unrepresentable");
            return f32_to_ui32(to_softfloat32(_eosio_f32_trunc(af)), 0, false);
        }

        uint32_t _eosio_f64_trunc_i32u(double af) {
            float64_t a = to_softfloat64(af);
            if (_eosio_f64_ge(af, 4294967296.0) || _eosio_f64_le(af, -1.0))
                FTL_THROW(wasm_runtime_exception, "Error, f64.convert_u/i32 overflow");
            if (is_nan(a))
                FTL_THROW(wasm_runtime_exception, "Error, f64.convert_u/i32 unrepresentable");
            return f64_to_ui32(to_softfloat64(_eosio_f64_trunc(af)), 0, false);
        }

        int64_t _eosio_f32_trunc_i64s(float af) {
            float32_t a = to_softfloat32(af);
            if (_eosio_f32_ge(af, 9223372036854775808.0f) || _eosio_f32_lt(af, -9223372036854775808.0f))
                FTL_THROW(wasm_runtime_exception, "Error, f32.convert_s/i64 overflow");
            if (is_nan(a))
                FTL_THROW(wasm_runtime_exception, "Error, f32.convert_s/i64 unrepresentable");
            return f32_to_i64(to_softfloat32(_eosio_f32_trunc(af)), 0, false);
        }

        int64_t _eosio_f64_trunc_i64s(double af) {
            float64_t a = to_softfloat64(af);
            if (_eosio_f64_ge(af, 9223372036854775808.0) || _eosio_f64_lt(af, -9223372036854775808.0))
                FTL_THROW(wasm_runtime_exception, "Error, f64.convert_s/i64 overflow");
            if (is_nan(a))
                FTL_THROW(wasm_runtime_exception, "Error, f64.convert_s/i64 unrepresentable");

            return f64_to_i64(to_softfloat64(_eosio_f64_trunc(af)), 0, false);
        }

        uint64_t _eosio_f32_trunc_i64u(float af) {
            float32_t a = to_softfloat32(af);
            if (_eosio_f32_ge(af, 18446744073709551616.0f) || _eosio_f32_le(af, -1.0f))
                FTL_THROW(wasm_runtime_exception, "Error, f32.convert_u/i64 overflow");
            if (is_nan(a))
                FTL_THROW(wasm_runtime_exception, "Error, f32.convert_u/i64 unrepresentable");
            return f32_to_ui64(to_softfloat32(_eosio_f32_trunc(af)), 0, false);
        }

        uint64_t _eosio_f64_trunc_i64u(double af) {
            float64_t a = to_softfloat64(af);
            if (_eosio_f64_ge(af, 18446744073709551616.0) || _eosio_f64_le(af, -1.0))
                FTL_THROW(wasm_runtime_exception, "Error, f64.convert_u/i64 overflow");
            if (is_nan(a))
                FTL_THROW(wasm_runtime_exception, "Error, f64.convert_u/i64 unrepresentable");
            return f64_to_ui64(to_softfloat64(_eosio_f64_trunc(af)), 0, false);
        }

        float _eosio_i32_to_f32(int32_t a) {
            return from_softfloat32(i32_to_f32(a));
        }

        float _eosio_i64_to_f32(int64_t a) {
            return from_softfloat32(i64_to_f32(a));
        }

        float _eosio_ui32_to_f32(uint32_t a) {
            return from_softfloat32(ui32_to_f32(a));
        }

        float _eosio_ui64_to_f32(uint64_t a) {
            return from_softfloat32(ui64_to_f32(a));
        }

        double _eosio_i32_to_f64(int32_t a) {
            return from_softfloat64(i32_to_f64(a));
        }

        double _eosio_i64_to_f64(int64_t a) {
            return from_softfloat64(i64_to_f64(a));
        }

        double _eosio_ui32_to_f64(uint32_t a) {
            return from_softfloat64(ui32_to_f64(a));
        }

        double _eosio_ui64_to_f64(uint64_t a) {
            return from_softfloat64(ui64_to_f64(a));
        }

        static bool is_nan(const float32_t f) {
            return f32_is_nan(f);
        }

        static bool is_nan(const float64_t f) {
            return f64_is_nan(f);
        }

        static bool is_nan(const float128_t &f) {
            return f128_is_nan(f);
        }

        static constexpr uint32_t inv_float_eps = 0x4B000000;
        static constexpr uint64_t inv_double_eps = 0x4330000000000000;

    protected:
        wasm_context &context;
    };

    class crypto_api {
    public:
        explicit crypto_api(wasm_context &ctx) : context(ctx) {}

        /**
         * This method can be optimized out during replay as it has
         * no possible side effects other than "passing".
         */
        void assert_recover_key(const sha256 &digest,
                                array_ptr<char> sig, size_t siglen,
                                array_ptr<char> pub, size_t publen) {
            context.use_gas(GAS_RECOVER_KEY);

            //TODO
            /*
            fc::crypto::signature s;
            fc::crypto::public_key p;
            datastream<const char *> ds(sig, siglen);
            datastream<const char *> pubds(pub, publen);

            fc::raw::unpack(ds, s);
            fc::raw::unpack(pubds, p);

            auto check = fc::crypto::public_key(s, digest, false);
            EOS_ASSERT(check == p, crypto_api_exception, "Error expected key different than recovered key");
             */
        }

        int recover_key(const sha256 &digest,
                        array_ptr<char> sig, size_t siglen,
                        array_ptr<char> pub, size_t publen) {
            context.use_gas(GAS_RECOVER_KEY);

            //TODO
            /*
            fc::crypto::signature s;
            datastream<const char *> ds(sig, siglen);
            datastream<char *> pubds(pub, publen);

            fc::raw::unpack(ds, s);
            fc::raw::pack(pubds, fc::crypto::public_key(s, digest, false));
            return pubds.tellp();
             */
            return 0;
        }

        template<class Encoder>
        auto encode(char *data, size_t datalen) {
            Encoder e;
            const size_t bs = 10 * 1024;
            while (datalen > bs) {
                e.write(data, bs);
                data += bs;
                datalen -= bs;
            }
            e.write(data, datalen);
            return e.result();
        }

        void assert_sha256(array_ptr<char> data, size_t datalen, const sha256 &hash_val) {
            context.use_gas(GAS_SHA256_BASE + GAS_SHA256_BYTE * datalen);

            //TODO
            /*
            auto result = encode<sha256::encoder>(data, datalen);
            EOS_ASSERT(result == hash_val, crypto_api_exception, "hash mismatch");
             */
        }

        void sha256(array_ptr<char> data, size_t datalen, sha256 &hash_val) {
            context.use_gas(GAS_SHA256_BASE + GAS_SHA256_BYTE * datalen);

            //TODO
            /*
            hash_val = encode<sha256::encoder>(data, datalen);
             */
        }

    protected:
        wasm_context &context;
    };

    class system_api {
    public:
        explicit system_api(wasm_context &ctx) : context(ctx) {}

        uint64_t current_time() {
            context.use_gas(GAS_CALL_BASE);
            return context.chain_current_time();
        }

        uint64_t current_height() {
            context.use_gas(GAS_CALL_BASE);
            return context.chain_current_height();
        }

        void current_hash(sha256 &simple_hash, sha256 &full_hash) {
            context.use_gas(GAS_CALL_BASE);
            context.chain_current_hash(simple_hash, full_hash);
        }

        void log_0(array_ptr<const char> data, size_t data_size, const sha256 &name) {
            context.use_gas(GAS_LOG_0 + data_size * GAS_LOG_DATA);
            context.log0(data, data_size, name);
        }

        void log_1(array_ptr<const char> data, size_t data_size, const sha256 &name, const sha256 &param1) {
            context.use_gas(GAS_LOG_1 + data_size * GAS_LOG_DATA);
            context.log1(data, data_size, name, param1);
        }

        void log_2(array_ptr<const char> data, size_t data_size, const sha256 &name, const sha256 &param1,
                   const sha256 &param2) {
            context.use_gas(GAS_LOG_2 + data_size * GAS_LOG_DATA);
            context.log2(data, data_size, name, param1, param2);
        }

    protected:
        wasm_context &context;
    };

    constexpr size_t max_assert_message = 1024;

    class context_free_system_api {
    public:
        explicit context_free_system_api(wasm_context &ctx) : context(ctx) {}

        void abort() {
            FTL_ASSERT(false, wasm_runtime_exception, "abort() called");
        }

        // Kept as intrinsic rather than implementing on WASM side (using eosio_assert_message and strlen) because strlen is faster on native side.
        void ftl_assert(bool condition, null_terminated_ptr msg) {
            context.use_gas(GAS_CALL_BASE);
            if (BOOST_UNLIKELY(!condition)) {
                const size_t sz = strnlen(msg, max_assert_message);
                std::string message(msg, sz);
                FTL_THROW(wasm_runtime_exception, "assertion failure with message: ${0}", message);
            }
        }

        void ftl_assert_message(bool condition, array_ptr<const char> msg, size_t msg_len) {
            context.use_gas(GAS_CALL_BASE);
            if (BOOST_UNLIKELY(!condition)) {
                const size_t sz = msg_len > max_assert_message ? max_assert_message : msg_len;
                std::string message(msg, sz);
                FTL_THROW(wasm_runtime_exception, "assertion failure with message: ${0}", message);
            }
        }

        void ftl_assert_code(bool condition, uint64_t error_code) {
            context.use_gas(GAS_CALL_BASE);
            if (BOOST_UNLIKELY(!condition)) {
                FTL_THROW(wasm_runtime_exception,
                          "assertion failure with error code: ${0}", error_code);
            }
        }

        void ftl_exit(int32_t code) {
            context.use_gas(GAS_CALL_BASE);
            context.get_wasm_interface().exit();
        }

    protected:
        wasm_context &context;
    };

    class action_api {
    public:
        explicit action_api(wasm_context &ctx) : context(ctx) {}

        void use_gas(uint64_t gas) {
            context.use_gas(gas);
        }

        int read_action_data(array_ptr<char> memory, size_t buffer_size) {
            context.use_gas(GAS_CALL_BASE);
            auto s = context.act.data.size();
            if (buffer_size == 0) return s;

            context.use_gas(s * GAS_MEMOP_BYTE);
            auto copy_size = std::min(buffer_size, s);
            memcpy(memory, context.act.data.data(), copy_size);

            return copy_size;
        }

        int action_data_size() {
            context.use_gas(GAS_CALL_BASE);
            return context.act.data.size();
        }

        void get_from(array_ptr<char> address, size_t addr_size) {
            context.use_gas(GAS_CALL_BASE + 20 * GAS_MEMOP_BYTE);
            int len = addr_size >= 20 ? 20 : addr_size;
            memcpy(address, context.from_addr_bytes, len);
        }

        void get_to(array_ptr<char> address, size_t addr_size) {
            context.use_gas(GAS_CALL_BASE + 20 * GAS_MEMOP_BYTE);
            int len = addr_size >= 20 ? 20 : addr_size;
            memcpy(address, context.to_addr_bytes, len);
        }

        void get_owner(array_ptr<char> address, size_t addr_size) {
            context.use_gas(GAS_CALL_BASE + 20 * GAS_MEMOP_BYTE);
            int len = addr_size >= 20 ? 20 : addr_size;
            memcpy(address, context.owner_addr_bytes, len);
        }

        void get_user(array_ptr<char> address, size_t addr_size) {
            context.use_gas(GAS_CALL_BASE + 20 * GAS_MEMOP_BYTE);
            int len = addr_size >= 20 ? 20 : addr_size;
            memcpy(address, context.user_addr_bytes, len);
        }

        void transfer(array_ptr<char> address, size_t addr_size, uint64_t amount) {
            context.use_gas(GAS_TRANSFER);
            if (addr_size != 20) {
                FTL_THROW(invalid_address_exception, "address exception");
            }
            context.transfer(address, amount);
        }

        uint64_t get_amount() {
            context.use_gas(GAS_CALL_BASE);
            return context.transfer_amount;
        }

        int call_action(array_ptr<char> address, size_t addr_size, array_ptr<char> action, size_t action_size,
                        uint64_t amount, int storage_delegate, int user_delegate) {
            context.use_gas(GAS_CALL_BASE);
            if (addr_size != 20) {
                FTL_THROW(invalid_address_exception, "address exception");
            }

            Runtime::theMemoryInstance = NULL;
            int ret = context.call_action(address, action, action_size, amount, storage_delegate, user_delegate);
            the_running_instance_context.memory = context.memory;
            the_running_instance_context.apply_ctx = &context;
            Runtime::theMemoryInstance = context.memory;
            return ret;
        }

        int call_result(array_ptr<char> result, size_t result_size) {
            context.use_gas(GAS_CALL_BASE);
            return context.call_result(result, result_size);
        }

        int set_result(array_ptr<char> result, size_t result_size) {
            context.use_gas(GAS_CALL_BASE);
            return context.set_result(result, result_size);
        }

    protected:
        wasm_context &context;
    };

    class console_api {
    public:
        console_api(wasm_context &ctx) : context(ctx), ignore(false) {}

        // Kept as intrinsic rather than implementing on WASM side (using prints_l and strlen) because strlen is faster on native side.
        void prints(null_terminated_ptr str) {
            if (!ignore) {
                context.console_append<const char *>(str);
            }
        }

        void prints_l(array_ptr<const char> str, size_t str_len) {
            if (!ignore) {
                context.console_append(string(str, str_len));
            }
        }

        void printi(int64_t val) {
            if (!ignore) {
                context.console_append(val);
            }
        }

        void printui(uint64_t val) {
            if (!ignore) {
                context.console_append(val);
            }
        }

        void printsf(float val) {
            if (!ignore) {
                // Assumes float representation on native side is the same as on the WASM side
                auto &console = context.get_console_stream();
                auto orig_prec = console.precision();

                console.precision(std::numeric_limits<float>::digits10);
                context.console_append(val);

                console.precision(orig_prec);
            }
        }

        void printdf(double val) {
            if (!ignore) {
                // Assumes double representation on native side is the same as on the WASM side
                auto &console = context.get_console_stream();
                auto orig_prec = console.precision();

                console.precision(std::numeric_limits<double>::digits10);
                context.console_append(val);

                console.precision(orig_prec);
            }
        }

        void printqf(const float128_t &val) {
            /*
             * Native-side long double uses an 80-bit extended-precision floating-point number.
             * The easiest solution for now was to use the Berkeley softfloat library to round the 128-bit
             * quadruple-precision floating-point number to an 80-bit extended-precision floating-point number
             * (losing precision) which then allows us to simply cast it into a long double for printing purposes.
             *
             * Later we might find a better solution to print the full quadruple-precision floating-point number.
             * Maybe with some compilation flag that turns long double into a quadruple-precision floating-point number,
             * or maybe with some library that allows us to print out quadruple-precision floating-point numbers without
             * having to deal with long doubles at all.
             */

            if (!ignore) {
                auto &console = context.get_console_stream();
                auto orig_prec = console.precision();

#ifdef __x86_64__
                console.precision(std::numeric_limits<long double>::digits10);
                extFloat80_t val_approx;
                f128M_to_extF80M(&val, &val_approx);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
                context.console_append(*(long double *) (&val_approx));
#pragma GCC diagnostic pop
#else
                console.precision( std::numeric_limits<double>::digits10 );
                double val_approx = from_softfloat64( f128M_to_f64(&val) );
                context.console_append(val_approx);
#endif
                console.precision(orig_prec);
            }
        }

        void printn(const name &value) {
            if (!ignore) {
                context.console_append(value.to_string());
            }
        }

        void printhex(array_ptr<const char> data, size_t data_len) {
            if (!ignore) {
                // TODO
                //context.console_append(fc::to_hex(data, data_len));
            }
        }

    protected:
        wasm_context &context;

    private:
        bool ignore;
    };

    class database_api {
    public:
        explicit database_api(wasm_context &ctx) : context(ctx) {}

        void db_store(uint64_t table, array_ptr<const char> key, size_t key_size, array_ptr<const char> buffer,
                      size_t buffer_size) {
            context.use_gas(GAS_DBSTORE_BASE + buffer_size * GAS_DBSTORE_BYTE);
            context.db_store(table, key, key_size, buffer, buffer_size);
        }

        int db_load(uint64_t table, array_ptr<const char> key, size_t key_size, array_ptr<char> buffer,
                    size_t buffer_size) {
            context.use_gas(GAS_DBLOAD_BASE + buffer_size * GAS_DBLOAD_BYTE);
            return context.db_load(table, key, key_size, buffer, buffer_size);
        }

        int db_has_key(uint64_t table, array_ptr<const char> key, size_t key_size) {
            context.use_gas(GAS_DB_HAS);
            return context.db_has_key(table, key, key_size);
        }

        void db_remove_key(uint64_t table, array_ptr<const char> key, size_t key_size) {
            context.use_gas(GAS_DB_RMV);
            context.db_remove_key(table, key, key_size);
        }

        int db_has_table(uint64_t table) {
            context.use_gas(GAS_DB_HAS);
            return context.db_has_table(table);
        }

        void db_remove_table(uint64_t table) {
            context.use_gas(GAS_DB_RMV);
            context.db_remove_table(table);
        }

    protected:
        wasm_context &context;
    };

    class memory_api {
    public:
        memory_api(wasm_context &ctx) : context(ctx) {}

        char *memcpy(array_ptr<char> dest, array_ptr<const char> src, size_t length) {
            context.use_gas(GAS_CALL_BASE + GAS_MEMOP_BYTE * length);
            FTL_ASSERT((size_t) (std::abs((ptrdiff_t) dest.value - (ptrdiff_t) src.value)) >= length,
                       overlapping_memory_error, "memcpy can only accept non-aliasing pointers");
            return (char *) ::memcpy(dest, src, length);
        }

        char *memmove(array_ptr<char> dest, array_ptr<const char> src, size_t length) {
            context.use_gas(GAS_CALL_BASE + GAS_MEMOP_BYTE * length);
            return (char *) ::memmove(dest, src, length);
        }

        int memcmp(array_ptr<const char> dest, array_ptr<const char> src, size_t length) {
            context.use_gas(GAS_CALL_BASE + GAS_MEMOP_BYTE * length);
            int ret = ::memcmp(dest, src, length);
            if (ret < 0)
                return -1;
            if (ret > 0)
                return 1;
            return 0;
        }

        char *memset(array_ptr<char> dest, int value, size_t length) {
            context.use_gas(GAS_CALL_BASE + GAS_MEMOP_BYTE * length);
            return (char *) ::memset(dest, value, length);
        }

    protected:
        wasm_context &context;
    };

    class compiler_builtins {
    public:
        compiler_builtins(wasm_context &ctx) : context(ctx) {}

        void __ashlti3(__int128 &ret, uint64_t low, uint64_t high, uint32_t shift) {
            context.use_gas(GAS_CALL_BASE);
            unsigned __int128 i = (static_cast<unsigned __int128>(high) >> 64) | low;
            i <<= shift;
            ret = (unsigned __int128) i;
        }

        void __ashrti3(__int128 &ret, uint64_t low, uint64_t high, uint32_t shift) {
            context.use_gas(GAS_CALL_BASE);
            // retain the signedness
            ret = high;
            ret <<= 64;
            ret |= low;
            ret >>= shift;
        }

        void __lshlti3(__int128 &ret, uint64_t low, uint64_t high, uint32_t shift) {
            context.use_gas(GAS_CALL_BASE);
            unsigned __int128 i = (static_cast<unsigned __int128>(high) >> 64) | low;
            i <<= shift;
            ret = (unsigned __int128) i;
        }

        void __lshrti3(__int128 &ret, uint64_t low, uint64_t high, uint32_t shift) {
            context.use_gas(GAS_CALL_BASE);
            unsigned __int128 i = (static_cast<unsigned __int128>(high) >> 64) | low;
            i >>= shift;
            ret = (unsigned __int128) i;
        }

        void __divti3(__int128 &ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            __int128 lhs = ha;
            __int128 rhs = hb;

            lhs <<= 64;
            lhs |= la;

            rhs <<= 64;
            rhs |= lb;

            FTL_ASSERT(rhs != 0, arithmetic_exception, "divide by zero");

            lhs /= rhs;

            ret = lhs;
        }

        void __udivti3(unsigned __int128 &ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            unsigned __int128 lhs = ha;
            unsigned __int128 rhs = hb;

            lhs <<= 64;
            lhs |= la;

            rhs <<= 64;
            rhs |= lb;

            FTL_ASSERT(rhs != 0, arithmetic_exception, "divide by zero");

            lhs /= rhs;
            ret = lhs;
        }

        void __multi3(__int128 &ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            __int128 lhs = ha;
            __int128 rhs = hb;

            lhs <<= 64;
            lhs |= la;

            rhs <<= 64;
            rhs |= lb;

            lhs *= rhs;
            ret = lhs;
        }

        void __modti3(__int128 &ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            __int128 lhs = ha;
            __int128 rhs = hb;

            lhs <<= 64;
            lhs |= la;

            rhs <<= 64;
            rhs |= lb;

            FTL_ASSERT(rhs != 0, arithmetic_exception, "divide by zero");

            lhs %= rhs;
            ret = lhs;
        }

        void __umodti3(unsigned __int128 &ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            unsigned __int128 lhs = ha;
            unsigned __int128 rhs = hb;

            lhs <<= 64;
            lhs |= la;

            rhs <<= 64;
            rhs |= lb;

            FTL_ASSERT(rhs != 0, arithmetic_exception, "divide by zero");

            lhs %= rhs;
            ret = lhs;
        }

        // arithmetic long double
        void __addtf3(float128_t &ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            float128_t a = {{la, ha}};
            float128_t b = {{lb, hb}};
            ret = f128_add(a, b);
        }

        void __subtf3(float128_t &ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            float128_t a = {{la, ha}};
            float128_t b = {{lb, hb}};
            ret = f128_sub(a, b);
        }

        void __multf3(float128_t &ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            float128_t a = {{la, ha}};
            float128_t b = {{lb, hb}};
            ret = f128_mul(a, b);
        }

        void __divtf3(float128_t &ret, uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            float128_t a = {{la, ha}};
            float128_t b = {{lb, hb}};
            ret = f128_div(a, b);
        }

        void __negtf2(float128_t &ret, uint64_t la, uint64_t ha) {
            context.use_gas(GAS_CALL_BASE);
            ret = {{la, (ha ^ (uint64_t) 1 << 63)}};
        }

        // conversion long double
        void __extendsftf2(float128_t &ret, float f) {
            context.use_gas(GAS_CALL_BASE);
            ret = f32_to_f128(to_softfloat32(f));
        }

        void __extenddftf2(float128_t &ret, double d) {
            context.use_gas(GAS_CALL_BASE);
            ret = f64_to_f128(to_softfloat64(d));
        }

        double __trunctfdf2(uint64_t l, uint64_t h) {
            context.use_gas(GAS_CALL_BASE);
            float128_t f = {{l, h}};
            return from_softfloat64(f128_to_f64(f));
        }

        float __trunctfsf2(uint64_t l, uint64_t h) {
            context.use_gas(GAS_CALL_BASE);
            float128_t f = {{l, h}};
            return from_softfloat32(f128_to_f32(f));
        }

        int32_t __fixtfsi(uint64_t l, uint64_t h) {
            context.use_gas(GAS_CALL_BASE);
            float128_t f = {{l, h}};
            return f128_to_i32(f, 0, false);
        }

        int64_t __fixtfdi(uint64_t l, uint64_t h) {
            context.use_gas(GAS_CALL_BASE);
            float128_t f = {{l, h}};
            return f128_to_i64(f, 0, false);
        }

        void __fixtfti(__int128 &ret, uint64_t l, uint64_t h) {
            context.use_gas(GAS_CALL_BASE);
            float128_t f = {{l, h}};
            ret = ___fixtfti(f);
        }

        uint32_t __fixunstfsi(uint64_t l, uint64_t h) {
            context.use_gas(GAS_CALL_BASE);
            float128_t f = {{l, h}};
            return f128_to_ui32(f, 0, false);
        }

        uint64_t __fixunstfdi(uint64_t l, uint64_t h) {
            context.use_gas(GAS_CALL_BASE);
            float128_t f = {{l, h}};
            return f128_to_ui64(f, 0, false);
        }

        void __fixunstfti(unsigned __int128 &ret, uint64_t l, uint64_t h) {
            context.use_gas(GAS_CALL_BASE);
            float128_t f = {{l, h}};
            ret = ___fixunstfti(f);
        }

        void __fixsfti(__int128 &ret, float a) {
            context.use_gas(GAS_CALL_BASE);
            ret = ___fixsfti(to_softfloat32(a).v);
        }

        void __fixdfti(__int128 &ret, double a) {
            context.use_gas(GAS_CALL_BASE);
            ret = ___fixdfti(to_softfloat64(a).v);
        }

        void __fixunssfti(unsigned __int128 &ret, float a) {
            context.use_gas(GAS_CALL_BASE);
            ret = ___fixunssfti(to_softfloat32(a).v);
        }

        void __fixunsdfti(unsigned __int128 &ret, double a) {
            context.use_gas(GAS_CALL_BASE);
            ret = ___fixunsdfti(to_softfloat64(a).v);
        }

        double __floatsidf(int32_t i) {
            context.use_gas(GAS_CALL_BASE);
            return from_softfloat64(i32_to_f64(i));
        }

        void __floatsitf(float128_t &ret, int32_t i) {
            context.use_gas(GAS_CALL_BASE);
            ret = i32_to_f128(i);
        }

        void __floatditf(float128_t &ret, uint64_t a) {
            context.use_gas(GAS_CALL_BASE);
            ret = i64_to_f128(a);
        }

        void __floatunsitf(float128_t &ret, uint32_t i) {
            context.use_gas(GAS_CALL_BASE);
            ret = ui32_to_f128(i);
        }

        void __floatunditf(float128_t &ret, uint64_t a) {
            context.use_gas(GAS_CALL_BASE);
            ret = ui64_to_f128(a);
        }

        double __floattidf(uint64_t l, uint64_t h) {
            context.use_gas(GAS_CALL_BASE);
            unsigned __int128 i = (static_cast<unsigned __int128>(h) >> 64) | l;
            return ___floattidf(*(__int128 *) &i);
        }

        double __floatuntidf(uint64_t l, uint64_t h) {
            context.use_gas(GAS_CALL_BASE);
            unsigned __int128 i = (static_cast<unsigned __int128>(h) >> 64) | l;
            return ___floatuntidf((unsigned __int128) i);
        }

        int ___cmptf2(uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb, int return_value_if_nan) {
            context.use_gas(GAS_CALL_BASE);
            float128_t a = {{la, ha}};
            float128_t b = {{lb, hb}};
            if (__unordtf2(la, ha, lb, hb))
                return return_value_if_nan;
            if (f128_lt(a, b))
                return -1;
            if (f128_eq(a, b))
                return 0;
            return 1;
        }

        int __eqtf2(uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            return ___cmptf2(la, ha, lb, hb, 1);
        }

        int __netf2(uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            return ___cmptf2(la, ha, lb, hb, 1);
        }

        int __getf2(uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            return ___cmptf2(la, ha, lb, hb, -1);
        }

        int __gttf2(uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            return ___cmptf2(la, ha, lb, hb, 0);
        }

        int __letf2(uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            return ___cmptf2(la, ha, lb, hb, 1);
        }

        int __lttf2(uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            return ___cmptf2(la, ha, lb, hb, 0);
        }

        int __cmptf2(uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            return ___cmptf2(la, ha, lb, hb, 1);
        }

        int __unordtf2(uint64_t la, uint64_t ha, uint64_t lb, uint64_t hb) {
            context.use_gas(GAS_CALL_BASE);
            float128_t a = {{la, ha}};
            float128_t b = {{lb, hb}};
            if (softfloat_api::is_nan(a) || softfloat_api::is_nan(b))
                return 1;
            return 0;
        }

        static constexpr uint32_t SHIFT_WIDTH = (sizeof(uint64_t) * 8) - 1;

    protected:
        wasm_context &context;
    };

    REGISTER_INTRINSICS(database_api,
                        (db_store, void(int64_t, int, int, int, int))
                                (db_load, int(int64_t, int, int, int, int))
                                (db_has_key, int(int64_t, int, int))
                                (db_remove_key, void(int64_t, int, int))
                                (db_has_table, int(int64_t))
                                (db_remove_table, void(int64_t))
    );

    REGISTER_INTRINSICS(crypto_api,
                        (assert_recover_key, void(int, int, int, int, int))
                                (recover_key, int(int, int, int, int, int))
                                (assert_sha256, void(int, int, int))
                                (sha256, void(int, int, int))
    );

    REGISTER_INTRINSICS(system_api,
                        (current_time, int64_t())
                                (current_height, int64_t())
                                (current_hash, void(int, int))
                                (log_0, void(int, int, int))
                                (log_1, void(int, int, int, int))
                                (log_2, void(int, int, int, int, int))
    );

    REGISTER_INTRINSICS(context_free_system_api,
                        (abort, void())
                                (ftl_assert, void(int, int))
                                (ftl_assert_message, void(int, int, int))
                                (ftl_assert_code, void(int, int64_t))
                                (ftl_exit, void(int))
    );

    REGISTER_INJECTED_INTRINSICS(action_api,
                                 (use_gas, void(int64_t))
    );

    REGISTER_INTRINSICS(action_api,
                        (read_action_data, int(int, int))
                                (action_data_size, int())
                                (get_from, void(int, int))
                                (get_to, void(int, int))
                                (get_owner, void(int, int))
                                (get_user, void(int, int))
                                (transfer, void(int, int, int64_t))
                                (get_amount, int64_t())
                                (call_action, int(int, int, int, int, int64_t, int, int))
                                (call_result, int(int, int))
                                (set_result, int(int, int))
    );

    REGISTER_INTRINSICS(console_api,
                        (prints, void(int))
                                (prints_l, void(int, int))
                                (printi, void(int64_t))
                                (printui, void(int64_t))
                                (printsf, void(float))
                                (printdf, void(double))
                                (printqf, void(int))
                                (printn, void(int64_t))
                                (printhex, void(int, int))
    );

    REGISTER_INTRINSICS(memory_api,
                        (memcpy, int(int, int, int))
                                (memmove, int(int, int, int))
                                (memcmp, int(int, int, int))
                                (memset, int(int, int, int))
    );

    REGISTER_INTRINSICS(compiler_builtins,
                        (__ashlti3, void(int, int64_t, int64_t, int))
                        (__ashrti3, void(int, int64_t, int64_t, int))
                        (__lshlti3, void(int, int64_t, int64_t, int))
                        (__lshrti3, void(int, int64_t, int64_t, int))
                        (__divti3, void(int, int64_t, int64_t, int64_t, int64_t))
                        (__udivti3, void(int, int64_t, int64_t, int64_t, int64_t))
                        (__modti3, void(int, int64_t, int64_t, int64_t, int64_t))
                        (__umodti3, void(int, int64_t, int64_t, int64_t, int64_t))
                        (__multi3, void(int, int64_t, int64_t, int64_t, int64_t))
                        (__addtf3, void(int, int64_t, int64_t, int64_t, int64_t))
                        (__subtf3, void(int, int64_t, int64_t, int64_t, int64_t))
                        (__multf3, void(int, int64_t, int64_t, int64_t, int64_t))
                        (__divtf3, void(int, int64_t, int64_t, int64_t, int64_t))
                        (__eqtf2, int(int64_t, int64_t, int64_t, int64_t))
                        (__netf2, int(int64_t, int64_t, int64_t, int64_t))
                        (__getf2, int(int64_t, int64_t, int64_t, int64_t))
                        (__gttf2, int(int64_t, int64_t, int64_t, int64_t))
                        (__lttf2, int(int64_t, int64_t, int64_t, int64_t))
                        (__letf2, int(int64_t, int64_t, int64_t, int64_t))
                        (__cmptf2, int(int64_t, int64_t, int64_t, int64_t))
                        (__unordtf2, int(int64_t, int64_t, int64_t, int64_t))
                        (__negtf2, void(int, int64_t, int64_t))
                        (__floatsitf, void(int, int))
                        (__floatunsitf, void(int, int))
                        (__floatditf, void(int, int64_t))
                        (__floatunditf, void(int, int64_t))
                        (__floattidf, double(int64_t, int64_t))
                        (__floatuntidf, double(int64_t, int64_t))
                        (__floatsidf, double(int))
                        (__extendsftf2, void(int, float))
                        (__extenddftf2, void(int, double))
                        (__fixtfti, void(int, int64_t, int64_t))
                        (__fixtfdi, int64_t(int64_t, int64_t))
                        (__fixtfsi, int(int64_t, int64_t))
                        (__fixunstfti, void(int, int64_t, int64_t))
                        (__fixunstfdi, int64_t(int64_t, int64_t))
                        (__fixunstfsi, int(int64_t, int64_t))
                        (__fixsfti, void(int, float))
                        (__fixdfti, void(int, double))
                        (__fixunssfti, void(int, float))
                        (__fixunsdfti, void(int, double))
                        (__trunctfdf2, double(int64_t, int64_t))
                        (__trunctfsf2, float(int64_t, int64_t))
    );

    REGISTER_INJECTED_INTRINSICS(softfloat_api,
                                        (_eosio_f32_add, float(float, float))
                                        (_eosio_f32_sub, float(float, float))
                                        (_eosio_f32_mul, float(float, float))
                                        (_eosio_f32_div, float(float, float))
                                        (_eosio_f32_min, float(float, float))
                                        (_eosio_f32_max, float(float, float))
                                        (_eosio_f32_copysign, float(float, float))
                                        (_eosio_f32_abs, float(float))
                                        (_eosio_f32_neg, float(float))
                                        (_eosio_f32_sqrt, float(float))
                                        (_eosio_f32_ceil, float(float))
                                        (_eosio_f32_floor, float(float))
                                        (_eosio_f32_trunc, float(float))
                                        (_eosio_f32_nearest, float(float))
                                        (_eosio_f32_eq, int(float, float))
                                        (_eosio_f32_ne, int(float, float))
                                        (_eosio_f32_lt, int(float, float))
                                        (_eosio_f32_le, int(float, float))
                                        (_eosio_f32_gt, int(float, float))
                                        (_eosio_f32_ge, int(float, float))
                                        (_eosio_f64_add, double(double, double))
                                        (_eosio_f64_sub, double(double, double))
                                        (_eosio_f64_mul, double(double, double))
                                        (_eosio_f64_div, double(double, double))
                                        (_eosio_f64_min, double(double, double))
                                        (_eosio_f64_max, double(double, double))
                                        (_eosio_f64_copysign, double(double, double))
                                        (_eosio_f64_abs, double(double))
                                        (_eosio_f64_neg, double(double))
                                        (_eosio_f64_sqrt, double(double))
                                        (_eosio_f64_ceil, double(double))
                                        (_eosio_f64_floor, double(double))
                                        (_eosio_f64_trunc, double(double))
                                        (_eosio_f64_nearest, double(double))
                                        (_eosio_f64_eq, int(double, double))
                                        (_eosio_f64_ne, int(double, double))
                                        (_eosio_f64_lt, int(double, double))
                                        (_eosio_f64_le, int(double, double))
                                        (_eosio_f64_gt, int(double, double))
                                        (_eosio_f64_ge, int(double, double))
                                        (_eosio_f32_promote, double(float))
                                        (_eosio_f64_demote, float(double))
                                        (_eosio_f32_trunc_i32s, int(float))
                                        (_eosio_f64_trunc_i32s, int(double))
                                        (_eosio_f32_trunc_i32u, int(float))
                                        (_eosio_f64_trunc_i32u, int(double))
                                        (_eosio_f32_trunc_i64s, int64_t(float))
                                        (_eosio_f64_trunc_i64s, int64_t(double))
                                        (_eosio_f32_trunc_i64u, int64_t(float))
                                        (_eosio_f64_trunc_i64u, int64_t(double))
                                        (_eosio_i32_to_f32, float(int32_t))
                                        (_eosio_i64_to_f32, float(int64_t))
                                        (_eosio_ui32_to_f32, float(int32_t))
                                        (_eosio_ui64_to_f32, float(int64_t))
                                        (_eosio_i32_to_f64, double(int32_t))
                                        (_eosio_i64_to_f64, double(int64_t))
                                        (_eosio_ui32_to_f64, double(int32_t))
                                        (_eosio_ui64_to_f64, double(int64_t))
    );


}

