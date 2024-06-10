#ifndef OWNINTERFACE
#define OWNINTERFACE

#include <QString>
#include <QDir>
#include <QDialog>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <regex>
#include "tcpclientsocket.h"
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <chrono>
#define FMT_HEADER_ONLY
#include "fmt/format.h"
using json=nlohmann::json;

inline std::string escapeJsonString(std::string input){
    regex re1("\\n");
    regex re2("\\\\");
    input = regex_replace(regex_replace(input, re2, "\\\\"), re1, "\\n");
    return input;
}

class Login{// login panel
protected:
    virtual void submit_login(QString userName,QString password)=0;// submit login request
public:
    virtual void open_login_panel()=0;// open login panel
};

class Admin{// system admin
protected:
    virtual void read_users()=0; // read all users in the system
    virtual void delete_user(QString userName)=0; // delete users in the system
    virtual void register_user(QString userName,QString password)=0; // create a new user in the system
public:
    virtual void open_admin_panel()=0;// do something before opening an admin panel
    virtual void close_admin_panel()=0;// slot for closing admin panel
};

class RuleMaker{ // rulemaker
protected:
    virtual void create_bulletin(QString teacherName, QString timeStamp)=0;// create bulletin
    virtual void delete_bulletin(QString fileName)=0;// delete selected bulletin
    virtual void read_bulletin(QString fileName)=0;// load the text of a selected bulletin into the text editor
    virtual void write_bulletin(QString fileName,QString bulletinText)=0;// submit the modified text
    virtual void get_bulletins()=0;// read the names of the bulletins and show them
    virtual void get_teachers()=0;
public:
    virtual void open_rulemaker_panel()=0;// open the rulemaker panel
    virtual void close_rulemaker_panel()=0;// close the rulemaker panel
};

class QuestionManagement{// question management (teacher)
protected:
    virtual void get_subjects()=0;// read all subjects into the subject area
    virtual void get_chapters(QString subject)=0;// read all chapters for a specific subject
    virtual void get_questions(QString subject,QString chapter)=0;// read all questions for a chapter
    virtual void add_subject(QString subject)=0;// add a new subject
    virtual void add_chapter(QString subject,QString chapter)=0;// add a new chapter
    virtual void add_question(QString subject,QString chapter,QString questionName)=0;// add a new question file
    virtual void delete_subject(QString subject)=0;// delete subject
    virtual void delete_chapter(QString subject,QString chapter)=0;// delete chapter of a subject
    virtual void delete_question(QString subject,QString chapter,QString questionName)=0;// delete one question
    virtual void read_question(QString subject,QString chapter,QString timeStamp)=0;// load the question text to the editor
    virtual void write_question(QString subject,QString chapter,QString timeStamp,QString questionText)=0;// submit question text after editing
public:
    virtual void open_question_management_panel()=0;// open the window
    virtual void close_question_management_panel()=0;// close the window
};

class PaperProduction{// paper generation
protected:
    QStringList questionsList;// store question list
    virtual void delete_question(QString questionName)=0;// delete question from the list
    virtual void output_paper(QString pathName,QStringList questionsList)=0;// output paper
    virtual void read_questions(QStringList questionsList)=0;// read all questions and display
public:
    virtual void add_question(QString subject,QString chapter,QString timeStamp)=0;// add question into the paper
    virtual void open_paper_production_panel()=0;
    virtual void close_paper_production_panel()=0;
};

#endif
