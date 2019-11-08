#include "name.hpp"
#include "exceptions.hpp"
#include <boost/algorithm/string.hpp>

namespace ftl {

    void name::set(const char *str) {
        const auto len = strnlen(str, 14);
        FTL_ASSERT(len <= 13, invalid_name_exception, "Name is longer than 13 characters (${0}) ", string(str));
        value = string_to_name(str);
        FTL_ASSERT(to_string() == string(str), invalid_name_exception,
                   "Name not properly normalized (name: ${0}, normalized: ${1}) ",
                   string(str), to_string());
    }

    // keep in sync with name::to_string() in contract definition for name
    name::operator string() const {
        static const char *charmap = ".12345abcdefghijklmnopqrstuvwxyz";

        string str(13, '.');

        uint64_t tmp = value;
        for (uint32_t i = 0; i <= 12; ++i) {
            char c = charmap[tmp & (i == 0 ? 0x0f : 0x1f)];
            str[12 - i] = c;
            tmp >>= (i == 0 ? 4 : 5);
        }

        boost::algorithm::trim_right_if(str, [](char c) { return c == '.'; });
        return str;
    }

}

