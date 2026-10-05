#pragma once

#include "database.hpp"
#include "coroutine_task.hpp"
using namespace std;

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif
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
            QuestionInfo() {}
            QuestionInfo(string path_, string content_, string chapter_, T category_): path(path_), content(content_), chapter(chapter_), category(category_), rubric(0) {};
            QuestionInfo(string path_, string content_, string chapter_, T category_, int rubric_): path(path_), content(content_), chapter(chapter_), category(category_), rubric(rubric_) {};
            std::tuple<string, string, string, T, int> getElements() const {return std::make_tuple(path, content, chapter, category, rubric);};
            auto operator<=>(const QuestionInfo<T>&) const = default;
            bool operator==(const QuestionInfo<T>&) const = default;
        };

class question_bank: public database{
    private:
        // sqlite3 *db;
        // sqlite3_stmt *stmt;
        // char *zErrMsg;
        int rc;
        string sql;
        
    public:
        question_bank();
        question_bank(const question_bank& database);
        virtual ~question_bank(); //drop the table?

        void create(bool = false, const char* = "questions.db");
        int insert(const std::shared_ptr<QuestionInfo<string>>&);
        int update(vector<pair<string, string>>, vector<pair<string, variant<string, int, double>>> changelist);

        template<hashable T_input = string>
        bool checkExistence(vector<pair<string, T_input>> constraints) {
            sql = "SELECT 1 FROM QUESTIONS ";
            if(!constraints.empty()){
                sql += "WHERE ";
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
            }
            sql += "limit 1;";
            vector<int> exec_res = sqlexec<int>(sql);
            return !exec_res.empty();
        }

        string getQuestionAttribute(optional<pair<string, variant<string, int, double>>> constraint, std::array<pair<string, string>, 3> primary_pairs, const string& target_attribute);

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
            
            return sqlexec<T>(sql);
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
            } else sql = fmt::format("SELECT DISTINCT {} FROM QUESTIONS where {} != 'placeholder';", target_attribute, target_attribute);
            
            return sqlexec<T>(sql);
        }

        string getQuestion(optional<pair<string, variant<string, int, double>>> constraint, string& primary_val);
        async::Generator<string> streamSubjects();
        async::Generator<QuestionInfo<string>> streamQuestions(const string& subject, const string& chapter);
        int count();
        int countDistinct(const string& target_attribute, optional<pair<string, variant<string, int, double>>> count_info);
        int countDistinct(const string& target_attribute, vector<pair<string, string>> count_info);
        vector<string> getQuestionPaths();
        int delet(vector<pair<string, string>>);
        void clean();
        void reorganize() {
            sql = "DELETE FROM QUESTIONS;";
            rc = sqlite3_exec(db, sql.c_str(), c_callback<question_bank>, 0, &zErrMsg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            } else {
                // fprintf(stdout, "Table truncated successfully\n");
                fmt::print("Table truncated successfully\n");
            }
        }
};