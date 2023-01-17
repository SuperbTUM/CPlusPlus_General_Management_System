#ifndef QUESTION_BANK_HPP
#define QUESTION_BANK_HPP
#include "database.hpp"
using namespace std;

#define FMT_HEADER_ONLY
#include "fmt/format.h"

template <hashable T>
struct QuestionInfo final {
        private:
            string path;
            string content;
            string chapter;
            T category;
            int rubric;
        public:
            QuestionInfo<T>(string path_, string content_, string chapter_, T category_): path(path_), content(content_), chapter(chapter_), category(category_), rubric(0) {};
            QuestionInfo<T>(string path_, string content_, string chapter_, T category_, int rubric_): path(path_), content(content_), chapter(chapter_), category(category_), rubric(rubric_) {};
            std::tuple<string, string, string, T, int> getElements() const {return std::make_tuple(path, content, chapter, category, rubric);};
        };

class question_bank: public database{
    private:
        // sqlite3 *db;
        // sqlite3_stmt *stmt;
        char *zErrMsg;
        int rc;
        string sql;
        
    public:
        question_bank();
        question_bank(const question_bank& database);
        virtual ~question_bank(); //drop the table?

        void create(bool = false, const char* = "questions.db");
        int insert(std::shared_ptr<QuestionInfo<string>>);
        int update(vector<pair<string, string>>, vector<pair<string, variant<string, int, double>>> changelist);

        string getQuestionAttribute(optional<pair<string, variant<string, int, double>>> constraint, vector<pair<string, string>> primary_pairs, string target_attribute);

        // vector<string> getQuestionAttributes(string target_attributes, string constraint_key, string constraint_val);
        template<hashable T = string, hashable T_input = string>
        vector<T> getQuestionAttributes(optional<pair<string, T_input>> constraint, const string& target_attribute){
            if(constraint){
                string constraint_key = constraint->first;
                // auto constraint_val = constraint->second;
                T_input constraint_val = constraint->second;
                string constraint_val_str;
                if constexpr(std::is_same_v<T_input, int> || std::is_same_v<T_input, double> || std::is_same_v<T_input, float>) constraint_val_str = to_string(constraint_val);
                else constraint_val_str = constraint_val;
                sql = fmt::format("SELECT DISTINCT {} FROM QUESTIONS " \
                     "WHERE {} = '{}' AND {} != 'placeholder'; ", target_attribute, constraint_key, constraint_val_str, target_attribute);
            } else sql = fmt::format("SELECT DISTINCT {} FROM QUESTIONS where {} != 'placeholder';", target_attribute, target_attribute);
            
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
        vector<T> getQuestionAttributes(vector<pair<string, T_input>> constraints, const string& target_attribute){
            if(!constraints.empty()){
                sql = fmt::format("SELECT DISTINCT {} FROM QUESTIONS WHERE ", target_attribute);
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
                
                sql += fmt::format(" AND {} != 'placeholder';", target_attribute);
            } else sql = fmt::format("SELECT DISTINCT {} FROM QUESTIONS where {} != 'placeholder';", target_attribute);
            
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

        string getQuestion(optional<pair<string, variant<string, int, double>>> constraint, string primary_val);
        int count();
        int countDistinct(const string target_attribute, optional<pair<string, variant<string, int, double>>> count_info);
        int countDistinct(const string target_attribute, vector<pair<string, string>> count_info);
        vector<string> getQuestionPaths();
        int delet(vector<pair<string, string>>);
        void clean();
};
#endif