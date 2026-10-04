#pragma once
#include "database.hpp"
#pragma comment(lib, "sqlite3.lib")
using namespace std;

#define FMT_HEADER_ONLY
#include "fmt/format.h"

#include <openssl/md5.h>

inline string& encrypt_password(string& password) {
    unsigned char hash[MD5_DIGEST_LENGTH];

    MD5_CTX md5;
    MD5_Init(&md5);
    MD5_Update(&md5, password.c_str(), password.size());
    MD5_Final(hash, &md5);

    char hex[MD5_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    }
    hex[MD5_DIGEST_LENGTH * 2] = '\0';
    password = string(hex);
    return password;
}

inline string decrypt_password(const string& password) {
    return password;
}

template<hashable T>
struct UserInfo final {
        private:
            string username;
            string password;
            T identity;
            T status;
            int activity = 0; // no boolean inside sqlite
        public:
            UserInfo() {};
            UserInfo(string username_, string password_, T identity_, T status_): username(username_), password(password_), identity(identity_), status(status_), activity(0) {};
            UserInfo(string username_, string password_, T identity_, T status_, int activity_): username(username_), password(password_), identity(identity_), status(status_), activity(activity_) {};
            UserInfo(const UserInfo<T>& newuser): username(newuser.username), password(newuser.password), identity(newuser.identity), status(newuser.status), activity(newuser.activity) {};
            UserInfo(UserInfo<T>&& newuser) noexcept: username(std::move(newuser.username)), password(std::move(newuser.password)), identity(std::move(newuser.identity)), status(std::move(newuser.status)), activity(std::exchange(newuser.activity, 0)) {};
            UserInfo& operator=(const UserInfo<T>& newuser) noexcept{
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
        int rc;
        string sql;
        string checkType(string target_attribute);
        
    public:
        db_user();
        db_user(const db_user& database);
        virtual ~db_user(); //drop the table?

        void create(bool = false, const char* = "userinfo.db");
        int insert(const std::shared_ptr<UserInfo<string>>& user);
        int update(const string& primary_val, vector<pair<string, variant<string, int, double>>> changelist);
        
        template<typename...Args>
        string getUserAttribute(string& primary_val, string& target_attribute, Args const&...params);
        
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
            
            return sqlexec<T>(sql);
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
            
            return sqlexec<T>(sql);
        }

        int count();
        int countDistinct(const string& target_attribute, pair<string, variant<string, int, double>> count_info);
        int delet(const string& primary_val, pair<string, variant<string, int, double>> deleted_info);
        void clean();
        void reorganize() {
            sql = "DELETE FROM USER;";
            rc = sqlite3_exec(db, sql.c_str(), c_callback<db_user>, 0, &zErrMsg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            } else {
                fprintf(stdout, "Table truncated successfully\n");
            }
        }
};