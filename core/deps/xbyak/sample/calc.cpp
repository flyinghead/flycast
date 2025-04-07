/*
	@author herumi

	tiny calculator
	This program generates a function to calc the value of
	polynomial given by user in run-time.
	use boost::spirit::classic
	see calc2.cpp for new version of boost::spirit
*/
#include <stdio.h>
#include <sstream>
#include <map>
#include "xbyak/xbyak.h"
#ifdef _MSC_VER
	#pragma warning(disable : 4127) // for boost(constant condition)
	#pragma warning(disable : 4512) // for boost
#endif
#include <boost/spirit/include/classic_file_iterator.hpp>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/bind.hpp>

enum Error {
	UNDEFINED_VARIABLE = 1
};

/*
	JIT assemble of given polynomial for VC or gcc
*/
class FuncGen : public Xbyak::CodeGenerator {
public:
	typedef std::map<std::string, int> Map;
private:
	enum {
		MAX_CONST_NUM = 32
	};
	double constTbl_[MAX_CONST_NUM];
	size_t constTblPos_;
	int regIdx_;
	Map varMap_; // map var name to index
#ifdef XBYAK32
	const Xbyak::Reg32& valTbl_;
	const Xbyak::Reg32& tbl_;
#else
	const Xbyak::Reg64& valTbl_;
	const Xbyak::Reg64& tbl_;
#endif
public:
	/*
		@param y [out] the value of f(var)
		@param var [in] table of input variables
		func(double *y, const double var[]);
		@note func does not return double to avoid difference of compiler
	*/
	FuncGen(const std::vector<std::string>& varTbl)
		: constTblPos_(0)
		, regIdx_(-1)
#ifdef XBYAK32
		, valTbl_(eax)
		, tbl_(edx)
#elif defined(XBYAK64_WIN)
		, valTbl_(rcx)
		, tbl_(rdx)
#else
		, valTbl_(rdi)
		, tbl_(rsi)
#endif
	{
#ifdef XBYAK32
		mov(valTbl_, ptr[esp+8]); // eax == varTbl
		mov(tbl_, (size_t)constTbl_);
#else
#ifdef XBYAK64_WIN
		movaps(ptr [rsp + 8], xm6); // save xm6, xm7
		movaps(ptr [rsp + 8 + 16], xm7);
#endif
		mov(tbl_, (size_t)constTbl_);
#endif
		for (int i = 0, n = static_cast<int>(varTbl.size()); i < n; i++) {
			varMap_[varTbl[i]] = i;
		}
	}
	// use edx
	void genPush(double n)
	{
		if (constTblPos_ >= MAX_CONST_NUM) throw;
		constTbl_[constTblPos_] = n;
		if (regIdx_ == 7) throw;
		movsd(Xbyak::Xmm(++regIdx_), ptr[tbl_ + (int)(constTblPos_ * sizeof(double))]);
		constTblPos_++;
	}
	// use eax
	void genVal(const char *begin, const char *end)
	{
		std::string var(begin, end);
		if (varMap_.find(var) == varMap_.end()) throw UNDEFINED_VARIABLE;
		if (regIdx_ == 7) throw;
		movsd(Xbyak::Xmm(++regIdx_), ptr[valTbl_ + varMap_[var] * sizeof(double)]);
	}
	void genAdd(const char*, const char*)
	{
		addsd(Xbyak::Xmm(regIdx_ - 1), Xbyak::Xmm(regIdx_)); regIdx_--;
	}
	void genSub(const char*, const char*)
	{
		subsd(Xbyak::Xmm(regIdx_ - 1), Xbyak::Xmm(regIdx_)); regIdx_--;
	}
	void genMul(const char*, const char*)
	{
		mulsd(Xbyak::Xmm(regIdx_ - 1), Xbyak::Xmm(regIdx_)); regIdx_--;
	}
	void genDiv(const char*, const char*)
	{
		divsd(Xbyak::Xmm(regIdx_ - 1), Xbyak::Xmm(regIdx_)); regIdx_--;
	}
	void complete()
	{
#ifdef XBYAK32
		mov(eax, ptr [esp + 4]); // eax = valTbl
		movsd(ptr [eax], xm0);
#else
#ifdef XBYAK64_WIN
		movaps(xm6, ptr [rsp + 8]);
		movaps(xm7, ptr [rsp + 8 + 16]);
#endif
#endif
		ret();
	}
};

