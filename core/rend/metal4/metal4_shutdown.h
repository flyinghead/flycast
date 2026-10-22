// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <exception>
#include <utility>

namespace metal_shutdown {
// Keep attempting independent cancellation/submission/cleanup operations.
// The owner must drain all previously submitted work before rethrowing.
class Drain {
	std::exception_ptr failure;
public:
	template<class Operation> void attempt(Operation&& operation) noexcept {
		try { std::forward<Operation>(operation)(); }
		catch(...) { if(!failure)failure=std::current_exception(); }
	}
	bool failed() const noexcept { return failure!=nullptr; }
	void rethrow() const { if(failure)std::rethrow_exception(failure); }
};
}
