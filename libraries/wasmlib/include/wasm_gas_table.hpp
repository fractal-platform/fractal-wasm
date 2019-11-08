#pragma once

#include <cstdint>

extern int32_t gas_table[];

const int32_t GAS_CALL_BASE = 100;

const int32_t GAS_RECOVER_KEY = 450000;  // about 150us
const int32_t GAS_SHA256_BASE = 2400;  // about 800ns
const int32_t GAS_SHA256_BYTE = 3;  // about 3ns

const int32_t GAS_LOG_0 = 80000;
const int32_t GAS_LOG_1 = 90000;
const int32_t GAS_LOG_2 = 100000;
const int32_t GAS_LOG_DATA = 1000;

const int32_t GAS_MEMOP_BYTE = 3;

const int32_t GAS_TRANSFER = 1000000;

const int32_t GAS_DBSTORE_BASE = 1000000;
const int32_t GAS_DBSTORE_BYTE = 100000;
const int32_t GAS_DBLOAD_BASE = 1000000;
const int32_t GAS_DBLOAD_BYTE = 100000;
const int32_t GAS_DB_HAS = 500000;
const int32_t GAS_DB_RMV = 500000;
