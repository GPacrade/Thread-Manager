#pragma once
#ifndef DEF_ONCE_THREAD_MGR_THREADS_STATUS_HPP
#define DEF_ONCE_THREAD_MGR_THREADS_STATUS_HPP
#include <atomic>
#include <list>
#include <chrono>
#include <thread>
#include "shadow_spin_lock.hpp"
#include "shared_ptr_custom.hpp"

namespace tmgr {

	enum class thread_status {
		none = 0,
		starting,
		work,
		sleep,
		wait,
		lock,
		end_of_life,
		has_solve,
		exception,
	};

	class thread_exception : public std::exception {
	public:
		thread_exception() : std::exception() {}
		thread_exception(std::string& str) : std::exception(str.c_str()) {}
		thread_exception(std::string str) : std::exception(str.c_str()) {}
		thread_exception(const char* str) : std::exception(str) {}
	};


	class broadcast_event {
	public:
		virtual void operator()() = 0;
		virtual bool operator==(broadcast_event&) = 0;
		virtual void* getVal() = 0;
	};

	template <class _Cl>
	class broadcast_event_class : public broadcast_event {
		_Cl* cl;
	public:
		broadcast_event_class(_Cl* _cl) noexcept : cl(_cl) {}
		void operator()() final {
			cl->handle();
		}
		bool operator==(broadcast_event& test) final {
			return test.getVal() == &cl;
		}
		void* getVal() final {
			return &cl;
		}
	};

	template <class _Fn>
	class broadcast_event_lambda : public broadcast_event {
		_Fn* _f;
	public:
		broadcast_event_lambda(_Fn& _Fx) noexcept : _f(&_Fx) {
		}
		broadcast_event_lambda(_Fn _Fx) noexcept : _f(&_Fx) {
		}
		void operator()() final {
			(*_f)();
		}
		bool operator==(broadcast_event& test) final {
			return test.getVal() == &_f;
		}
		void* getVal() final {
			return &_f;
		}
	};




	namespace this_thread {
		inline void wanna_execute();
		inline void has_solve(void* solve_ptr);
		template <class _Rep, class _Period>
		void sleep_for(const std::chrono::duration<_Rep, _Period>& _Rel_time);
		template <class _Clock, class _Duration>
		void sleep_until(const std::chrono::time_point<_Clock, _Duration>& _Abs_time);
	}


	class thread_context_t {
		friend class thread_object_t;
		friend class spin_lock;

		template <class _Rep, class _Period>
		friend void this_thread::sleep_for(const std::chrono::duration<_Rep, _Period>& _Rel_time);
		template <class _Clock, class _Duration>
		friend void sleep_until(const std::chrono::time_point<_Clock, _Duration>& _Abs_time);

		friend void this_thread::wanna_execute();
		friend void this_thread::has_solve(void*);
		friend shared_ptr_custom<void> get_result(thread_context_t& context);

		template<class FN>
		friend thread_context_t& create_thread(FN run_f);
		template<class FN>
		friend thread_context_t& create_locked_thread(FN run_f);


		template<class FN>
		static void start_new_thread(thread_context_t** creat, FN run_f);
		template<class FN>
		static void start_new_locked_thread(thread_context_t** creat, FN run_f);


		class thread_abort {
		public:
			thread_abort() {}
		};


		void broadcast_status(thread_status status) {
			if (status == current_status) return;
			handlers_mutex.lock();
			current_status = status;
			for (auto inter : stat_handle)
				(*inter)();
			handlers_mutex.unlock();
		}


		bool abort:1;
		bool exception_catched : 1;
		thread_status current_status : 6;
		std::thread::id current = std::this_thread::get_id();
		shadow_spin_lock handlers_mutex;
		shared_ptr_custom<void> thread_result = nullptr;
		std::list<broadcast_event*> stat_handle;
		std::exception_ptr exception_memory;

		std::atomic_flag is_pause;
	public:
		thread_context_t() noexcept : abort(0), exception_catched(0), current_status(thread_status::starting) {}
		~thread_context_t() {
			broadcast_status(thread_status::end_of_life);
		}
		void sub_status(broadcast_event&& status_sub) {
			sub_status(status_sub);
		}
		void sub_status(broadcast_event& status_sub) {
			sizeof(std::list<broadcast_event*>);
			if (!this)return;
			handlers_mutex.lock();
			stat_handle.push_back(&status_sub);
			handlers_mutex.unlock();
		}
		void unsub_status(broadcast_event&& status_unsub) {
			unsub_status(status_unsub);
		}
		void unsub_status(broadcast_event& status_unsub) {
			if (!this)return;
			handlers_mutex.lock();
			stat_handle.remove_if(
				[&status_unsub](broadcast_event*& cmp) {return cmp == &status_unsub; }
			);
			handlers_mutex.unlock();
		}
		thread_status get_status() const {
			return current_status;
		}
		std::thread::id get_id() const {
			return current;
		}
		std::exception_ptr get_exception() {
			return exception_memory;
		}
		void exception_catch() {
			if (exception_memory) {
				exception_catched = 1;
			}
			else throw std::invalid_argument("Exception not exist");
		}
	};


