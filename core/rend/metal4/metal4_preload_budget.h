// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace metal_preload {
// Accounts admitted speculative GPU images, including adopted/in-flight ones.
// On-demand texture-cache allocations have their existing lifetime/policy.
class Budget : public std::enable_shared_from_this<Budget> {
public:
	struct Usage { uint64_t bytes, images; };
	class Ticket {
		friend class Budget;
		std::shared_ptr<Budget> owner;
		uint64_t bytes;
		Ticket(std::shared_ptr<Budget> owner, uint64_t bytes) : owner(std::move(owner)), bytes(bytes) {}
	public:
		Ticket(const Ticket&) = delete;
		Ticket& operator=(const Ticket&) = delete;
		~Ticket() {
			std::lock_guard<std::mutex> lock(owner->mutex);
			owner->usedBytes -= bytes; --owner->usedImages;
		}
		bool resize(uint64_t actualBytes) {
			std::lock_guard<std::mutex> lock(owner->mutex);
			const uint64_t other = owner->usedBytes - bytes;
			if (!actualBytes || actualBytes > owner->byteLimit - other) return false;
			owner->usedBytes = other + actualBytes; bytes = actualBytes; return true;
		}
	};
	const uint64_t byteLimit, imageLimit;
	Budget(uint64_t byteLimit, uint64_t imageLimit) : byteLimit(byteLimit), imageLimit(imageLimit) {}
	std::shared_ptr<Ticket> reserve(uint64_t bytes) {
		auto owner = shared_from_this();
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (!bytes || usedImages == imageLimit || bytes > byteLimit - usedBytes) return {};
			usedBytes += bytes; ++usedImages;
		}
		Ticket *ticket;
		try { ticket = new Ticket(std::move(owner),bytes); }
		catch (...) {
			std::lock_guard<std::mutex> lock(mutex);
			usedBytes -= bytes; --usedImages; throw;
		}
		// Control-block allocation failure deletes Ticket and releases its
		// charge. No budget lock is held while either allocation can fail.
		return std::shared_ptr<Ticket>(ticket);
	}
	Usage usage() const { std::lock_guard<std::mutex> lock(mutex); return {usedBytes,usedImages}; }
private:
	mutable std::mutex mutex;
	uint64_t usedBytes = 0, usedImages = 0;
};
}
