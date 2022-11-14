#ifndef DB_HPP
#define DB_HPP
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h> 
#include <string.h>
#include <vector>
#include <set>
#include <variant>
#include <optional>
#include <iostream>
using namespace std;

#define FMT_HEADER_ONLY
#include "fmt/format.h"

inline string custom_to_string(variant<string, int, double> const& value) {
    if(int const* pval = std::get_if<int>(&value))
    return std::to_string(*pval);
    
    if(double const* pval = std::get_if<double>(&value))
    return std::to_string(*pval);
    
    return std::get<string>(value);
}


struct UserInfo{
            string account;
            string password;
            string identity;
            string status;
            UserInfo operator=(UserInfo newuser){
                account = newuser.account;
                password = newuser.password;
                identity = newuser.identity;
                status = newuser.status;
                return *this;
            }
        };

class db_user{
    private:
        sqlite3 *db;
        sqlite3_stmt *stmt;
        char *zErrMsg;
        int rc;
        string sql;
        
    public:
        db_user();
        db_user(const db_user& database);
        virtual ~db_user(); //drop the table?

        void create();
        int insert(UserInfo user);
        int update(auto primary_val, vector<pair<string, variant<string, int, double>>> changelist);
        bool find(optional<pair<string, variant<string, int, double>>> constraint, auto primary_val);
        void delet(auto primary_val);
        void drop();
};
#endif