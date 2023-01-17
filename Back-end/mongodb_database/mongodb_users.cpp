#include <iostream>
#include <cassert>
#include "mongocxx/instance.hpp"
#include "mongodb_users.hpp"

using namespace std;

int main(){
    mongocxx::instance instance;
    defaultsetting::mongodb_database mdatabase;
    string username = "";
    string password = "";
    string identity = "";
    string status = "";
    bool success = mdatabase.insert(username, password, identity, status);
    assert(success == true);
    return 0;
}