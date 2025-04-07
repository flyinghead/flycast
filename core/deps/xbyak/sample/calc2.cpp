/*
	@author herumi

	tiny calculator 2
	This program generates a function to calc the value of
	polynomial given by user in run-time.
	use boost::spirit::qi
*/
#ifdef _WIN32
	#pragma warning(disable : 4127) // for boost(constant condition)
	#pragma warning(disable : 4512) // for boost
	#pragma warning(disable : 4819)
#endif
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_container.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/timer.hpp>

#include <stdio.h>
#include <assert.h>
#include <string>
#include <vector>
#include "xbyak/xbyak.h"

enum Operand {
	OpAdd,
	OpSub,
	OpMul,
	OpDiv,
	OpNeg,
	OpImm,
	OpVarX
};

struct Code {
	Operand op_;
	double val_;
	Code(Operand op)
		: op_(op)
		, val_(0)
	{
	}
	Code(double val)
		: op_(OpImm)
		, val_(val)
	{
	}
};

typedef std::vector<Code> CodeSet;

struct Vm {
	CodeSet code_;
	double operator()(double x) const
	{
		const size_t maxStack = 16;
		double stack[maxStack];
		double *p = stack;
		CodeSet::const_iterator pc = code_.begin();

		while (pc != code_.end()) {
			switch (pc->op_) {
			case OpVarX:
				*p++ = x;
				break;
			case OpImm:
				*p++ = pc->val_;
				break;
			case OpNeg:
				p[-1] = -p[-1];
				break;
			case OpAdd:
				--p;
				p[-1] += p[0];
				break;
			case OpSub:
				--p;
				p[-1] -= p[0];
				break;
			case OpMul:
				--p;
				p[-1] *= p[0];
				break;
			case OpDiv:
				--p;
				p[-1] /= p[0];
				break;
			}
			++pc;
			assert(p < stack + maxStack);
		}
		return p[-1];
	}
};

class Jit : public Xbyak::CodeGenerator {
private:
	enum {
		MAX_CONST_NUM = 32
	};
	MIE_ALIGN(16) double constTbl_[MAX_CONST_NUM];
	Xbyak::uint64_t negConst_;
	size_t constTblPos_;
#ifdef XBYAK32
	const Xbyak::Reg32& varTbl_;
	const Xbyak::Reg32& tbl_;
#else
	const Xbyak::Reg64& tbl_;
#endif
	int regIdx_;
public:
	/*
		double jit(double x);
		@note 32bit: x : [esp+4], return fp0
		      64bit: x [rcx](win), xmm0(gcc), return xmm0
	*/
	Jit()
		: negConst_(Xbyak::uint64_t(1) << 63)
		, constTblPos_(0)
#ifdef XBYAK32
		, varTbl_(eax)
		, tbl_(edx)
#elif defined(XBYAK64_WIN)
		, tbl_(rcx)
#else
		, tbl_(rdi)
#endif
		, regIdx_(-1)
	{
#ifdef XBYAK32
		lea(varTbl_, ptr [esp+4]);
#else
#ifdef XBYAK64_WIN
		movaps(ptr [rsp + 8], xm6); // save xm6, xm7
		movaps(ptr [rsp + 8 + 16], xm7);
#endif
		movaps(xm7, xm0); // save xm0
#endif
		mov(tbl_, (size_t)constTbl_);
	}
	void genPush(double n)
	{
		if (constTblPos_ >= MAX_CONST_NUM) throw;
		constTbl_[constTblPos_] = n;
		if (regIdx_ == 7) throw;
		movsd(Xbyak::Xmm(++regIdx_), ptr[tbl_ + constTblPos_ * sizeof(double)]);
		constTblPos_++;
	}
	void genVarX()
	{
#ifdef XBYAK32
		if (regIdx_ == 7) throw;
		movsd(Xbyak::Xmm(++regIdx_), ptr[varTbl_]);
#else
		if (regIdx_ == 6) throw;
		movsd(Xbyak::Xmm(++regIdx_), xm7);
#endif
	}
	void genAdd()
	{
		addsd(Xbyak::Xmm(regIdx_ - 1), Xbyak::Xmm(regIdx_)); regIdx_--;
	}
	void genSub()
	{
		subsd(Xbyak::Xmm(regIdx_ - 1), Xbyak::Xmm(regIdx_)); regIdx_--;
	}
	void genMul()
	{
		mulsd(Xbyak::Xmm(regIdx_ - 1), Xbyak::Xmm(regIdx_)); regIdx_--;
	}
	void genDiv()
	{
		divsd(Xbyak::Xmm(regIdx_ - 1), Xbyak::Xmm(regIdx_)); regIdx_--;
	}
	void genNeg()
	{
		xorpd(Xbyak::Xmm(regIdx_), ptr [tbl_ + MAX_CONST_NUM * sizeof(double)]);
	}
	void complete()
	{
#ifdef XBYAK32
		sub(esp, 8);
		movsd(ptr [esp], xm0);
		fld(qword [esp]);
		add(esp, 8);
#else
#ifdef XBYAK64_WIN
		movaps(xm6, ptr [rsp + 8]);
		movaps(xm7, ptr [rsp + 8 + 16]);
#endif
#endif
		ret();
	}
};

