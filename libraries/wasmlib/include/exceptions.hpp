#pragma once

#include "format.hpp"

#define FTL_ASSERT(expr, exc_type, FORMAT, ...)                \
   do   { \
   if( !(expr) )                                                      \
     throw exc_type( util::Format(FORMAT, ##__VA_ARGS__) ); \
   } while(0);

#define FTL_THROW(exc_type, FORMAT, ...) \
    throw exc_type( util::Format(FORMAT, ##__VA_ARGS__) );

namespace ftl {

    class exception {
    public:
        exception(int64_t code,
                  const std::string &name_value = "exception",
                  const std::string &what_value = "unspecified") : _code(code), _name(name_value), _what(what_value) {}

        virtual ~exception() {}

        const char *name() const throw() { return _name.c_str(); }

        int64_t code() const throw() { return _code; }

        virtual const char *what() const throw() { return _what.c_str(); }

    private:
        int64_t _code;
        std::string _name;
        std::string _what;
    };

#define FTL_DECLARE_DERIVED_EXCEPTION(TYPE, BASE, CODE, NAME) \
   class TYPE : public BASE  \
   { \
      public: \
       explicit TYPE( const std::string& what_value ) \
       :BASE( CODE, NAME, what_value ){} \
   };

    FTL_DECLARE_DERIVED_EXCEPTION(invalid_name_exception, exception,
                                 20001, "invalid name exception")

    FTL_DECLARE_DERIVED_EXCEPTION(invalid_address_exception, exception,
                                 20002, "invalid address exception")

    FTL_DECLARE_DERIVED_EXCEPTION(wasm_runtime_exception, exception,
                                 20003, "wasm runtime exception")

    FTL_DECLARE_DERIVED_EXCEPTION(wasm_serialization_exception, exception,
                                 20004, "wasm serialization exception")

    FTL_DECLARE_DERIVED_EXCEPTION(overlapping_memory_error, exception,
                                 20005, "memcpy with overlapping memory")

    FTL_DECLARE_DERIVED_EXCEPTION(crypto_api_exception, exception,
                                 20006, "crypto api exception")

    FTL_DECLARE_DERIVED_EXCEPTION(arithmetic_exception, exception,
                                 20007, "arithmetic exception")
}
