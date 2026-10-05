#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <compare>
#include <ranges>
#include <algorithm>
#include <expected>
#include <print>

#include "user_info.hpp"
#include "question_bank.hpp"
#include "server_unencrypted.hpp"

#include "user_info.cpp"
#include "question_bank.cpp"

void test_spaceship_operator() {
    std::println("\n[TEST 1] Testing C++20 operator<=> (Three-Way Comparison)...");

    // 1. UserInfo comparison
    UserInfo<string> user1("alice", "pass123", "admin", "active", 1);
    UserInfo<string> user2("alice", "pass123", "admin", "active", 1);
    UserInfo<string> user3("bob",   "pass456", "user",  "active", 1);

    assert(user1 == user2);
    assert(user1 != user3);
    assert(user1 < user3);
    assert(user3 > user1);
    assert((user1 <=> user2) == 0);
    assert((user1 <=> user3) < 0);

    // 2. QuestionInfo comparison
    QuestionInfo<string> q1("path1", "content1", "ch1", "cat1", 10);
    QuestionInfo<string> q2("path1", "content1", "ch1", "cat1", 10);
    QuestionInfo<string> q3("path2", "content2", "ch2", "cat2", 20);

    assert(q1 == q2);
    assert(q1 < q3);
    assert((q1 <=> q3) < 0);

    // 3. struct s1 comparison
    s1 msg1{.command = "login", .username = "alice"};
    s1 msg2{.command = "login", .username = "alice"};
    s1 msg3{.command = "signup", .username = "bob"};

    assert(msg1 == msg2);
    assert(msg1 != msg3);
    assert(msg1 < msg3);
    assert((msg1 <=> msg3) < 0);

    std::println("  -> operator<=> checks passed for UserInfo, QuestionInfo, and s1.");
}

void test_ranges_and_views() {
    std::println("\n[TEST 2] Testing C++20 std::ranges and std::views...");

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Filter even numbers and square them using range views
    auto even_squares = numbers 
        | std::views::filter([](int n) { return n % 2 == 0; })
        | std::views::transform([](int n) { return n * n; });

    std::vector<int> result(even_squares.begin(), even_squares.end());
    std::vector<int> expected = {4, 16, 36, 64, 100};
    assert(result == expected);

    // Sort using std::ranges::sort with <=> ordering
    std::vector<UserInfo<string>> user_list = {
        {"charlie", "p3", "user", "active", 3},
        {"alice",   "p1", "admin", "active", 1},
        {"bob",     "p2", "user", "active", 2}
    };

    std::ranges::sort(user_list);
    auto [u0, p0, id0, st0, ac0] = user_list[0].getElements();
    auto [u1, p1, id1, st1, ac1] = user_list[1].getElements();
    auto [u2, p2, id2, st2, ac2] = user_list[2].getElements();

    assert(u0 == "alice");
    assert(u1 == "bob");
    assert(u2 == "charlie");

    // In-place transformation using std::ranges::transform
    std::vector<std::string> words = {"hello", "world"};
    std::ranges::transform(words, words.begin(), [](std::string s) {
        std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::toupper(c); });
        return s;
    });
    assert(words[0] == "HELLO" && words[1] == "WORLD");

    std::println("  -> std::ranges and std::views pipeline operations passed.");
}

void test_expected_and_monadic_db() {
    std::println("\n[TEST 3] Testing C++23 std::expected and monadic operations...");

    db_user db;
    db.create(true, "test_modern.db");

    // 1. Valid SQL returns std::expected with rows
    auto res = db.execute_query<string>("SELECT username, identity FROM USER;");
    assert(res.has_value());
    std::println("  -> Valid query succeeded. Rows returned: {}", res.value().size());

    // 2. Monadic transform test
    auto row_count = res.transform([](const auto& rows) { return rows.size(); }).value_or(0);
    std::println("  -> Monadic .transform() extracted count: {}", row_count);

    // 3. Invalid SQL returns std::unexpected with error description
    auto err_res = db.execute_query<string>("SELECT * FROM NON_EXISTENT_TABLE_ABC;");
    assert(!err_res.has_value());
    assert(err_res.error().find("SQL prepare failed") != std::string::npos);
    std::println("  -> Invalid query returned expected error: '{}'", err_res.error());

    db.clean();
    db.close();
    std::remove("test_modern.db");
    std::println("  -> std::expected database operations passed.");
}

