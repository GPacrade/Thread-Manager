#pragma once
#ifndef DEF_ONCE_THREAD_MGR_THREADS_HPP
#define DEF_ONCE_THREAD_MGR_THREADS_HPP
#include "threads_status.hpp"
#include "spin_lock.hpp"
#include <list>


namespace tmgr {
	class thread_object_t {
		friend class broadcast_event_class<thread_object_t>;


		bool destructed : 1;
		bool async_result : 1;
		std::atomic_flag wait_async_the_thread_result;
		thread_context_t* thread_handle;
		shadow_spin_lock async_result_lock;
		broadcast_event_class<thread_object_t> tmp = broadcast_event_class<thread_object_t>(this);
		std::list<shared_ptr_custom<void>> result_list;


		void handle() {
			if (thread_handle->current_status == thread_status::end_of_life)
				destructed = 1;
			if (async_result)
				if (thread_handle->current_status == thread_status::has_solve) {
					async_result_lock.lock();
					result_list.emplace_back(thread_handle->thread_result);
					async_result_lock.unlock();
					wait_async_the_thread_result.clear();
				}
		}

		void prepare_result() {
			if (destructed) throw thread_exception("Thread death");
			if (async_result) {
				current_context.broadcast_status(thread_status::wait);
				wait_async_the_thread_result.test_and_set(std::memory_order_acquire);
				while (wait_async_the_thread_result.test_and_set(std::memory_order_acquire)) {
					if (destructed) break;
					std::this_thread::sleep_for(std::chrono::microseconds(15));
				}
				current_context.broadcast_status(thread_status::work);
			}
			else result_list.emplace_back(get_result(*thread_handle));
		}
	public:
		thread_object_t() : thread_handle(nullptr) {
			destructed = 1;
			async_result = 0;
		}
		thread_object_t(thread_context_t& thread) : thread_handle(&thread) {
			async_result = 0;
			if (thread_handle) {
				thread_handle->sub_status(tmp);
				destructed = 0;
			}else destructed = 1;
		}

		thread_status getStatus() {
			if (!this)return thread_status::none;
			if (destructed) return thread_status::end_of_life;
			else return thread_handle->current_status;
		}

		inline void pause() noexcept {
			lock();
		}
		inline void continueTh() noexcept {
			unlock();
		}

		void lock() noexcept {
			if (!destructed) thread_handle->is_pause.test_and_set(std::memory_order_acquire);
		}
		void unlock() noexcept {
			if (!destructed) thread_handle->is_pause.clear();
		}

		void wait(thread_status need_status_type) {
			if (destructed)return;
			std::atomic_flag lock_flag;
			lock_flag.test_and_set(std::memory_order_acquire);

			broadcast_event_lambda tmpy([&]() {
				if (thread_handle->current_status == thread_status::end_of_life)
					destructed = 1;
				if (thread_handle->current_status == need_status_type || thread_handle->current_status == thread_status::end_of_life)
					lock_flag.clear();
				}
			);
			if (!destructed)thread_handle->sub_status(tmpy);
			while (lock_flag.test_and_set(std::memory_order_acquire)) {
				current_context.broadcast_status(thread_status::wait);
				if (destructed) break;
				std::this_thread::sleep_for(std::chrono::microseconds(15));
			}
			current_context.broadcast_status(thread_status::work);
			if (!destructed)thread_handle->unsub_status(tmpy);
			else if (need_status_type != thread_status::end_of_life) throw thread_exception("Thread death");
		}

		void subResultHandle(bool swithcer) {
			async_result_lock.lock();
			async_result = swithcer;
			async_result_lock.unlock();
		}

		void getResult() {
			prepare_result();
		}
		void tryGetResult() {
			if(!destructed)
				prepare_result();
		}

		//	just read first result
		template<class T>
		T getNewResult() {
			prepare_result();
			rail_lock rail_locker(async_result_lock);
			return *(T*)result_list.front().get();
		}

		//	read and remove first result
		template<class T>
		T takeNewResult() {
			prepare_result();
			async_result_lock.lock();
			auto tmp = result_list.front();
			result_list.pop_front();
			async_result_lock.unlock();
			return *(T*)tmp.get();
		}
		template<class T>
		T takeResult() {
			async_result_lock.lock();
			if (!result_list.size()) {
				async_result_lock.unlock();
				prepare_result();
				async_result_lock.lock();
			}
			auto tmp = result_list.front();
			result_list.pop_front();
			async_result_lock.unlock();
			return *(T*)tmp.get();
		}

		void clearResult() {
			result_list.clear();
		}

		void abort() {
			thread_handle->abort = 1;
		}


		bool isDeath() const {
			return destructed;
		}
		bool hasResult() const {
			return result_list.size();
		}
		std::thread::id getID() const {
			if (destructed) throw thread_exception("Thread death");
			return thread_handle->current;
		}
		bool resultIsAsync() const {
			return async_result;
		}

		~thread_object_t() {
			if (!destructed)
				thread_handle->unsub_status(tmp);
		}
	};
}
#endif