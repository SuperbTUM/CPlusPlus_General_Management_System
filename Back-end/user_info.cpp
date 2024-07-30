#include "user_info.hpp"

db_user::db_user(){
   db = 0;
   stmt = 0;
   zErrMsg = 0;
   rc = 0;
   // sql = NULL;
}

db_user::db_user(const db_user& database){

}

db_user::~db_user(){
   sqlite3_finalize(stmt); // Is this necessary?
   sqlite3_finalize(stmt_insert);
   sqlite3_close(db);
}

void db_user::create(bool clear/*= false*/, const char* database_name/*= "userinfo.db"*/){
   // rc = sqlite3_open(database_name, &db);
   // CREATE/OPEN
   rc = sqlite3_open_v2(database_name, std::out_ptr(up), SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, 
                        NULL);
   
   if(rc != SQLITE_OK) {
      fprintf(stderr, "No such database, creating a new one: %s\n", sqlite3_errmsg(db));
      rc = sqlite3_open_v2(database_name, std::out_ptr(up), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, 
                     NULL); // open in multi-threading mode

   } else {
      db = up.get();
      if(clear) clean();
      fprintf(stdout, "Opened database successfully\n");
   }
   /* Create SQL statement */
   // experimental
   rc = sqlite3_exec(db, "pragma journal_mode = WAL", NULL, 0, &zErrMsg);
   if(rc != SQLITE_OK){
      fprintf(stderr, "Journal mode: %s\n", zErrMsg);
   } else {
      fprintf(stderr, "Journal mode setting successfully\n");
   }

   rc = sqlite3_exec(db, "pragma synchronous = NORMAL", NULL, 0, &zErrMsg);
   if(rc != SQLITE_OK){
      fprintf(stderr, "synchronous mode: %s\n", zErrMsg);
   } else {
      fprintf(stderr, "synchronous mode setting successfully\n");
   }

   sql = "CREATE TABLE IF NOT EXISTS USER( \
            USERNAME TEXT NOT NULL PRIMARY KEY,  \
            PASSWORD TEXT NOT NULL, \
            IDENTITY VARCHAR(15) CHECK(IDENTITY IN ('admin', 'rule maker', 'teacher')), \
            STATUS VARCHAR(10) DEFAULT valid, \
            ACTIVITY BOOLEAN DEFAULT 0 CHECK(ACTIVITY IN (0, 1))\
            ) WITHOUT ROWID;" ;

   /* Execute SQL statement */
   rc = sqlite3_exec(db, sql.c_str(), c_callback<db_user>, 0, &zErrMsg);

   if(rc != SQLITE_OK){
      fprintf(stderr, "SQL dropped: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   } else {
      fprintf(stdout, "Table created successfully\n");
   }
   // const char* encrypted_password = "password";
   // int n;
   // try{
   //    n = sqlite3_key(db, encrypted_password, strlen(encrypted_password));
   //    throw(n);
   // } catch(int response){
   //    if(response < 0) n = sqlite3_rekey(db, encrypted_password, strlen(encrypted_password));
   // }
   string sql_insert = "INSERT INTO USER VALUES (@UN, @PW, @IT, @ST, @AT)";
   sqlite3_prepare_v2(db, sql_insert.c_str(), -1, &stmt_insert, NULL);
   
}

int db_user::insert(const std::shared_ptr<UserInfo<string>>& user){
   // string identity = user->identity;
   // string username = user->username;
   // string password = user->password;
   // string status = user->status;
   // int activity = user->activity;

   auto [username, password, identity, status, activity] = user->getElements();
   password = encrypt_password(password);
   if(status.empty()) status = "valid";
   // sql = fmt::format("INSERT INTO USER (USERNAME, PASSWORD, IDENTITY, STATUS, ACTIVITY) "  \
   //          "VALUES ('{}', '{}', '{}', '{}', '{}'); COMMIT;", username, password, identity, status, activity);
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", NULL, NULL, &zErrMsg);

   sqlite3_bind_text(stmt_insert, 1, username.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_insert, 2, password.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_insert, 3, identity.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_insert, 4, status.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_int(stmt_insert, 5, activity);

   // rc = sqlite3_exec(db, sql.c_str(), c_callback<db_user>, 0, &zErrMsg);
   rc = sqlite3_step(stmt_insert);
   if (rc != SQLITE_DONE) {
         fprintf(stderr, "SQL error: %s\n", zErrMsg);
         sqlite3_free(zErrMsg);
         return -1;
   } else {
         fprintf(stdout, "Inserted into table successfully\n");
   }
   sqlite3_exec(db, "END TRANSACTION", NULL, NULL, &zErrMsg);
   // sqlite3_clear_bindings(stmt_insert); // This is not necessary
   sqlite3_reset(stmt_insert);
   return rc; // 101
}

int db_user::update(const string& primary_val, vector<pair<string, variant<string, int, double>>> changelist){
   std::set<string> keys;
   string key;
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", NULL, NULL, &zErrMsg);
   sql.clear();
   while(!changelist.empty()){
      auto changed = changelist.back();
      key = changed.first;
      transform(key.begin(), key.end(), key.begin(), ::toupper);
      auto value = changed.second;
      changelist.pop_back();
      if(keys.count(key)){
         continue;
      }
      keys.insert(key);
      
      if(key == "USERNAME") return -1;
      if(key == "PASSWORD") {
         value = encrypt_password(std::get<string>(value));
      }
      sql += fmt::format("UPDATE USER set {} = '{}' where USERNAME = '{}';", key, custom_to_string(value), primary_val);
   }
   rc = sqlite3_exec(db, sql.c_str(), c_callback<db_user>, 0, &zErrMsg);
   if (rc != SQLITE_OK) {
         fprintf(stderr, "SQL error: %s\n", zErrMsg);
         sqlite3_free(zErrMsg);
         return -1;
   } else {
         fprintf(stdout, "Table updated successfully\n");
   }
   sqlite3_exec(db, "END TRANSACTION", NULL, NULL, &zErrMsg);
   return rc;
}

string db_user::checkType(string target_attribute){
   sql = fmt::format("SELECT typeof({}) FROM USER LIMIT 1;", target_attribute);
   sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", 0, 0, &zErrMsg);
   int num_cols;
   int bytes;
   char* row_content_raw;
   string res;
   while(sqlite3_step(stmt) != SQLITE_DONE){
         num_cols = sqlite3_column_count(stmt);
         for(int i = 0; i < num_cols; i++){
            row_content_raw = const_cast<char*>(static_cast<const char *>(sqlite3_column_blob(stmt, i)));
            bytes = sqlite3_column_bytes(stmt, i);
            res = string(row_content_raw, bytes);
         }
   }
   transform(res.begin(), res.end(), res.begin(), ::toupper);
   sqlite3_finalize(stmt);
   return res;
}

template<typename...Args>
string db_user::getUserAttribute(string& primary_val, string& target_attribute, Args const& ...constraints)
{
   // if(constraint){
   //       auto constraint_val = constraint.value();
   //       string key = constraint_val.first;
   //       auto value = constraint_val.second;
   //       sql = fmt::format("SELECT {} FROM USER "  \
   //                   "WHERE USERNAME = '{}' AND {} = '{}' LIMIT 1; ", target_attribute, primary_val, key, custom_to_string(value));
   // }
   // else sql = fmt::format("SELECT {} FROM USER " \
   //                   "WHERE USERNAME = '{}' LIMIT 1; ", target_attribute, primary_val);
   const string base = fmt::format("SELECT {} FROM USER WHERE USERNAME = '{}' " , target_attribute, primary_val);
   sql = concat(base, constraints...) + " LIMIT 1;";

   // rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
   sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", 0, 0, &zErrMsg);
   int num_cols;
   int bytes = 0;
   char* row_content_raw;
   string res;
   
   while(sqlite3_step(stmt) != SQLITE_DONE){
         num_cols = sqlite3_column_count(stmt);
         for(int i = 0; i < num_cols; i++){
            row_content_raw = const_cast<char*>(static_cast<const char *>(sqlite3_column_blob(stmt, i)));
            bytes = sqlite3_column_bytes(stmt, i);
            res = string(row_content_raw, bytes);
         }
   }
   sqlite3_finalize(stmt);
   if(bytes) return res;
   else return {};
}


// vector<string> db_user::getUserAttributes(string target_attribute, string constraint_key, string constraint_val)
// {
//    sql = fmt::format("SELECT {} FROM USER " \
//                      "WHERE {} = '{}'; ", target_attribute, constraint_key, constraint_val);
//    // rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
//    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
//    sqlite3_exec(db, "BEGIN TRANSACTION", 0, 0, 0);

//    int num_cols;
//    int bytes;
//    vector<string> output;
//    char* row_content_raw;
//    string res;
//    while(sqlite3_step(stmt) != SQLITE_DONE){
//          vector<string> row;
//          num_cols = sqlite3_column_count(stmt);
//          for(int i = 0; i < num_cols; i++){
//             row_content_raw = const_cast<char*>(static_cast<const char *>(sqlite3_column_blob(stmt, i)));
//             bytes = sqlite3_column_bytes(stmt, i);

//             res = string(row_content_raw, bytes);
            
//             row.push_back(res);
//          }
//          output.insert(output.end(), row.begin(), row.end());
//    }
   
//    if(output.empty()) {
//          cout<<"User attribute not found"<<endl;
//          return {};
//    }
//    return output;
// }


int db_user::count(){
   sql = "SELECT COUNT (*) from USER LIMIT 1;"; 
   sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", 0, 0, &zErrMsg);
   int num_cols;
   vector<int> output;
   while(sqlite3_step(stmt) != SQLITE_DONE){
      vector<int> row;
      num_cols = sqlite3_column_count(stmt);
      for(int i = 0; i < num_cols; i++){
         assert(sqlite3_column_type(stmt, i) == SQLITE_INTEGER);
         row.push_back(sqlite3_column_int(stmt, i));
      }
      output.insert(output.end(), row.begin(), row.end());
   }
   sqlite3_finalize(stmt);
   if(output.empty()) return -1;
   return output[0];
}

int db_user::countDistinct(const string& target_attribute, pair<string, variant<string, int, double>> count_info) {
   string countKey = count_info.first;
   auto countValue =  count_info.second;
   sql = fmt::format("SELECT COUNT(DISTINCT {}) from USER WHERE {}='{}' LIMIT 1;", target_attribute, countKey, custom_to_string(countValue));
   sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", 0, 0, &zErrMsg);
   int num_cols;
   vector<int> output;
   while(sqlite3_step(stmt) != SQLITE_DONE){
      vector<int> row;
      num_cols = sqlite3_column_count(stmt);
      for(int i = 0; i < num_cols; i++){
         assert(sqlite3_column_type(stmt, i) == SQLITE_INTEGER);
         row.push_back(sqlite3_column_int(stmt, i));
      }
      output.insert(output.end(), row.begin(), row.end());
   }
   sqlite3_finalize(stmt);
   if(output.empty()) return -1;
   return output[0];
}

int db_user::delet(const string& primary_val, pair<string, variant<string, int, double>> authenticated_info){
   string key = authenticated_info.first;
   auto value = authenticated_info.second;
   sql = fmt::format("DELETE from USER where USERNAME = '{}' AND {} = '{}'; COMMIT;", primary_val, key, custom_to_string(value));
   rc = sqlite3_exec(db, sql.c_str(), c_callback<db_user>, 0, &zErrMsg);
   if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return -1;
   } else {
      fprintf(stdout, "Element(s) deleted successfully\n");
   }
   return rc;
}

void db_user::clean(){
   sql = "DROP TABLE IF EXISTS USER;";
   rc = sqlite3_exec(db, sql.c_str(), c_callback<db_user>, 0, &zErrMsg);
   if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   } else {
      fprintf(stdout, "Table dropped successfully\n");
   }
}

