#include "stdafx.h"
#include "CCodeILFactory.h"
#include ".\pestructure.h"

int main()
{
	CPEStructure pestruct;
	CCodeILFactory codefactory;
	//CLink CodeList;
	list<CodeNode*> CodeList;
	list<CodeNode*> CodeList1;
	pestruct.OpenFileName("E:\\Code\\VMSample\\Release\\VMSample.exe");

	// 读取map文件，解析其中的符号信息并存于pestruct.MapVector中
	// 注：MAP文件是一个文本文件，记录了程序的入口地址、基地址、符号及其对应的段、文件偏址等信息
	pestruct.LoadMap("E:\\Code\\VMSample\\Release\\VMSample.map");
	//getchar();

	// 为PE文件pestruct分配一个新的区块，并对该区块进行初始化，并生成某
	// 些虚拟机核心指令，并将指令代码复制到该区块的某个子块中
	// 注：具体此函数分新的区块结构怎样，可查看“PE新区块结构图.jpg”
	codefactory.Init(0x400000+pestruct.GetNewSection());//创建虚拟内存段

	// 查找CrackMe()函数的符号信息
	MapStructrue* stu = pestruct.GetMap("CrackMe(struct HWND__ *)");
	if( !stu )
		return 0;
	// 得到符号的基地址
	char * Base_Addr = pestruct.image_section[stu->Segment-1] + stu->Offset;//得到基地址

	// 反汇编Base_Addr的代码，即函数CrackMe()的代码
	codefactory.DisasmFunction(&CodeList,Base_Addr,stu->VirtualAddress);

	list<CodeNode*>::iterator itr;
	for (itr = CodeList.begin(); itr != CodeList.end(); itr++)
	{
		CodeNode* code = *itr;
		if (code)
		{
			char str[255] = "";
			sprintf_s(str, 255, "vcode:%s\n", code->disasm.vm_name);
			OutputDebugStringA(str);
			printf("%-24s  %-24s   (MASM)\n", code->disasm.dump, code->disasm.result);
		}
	}

	// 获取CodeList指令列表中起始指令
	itr = CodeList.begin();
	CodeNode* code = *itr;
	itr = CodeList.end();
	itr--;
	// 获取CodeList指令列表中结束指令
	CodeNode* endcode = *itr;
	// 计算CodeList的总代码长度
	int asmlen = endcode->disasm.ip+endcode->disasm.codelen - code->disasm.ip;
	// 组织一段进入虚拟机的指令(即jmp VStartVM)，存储于Base_Addr
	codefactory.VMFactory.CompileEnterStubCode(Base_Addr,code->disasm.ip,asmlen);
	// 将文件各头部与区块数据写回至PE文件中
	pestruct.UpdateHeadersSections(FALSE);

	// 将CodeList指令列表编译为虚拟机字节码
	char errtext[255] = {0};
	codefactory.BuildCode(Base_Addr,&CodeList,errtext);

	// 计算存放虚拟机各个部件的虚存空间长度
	int len = codefactory.m_JumpTable.m_addrlen + codefactory.m_CodeEngine.m_addrlen +
				codefactory.m_EnterStub.m_addrlen + codefactory.m_VMEnterStubCode.m_addrlen + 
				codefactory.m_VMCode.m_addrlen + 0x4000;
	char *newdata = new char[len]; // 分配足够的空间

	// 将跳转表复制到p空间中
	char *p = newdata;
	memcpy( p,codefactory.m_JumpTable.m_BaseAddr,codefactory.m_JumpTable.m_addrlen );
	// 将虚拟机其他部分复制到p空间中
	p += codefactory.m_JumpTable.m_addrlen;
	memcpy( p,codefactory.m_CodeEngine.m_BaseAddr,codefactory.m_CodeEngine.m_addrlen );
	p += codefactory.m_CodeEngine.m_addrlen;
	memcpy( p,codefactory.m_EnterStub.m_BaseAddr,codefactory.m_EnterStub.m_addrlen );
	p += codefactory.m_EnterStub.m_addrlen;
	memcpy( p,codefactory.m_VMEnterStubCode.m_BaseAddr,codefactory.m_VMEnterStubCode.m_addrlen );
	p += codefactory.m_VMEnterStubCode.m_addrlen;
	memcpy( p,codefactory.m_VMCode.m_BaseAddr,codefactory.m_VMCode.m_addrlen );
	p += codefactory.m_VMCode.m_addrlen;

	// 为PE文件添加.bug的新区块，即虚拟机
	pestruct.AddSection(newdata,len,".bug");
	pestruct.UpdateHeaders(FALSE);
	pestruct.UpdateHeadersSections(TRUE);
	pestruct.UpdateHeadersSections(FALSE);
	pestruct.MakePE("E:\\Code\\VMSample\\Release\\VMSample.vm.exe", len); // 创建一个新的可执行文件

	//#include "asm\disasm.h"
	//ulong l = 0;
	//t_disasm da;
	//char errtext[TEXTLEN] = {0};

	//memset(&da,0,sizeof(t_disasm));
	//// Demonstration of Disassembler.
	//printf("Disassembler:\n");
	//ideal=0; lowercase=1; putdefseg=0;
	//l=Disasm("\xCC",
	//	MAXCMDSIZE,0x400000,&da,DISASM_CODE);
	//printf("%3i  %-24s  %-24s   (MASM)\n",l,da.dump,da.result);
	//getchar();
	return 0;
}

