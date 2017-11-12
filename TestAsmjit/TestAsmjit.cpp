// TestAsmjit.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "AsmJit.h"
using namespace asmjit;

typedef int(*FUNC)();

void MinimalExample()
{
	JitRuntime rt;
	CodeHolder code;  //存储代码和重定位信息
	code.init(rt.getCodeInfo());//

	X86Assembler assm(&code); //创建汇编器，并绑定code
	assm.mov(x86::eax, 1); //mov eax, 1
	assm.ret(); //从函数返回
	// **** X86Assember 不在需要，可以被销毁和释放 

	FUNC pfn = 0;
	Error err = rt.add(&pfn, &code); //将产生的代码添加到runtime
	if (err)
	{
		return; //错误
	}
	// *** CodeHoder 不在需要，可以被销毁

	int result = pfn(); //执行代码
	printf("%d\r\n", result);

	rt.release(pfn);
}

/*
* 操作数
*/
void UsingOpransExample(X86Assembler& assm)
{
	// 创建操作数
	X86Gp dst = x86::ecx;
	X86Gp src = x86::rax;
	X86Gp idx = x86::gpq(10); //获取r10
	X86Mem m = x86::ptr(src, idx); //构造内存地址[src+idx] - 引用[rax+r10]

	//测试 m
	m.getIndexType(); //返回X86Reg::kRegGpq
	m.getIndexId(); //返回 10 （r10）

	//重新构造mem中的idx
	X86Gp idx_2 = X86Gp::fromTypeAndId(m.getIndexType(), m.getIndexId());
	idx == idx_2; //true, 完全相同

	Operand op = m;
	op.isMem(); //True,可以强转成mem和x86mem

	m == op; //true, op 是mm份拷贝
	static_cast<Mem&>(op).addOffset(1); //安全有效的
	m == op; //False, op指向[rax+r10+1], 而不是[rax+r10]

	//发射mov
	assm.mov(dst, m);//类型安全
	//assm.mov(dst, op); //没有这个重载

	assm.emit(X86Inst::kIdMov, dst, m); //可以使用，但是类型不安全
	assm.emit(X86Inst::kIdMov, dst, op);//也可以，emit是类型不安全的，并且可以动态使用
}

typedef int(*PFN_SUM)(int* arr, int nCount);

void AssemblerExample()
{

	JitRuntime rt;
	CodeHolder code;  //存储代码和重定位信息
	code.init(rt.getCodeInfo());//

	X86Assembler a(&code); //创建汇编器，并绑定code

	X86Gp arr, cnt;
	X86Gp sum = x86::eax; //eax保存和值

	arr = x86::edx; //保存数组指针
	cnt = x86::ecx; //保存计数器

	a.push(x86::ecx);
	a.push(x86::edx);

	a.mov(arr, x86::ptr(x86::esp, 12)); //取第一个参数
	a.mov(cnt, x86::ptr(x86::esp, 16)); //去第二个参数

	Label lblLoop = a.newLabel(); //为了构造循环，这里需要标识符
	Label lblExit = a.newLabel();

	a.xor_(sum, sum); //清零sum寄存器
	a.test(cnt, cnt); //边界检查
	a.jz(lblExit); // if cnt == 0 jmp Exit
	
	a.bind(lblLoop); //循环迭代的起始
	a.add(sum, x86::dword_ptr(arr)); //sum += [arr]
	a.add(arr, 4); //arr++
	a.dec(cnt); //cnt --
	a.jnz(lblLoop);// if cnt != 0 jmp Loop

	a.bind(lblExit); //退出边界
	a.pop(x86::edx);
	a.pop(x86::ecx);
	a.ret();//返回 sum(eax)


	PFN_SUM pfnSum = 0;
	Error err = rt.add(&pfnSum, &code); //将产生的代码添加到runtime
	if (err)
	{
		return; //错误
	}
	// *** CodeHoder 不在需要，可以被销毁
	int anArr[10] = {1,2,3,4,5,6,7,8,9,10};
	int result = pfnSum(anArr, 10); //执行代码
	printf("%d\r\n", result);
}

