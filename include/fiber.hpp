// #pragma once

// #include <coroutine>
// #include <functional>
// #include <optional>
// #include <variant>
// #include <iostream>

// #include "uvpp/work.hpp"
// #include "cppcoro/async_manual_reset_event.hpp"

// template <typename T = void>
// struct [[nodiscard]] task {
//   using value_type = std::conditional_t<std::is_same_v<T, void>, std::nullopt_t, T>;

//   static constexpr bool is_void_v = std::is_same_v<value_type, std::nullopt_t>;
//   static constexpr bool is_integral_v = std::is_integral_v<value_type>;
//   static constexpr bool is_move_constructible_v = std::is_move_constructible_v<value_type>;
//   static constexpr bool is_copy_constructible_v = std::is_copy_constructible_v<value_type>;
//   static constexpr bool is_basic_type_v = std::is_arithmetic_v<value_type> || std::is_pointer_v<value_type>;

//   static constexpr bool use_movable_param = is_move_constructible_v && !is_basic_type_v && !is_void_v;
//   static constexpr bool use_const_param = is_copy_constructible_v && !is_move_constructible_v && !is_basic_type_v && !is_void_v;
//   static constexpr bool use_value_param = ((!is_move_constructible_v && !is_copy_constructible_v) || is_basic_type_v) && !is_void_v;

//   struct promise_type_base {
//     std::variant<std::monostate, value_type, std::exception_ptr> result;
//     std::coroutine_handle<> waiter; // who waits on this coroutine

//     void unhandled_exception(std::exception_ptr error) {
//       result.template emplace<2>(error);
//     }

//     void unhandled_exception() {
//       unhandled_exception(std::current_exception());
//     }

//     std::suspend_always initial_suspend() {
//       return {};
//     }

//     auto final_suspend() noexcept {
//       struct final_awaiter {
//         bool await_ready() {
//           return false;
//         }

//         void await_resume() {
//         }

//         auto await_suspend(std::coroutine_handle<promise_type> me) {
//           return me.promise().waiter;
//         }
//       };

//       return final_awaiter{};
//     }
//   };

//   struct promise_type_movable_param : public promise_type_base {
//     auto get_return_object() {
//       return task{*this};
//     }

//     void return_value(value_type&& value) {
//       this->result.template emplace<1>(std::move(value));
//     }

//     void return_value(value_type& value) {
//       this->result.template emplace<1>(std::move(value));
//     }

//     value_type&& unpack() {
//       if (this->result.index() == 2) {
//         std::rethrow_exception(std::get<2>(this->result));
//       }

//       return std::move(std::get<1>(this->result));
//     }
//   };

//   struct promise_type_const_param : public promise_type_base {
//     auto get_return_object() {
//       return task{*this};
//     }

//     void return_value(const value_type& value) {
//       this->result.template emplace<1>(value);
//     }

//     value_type unpack() {
//       if (this->result.index() == 2) {
//         std::rethrow_exception(std::get<2>(this->result));
//       }

//       return std::get<1>(this->result);
//     }
//   };

//   struct promise_type_value_param : public promise_type_base {
//     auto get_return_object() {
//       return task{*this};
//     }

//     void return_value(value_type value) {
//       this->result.template emplace<1>(value);
//     }

//     value_type unpack() {
//       if (this->result.index() == 2) {
//         std::rethrow_exception(std::get<2>(this->result));
//       }

//       return std::get<1>(this->result);
//     }
//   };

//   struct promise_type_void_param : public promise_type_base {
//     auto get_return_object() {
//       return task{*this};
//     }

//     void return_void() {
//     }

//     value_type unpack() {
//       if (this->result.index() == 2) {
//         std::rethrow_exception(std::get<2>(this->result));
//       }

//       return std::nullopt;
//     }
//   };

//   using promise_type = std::conditional_t<is_void_v, promise_type_void_param,
//       std::conditional_t<use_movable_param, promise_type_movable_param,
//           std::conditional_t<use_const_param, promise_type_const_param,
//               std::conditional_t<use_value_param, promise_type_value_param, void>>>>;

//   task() : _handle(nullptr) {}

//   task(task&& rhs) : _handle(rhs._handle) {
//     rhs._handle = nullptr;
//   }

//   task& operator=(task&& rhs) {
//     if (_handle) {
//       _handle.destroy();
//     }

//     _handle = rhs._handle;
//     rhs._handle = nullptr;

//     return *this;
//   }

//   task(const task&) = delete;
  
//   task& operator=(const task&) = delete;

//   ~task() {
//     if (_handle) {
//       _handle.destroy();
//     }
//   }

//   explicit task(std::coroutine_handle<promise_type>& handle) : _handle(std::move(handle)) {
//   }

//   explicit task(promise_type& promise) : _handle(std::coroutine_handle<promise_type>::from_promise(promise)) {
//   }

//   bool await_ready() {
//     return false; // no idea why i return false here
//   }

//   auto await_resume() {
//     return unpack();
//   }

//   void await_suspend(std::coroutine_handle<> waiter) {
//     if constexpr (is_void_v) {
//       if (!_handle) {
//         waiter.resume();
//         return;
//       }
//     }

//     uv::work::queue<void>(
//         [this]() {
//           _handle.promise().waiter = waiter;
//           _handle.resume();
//         },
//         [this, waiter](auto, auto error) {
//           waiter.resume();
//         });
//   }

//   void start() {
//     await_suspend(std::noop_coroutine());
//   }

//   bool done() {
//     if constexpr (is_void_v) {
//       if (!_handle) {
//         return true;
//       }
//     }

//     return _handle.done();
//   }

//   auto unpack() {
//     if constexpr (is_void_v) {
//       if (!_handle) {
//         return std::nullopt;
//       }
//     }

//     return _handle.promise().unpack();
//   }

// private:
//   std::coroutine_handle<promise_type> _handle;
// };
