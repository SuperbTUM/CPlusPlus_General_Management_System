#pragma once
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
#include <type_traits>
#include <utility>
#include <memory>

using namespace std;

inline string custom_to_string(variant<string, int, double> const& value) {
    if(int const* pval = std::get_if<int>(&value))
    return std::to_string(*pval);
    
    if(double const* pval = std::get_if<double>(&value))
    return std::to_string(*pval);
    
    return std::get<string>(value);
}

string concat(pair<string, variant<string, int, double>> s) {return s.first + "=" + custom_to_string(s.second);}

template <typename...Args>
string concat(pair<string, string> s, Args const& ...params) {
    return s.first + "=" + s.second + " AND " + concat(params...);
}

template <typename...Args>
string concat(string const& base, Args const& ...params) {
    if(base.find("where") != std::string::npos || base.find("WHERE") != std::string::npos)
        return base + " AND " + concat(params...);
    else return base + " WHERE " + concat(params...);
}

string concat(string const& base) {return base;}

string concat() {return {};}

string concat(string const& base, pair<string, variant<string, int, double>> s) {
    return base + " AND " + concat(s);
}

template<typename T>
concept hashable = requires(T a)
{
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

class database;

template<typename T> static int c_callback(void *params, int argc, char **argv, char **azColName) requires std::is_base_of<database, T>::value
{
   T* database = reinterpret_cast<T*>(params);
   return database->callback(argc, argv, azColName);
}

class database_factory {
    public:
        virtual shared_ptr<database> CreateDatabase() = 0;
        virtual ~database_factory() {}
};

class database{
    protected:
        sqlite3 *db;
        sqlite3_stmt *stmt;
        sqlite3_stmt *stmt_insert;
        char *zErrMsg;
    public:
        virtual void create(bool, const char*) = 0;
        virtual int count() = 0;
        virtual void clean() = 0;
        virtual void close(){
            sqlite3_finalize(stmt); // Is this necessary?
            sqlite3_finalize(stmt_insert); // Is this necessary?
            sqlite3_close(db);
        }
        virtual void reorganize() = 0;
        template<typename T>
        void rollback() {
            const char* statement = "ROLLBACK;";
            int rc = sqlite3_exec(db, statement, c_callback<T>, 0, &zErrMsg);
            if(rc != SQLITE_OK){
                fprintf(stderr, "SQL transaction rollback: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            } else {
                fprintf(stdout, "Transaction rollback successfully\n");
            }
        }
        virtual int callback(int argc, char **argv, char **azColName) {
            int i;
            for(i = 0; i<argc; i++) {
                printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
            }
            printf("\n");
            return 0;
        }
        
        template<hashable T = string>
        vector<T> sqlexec(const string& sql) {
            sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
            sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", 0, 0, 0);
            int num_cols;
            vector<T> output;
            output.reserve(20);
            while(sqlite3_step(stmt) != SQLITE_DONE){
                vector<T> row;
                num_cols = sqlite3_column_count(stmt);
                for(int i = 0; i < num_cols; i++){
                    switch(sqlite3_column_type(stmt, i)){
                        case(SQLITE3_TEXT):
                        if constexpr(std::is_same_v<T, std::string>) row.push_back(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i))));
                        break;
                        case(SQLITE_INTEGER):
                        if constexpr(std::is_same_v<T, int>) row.push_back(sqlite3_column_int(stmt, i));
                        break;
                        case(SQLITE_FLOAT):
                        if constexpr(std::is_same_v<T, double>) row.push_back(sqlite3_column_double(stmt, i));
                        break;
                        default:
                        break;
                    }
                }
                output.insert(output.end(), row.begin(), row.end());
            }
            output.shrink_to_fit();
            sqlite3_finalize(stmt);
            return output;
        }

        // could be used to customize postprocessing
        vector<string> sqlGetTable(const string& sql) {
            char** pResult;
            int nRow;
            int nCol;
            int nResult = sqlite3_get_table(db, sql.c_str(), &pResult, &nRow, &nCol, &zErrMsg);

            vector<string> output;
            output.reserve(20);
            if (nResult != SQLITE_OK)
            {
                sqlite3_free(zErrMsg);
                return output;
            }

            int nIndex = nCol;
            for(int i=0;i<nRow;i++)
            {
                for(int j=0;j<nCol;j++)
                {
                    output.push_back(pResult[j]); // attribute name
                    output.push_back(pResult[nIndex]);
                    ++nIndex;
                }
            }
            sqlite3_free_table(pResult);
            output.shrink_to_fit();
            return output;
        }

        bool check_threadsafe() {
            return (bool)sqlite3_threadsafe();
        }
};