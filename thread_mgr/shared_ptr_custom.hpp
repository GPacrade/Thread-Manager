#pragma once
#ifndef DEF_ONCE_THREAD_MGR_SHARED_PTR_CUSTOM_HPP
#define DEF_ONCE_THREAD_MGR_SHARED_PTR_CUSTOM_HPP
#include <atomic>
namespace tmgr {
	template<class T>
	class shared_ptr_custom {
		T* ptr = nullptr;
		std::atomic_size_t* total = nullptr;
		void exit() {
			if (total) {
				if (*total > 1) (*total)--;
				else {
					if (ptr)delete ptr;
					ptr = nullptr;
					delete total;
					total = nullptr;
				}
			}
		}
		void join(std::atomic_size_t*& p_total_links) {
			if (total) exit();
			if (p_total_links) {
				(*p_total_links)++;
				total = p_total_links;
			}
		}
	public:
		shared_ptr_custom() {}
		shared_ptr_custom(T* copy) {
			*this = copy;
		}
		shared_ptr_custom(const shared_ptr_custom& copy) {
			*this = *const_cast<shared_ptr_custom*>(&copy);
		}
		shared_ptr_custom& operator=(T* copy) {
			if (ptr == copy)return *this;
			exit();
			ptr = copy;
			total = new std::atomic_size_t{ 1 };
			return *this;
		}
		shared_ptr_custom& operator=(const shared_ptr_custom& copy) {
			*this = const_cast<shared_ptr_custom&>(copy);
		}

		shared_ptr_custom& operator=(shared_ptr_custom& copy) {
			if (this == &copy)return *this;
			join(copy.total);
			ptr = copy.ptr;
			return *this;
		}
		T* get() {
			return ptr;
		}

	};
}
#endif