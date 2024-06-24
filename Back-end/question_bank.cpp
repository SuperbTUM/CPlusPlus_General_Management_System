#include "question_bank.hpp"

question_bank::question_bank(){
   db = 0;
   stmt = 0;
   zErrMsg = 0;
   rc = 0;
   // sql = NULL;
}

question_bank::question_bank(const question_bank& database){

}

question_bank::~question_bank(){
   sqlite3_finalize(stmt); // Is it necessary?
   sqlite3_finalize(stmt_insert);
   sqlite3_close(db);
}

void question_bank::create(bool clear/*= false*/, const char* database_name/*= "questions.db"*/){
   // rc = sqlite3_open(database_name, &db);
   // CREATE/OPEN
   rc = sqlite3_open_v2(database_name, std::out_ptr(db), SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, 
                        NULL);
   
   if(rc != SQLITE_OK) {
      fprintf(stderr, "No such database, creating a new one: %s\n", sqlite3_errmsg(db));
      rc = sqlite3_open_v2(database_name, std::out_ptr(db), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, 
                     NULL);

   } else {
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

   sql = "CREATE TABLE IF NOT EXISTS QUESTIONS( \
            PATH varchar(20) NOT NULL,  \
            CONTENT TEXT, \
            CHAPTER varchar(20) NOT NULL, \
            SUBJECT varchar(20) NOT NULL, \
            RUBRIC INTEGER CHECK(RUBRIC >= 0),\
            PRIMARY KEY (PATH, CHAPTER, SUBJECT)\
            ) WITHOUT ROWID" ; // without rowid means using clustered index

   /* Execute SQL statement */
   rc = sqlite3_exec(db, sql.c_str(), c_callback<question_bank>, 0, &zErrMsg);

   if(rc != SQLITE_OK){
      fprintf(stderr, "SQL dropped: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   } else {
      fprintf(stdout, "Table created successfully\n");
   }
   string sql_insert = "INSERT INTO QUESTIONS VALUES (@PT, @CT, @CH, @SJ, @RB)";
   sqlite3_prepare_v2(db, sql_insert.c_str(), -1, &stmt_insert, NULL);
}

int question_bank::insert(const std::shared_ptr<QuestionInfo<string>>& question){
   // string path = question->path;
   // string content = question->content;
   // string chapter = question->chapter;
   // string category = question->category;
   // int rubric = question->rubric;
   auto [path, content, chapter, category, rubric] = question->getElements();
   if(category.empty()) category = "undefined";
   // sql = fmt::format("INSERT INTO QUESTIONS (PATH, CONTENT, CHAPTER, SUBJECT, RUBRIC) "  \
   //          "VALUES ('{}', '{}', '{}', '{}', '{}'); COMMIT;", path, content, chapter, category, rubric);
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", NULL, NULL, &zErrMsg);

   sqlite3_bind_text(stmt_insert, 1, path.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_insert, 2, content.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_insert, 3, chapter.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_insert, 4, category.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_int(stmt_insert, 5, rubric);

   // rc = sqlite3_exec(db, sql.c_str(), c_callback<question_bank>, 0, &zErrMsg);
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
   return rc;
}

int question_bank::update(vector<pair<string, string>> primary_pairs, vector<pair<string, variant<string, int, double>>> changelist){
   std::set<string> keys;
   string key;
   sql.clear();
   string sql_tmp = "";
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", NULL, NULL, &zErrMsg);
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

      // std::array<string, 3> primary_keys({"PATH", "CHAPTER", "SUBJECT"});
      auto primary_keys = std::to_array<string>({"PATH", "CHAPTER", "SUBJECT"});
      if(std::find(primary_keys.begin(), primary_keys.end(), key) != primary_keys.end()) return -1;
      
      sql_tmp = fmt::format("UPDATE QUESTIONS set {} = '{}' where ", key, custom_to_string(value));
      for(int i=0; i<primary_pairs.size()-1; i++) sql_tmp += fmt::format("{} = '{}' AND ", primary_pairs[i].first, primary_pairs[i].second);
      sql_tmp += fmt::format("{} = '{}';", primary_pairs[primary_pairs.size()-1].first, primary_pairs[primary_pairs.size()-1].second);
      sql += sql_tmp;
   }
   rc = sqlite3_exec(db, sql.c_str(), c_callback<question_bank>, 0, &zErrMsg);
   if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return -1;
   } else {
      // fprintf(stdout, "Table updated successfully\n");
      fmt::print("Table updated successfully\n");
   }
   sqlite3_exec(db, "END TRANSACTION", NULL, NULL, &zErrMsg);
   return rc;
}

string question_bank::getQuestion(optional<pair<string, variant<string, int, double>>> constraint, string& primary_val){
   if(constraint){
      auto constraint_val = constraint.value();
      string key = constraint_val.first;
      auto value = constraint_val.second;
      sql = fmt::format("SELECT CONTENT FROM QUESTIONS "  \
                  "WHERE PATH = '{}' AND {} = '{}'; ", primary_val, key, custom_to_string(value));
   }
   else sql = fmt::format("SELECT CONTENT FROM QUESTIONS " \
                  "WHERE PATH = '{}'; ", primary_val);

   vector<string> output = sqlexec<string>(sql);
   
   if(!output.empty()) return output[0];
   // cout<<"Question not found!"<<endl;
   fmt::print("Question not found!\n");
   return {};
}

string question_bank::getQuestionAttribute(optional<pair<string, variant<string, int, double>>> constraint, std::array<pair<string, string>, 3> primary_pairs, const string& target_attribute)
{
   sql = fmt::format("SELECT {} FROM QUESTIONS WHERE ", target_attribute);
   for(int i=0; i<primary_pairs.size(); i++) {
      if(i<primary_pairs.size()-1) sql += fmt::format("{} = '{}' AND ", primary_pairs[i].first, primary_pairs[i].second);
      else sql += fmt::format("{} = '{}' ", primary_pairs[i].first, primary_pairs[i].second);
   }
   if(constraint){
         auto constraint_val = constraint.value();
         string key = constraint_val.first;
         auto value = constraint_val.second;
         sql += fmt::format("AND {} = '{}' ", key, custom_to_string(value));
   }
   sql += "LIMIT 1; ";
   // cout<<sql<<endl;
   fmt::print("{}", sql);
   // rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
   sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", 0, 0, 0);
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

int question_bank::count(){
   sql = "SELECT COUNT (*) from QUESTIONS LIMIT 1;"; 
   sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", 0, 0, 0);
   int num_cols;
   int output = -1;
   while(sqlite3_step(stmt) != SQLITE_DONE){
      int row;
      num_cols = sqlite3_column_count(stmt);
      for(int i = 0; i < num_cols; i++){
         assert(sqlite3_column_type(stmt, i) == SQLITE_INTEGER);
         row = sqlite3_column_int(stmt, i);
      }
      output = row;
   }
   sqlite3_finalize(stmt);
   return output;
}

int question_bank::countDistinct(const string& target_attribute, vector<pair<string, string>> count_info) {
   sql = fmt::format("select count(DISTINCT {}) from QUESTIONS WHERE ", target_attribute);
   string constraint_key;
   string constraint_val;
   for(int i=0; i<count_info.size(); i++){
      constraint_key = count_info[i].first;
      constraint_val = count_info[i].second;
      if(i<count_info.size())  sql += fmt::format("{} = '{}' AND ", constraint_key, constraint_val);
   }
   sql += fmt::format("{} != 'placeholder' LIMIT 1;", target_attribute);
   sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", 0, 0, 0);
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

int question_bank::countDistinct(const string& target_attribute, optional<pair<string, variant<string, int, double>>> count_info) {
   if(count_info){
      auto count_info_detail = count_info.value();
      string countKey = count_info_detail.first;
      auto countValue =  count_info_detail.second;
      //SELECT COUNT((SELECT DISTINCT path FROM QUESTIONS)) from QUESTIONS WHERE subject='English' and chapter='chapter1' LIMIT 1;
      sql = fmt::format("select count(DISTINCT {}) from QUESTIONS WHERE {}='{}' AND {} != 'placeholder' LIMIT 1;", target_attribute, countKey, custom_to_string(countValue), target_attribute);
   } else {
      sql = fmt::format("select count(DISTINCT {}) from QUESTIONS where {} != 'placeholder' LIMIT 1;", target_attribute, target_attribute);
   }
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

vector<string> question_bank::getQuestionPaths(){
   sql = "SELECT PATH from QUESTIONS;";
   sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
   sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION", 0, 0, &zErrMsg);
   int num_cols;
   vector<string> output;
   while(sqlite3_step(stmt) != SQLITE_DONE){
      vector<string> row;
      num_cols = sqlite3_column_count(stmt);
      for(int i = 0; i < num_cols; i++){
         assert(sqlite3_column_type(stmt, i) == SQLITE_TEXT);
         row.push_back(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i))));    
      }
      output.insert(output.end(), row.begin(), row.end());
   }
   sqlite3_finalize(stmt);
   return output;
}

int question_bank::delet(vector<pair<string, string>> primary_pairs){
   sql = fmt::format("DELETE from QUESTIONS where ");
   for(int i=0; i<primary_pairs.size(); i++){
      if(i < primary_pairs.size()-1)  sql += fmt::format("{} = '{}' AND ", primary_pairs[i].first, primary_pairs[i].second);
      else sql += fmt::format("{} = '{}' ;", primary_pairs[i].first, primary_pairs[i].second);
   }
   // DML needs commit
   sql += "commit;";
    
   rc = sqlite3_exec(db, sql.c_str(), c_callback<question_bank>, 0, &zErrMsg);
   if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return -1;
   } else {
      fprintf(stdout, "Element(s) deleted successfully\n");
   }
   return rc;
}

