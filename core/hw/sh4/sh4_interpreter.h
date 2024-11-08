#pragma once
#include "types.h"
#include "sh4_cycles.h"

class Sh4Interpreter : public Sh4Executor
{
public:
	void Run() override;
	void ResetCache() override  {}
	void Start() override;
	void Stop() override;
	void Step() override;
	void Reset(bool hard) override;
	void Init() override;
	void Term() override;
	bool IsCpuRunning() override;
	void ExecuteDelayslot();
	void ExecuteDelayslot_RTE();
	Sh4Context *getContext() { return ctx; }

	static Sh4Interpreter *Instance;

protected:
	Sh4Context *ctx = nullptr;

private:
	void ExecuteOpcode(u16 op);
	u16 ReadNexOp();

	Sh4Cycles sh4cycles{CPU_RATIO};
	// SH4 underclock factor when using the interpreter so that it's somewhat usable
#ifdef STRICT_MODE
	static constexpr int CPU_RATIO = 1;
#else
	static constexpr int CPU_RATIO = 8;
#endif
};