typedef void(*PFN_SUMINTS)(int* dst, const int* a, const int* b);
void RelocationExample()
{
	CodeHolder code;
	code.init(CodeInfo(ArchInfo::kTypeHost));//使用本地处理器
	X86Assembler a(&code);

	X86Gp dst = x86::eax;
	X86Gp src_a = x86::ecx;
	X86Gp src_b = x86::edx;
	a.mov(dst, x86::dword_ptr(x86::esp, 4)); //获取目的指针
	a.mov(src_a, x86::dword_ptr(x86::esp, 8)); //获取源指针
	a.mov(src_b, x86::dword_ptr(x86::esp, 12)); //获取源指针

	a.movdqu(x86::xmm0, x86::ptr(src_a)); //从[src_a]载入4个int到xmm0
	a.movdqu(x86::xmm1, x86::ptr(src_b));
	a.paddd(x86::xmm0, x86::xmm1); //相加
	a.movdqu(x86::ptr(dst), x86::xmm0);//存储到[dst]
	a.ret();

	size_t size = code.getCodeSize(); //获取代码的大小（最大）

	VMemMgr vm;
	void* p = vm.alloc(size);
	size_t ssre = code.relocate(p); //重定位并存储到p
	int  intA[4] = { 1, 2, 3, 4 };
	int intB[4] = { 5, 6 , 7, 8 };
	int intC[4];
	((PFN_SUMINTS)p)(intC, intA, intB);

}

typedef int(*PFN_TEST)();
void CompilerBasic()
{
	JitRuntime rt;

	CodeHolder code;
	code.init(CodeInfo(ArchInfo::kTypeHost));//使用本地处理器
	X86Compiler cc(&code);

	cc.addFunc(FuncSignature0<int>()); //函数原型： int fn(void);

	X86Gp vReg = cc.newGpd(); //创建一个32的寄存器
	cc.mov(vReg, 1); 
	cc.ret(vReg);

	cc.endFunc();//函数结束
	cc.finalize(); //将cc的内容进行汇编

	PFN_TEST pfnSum = 0;
	Error err = rt.add(&pfnSum, &code); //将产生的代码添加到runtime
	if (err)
	{
		return; //错误
	}
	// *** CodeHoder 不在需要，可以被销毁
	int anArr[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	int result = pfnSum(); //执行代码
	printf("%d\r\n", result);
}


typedef int(*PFN_MEMCPY)(uint32_t* dst, const uint32_t* src, size_t count);
void CompilerMemcpy()
{
	JitRuntime rt;

	CodeHolder code;
	code.init(CodeInfo(ArchInfo::kTypeHost));//使用本地处理器
	X86Compiler cc(&code);

	cc.addFunc(FuncSignature3<void, uint32_t*, uint32_t, size_t>());

	Label lblLoop = cc.newLabel();
	Label lblExit = cc.newLabel();

	X86Gp dst = cc.newIntPtr();
	X86Gp src = cc.newIntPtr();
	X86Gp cnt = cc.newIntPtr();


	cc.setArg(0, src);
	cc.setArg(1, dst);
	cc.setArg(2, cnt);

	cc.test(cnt, cnt);
	cc.jz(lblExit);
	cc.bind(lblLoop);

	X86Gp tmp = cc.newInt32();
	cc.mov(tmp, x86::dword_ptr(src));
	cc.mov(x86::dword_ptr(dst), tmp);

	cc.add(src, 4);
	cc.add(dst, 4);

	cc.dec(cnt);
	cc.jnz(lblLoop);

	cc.bind(lblExit);
	cc.endFunc();

	cc.finalize();

	PFN_MEMCPY pfnSum = 0;
	Error err = rt.add(&pfnSum, &code); //将产生的代码添加到runtime
	if (err)
	{
		return; //错误
	}
	// *** CodeHoder 不在需要，可以被销毁
	uint32_t anArr[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	uint32_t anArrd[10] = { 0 };
	size_t result = pfnSum(anArr, anArrd, 10); //执行代码
	printf("%d\r\n", result);
}

int _tmain(int argc, _TCHAR* argv[])
{
	//MinimalExample();
//	AssemblerExample();
	//RelocationExample();
	CompilerMemcpy();

	JitRuntime rt;
	CodeHolder code;  //存储代码和重定位信息
	code.init(rt.getCodeInfo());//

	X86Assembler assm(&code); //创建汇编器，并绑定code
	UsingOpransExample(assm);

	return 0;
}