template<typename Iterator>
struct Parser : boost::spirit::qi::grammar<Iterator, boost::spirit::ascii::space_type> {
	boost::spirit::qi::rule<Iterator, boost::spirit::ascii::space_type> expression, term, factor;
	CodeSet& code_;
	Parser(CodeSet& code)
		: Parser::base_type(expression)
		, code_(code)
	{
		namespace qi = boost::spirit::qi;
		using namespace qi::labels;

		using boost::phoenix::ref;
		using boost::phoenix::push_back;

		expression = term >> *(('+' > term[push_back(ref(code_), OpAdd)])
							 | ('-' > term[push_back(ref(code_), OpSub)]));

		term = factor >> *(('*' > factor[push_back(ref(code_), OpMul)])
						 | ('/' > factor[push_back(ref(code_), OpDiv)]));

		factor = qi::double_[push_back(ref(code_), _1)]
				| qi::lit('x')[push_back(ref(code_), OpVarX)]
				| ('(' > expression > ')')
				| ('-' > factor[push_back(ref(code_), OpNeg)])
				| ('+' > factor);
	}
};

template<typename Iterator>
struct ParserJit : boost::spirit::qi::grammar<Iterator, boost::spirit::ascii::space_type> {
	boost::spirit::qi::rule<Iterator, boost::spirit::ascii::space_type> expression, term, factor;
	Jit code_;
	ParserJit()
		: ParserJit::base_type(expression)
	{
		namespace qi = boost::spirit::qi;
		using namespace qi::labels;

		using boost::phoenix::ref;
		using boost::phoenix::push_back;
		using boost::phoenix::bind;

		expression = term >> *(('+' > term[bind(&Jit::genAdd, ref(code_))])
							 | ('-' > term[bind(&Jit::genSub, ref(code_))]));

		term = factor >> *(('*' > factor[bind(&Jit::genMul, ref(code_))])
						 | ('/' > factor[bind(&Jit::genDiv, ref(code_))]));

		factor = qi::double_[bind(&Jit::genPush, ref(code_), _1)]
				| qi::lit('x')[bind(&Jit::genVarX, ref(code_))]
				| ('(' > expression > ')')
				| ('-' > factor[bind(&Jit::genNeg, ref(code_))])
				| ('+' > factor);
	}
};

template<class Func>
void Test(const char *msg, const Func& f)
{
	printf("%s:", msg);
	boost::timer t;
	double sum = 0;
	for (double x = 0; x < 1000; x += 0.0001) {
		sum += f(x);
	}
	printf("sum=%f, %fsec\n", sum, t.elapsed());
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "input formula\n");
		return 1;
	}
	const std::string str(argv[1]);

	try {
		Vm vm;
		Parser<std::string::const_iterator> parser(vm.code_);
		ParserJit<std::string::const_iterator> parserJit;

		const std::string::const_iterator end = str.end();

		std::string::const_iterator i = str.begin();
		if (!phrase_parse(i, end, parser, boost::spirit::ascii::space) || i != end) {
			puts("err 1");
			return 1;
		}
		printf("ret=%f\n", vm(2.3));

		i = str.begin();
		if (!phrase_parse(i, end, parserJit, boost::spirit::ascii::space) || i != end) {
			puts("err 2");
			return 1;
		}
		parserJit.code_.complete();
		double (*jit)(double) = parserJit.code_.getCode<double (*)(double)>();

		Test("VM ", vm);
		Test("JIT", jit);
	} catch (...) {
		fprintf(stderr, "err\n");
	}
}
