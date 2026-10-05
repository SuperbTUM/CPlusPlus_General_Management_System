#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <print>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_NO_MAIN
#include "server_unencrypted.hpp"
#include "server_unencrypted.cpp"

std::string exchange_json(int sock_fd, const std::string& request) {
    ssize_t sent = send(sock_fd, request.c_str(), request.length(), 0);
    assert(sent == static_cast<ssize_t>(request.length()));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    char buffer[2048] = {0};
    ssize_t received = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
    assert(received > 0);
    return std::string(buffer, received);
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    std::println("==================================================");
    std::println(" Running End-to-End Client-Server Integration Test");
    std::println("==================================================");

    const int test_port = 19999;

    // 1. Initialize Server on dedicated test port
    std::println("\n[E2E STEP 1] Initializing Server on port {}...", test_port);
    auto server = Server::getInstance(test_port);
    server->init();

    // 2. Start Server loop in background std::jthread
    std::println("[E2E STEP 2] Starting Server loop in std::jthread...");
    std::jthread server_thread = server->start_in_thread();

    // Give server a moment to spin up epoll listener
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // 3. Connect TCP Client Socket to Server
    std::println("[E2E STEP 3] Connecting client socket to 127.0.0.1:{}...", test_port);
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(client_fd >= 0);

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(test_port);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    int conn_rc = connect(client_fd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr));
    assert(conn_rc == 0);
    std::println("  -> Client connected successfully.");

    // Unique user for this run
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string test_user = "prof_e2e_" + std::to_string(now_ms % 100000);
    std::string test_pass = "ModernC++2026";

    // 4. Test User Registration
    std::println("\n[E2E STEP 4] Sending 'register user' command for '{}'...", test_user);
    std::string register_req = fmt::format(
        "{{\"command\":\"register user\",\"username\":\"{}\",\"password\":\"{}\",\"identity\":\"teacher\"}}",
        test_user, test_pass);
    std::string register_resp = exchange_json(client_fd, register_req);
    std::println("  -> Server response: {}", register_resp);
    assert(register_resp.find("\"code\": 200") != std::string::npos || register_resp.find("\"code\":200") != std::string::npos);

    // 5. Test User Login (authenticated with SHA-256 password hash)
    std::println("\n[E2E STEP 5] Sending 'login' command with credentials...");
    std::string login_req = fmt::format(
        "{{\"command\":\"login\",\"username\":\"{}\",\"password\":\"{}\"}}",
        test_user, test_pass);
    std::string login_resp = exchange_json(client_fd, login_req);
    std::println("  -> Server response: {}", login_resp);
    assert(login_resp.find("\"code\": 200") != std::string::npos || login_resp.find("\"code\":200") != std::string::npos);
    assert(login_resp.find("teacher") != std::string::npos);

    // 6. Test Teacher Action: 'write subject'
    std::string test_subject = fmt::format("Subject_{}", now_ms % 100000);
    std::println("\n[E2E STEP 6] Sending 'write subject' command for '{}'...", test_subject);
    std::string subject_req = fmt::format("{{\"command\":\"write subject\",\"subject_name\":\"{}\"}}", test_subject);
    std::string subject_resp = exchange_json(client_fd, subject_req);
    std::println("  -> Server response: {}", subject_resp);
    assert(subject_resp.find("\"code\": 200") != std::string::npos || subject_resp.find("\"code\":200") != std::string::npos);

    // 7. Test: 'get subjects'
    std::println("\n[E2E STEP 7] Sending 'get subjects' command...");
    std::string get_subjects_req = "{\"command\":\"get subjects\"}";
    std::string get_subjects_resp = exchange_json(client_fd, get_subjects_req);
    std::println("  -> Server response: {}", get_subjects_resp);
    assert(get_subjects_resp.find(test_subject) != std::string::npos || get_subjects_resp.find("\"code\": 200") != std::string::npos || get_subjects_resp.find("\"code\":200") != std::string::npos);

    // 8. Test Logout
    std::println("\n[E2E STEP 8] Sending 'logout' command...");
    std::string logout_req = "{\"command\":\"logout\"}";
    std::string logout_resp = exchange_json(client_fd, logout_req);
    std::println("  -> Server response: {}", logout_resp);
    assert(logout_resp.find("\"code\": 200") != std::string::npos || logout_resp.find("\"code\":200") != std::string::npos);

    // 9. Close client connection
    std::println("\n[E2E STEP 9] Closing client connection...");
    close(client_fd);

    // 10. Cooperative Server Shutdown
    std::println("[E2E STEP 10] Requesting cooperative shutdown of server thread via stop_token...");
    server_thread.request_stop();
    // server_thread automatically joins here via RAII
    std::println("  -> Server thread stopped and auto-joined successfully!");

    std::println("\n==================================================");
    std::println(" ALL E2E NETWORK INTEGRATION TESTS PASSED!       ");
    std::println("==================================================");

    return 0;
}
