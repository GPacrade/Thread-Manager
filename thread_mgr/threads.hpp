#pragma once
#ifndef DEF_ONCE_THREAD_MGR_THREADS_HPP
#define DEF_ONCE_THREAD_MGR_THREADS_HPP
#include "threads_status.hpp"
#include "spin_lock.hpp"
#include <list>

namespace tmgr {
	class thread_object_t {
		friend class broadcast_event_class<thread_object_t>;


		bool async_result : 1;
		std::atomic_bool destructed;
		std::atomic_flag wait_async_the_thread_result;
		thread_context_t* thread_handle;
		shadow_spin_lock async_result_lock;
		broadcast_event_class<thread_object_t> tmp = broadcast_event_class<thread_object_t>(this);
		std::list<shared_ptr_custom<void>> result_list;


		void handle() {
			destructed = current_context->current_status == thread_status::end_of_life;

			if (async_result)
				if (current_context->current_status == thread_status::has_solve) {
					async_result_lock.lock();
					result_list.emplace_back(current_context->thread_result);
					async_result_lock.unlock();
					wait_async_the_thread_result.clear();
				}
		}

		void prepare_result() {
			if (destructed) throw thread_exception("Thread death");
			if (async_result) {
				if (current_context)
					current_context->broadcast_status(thread_status::wait);
				wait_async_the_thread_result.test_and_set(std::memory_order_acquire);
				while (wait_async_the_thread_result.test_and_set(std::memory_order_acquire)) {
					if (destructed) break;
					std::this_thread::sleep_for(std::chrono::microseconds(15));
				}
				if (current_context)
					current_context->broadcast_status(thread_status::work);
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
			}else destructed = 1;
		}


		thread_object_t& operator=(thread_object_t& thread) {
			if (!destructed) 
				thread_handle->unsub_status(tmp);
			

			async_result = thread.async_result;

			destructed.store(thread.destructed);

			wait_async_the_thread_result.clear();
			if(thread.wait_async_the_thread_result.test())
				wait_async_the_thread_result.test_and_set(std::memory_order_acquire);

			thread_handle = thread.thread_handle;

			async_result_lock = thread.async_result_lock;


			if (!destructed)
				thread_handle->sub_status(tmp);

			result_list.clear();
			for (shared_ptr_custom<void>& inter : thread.result_list)
				result_list.push_back(inter);
			return *this;
		}

		inline thread_object_t& operator=(thread_object_t&& thread) {
			return *this = thread;
		}
		inline thread_object_t& operator=(thread_context_t& thread) {
			return *this = thread_object_t(thread);
		}

		thread_status getStatus() {
			if (!this)return thread_status::none;
			if (destructed) return thread_status::end_of_life;
			else return thread_handle->get_status();
		}
		
		inline void pause() {
			if (!destructed) thread_handle->lock();
		}
		inline void continueTh() {
			if (!destructed) thread_handle->unlock();
		}
		inline void lock() {
			if (!destructed) thread_handle->lock();
		}
		inline void unlock() {
			if (!destructed) thread_handle->unlock();
		}
		
		void wait(thread_status need_status_type) {
			if (destructed)return;
			std::atomic_flag lock_flag;
			lock_flag.test_and_set(std::memory_order_acquire);


			//======================
			//for self unsub
			void* tmpy;



			struct {
				std::atomic_flag* lock_flag;
				void*& tmpy;
				thread_status need_status_type;
			} link(&lock_flag, tmpy, need_status_type);

			broadcast_event_function tempy([&](auto tmp) {
					if (!current_context)
						return;
					if (current_context->current_status == tmp.need_status_type || current_context->current_status == thread_status::end_of_life) {
						current_context->unsub_status(*(broadcast_event*)tmp.tmpy);
						tmp.lock_flag->clear();
					}
				}
				, link
			);
			tmpy = &tempy;

			//======================
			if (!destructed)
				thread_handle->sub_status(*(broadcast_event*)tmpy);

			if (current_context)
				current_context->broadcast_status(thread_status::wait);

			while (lock_flag.test(std::memory_order_acquire)) 
				std::this_thread::sleep_for(std::chrono::microseconds(15));

			if (current_context)
				current_context->broadcast_status(thread_status::work);

			else if (need_status_type != thread_status::end_of_life)
				throw thread_exception("Thread death");
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


		template<class T>
		T getNewResult() {
			prepare_result();
			rail_lock rail_locker(async_result_lock);
			return *(T*)result_list.front().get();
		}


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
			if(!destructed)
				thread_handle->_abort = 1;
		}

		bool isDeath() const {
			return destructed;
		}
		bool hasResult() const {
			return result_list.size();
		}
		std::thread::id getID() const {
			if (destructed) throw thread_exception("Thread death");
			return thread_handle->get_id();
		}
		bool resultIsAsync() const {
			return async_result;
		}
		thread_context_t& get_context() const{
			if(destructed) 
				throw thread_exception("Thread destructed");
			return *thread_handle;
		}
		
		
		
		
		
		
		
		~thread_object_t() {
			if (!destructed) 
				thread_handle->unsub_status(tmp);
		}
	};

	template<class EX_T>
	class thread_exception_handler {
		bool destructed = 0;
		void (*_f)(EX_T);
		thread_context_t* thread_handle;
		friend class broadcast_event_class<thread_exception_handler>;
		broadcast_event_class<thread_exception_handler> tmp = broadcast_event_class<thread_exception_handler>(this);
		void handle() {
			if (thread_handle->get_status() == thread_status::end_of_life)
				destructed = 1;
			if (thread_handle->get_status() == thread_status::exception) {
				try {
					std::rethrow_exception(thread_handle->get_exception());
				}
				catch(EX_T ex){
					_f(ex);
					thread_handle->exception_catch();
				}
				catch(...){}
			}
		}
	public:
		thread_exception_handler(thread_context_t& thread,void (*_function)(EX_T)) : thread_handle(&thread) {
			if (thread_handle) {
				thread_handle->sub_status(tmp);
				destructed = 0;
			}
			else destructed = 1;
			_f = _function;
		}
		~thread_exception_handler() {
			if (!destructed)
				thread_handle->unsub_status(tmp);
		}

	};

}
#endif