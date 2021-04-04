#pragma once
#ifndef DEF_ONCE_THREAD_MGR_SPIN_LOCK_HPP
#define DEF_ONCE_THREAD_MGR_SPIN_LOCK_HPP
#include "threads_status.hpp"

namespace tmgr {
	class spin_lock {
	private:
		std::atomic_flag lock_flag;
		std::thread::id tmp;
	public:
		spin_lock() {}
		spin_lock(const spin_lock& copy) {
			if (copy.lock_flag.test(std::memory_order_acquire))
				lock_flag.test_and_set(std::memory_order_acquire); 
			tmp = copy.tmp;
		}
		void lock() {
			if(current_context)
				current_context->broadcast_status(thread_status::lock);
			while (lock_flag.test_and_set(std::memory_order_acquire))
				std::this_thread::sleep_for(std::chrono::microseconds(15));
			
			tmp = std::this_thread::get_id();
			if (current_context)
				current_context->broadcast_status(thread_status::work);
		}
		bool try_lock() {
			if (lock_flag.test_and_set(std::memory_order_acquire))
				return 0;
			tmp = std::this_thread::get_id();
			return 1;
		}

		template <class _Rep, class _Period>
		bool try_lock_for(const std::chrono::duration<_Rep, _Period>& _Rel_time) {
			auto end = std::chrono::system_clock::now();
			end += _Rel_time;

			if (current_context)
				current_context->broadcast_status(thread_status::lock);
			while (lock_flag.test_and_set(std::memory_order_acquire)) {
				std::this_thread::sleep_for(std::chrono::microseconds(15));
				if (end <= std::chrono::system_clock::now()){
					if (current_context)
						current_context->broadcast_status(thread_status::work);
					return 0;
				}
			}

			tmp = std::this_thread::get_id();
			if (current_context)
				current_context->broadcast_status(thread_status::work);
			return 1;
		}

		template <class _Clock, class _Duration>
		bool try_lock_until(const std::chrono::time_point<_Clock, _Duration>& _Abs_time) {
			if (current_context)
				current_context->broadcast_status(thread_status::lock);
			while (lock_flag.test_and_set(std::memory_order_acquire)) {
				std::this_thread::sleep_for(std::chrono::microseconds(15));
				if (_Abs_time <= std::chrono::system_clock::now()) {
					if (current_context)
						current_context->broadcast_status(thread_status::work);
					return 0;
				}
			}

			tmp = std::this_thread::get_id();
			if (current_context)
				current_context->broadcast_status(thread_status::work);
			return 1;
		}



		void unlock() {
			if (tmp != std::this_thread::get_id())
				throw std::exception("Dissallowed unlock");
			tmp = std::thread::id();
			lock_flag.clear();
		}

		std::thread::id locker_id() {
			return tmp;
		}

	};

	template<class T>
	class rail_lock;
	template<>
	class rail_lock<spin_lock> {
		spin_lock& locker;
	public:
		rail_lock(const rail_lock&) = delete;
		rail_lock(rail_lock&) = delete;
		rail_lock(const spin_lock& lock) : locker(const_cast<spin_lock&>(lock)) {
			locker.lock();
		}
		~rail_lock() {
			locker.unlock();
		}
	};
}
#endif