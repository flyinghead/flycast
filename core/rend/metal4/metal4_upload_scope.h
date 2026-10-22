// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <exception>

namespace metal_upload {
// Normal finalization may report a submission failure to the caller. During
// exception unwinding only cancel; never submit partial work or throw twice.
template<class Context> class Scope {
	Context *context;
	const int exceptions=std::uncaught_exceptions();
	bool deferForRender;
public:
	explicit Scope(Context *context,bool deferForRender=false):context(context),deferForRender(deferForRender)
		{ context->beginTextureUpdates(); }
	Scope(const Scope&)=delete;
	Scope& operator=(const Scope&)=delete;
	~Scope() noexcept(false) {
		if(std::uncaught_exceptions()>exceptions)context->cancelTextureUpdates();
		else context->endTextureUpdates(deferForRender);
	}
};
}