// int main(int argc, char* argv[]) {
//    std::shared_ptr<db_user> user = std::make_shared<db_user>();
//    bool clear = true;
//    user->create(clear);

//    std::shared_ptr<UserInfo<string>> user_example = std::make_shared<UserInfo<string>>("admin", "123456", "admin", "valid");
//    // UserInfo<string> user_example = {username: string("admin"), 
//    //                          password: string("123456"),
//    //                          identity: "admin",
//    //                          status: "valid"
//    //                          };
//    user->insert(user_example);

//    string primekey_val = "admin";

//    vector<pair<string, variant<string, int, double>>> changelist; // (key, value pair)
//    changelist.emplace_back("STATUS", "expired");
//    changelist.emplace_back("STATUS", "valid");
//    int status = user->update(primekey_val, changelist);
//    cout<<"update status "<<status<<endl;

//    optional<pair<string, string>> constraint2;
//    vector<string> usernames = user->getUserAttributes(constraint2, "USERNAME");
//    for(int i=0; i<std::ssize(usernames); i++) cout<<usernames[i]<<"\t";
//    cout<<endl;
//    int user_num = user->count();
//    cout<<user_num<<endl;

//    string db_key = "IDENTITY";
//    auto identity_val = "admin";
//    optional<pair<string, variant<string, int, double>>> constraint;
//    constraint = std::make_pair(db_key, identity_val);
//    string target_attributes = "ACTIVITY";
//    string activity = user->getUserAttribute(constraint, primekey_val, target_attributes);

//    int a = stoi(activity);
//    cout<<"find status "<<a<<endl;

//    // target_attributes = "PASSWORD";
//    vector<pair<string, string>> constraint3;
//    constraint3.emplace_back(std::make_pair("ACTIVITY", "0"));
//    constraint3.emplace_back(std::make_pair("IDENTITY", "admin"));
//    vector<int> activity_int = user->getUserAttributes<int>(constraint3, target_attributes);
//    cout<<*activity_int.begin()<<endl;

//    string key = "password";
//    auto val = static_cast<string>("123456");
//    pair<string, variant<string, int, double>> deleted_info = std::make_pair("password", "123456");

//    // status = user->delet(primekey_val, deleted_info);
//    // cout<<"delete status "<<status<<endl;
//    user->close();
//    return 0;
// }