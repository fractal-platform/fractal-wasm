#pragma once

#include <cstring>
#include <vector>

namespace ftl {
    struct sha256 {
        uint8_t _hash[32];

        char *data() const {
            return (char *) &_hash[0];
        }
    };

    inline bool operator>=(const sha256 &h1, const sha256 &h2) {
        return memcmp(h1._hash, h2._hash, sizeof(h1._hash)) >= 0;
    }

    inline bool operator>(const sha256 &h1, const sha256 &h2) {
        return memcmp(h1._hash, h2._hash, sizeof(h1._hash)) > 0;
    }

    inline bool operator<(const sha256 &h1, const sha256 &h2) {
        return memcmp(h1._hash, h2._hash, sizeof(h1._hash)) < 0;
    }

    inline bool operator==(const sha256 &h1, const sha256 &h2) {
        for (int i = 0; i < 32; i++) {
            if (h1._hash[i] != h2._hash[i]) {
                return false;
            }
        }
        return true;
    }

    inline bool operator!=(const sha256 &h1, const sha256 &h2) {
        return !(h1 == h2);
    }

    typedef std::vector <uint8_t> bytes;

    typedef int c_sha256(char *input, int length, char *hash);
    extern c_sha256 *g_sha256;
    sha256 hash(const bytes &input);
}
