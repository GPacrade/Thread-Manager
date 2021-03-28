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
			while (lock_flag.test_and_set(std::memory_order_acquire)) {
				current_context->broadcast_status(thread_status::lock);
				std::this_thread::sleep_for(std::chrono::microseconds(15));
			}
			tmp = std::this_thread::get_id();
			current_context->broadcast_status(thread_status::work);
		}
		bool try_lock() {
			return !lock_flag.test_and_set(std::memory_order_acquire);
		}
		void unlock() {
			if (tmp != std::this_thread::get_id()) throw std::exception("Dissallowed unlock");
			lock_flag.clear();
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