void question_bank::clean(){
   sql = "DROP TABLE IF EXISTS QUESTIONS;";
   rc = sqlite3_exec(db, sql.c_str(), c_callback<question_bank>, 0, &zErrMsg);
   if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   } else {
      fprintf(stdout, "Table dropped successfully\n");
   }
}

// int main(int argc, char* argv[]) {
//    question_bank question = question_bank();
//    bool clear = true;
//    question.create(clear);

//    std::shared_ptr<QuestionInfo<string>> question_example = std::make_shared<QuestionInfo<string>>("http://123.com", "123456", "", "multiple choice", 10);
//    question.insert(question_example);
//    string primekey_val = "http://123.com";

//    vector<pair<string, variant<string, int, double>>> changelist; // (key, value pair)
//    changelist.emplace_back("category", "normal");
//    changelist.emplace_back("category", "multiple choice");
//    int status = question.update(primekey_val, changelist);
//    cout<<"update status "<<status<<endl;

//    int question_num = question.count();
//    cout<<question_num<<endl;

//    optional<pair<string, variant<string, int, double>>> constraint;
//    // constraint = std::make_pair(db_key, identity_val);
//    string found = question.getQuestion(constraint, primekey_val);
//    cout<<"find status "<<found<<endl;

//    string key = "CONTENT";
//    auto val = static_cast<string>("123456");
//    pair<string, variant<string, int, double>> deleted_info = std::make_pair(key, "123456");

//    status = question.delet(primekey_val, deleted_info);
//    cout<<"delete status "<<status<<endl;
//    return 0;
// }