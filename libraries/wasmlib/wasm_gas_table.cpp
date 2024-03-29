
#include <stdint.h>

int32_t gas_table[] = {
   0, //unreachable_code           = 0x00,
   1, //nop_code                   = 0x01,
   0, //block_code                 = 0x02,
   0, //loop_code                  = 0x03,
   0, //if__code                   = 0x04,
   2, //else__code                 = 0x05,
   0, 0, 0, 0, 0,
   0, //end_code                   = 0x0B,
   2, //br_code                    = 0x0C,
   3, //br_if_code                 = 0x0D,
   2, //br_table_code              = 0x0E,
   2, //return__code               = 0x0F,
   2, //call_code                  = 0x10,
   10, //call_indirect_code         = 0x11, TBD
   0, 0, 0, 0, 0, 0, 0, 0,
   3, //drop_code                  = 0x1A,
   3, //select_code                = 0x1B,
   0, 0, 0, 0,
   3, //get_local_code             = 0x20,
   3, //set_local_code             = 0x21,
   3, //tee_local_code             = 0x22,
   3, //get_global_code            = 0x23,
   3, //set_global_code            = 0x24,
   0, 0, 0,
   3, //i32_load_code              = 0x28,
   3, //i64_load_code              = 0x29,
   3, //f32_load_code              = 0x2A,
   3, //f64_load_code              = 0x2B,
   3, //i32_load8_s_code           = 0x2C,
   3, //i32_load8_u_code           = 0x2D,
   3, //i32_load16_s_code          = 0x2E,
   3, //i32_load16_u_code          = 0x2F,
   3, //i64_load8_s_code           = 0x30,
   3, //i64_load8_u_code           = 0x31,
   3, //i64_load16_s_code          = 0x32,
   3, //i64_load16_u_code          = 0x33,
   3, //i64_load32_s_code          = 0x34,
   3, //i64_load32_u_code          = 0x35,
   3, //i32_store_code             = 0x36,
   3, //i64_store_code             = 0x37,
   3, //f32_store_code             = 0x38,
   3, //f64_store_code             = 0x39,
   3, //i32_store8_code            = 0x3A,
   3, //i32_store16_code           = 0x3B,
   3, //i64_store8_code            = 0x3C,
   3, //i64_store16_code           = 0x3D,
   3, //i64_store32_code           = 0x3E,
   100, //current_memory_code        = 0x3F, TBD
   1000, //grow_memory_code           = 0x40, TBD
   0, //i32_const_code             = 0x41,
   0, //i64_const_code             = 0x42,
   0, //f32_const_code             = 0x43,
   0, //f64_const_code             = 0x44,
   1, //i32_eqz_code               = 0x45,
   1, //i32_eq_code                = 0x46,
   1, //i32_ne_code                = 0x47,
   1, //i32_lt_s_code              = 0x48,
   1, //i32_lt_u_code              = 0x49,
   1, //i32_gt_s_code              = 0x4A,
   1, //i32_gt_u_code              = 0x4B,
   1, //i32_le_s_code              = 0x4C,
   1, //i32_le_u_code              = 0x4D,
   1, //i32_ge_s_code              = 0x4E,
   1, //i32_ge_u_code              = 0x4F,
   1, //i64_eqz_code               = 0x50,
   1, //i64_eq_code                = 0x51,
   1, //i64_ne_code                = 0x52,
   1, //i64_lt_s_code              = 0x53,
   1, //i64_lt_u_code              = 0x54,
   1, //i64_gt_s_code              = 0x55,
   1, //i64_gt_u_code              = 0x56,
   1, //i64_le_s_code              = 0x57,
   1, //i64_le_u_code              = 0x58,
   1, //i64_ge_s_code              = 0x59,
   1, //i64_ge_u_code              = 0x5A,
   100, //f32_eq_code                = 0x5B, TBD
   100, //f32_ne_code                = 0x5C, TBD
   100, //f32_lt_code                = 0x5D, TBD
   100, //f32_gt_code                = 0x5E, TBD
   100, //f32_le_code                = 0x5F, TBD
   100, //f32_ge_code                = 0x60, TBD
   100, //f64_eq_code                = 0x61, TBD
   100, //f64_ne_code                = 0x62, TBD
   100, //f64_lt_code                = 0x63, TBD
   100, //f64_gt_code                = 0x64, TBD
   100, //f64_le_code                = 0x65, TBD
   100, //f64_ge_code                = 0x66, TBD
   105, //i32_clz_code               = 0x67,
   105, //i32_ctz_code               = 0x68,
   105, //i32_popcnt_code            = 0x69, TBD
   1, //i32_add_code               = 0x6A,
   1, //i32_sub_code               = 0x6B,
   3, //i32_mul_code               = 0x6C,
   80, //i32_div_s_code             = 0x6D,
   80, //i32_div_u_code             = 0x6E,
   80, //i32_rem_s_code             = 0x6F,
   80, //i32_rem_u_code             = 0x70,
   1, //i32_and_code               = 0x71,
   1, //i32_or_code                = 0x72,
   1, //i32_xor_code               = 0x73,
   2, //i32_shl_code               = 0x74,
   2, //i32_shr_s_code             = 0x75,
   2, //i32_shr_u_code             = 0x76,
   2, //i32_rotl_code              = 0x77,
   2, //i32_rotr_code              = 0x78,
   150, //i64_clz_code               = 0x79, TBD
   150, //i64_ctz_code               = 0x7A, TBD
   150, //i64_popcnt_code            = 0x7B, TBD
   1, //i64_add_code               = 0x7C,
   1, //i64_sub_code               = 0x7D,
   3, //i64_mul_code               = 0x7E,
   80, //i64_div_s_code             = 0x7F,
   80, //i64_div_u_code             = 0x80,
   80, //i64_rem_s_code             = 0x81,
   80, //i64_rem_u_code             = 0x82,
   1, //i64_and_code               = 0x83,
   1, //i64_or_code                = 0x84,
   1, //i64_xor_code               = 0x85,
   2, //i64_shl_code               = 0x86,
   2, //i64_shr_s_code             = 0x87,
   2, //i64_shr_u_code             = 0x88,
   2, //i64_rotl_code              = 0x89,
   2, //i64_rotr_code              = 0x8A,
   100, //f32_abs_code               = 0x8B, TBD
   100, //f32_neg_code               = 0x8C, TBD
   100, //f32_ceil_code              = 0x8D, TBD
   100, //f32_floor_code             = 0x8E, TBD
   100, //f32_trunc_code             = 0x8F, TBD
   100, //f32_nearest_code           = 0x90, TBD
   2000, //f32_sqrt_code              = 0x91, TBD
   200, //f32_add_code               = 0x92, TBD
   200, //f32_sub_code               = 0x93, TBD
   400, //f32_mul_code               = 0x94, TBD
   1000, //f32_div_code               = 0x95, TBD
   100, //f32_min_code               = 0x96, TBD
   100, //f32_max_code               = 0x97, TBD
   100, //f32_copysign_code          = 0x98, TBD
   100, //f64_abs_code               = 0x99, TBD
   100, //f64_neg_code               = 0x9A, TBD
   100, //f64_ceil_code              = 0x9B, TBD
   100, //f64_floor_code             = 0x9C, TBD
   100, //f64_trunc_code             = 0x9D, TBD
   100, //f64_nearest_code           = 0x9E, TBD
   2000, //f64_sqrt_code              = 0x9F, TBD
   200, //f64_add_code               = 0xA0, TBD
   200, //f64_sub_code               = 0xA1, TBD
   400, //f64_mul_code               = 0xA2, TBD
   1000, //f64_div_code               = 0xA3, TBD
   100, //f64_min_code               = 0xA4, TBD
   100, //f64_max_code               = 0xA5, TBD
   100, //f64_copysign_code          = 0xA6, TBD
   3, //i32_wrap_i64_code          = 0xA7,
   100, //i32_trunc_s_f32_code       = 0xA8, TBD
   100, //i32_trunc_u_f32_code       = 0xA9, TBD
   100, //i32_trunc_s_f64_code       = 0xAA, TBD
   100, //i32_trunc_u_f64_code       = 0xAB, TBD
   3, //i64_extend_s_i32_code      = 0xAC, TBD
   3, //i64_extend_u_i32_code      = 0xAD,
   100, //i64_trunc_s_f32_code       = 0xAE, TBD
   100, //i64_trunc_u_f32_code       = 0xAF, TBD
   100, //i64_trunc_s_f64_code       = 0xB0, TBD
   100, //i64_trunc_u_f64_code       = 0xB1, TBD
   100, //f32_convert_s_i32_code     = 0xB2, TBD
   100, //f32_convert_u_i32_code     = 0xB3, TBD
   100, //f32_convert_s_i64_code     = 0xB4, TBD
   100, //f32_convert_u_i64_code     = 0xB5, TBD
   100, //f32_demote_f64_code        = 0xB6, TBD
   100, //f64_convert_s_i32_code     = 0xB7, TBD
   100, //f64_convert_u_i32_code     = 0xB8, TBD
   100, //f64_convert_s_i64_code     = 0xB9, TBD
   100, //f64_convert_u_i64_code     = 0xBA, TBD
   100, //f64_promote_f32_code       = 0xBB, TBD
   100, //i32_reinterpret_f32_code   = 0xBC, TBD
   100, //i64_reinterpret_f64_code   = 0xBD, TBD
   100, //f32_reinterpret_i32_code   = 0xBE, TBD
   100, //f64_reinterpret_i64_code   = 0xBF, TBD
}; // gas_table
