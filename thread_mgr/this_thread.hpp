#pragma once
#ifndef DEF_ONCE_THREAD_MGR_THIS_THREAD_HPP
#define DEF_ONCE_THREAD_MGR_THIS_THREAD_HPP
#include "threads_status.hpp"
#include <chrono>
#include <thread>

namespace tmgr {
	namespace this_thread {
		inline void wanna_execute() {
			if (!current_context) return;
			if (current_context->_abort)
				throw thread_context_t::thread_abort();
			if (current_context->is_pause.test(std::memory_order_acquire)) {
				current_context->broadcast_status(thread_status::lock);
				while (current_context->is_pause.test_and_set(std::memory_order_acquire))
					std::this_thread::sleep_for(std::chrono::microseconds(15));
				current_context->is_pause.clear();
			}
			current_context->broadcast_status(thread_status::work);
		}
		inline void has_solve(void* solve_ptr) {
			if (!current_context) return;
			current_context->thread_result = solve_ptr;
			current_context->broadcast_status(thread_status::has_solve);
			wanna_execute();
		}

		template <class _Rep, class _Period>
		void sleep_for(const std::chrono::duration<_Rep, _Period>& _Rel_time) {
			if (!current_context) {
				std::this_thread::sleep_for(_Rel_time);
				return;
			}
			if (current_context->_abort)
				throw thread_context_t::thread_abort();
			current_context->broadcast_status(thread_status::sleep);
			std::this_thread::sleep_for(_Rel_time);
			wanna_execute();
		}

		template <class _Clock, class _Duration>
		void sleep_until(const std::chrono::time_point<_Clock, _Duration>& _Abs_time) {
			if (!current_context) {
				std::this_thread::sleep_until(_Abs_time);
				return;
			}
			if (current_context->abort)
				throw thread_context_t::thread_abort();
			current_context->broadcast_status(thread_status::sleep);
			std::this_thread::sleep_until(_Abs_time);
			wanna_execute();
		}

		inline std::thread::id get_id() {
			return std::this_thread::get_id();
		}

		inline bool can_manage() {
			return current_context;
		}
		inline bool can_manage(thread_context_t& context) {
			return &context;
		}
	}
}


#endif