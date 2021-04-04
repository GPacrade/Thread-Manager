#pragma once
#ifndef DEF_ONCE_THREAD_MGR_SHADOW_SPIN_LOCK_HPP
#define DEF_ONCE_THREAD_MGR_SHADOW_SPIN_LOCK_HPP
#include <atomic>
#include <thread>
namespace tmgr {
	class shadow_spin_lock {
	private:
		std::atomic_flag lock_flag;
		std::thread::id tmp;
	public:
		shadow_spin_lock() {}
		shadow_spin_lock(const shadow_spin_lock& copy) noexcept {
			if (copy.lock_flag.test(std::memory_order_acquire)) {
				lock_flag.test_and_set(std::memory_order_acquire);
			}
			tmp = copy.tmp;
		}
		shadow_spin_lock& operator=(const shadow_spin_lock& copy){
			lock();
			if (!copy.lock_flag.test(std::memory_order_acquire))
				lock_flag.clear();
			tmp = copy.tmp;
			return *this;
		}
		

		void lock() noexcept {
			while (lock_flag.test_and_set(std::memory_order_acquire))
				std::this_thread::sleep_for(std::chrono::microseconds(15));
			tmp = std::this_thread::get_id();
		}
		bool try_lock() noexcept {
			if (lock_flag.test_and_set(std::memory_order_acquire))
				return 0;
			tmp = std::this_thread::get_id();
			return 1;
		}
		template <class _Rep, class _Period>
		bool try_lock_for(const std::chrono::duration<_Rep, _Period>& _Rel_time) {
			auto end = std::chrono::system_clock::now();
			end += _Rel_time;
			while (lock_flag.test_and_set(std::memory_order_acquire)) {
				std::this_thread::sleep_for(std::chrono::microseconds(15));
				if (end <= std::chrono::system_clock::now())
					return 0;
			}
			tmp = std::this_thread::get_id();
			return 1;
		}

		template <class _Clock, class _Duration>
		bool try_lock_until(const std::chrono::time_point<_Clock, _Duration>& _Abs_time) {
			while (lock_flag.test_and_set(std::memory_order_acquire)) {
				std::this_thread::sleep_for(std::chrono::microseconds(15));
				if (_Abs_time <= std::chrono::system_clock::now()) 
					return 0;
			}
			tmp = std::this_thread::get_id();
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
	class recursive_lock;

	template<>
	class recursive_lock<shadow_spin_lock> {
		uint32_t counter = 0;
		shadow_spin_lock locker;
		recursive_lock(const recursive_lock&) = delete;
		recursive_lock(recursive_lock&) = delete;
	public:
		recursive_lock() {}

		void lock() noexcept {
			if (locker.locker_id() == std::this_thread::get_id()) {
				counter++;
			}
			else {
				locker.lock();
				counter++;
			}
		}
		bool try_lock() noexcept {
			if (locker.locker_id() == std::this_thread::get_id()) 
				counter++;
			else if(locker.try_lock()) 
				counter++;
			return counter;
		}
		template <class _Rep, class _Period>
		bool try_lock_for(const std::chrono::duration<_Rep, _Period>& _Rel_time) {
			if (locker.locker_id() == std::this_thread::get_id())
				counter++;
			else if (locker.try_lock_for(_Rel_time))
				counter++;
			return counter;
		}

		template <class _Clock, class _Duration>
		bool try_lock_until(const std::chrono::time_point<_Clock, _Duration>& _Abs_time) {
			if (locker.locker_id() == std::this_thread::get_id())
				counter++;
			else if (locker.try_lock_until(_Abs_time))
				counter++;
			return counter;
		}


		void unlock() {
			if (locker.locker_id() == std::this_thread::get_id()) {
				if (counter)
					if (!--counter)
						locker.unlock();
			}
			else
				throw std::exception("Dissallowed unlock");
		}
		inline std::thread::id locker_id() {
			return locker.locker_id();
		}


	};

	template<class T>
	class rail_lock;
	template<>
	class rail_lock<shadow_spin_lock> {
		shadow_spin_lock& locker;
	public:
		rail_lock(const rail_lock&) = delete;
		rail_lock(rail_lock&) = delete;
		rail_lock(const shadow_spin_lock& lock) : locker(const_cast<shadow_spin_lock&>(lock)) {
			locker.lock();
		}
		~rail_lock() {
			locker.unlock();
		}
	};


}
#endif