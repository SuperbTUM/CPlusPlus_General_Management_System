#pragma once

#include <coroutine>
#include <exception>
#include <utility>
#include <optional>
#include <iterator>
#include <stdexcept>
#include <type_traits>

namespace async {

template <typename T = void>
class Task;

namespace detail {

struct TaskPromiseBase {
    std::coroutine_handle<> continuation{nullptr};
    std::exception_ptr exception{nullptr};

    struct FinalAwaiter {
        bool await_ready() const noexcept { return false; }

        template <typename PromiseType>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> h) noexcept {
            if (h.promise().continuation) {
                return h.promise().continuation;
            }
            return std::noop_coroutine();
        }

        void await_resume() const noexcept {}
    };

    std::suspend_always initial_suspend() const noexcept { return {}; }
    FinalAwaiter final_suspend() const noexcept { return {}; }
    void unhandled_exception() noexcept { exception = std::current_exception(); }
};

template <typename T>
struct TaskPromise : TaskPromiseBase {
    std::optional<T> result;

    Task<T> get_return_object() noexcept;

    void return_value(T value) noexcept(std::is_nothrow_move_constructible_v<T>) {
        result.emplace(std::move(value));
    }
};

template <>
struct TaskPromise<void> : TaskPromiseBase {
    Task<void> get_return_object() noexcept;

    void return_void() noexcept {}
};

} // namespace detail

/**
 * @brief C++20 Coroutine Task<T> representing a lazy asynchronous operation.
 * Supports co_await, co_return, and synchronous execution via .run_sync() / .get().
 */
template <typename T>
class Task {
public:
    using promise_type = detail::TaskPromise<T>;

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task() {
        if (handle_) handle_.destroy();
    }

    bool await_ready() const noexcept {
        return !handle_ || handle_.done();
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
        handle_.promise().continuation = awaiting;
        return handle_;
    }

    T await_resume() {
        if (!handle_) throw std::runtime_error("Task handle is null");
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return std::move(*handle_.promise().result);
    }

    /// Synchronous execution to completion
    T run_sync() {
        if (!handle_) throw std::runtime_error("Task handle is null");
        if (!handle_.done()) {
            handle_.resume();
        }
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return std::move(*handle_.promise().result);
    }

    T get() { return run_sync(); }

    bool done() const noexcept { return !handle_ || handle_.done(); }

private:
    std::coroutine_handle<promise_type> handle_{nullptr};
};

/**
 * @brief Specialization of Task for void return type.
 */
template <>
class Task<void> {
public:
    using promise_type = detail::TaskPromise<void>;

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task() {
        if (handle_) handle_.destroy();
    }

    bool await_ready() const noexcept {
        return !handle_ || handle_.done();
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
        handle_.promise().continuation = awaiting;
        return handle_;
    }

    void await_resume() {
        if (!handle_) throw std::runtime_error("Task handle is null");
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
    }

    void run_sync() {
        if (!handle_) throw std::runtime_error("Task handle is null");
        if (!handle_.done()) {
            handle_.resume();
        }
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
    }

    void get() { run_sync(); }

    bool done() const noexcept { return !handle_ || handle_.done(); }

private:
    std::coroutine_handle<promise_type> handle_{nullptr};
};

namespace detail {
template <typename T>
inline Task<T> TaskPromise<T>::get_return_object() noexcept {
    return Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
}

inline Task<void> TaskPromise<void>::get_return_object() noexcept {
    return Task<void>{std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
}
} // namespace detail

/**
 * @brief C++20 Coroutine Generator<T> for yielding sequential items with co_yield.
 * Conforms to C++20 input range concepts and can be used in std::ranges pipelines.
 */
template <typename T>
class Generator {
public:
    struct promise_type {
        const T* current_value{nullptr};
        std::exception_ptr exception{nullptr};

        Generator get_return_object() noexcept {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }

        std::suspend_always yield_value(const T& val) noexcept {
            current_value = std::addressof(val);
            return {};
        }

        std::suspend_always yield_value(T&& val) noexcept {
            current_value = std::addressof(val);
            return {};
        }

        void return_void() const noexcept {}
        void unhandled_exception() noexcept { exception = std::current_exception(); }
    };

    explicit Generator(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}
    Generator(Generator&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    ~Generator() {
        if (handle_) handle_.destroy();
    }

    class Iterator {
    public:
        using iterator_concept = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        Iterator() = default;
        explicit Iterator(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

        Iterator& operator++() {
            handle_.resume();
            if (handle_.done()) {
                if (handle_.promise().exception) {
                    std::rethrow_exception(handle_.promise().exception);
                }
                handle_ = nullptr;
            }
            return *this;
        }

        void operator++(int) {
            (void)operator++();
        }

        reference operator*() const noexcept {
            return *handle_.promise().current_value;
        }

        pointer operator->() const noexcept {
            return handle_.promise().current_value;
        }

        bool operator==(std::default_sentinel_t) const noexcept {
            return handle_ == nullptr || handle_.done();
        }

    private:
        std::coroutine_handle<promise_type> handle_{nullptr};
    };

    Iterator begin() {
        if (!handle_) return Iterator{nullptr};
        handle_.resume();
        if (handle_.done()) {
            if (handle_.promise().exception) {
                std::rethrow_exception(handle_.promise().exception);
            }
            return Iterator{nullptr};
        }
        return Iterator{handle_};
    }

    std::default_sentinel_t end() const noexcept {
        return std::default_sentinel;
    }

private:
    std::coroutine_handle<promise_type> handle_{nullptr};
};

} // namespace async