void test_jthread_and_stop_token() {
    std::println("\n[TEST 4] Testing C++20 std::jthread & std::stop_token (Phase 2)...");

    std::atomic<int> counter{0};
    std::atomic<bool> callback_fired{false};

    {
        // 1. Launch a cooperative worker with std::jthread
        std::jthread worker([&counter, &callback_fired](std::stop_token st) {
            std::stop_callback cb(st, [&callback_fired]() {
                callback_fired.store(true);
            });

            while (!st.stop_requested()) {
                counter.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });

        assert(worker.joinable());
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        // 2. Request cooperative cancellation
        bool stopped = worker.request_stop();
        assert(stopped == true);
        assert(worker.get_stop_token().stop_requested());

        // 3. Worker auto-joins upon destructor exit via RAII
    }

    assert(callback_fired.load() == true);
    assert(counter.load() > 0);
    std::println("  -> std::jthread executed {} iterations, fired stop_callback, and auto-joined via RAII.", counter.load());
}

void test_sha256_encryption() {
    std::println("\n[TEST 5] Testing OpenSSL 3.0 EVP SHA-256 Password Hashing...");

    std::string pass1 = "mySecretPassword123";
    std::string pass2 = "mySecretPassword123";
    std::string pass3 = "differentPassword";

    std::string hash1 = encrypt_password(pass1);
    std::string hash2 = encrypt_password(pass2);
    std::string hash3 = encrypt_password(pass3);

    // SHA-256 in hex is 64 characters (256 bits / 4 bits per hex char)
    assert(hash1.length() == 64);
    assert(hash1 == hash2);
    assert(hash1 != hash3);
    std::println("  -> SHA-256 hash length is 64 chars. Deterministic hashing verified.");
}

// Coroutine helper tasks
async::Task<int> async_double(int x) {
    co_return x * 2;
}

async::Task<std::string> async_calculate_pipeline(int a, int b) {
    int val_a = co_await async_double(a);
    int val_b = co_await async_double(b);
    co_return fmt::format("Result: {}", val_a + val_b);
}

async::Task<void> async_mutate(int& out_val) {
    out_val += 42;
    co_return;
}

async::Task<int> async_throw_task() {
    throw std::runtime_error("Simulated async error");
    co_return 0;
}

void test_coroutine_task() {
    std::println("\n[TEST 6] Testing C++20 Coroutine Task<T> & co_await...");

    // 1. Test Task with co_await composition
    auto task = async_calculate_pipeline(10, 25);
    assert(!task.done());
    std::string result = task.run_sync();
    assert(result == "Result: 70");
    assert(task.done());
    std::println("  -> Coroutine composition: async_calculate_pipeline(10, 25) = '{}'", result);

    // 2. Test Task<void>
    int side_effect = 100;
    auto void_task = async_mutate(side_effect);
    void_task.run_sync();
    assert(side_effect == 142);
    std::println("  -> Task<void> side effect executed: {}", side_effect);

    // 3. Test exception propagation across coroutine boundaries
    auto err_task = async_throw_task();
    bool caught = false;
    try {
        err_task.run_sync();
    } catch (const std::runtime_error& e) {
        caught = true;
        assert(std::string(e.what()) == "Simulated async error");
    }
    assert(caught);
    std::println("  -> Exception successfully caught from coroutine: 'Simulated async error'");
}

// Coroutine Generator
async::Generator<int> fibonacci_generator(int count) {
    int a = 0, b = 1;
    for (int i = 0; i < count; ++i) {
        co_yield a;
        int next = a + b;
        a = b;
        b = next;
    }
}

void test_coroutine_generator() {
    std::println("\n[TEST 7] Testing C++20 Coroutine Generator<T> & co_yield with std::ranges...");

    // 1. Basic generator iteration
    std::vector<int> fibs;
    for (int val : fibonacci_generator(8)) {
        fibs.push_back(val);
    }
    std::vector<int> expected_fibs = {0, 1, 1, 2, 3, 5, 8, 13};
    assert(fibs == expected_fibs);
    std::println("  -> Generator yielded 8 Fibonacci numbers: [0, 1, 1, 2, 3, 5, 8, 13]");

    // 2. Integration with C++20 ranges
    auto gen = fibonacci_generator(8);
    auto evens = gen 
        | std::views::filter([](int x) { return x % 2 == 0; })
        | std::views::transform([](int x) { return x * 10; });

    std::vector<int> transformed_evens;
    for (int val : evens) {
        transformed_evens.push_back(val);
    }
    std::vector<int> expected_evens = {0, 20, 80};
    assert(transformed_evens == expected_evens);
    std::println("  -> std::ranges views pipeline over Generator yielded: [0, 20, 80]");
}

void test_sqlite_coroutine_streaming() {
    std::println("\n[TEST 8] Testing SQLite Coroutine Database Streaming (streamUsers & streamSubjects)...");

    db_user user_db;
    user_db.create(true, "test_stream.db");

    // Insert test users (valid identities: 'admin', 'rule maker', 'teacher')
    auto u1 = std::make_shared<UserInfo<string>>("stream_user1", "pass1", "admin", "valid", 1);
    auto u2 = std::make_shared<UserInfo<string>>("stream_user2", "pass2", "teacher", "valid", 1);
    auto u3 = std::make_shared<UserInfo<string>>("stream_user3", "pass3", "teacher", "valid", 0);
    user_db.insert(u1);
    user_db.insert(u2);
    user_db.insert(u3);

    // Stream all users
    int total_users = 0;
    for (const auto& user : user_db.streamUsers()) {
        total_users++;
    }
    assert(total_users == 3);
    std::println("  -> streamUsers() lazily streamed {} users.", total_users);

    // Stream filtered users by constraint
    int teacher_count = 0;
    for (const auto& user : user_db.streamUsers(std::make_pair("identity", "teacher"))) {
        teacher_count++;
        assert(std::get<2>(user.getElements()) == "teacher");
    }
    assert(teacher_count == 2);
    std::println("  -> streamUsers(identity=teacher) lazily streamed {} matching users.", teacher_count);

    user_db.clean();
    user_db.close();
    std::remove("test_stream.db");

    // Test question_bank streaming
    question_bank q_db;
    q_db.create(true, "test_qstream.db");
    auto q1 = std::make_shared<QuestionInfo<string>>("q101", "What is C++20?", "Chapter1", "ComputerScience");
    auto q2 = std::make_shared<QuestionInfo<string>>("q102", "What are Coroutines?", "Chapter1", "ComputerScience");
    auto q3 = std::make_shared<QuestionInfo<string>>("q201", "What is Calculus?", "Chapter1", "Mathematics");
    q_db.insert(q1);
    q_db.insert(q2);
    q_db.insert(q3);

    std::vector<string> streamed_subjects;
    for (const auto& subj : q_db.streamSubjects()) {
        streamed_subjects.push_back(subj);
    }
    assert(streamed_subjects.size() == 2);
    std::println("  -> streamSubjects() lazily streamed {} unique subjects.", streamed_subjects.size());

    int cs_questions = 0;
    for (const auto& q : q_db.streamQuestions("ComputerScience", "Chapter1")) {
        cs_questions++;
    }
    assert(cs_questions == 2);
    std::println("  -> streamQuestions() lazily streamed {} questions for ComputerScience/Chapter1.", cs_questions);

    q_db.clean();
    q_db.close();
    std::remove("test_qstream.db");
}

void test_shared_mutex_concurrency() {
    std::println("\n[TEST 9] Testing Modern Read/Write Concurrency with std::shared_mutex...");

    std::shared_mutex rw_mutex;
    int shared_resource = 100;
    std::atomic<int> concurrent_readers{0};
    std::atomic<int> max_concurrent_readers{0};

    // Spawn 4 concurrent readers
    std::vector<std::jthread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&rw_mutex, &shared_resource, &concurrent_readers, &max_concurrent_readers]() {
            std::shared_lock<std::shared_mutex> lock(rw_mutex);
            int current = concurrent_readers.fetch_add(1) + 1;
            
            // Track peak concurrent readers
            int prev_max = max_concurrent_readers.load();
            while (current > prev_max && !max_concurrent_readers.compare_exchange_weak(prev_max, current)) {}

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            assert(shared_resource == 100);
            concurrent_readers.fetch_sub(1);
        });
    }

    for (auto& r : readers) {
        if (r.joinable()) r.join();
    }

    assert(max_concurrent_readers.load() > 1);
    std::println("  -> Verified {} concurrent readers held shared_lock simultaneously.", max_concurrent_readers.load());

    // Exclusive write lock
    {
        std::unique_lock<std::shared_mutex> write_lock(rw_mutex);
        shared_resource = 200;
    }
    assert(shared_resource == 200);
    std::println("  -> Verified unique_lock exclusive modification.");
}

int main() {
    std::println("==================================================");
    std::println(" Running Modern C++ (C++20 / C++23) Feature Tests ");
    std::println("==================================================");

    test_spaceship_operator();
    test_ranges_and_views();
    test_expected_and_monadic_db();
    test_jthread_and_stop_token();
    test_sha256_encryption();
    test_coroutine_task();
    test_coroutine_generator();
    test_sqlite_coroutine_streaming();
    test_shared_mutex_concurrency();

    std::println("\n==================================================");
    std::println(" ALL MODERN C++ TESTS PASSED SUCCESSFULLY!       ");
    std::println("==================================================");

    return 0;
}
