#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace ebasic::rt {

/// M12: the runtime type behind `Async ... AS Task(OF T)` - a coroutine
/// that produces exactly one value. `initial_suspend` is `suspend_never`
/// (the body starts running the instant the coroutine function is called,
/// not lazily) - this runtime has no real scheduler/thread pool/async I/O
/// (eBasic itself has no concurrency primitives at all yet), so a Task
/// always runs synchronously, straight through to completion or its own
/// nested AWAIT, by the time control returns to its caller. `AWAIT` on a
/// Task is therefore always immediately ready in practice - `await_ready`/
/// `await_suspend` still implement the real Awaitable protocol correctly
/// (required for `co_await` to type-check at all), they just never
/// actually suspend the awaiting coroutine, since there is never anything
/// left to wait for by the time anyone awaits.
///
/// Default-constructible (an empty, null-handle state) - required so
/// `DIM x AS Task(OF T)` (always zero/default-initialized, `Type var{};`,
/// like every other DIM in this compiler) compiles at all; every member
/// below guards against a null/empty handle rather than assuming a real
/// coroutine always produced this object.
template <typename T>
struct Task {
    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;

        Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(T v) { value = std::move(v); }
        void unhandled_exception() { exception = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task() noexcept : handle(nullptr) {}
    explicit Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task() {
        if (handle) handle.destroy();
    }

    bool await_ready() const noexcept { return !handle || handle.done(); }
    void await_suspend(std::coroutine_handle<>) const noexcept {
        /// Never actually reached in this synchronous-only runtime - see
        /// the class-level comment - but a real, correctly-typed
        /// implementation is required for the Awaitable protocol itself.
    }
    T await_resume() {
        if (handle.promise().exception) std::rethrow_exception(handle.promise().exception);
        return std::move(*handle.promise().value);
    }

    /// A plain, non-`co_await` accessor - `co_await` is only legal inside
    /// a real coroutine body (C++ itself forbids `main` from ever being
    /// one), so top-level eBasic code (which compiles into `main`) or an
    /// ordinary, non-Async SUB/FUNCTION needs another way to read an
    /// already-completed Task's value. Since `initial_suspend` is
    /// `suspend_never`, by the time any Task object exists at all its
    /// body has already run straight through to completion (or its own
    /// nested AWAIT) - safe to call unconditionally, unlike await_resume,
    /// which moves the value out (Result() reads it repeatably, matching
    /// Generator::Current()'s own repeatable-read shape).
    T Result() const {
        if (handle.promise().exception) std::rethrow_exception(handle.promise().exception);
        return *handle.promise().value;
    }
};

/// Task<void> - the `Async` SUB case (no return value at all). Otherwise
/// identical to the general template above.
template <>
struct Task<void> {
    struct promise_type {
        std::exception_ptr exception;

        Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { exception = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task() noexcept : handle(nullptr) {}
    explicit Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task() {
        if (handle) handle.destroy();
    }

    bool await_ready() const noexcept { return !handle || handle.done(); }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    void await_resume() {
        if (handle && handle.promise().exception) std::rethrow_exception(handle.promise().exception);
    }
};

/// M12: the runtime type behind `Async ... AS Generator(OF T)` - a lazily-
/// pulled sequence of `YIELD`ed values. Unlike Task, `initial_suspend` is
/// `suspend_always`: the body doesn't run at all until the first
/// `MoveNext()` call, matching a real generator's own "nothing happens
/// until you ask for the first value" semantics. Exposes a plain pull API
/// (`MoveNext`/`Current`) rather than a C++ range/iterator pair - eBasic
/// itself has no `FOR EACH`/range-based-for concept yet, so Codegen drives
/// consumption with an ordinary `DO WHILE MoveNext()` loop instead.
///
/// Default-constructible (an empty, null-handle state) - see Task's own
/// doc comment for why; MoveNext() already needed a null-handle guard
/// regardless (a freshly-created real Generator hasn't run its body yet
/// either), so this reuses the exact same check.
template <typename T>
struct Generator {
    struct promise_type {
        T currentValue{};
        std::exception_ptr exception;

        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(T v) {
            currentValue = std::move(v);
            return {};
        }
        void return_void() {}
        void unhandled_exception() { exception = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> handle;

    Generator() noexcept : handle(nullptr) {}
    explicit Generator(std::coroutine_handle<promise_type> h) : handle(h) {}
    Generator(Generator&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    ~Generator() {
        if (handle) handle.destroy();
    }

    /// Resumes the generator until its next YIELD (or completion). Returns
    /// true iff a new value is now available via Current() - false means
    /// the generator has run to completion (or is a default-constructed,
    /// never-assigned empty Generator) and Current() must not be called
    /// again. Must be called at least once before the first Current()
    /// (initial_suspend is suspend_always, so nothing has run yet on a
    /// freshly-created Generator).
    bool MoveNext() {
        if (!handle || handle.done()) return false;
        handle.resume();
        if (handle.promise().exception) std::rethrow_exception(handle.promise().exception);
        return !handle.done();
    }
    T Current() const { return handle.promise().currentValue; }
};

} // namespace ebasic::rt
