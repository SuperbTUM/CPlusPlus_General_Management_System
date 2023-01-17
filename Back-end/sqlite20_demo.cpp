#include <msqlite.hpp>

#include <iostream>

namespace sql = msqlite;
using namespace std;

template<typename T>
auto getAttribute(string_view constraint_key, string_view constraint_val, string_view target_attribute) {
    return sql::open("userinfo.db", sql::cache_stmts)
    | sql::prepare("select ? from USER where ?= ?", target_attribute, constraint_key, constraint_val)
    | sql::step([](T val){ return val; });
}

int main() {
    if(auto s = getAttribute<int>("username", "admin", "activity"))
        cout << *s << endl;
    else {
        if(s.error() == sql::error::empty) cout << "not exists";
        else cout << s.error();
        cout << endl;
    }
}
