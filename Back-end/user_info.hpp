#ifndef DB_HPP
#define DB_HPP
#include "database.hpp"
#pragma comment(lib, "sqlite3.lib")
using namespace std;

#define FMT_HEADER_ONLY
#include "fmt/format.h"

template<hashable T>
struct UserInfo final {
        private:
            string username;
            string password;
            T identity;
            T status;
            int activity = 0; // no boolean inside sqlite
        public:
            UserInfo<T>(string username_, string password_, T identity_, T status_): username(username_), password(password_), identity(identity_), status(status_), activity(0) {};
            UserInfo<T>(string username_, string password_, T identity_, T status_, int activity_): username(username_), password(password_), identity(identity_), status(status_), activity(activity_) {};
            UserInfo<T> operator=(UserInfo<T> newuser){
                username = newuser.username;
                password = newuser.password;
                identity = newuser.identity;
                status = newuser.status;
                activity = newuser.activity;
                return *this;
            }
            std::tuple<string, string, T, T, int> getElements() const {return std::make_tuple(username, password, identity, status, activity);};
        };

class db_user: public database{
    private:
        // sqlite3 *db;
        // sqlite3_stmt *stmt;
        char *zErrMsg;
        int rc;
        string sql;
        string checkType(string target_attribute);
        
    public:
        db_user();
        db_user(const db_user& database);
        virtual ~db_user(); //drop the table?

        void create(bool = false, const char* = "userinfo.db");
        int insert(std::shared_ptr<UserInfo<string>> user);
        int update(const string& primary_val, vector<pair<string, variant<string, int, double>>> changelist);
        
        string getUserAttribute(optional<pair<string, variant<string, int, double>>> constraint, string primary_val, string target_attribute);
        
        // vector<string> getUserAttributes(string target_attributes, string constraint_key, string constraint_val);
        template<hashable T = string, hashable T_input = string>
        vector<T> getUserAttributes(optional<pair<string, T_input>> constraint, string&& target_attribute){
            if(constraint){
                string constraint_key = constraint->first;
                // auto constraint_val = constraint->second;
                T_input constraint_val = constraint->second;
                string constraint_val_str;
                if constexpr(std::is_same_v<T_input, int> || std::is_same_v<T_input, double> || std::is_same_v<T_input, float>) constraint_val_str = to_string(constraint_val);
                else constraint_val_str = constraint_val;
                sql = fmt::format("SELECT {} FROM USER " \
                     "WHERE {} = '{}'; ", target_attribute, constraint_key, constraint_val_str);
            } else sql = fmt::format("SELECT {} FROM USER ;", target_attribute);
            
            sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
            sqlite3_exec(db, "BEGIN TRANSACTION", 0, 0, 0);
            int num_cols;
            vector<T> output;
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
            return output;
        }

        template<hashable T = string, hashable T_input = string>
        vector<T> getUserAttributes(vector<pair<string, T_input>> constraints, string&& target_attribute){
            if(!constraints.empty()){
                sql = fmt::format("SELECT {} FROM USER WHERE ", target_attribute);
                int cnt = 0;
                for(auto constraint=constraints.begin(); constraint != constraints.end(); constraint++){
                    string constraint_key = constraint->first;
                    T_input constraint_val = constraint->second;
                    
                    string constraint_val_str;
                    if constexpr(std::is_same_v<T_input, int> || std::is_same_v<T_input, double> || std::is_same_v<T_input, float>) constraint_val_str = to_string(constraint_val);
                    else constraint_val_str = constraint_val;

                    sql += cnt? fmt::format("AND {} = '{}' ", constraint_key, constraint_val_str): fmt::format("{} = '{}' ", constraint_key, constraint_val_str);
                    cnt ++;
                }
                
                sql += ";";
            } else sql = fmt::format("SELECT {} FROM USER ;", target_attribute);
            
            sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
            sqlite3_exec(db, "BEGIN TRANSACTION", 0, 0, 0);
            int num_cols;
            vector<T> output;
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
            return output;
        }

        int count();
        int countDistinct(const string& target_attribute, pair<string, variant<string, int, double>> count_info);
        int delet(const string& primary_val, pair<string, variant<string, int, double>> deleted_info);
        void clean();
};
#endif