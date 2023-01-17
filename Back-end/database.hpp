#ifndef DATABASE_HPP
#define DATABASE_HPP
#include <stdio.h>
#include <stdlib.h>
#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC 1
#endif

#ifndef SQLITE_TEMP_STORE
#define SQLITE_TEMP_STORE 2
#endif

#include <sqlite3.h> 
#include <string.h>
#include <vector>
#include <set>
#include <variant>
#include <optional>
#include <iostream>
#include <cstddef>
#include <concepts>
#include <cctype>
#include <cassert>

using namespace std;

inline string custom_to_string(variant<string, int, double> const& value) {
    if(int const* pval = std::get_if<int>(&value))
    return std::to_string(*pval);
    
    if(double const* pval = std::get_if<double>(&value))
    return std::to_string(*pval);
    
    return std::get<string>(value);
}

template<typename T>
concept hashable = requires(T a)
{
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

class database{
    protected:
        sqlite3 *db;
        sqlite3_stmt *stmt;
    public:
        virtual void create(bool, const char*) = 0;
        virtual int count() = 0;
        virtual void clean() = 0;
        virtual void close(){
            sqlite3_finalize(stmt);
            sqlite3_close(db);
        }
};
#endif