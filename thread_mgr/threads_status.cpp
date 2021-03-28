#include "threads_status.hpp"

namespace tmgr {
	thread_local thread_context_t* current_context = nullptr;
}