struct Grammar : public boost::spirit::classic::grammar<Grammar> {
	FuncGen& f_;
	Grammar(FuncGen& f) : f_(f) { }
	template<typename ScannerT>
	struct definition {
		boost::spirit::classic::rule<ScannerT> poly0, poly1, poly2, var;

		definition(const Grammar& self)
		{
			using namespace boost;
			using namespace boost::spirit::classic;

			poly0 = poly1 >> *(('+' >> poly1)[bind(&FuncGen::genAdd, ref(self.f_), _1, _2)]
			                 | ('-' >> poly1)[bind(&FuncGen::genSub, ref(self.f_), _1, _2)]);
			poly1 = poly2 >> *(('*' >> poly2)[bind(&FuncGen::genMul, ref(self.f_), _1, _2)]
			                 | ('/' >> poly2)[bind(&FuncGen::genDiv, ref(self.f_), _1, _2)]);
			var = (+alpha_p)[bind(&FuncGen::genVal, ref(self.f_), _1, _2)];
			poly2 = real_p[bind(&FuncGen::genPush, ref(self.f_), _1)]
				| var
				| '(' >> poly0 >> ')';
		}
		const boost::spirit::classic::rule<ScannerT>& start() const { return poly0; }
	};
};

void put(const std::vector<double>& x)
{
	for (size_t i = 0, n = x.size(); i < n; i++) {
		if (i > 0) printf(", ");
		printf("%f", x[i]);
	}
}

int main(int argc, char *argv[])
{
	if (argc <= 2) {
		fprintf(stderr, "calc \"var1 var2 ...\" \"function of var\"\n");
		fprintf(stderr, "eg. calc x \"x*x\"\n");
		fprintf(stderr, "eg. calc \"x y z\"  \"x*x + y - z\"\n");
		return 1;
	}
	const char *poly = argv[2];
	try {
		std::vector<std::string> varTbl;

		// get varTbl from argv[1]
		{
			std::istringstream is(argv[1]);
			int i = 0;
			printf("varTbl = { ");
			while (is) {
				std::string var;
				is >> var;
				if (var.empty()) break;
				printf("%s:%d, ", var.c_str(), i);
				varTbl.push_back(var);
				i++;
			}
			printf("}\n");
		}
		FuncGen funcGen(varTbl);
		Grammar calc(funcGen);
		boost::spirit::classic::parse_info<> r = parse(poly, calc, boost::spirit::classic::space_p);
		if (!r.full) {
			printf("err poly=%s\n", poly);
			return 1;
		}
		funcGen.complete();
		std::vector<double> valTbl;
		valTbl.resize(varTbl.size());
#ifdef XBYAK32
		puts("32bit mode");
		void (*func)(double *ret, const double *valTbl) = funcGen.getCode<void (*)(double *, const double*)>();
#else
		puts("64bit mode");
		double (*func)(const double *valTbl) = funcGen.getCode<double (*)(const double*)>();
#endif
		for (int i = 0; i < 10; i++) {
			for (size_t j = 0, n = valTbl.size(); j < n; j++) {
				valTbl[j] = rand() % 7;
			}
			double y;
#ifdef XBYAK32
			func(&y, &valTbl[0]);
#else
			y = func(&valTbl[0]);
#endif
			printf("f("); put(valTbl); printf(")=%f\n", y);
		}
	} catch (std::exception& e) {
		printf("ERR:%s\n", e.what());
	} catch (Error err) {
		printf("ERR:%d\n", err);
	} catch (...) {
		printf("unknown error\n");
	}

	return 0;
}