	extern thread_local thread_context_t *current_context;


	namespace this_thread {
		inline void wanna_execute() {
			if (current_context->abort)
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
			current_context->thread_result = solve_ptr;
			current_context->broadcast_status(thread_status::has_solve);
			if (current_context->abort)
				throw thread_context_t::thread_abort();
			wanna_execute();
		}

		template <class _Rep, class _Period>
		void sleep_for(const std::chrono::duration<_Rep, _Period>& _Rel_time){
			if (current_context->abort)
				throw thread_context_t::thread_abort();
			current_context->broadcast_status(thread_status::sleep);
			std::this_thread::sleep_for(_Rel_time);
			wanna_execute();
		}

		template <class _Clock, class _Duration>
		void sleep_until(const std::chrono::time_point<_Clock, _Duration>& _Abs_time){
			if (current_context->abort)
				throw thread_context_t::thread_abort();
			current_context->broadcast_status(thread_status::sleep);
			std::this_thread::sleep_until(_Abs_time);
			wanna_execute();
		}

		inline std::thread::id get_id() noexcept {
			return std::this_thread::get_id();
		}
	}


	template<class FN>
	static void thread_context_t::start_new_thread(thread_context_t** creat, FN run_f) {
		*creat = current_context = new thread_context_t();
		try {
			run_f();
		}
		catch (const thread_context_t::thread_abort&) {}
		catch (...) {
			current_context->exception_memory = std::current_exception();
			current_context->broadcast_status(thread_status::exception);
			if (!current_context->exception_catched)exit(3);
		}
		delete current_context;
	}
	template<class FN>
	static void thread_context_t::start_new_locked_thread(thread_context_t** creat, FN run_f) {
		*creat = current_context = new thread_context_t();
		try {
			current_context->is_pause.test_and_set(std::memory_order_acquire);
			this_thread::wanna_execute();
			run_f();
		}
		catch (const thread_context_t::thread_abort&) {}
		catch (...) {
			current_context->exception_memory = std::current_exception();
			current_context->broadcast_status(thread_status::exception);
			if (!current_context->exception_catched)exit(3);
		}
		delete current_context;
	}



	template<class FN>
	thread_context_t& create_thread(FN run_f) {
		thread_context_t* tmp = nullptr;
		std::thread createaaaaa(
			thread_context_t::start_new_thread<FN>,
			&tmp,
			run_f
		);
		createaaaaa.detach();
		int i = 0;
		while (!tmp) {
			if (i++ == 1500)throw thread_exception("fail get thread context");
			std::this_thread::sleep_for(std::chrono::microseconds(15));
		}
		return *tmp;
	}
	template<class FN>
	thread_context_t& create_locked_thread(FN run_f) {
		thread_context_t* tmp = nullptr;
		std::thread createaaaaa(
			thread_context_t::start_new_locked_thread<FN>,
			&tmp,
			run_f
		);
		createaaaaa.detach();
		int i = 0;
		while (!tmp) {
			if (i++ == 1500)throw thread_exception("fail get thread context");
			std::this_thread::sleep_for(std::chrono::microseconds(15));
		}
		return *tmp;
	}

	inline shared_ptr_custom<void> get_result(thread_context_t& context) {
		if (&context == current_context) throw thread_exception("Self lock");
		bool destructed = 0;
		std::atomic_flag lock_flag;

		lock_flag.test_and_set(std::memory_order_acquire);
		broadcast_event_lambda tmpy([&]() {
			if (context.current_status == thread_status::end_of_life)
				destructed = 1;
			if (context.current_status == thread_status::has_solve || context.current_status == thread_status::end_of_life)
				lock_flag.clear();
			std::this_thread::sleep_for(std::chrono::microseconds(30));
		});

		context.sub_status(tmpy);
		current_context->broadcast_status(thread_status::wait);
		while (lock_flag.test_and_set(std::memory_order_acquire)) {
			if (destructed) break;
			std::this_thread::sleep_for(std::chrono::microseconds(15));
		}
		auto tmp = context.thread_result;
		context.unsub_status(tmpy);
		current_context->broadcast_status(thread_status::work);
		return tmp;
	}
}
#endif