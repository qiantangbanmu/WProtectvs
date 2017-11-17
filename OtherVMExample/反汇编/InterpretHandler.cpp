#include "StdAfx.h"
#include "InterpretHandler.h"
#include "asm/disasm.h"

// 每个普通的汇编指令，一般都有一个虚拟机指令对应着，而每个虚拟机指令，实际上又对应着某段普通的汇编代码，换句话说，一个普通的汇编指令就由另一段汇编代码来模拟完成了
// 此文件就是用于为某个汇编指令相应的虚拟机指令生成相应的汇编代码
// 对于普通汇编指令与虚拟机指令的对应关系由一张表定义着（即vmtable，见vmserv.cpp中定义）

CInterpretHandler::CInterpretHandler(void)
{
}

CInterpretHandler::~CInterpretHandler(void)
{
}
/**
* 获取指定寄存器RegType在VMContext虚拟机上下文中的偏移
*
*                针对不同的寄存器，偏移值不同，如
*                        32位寄存器：EAX
*                        16位寄存器：AX
*                        低8位寄存器：AL
*                以上情况直接取得偏移
*                        高8位寄存器：AH
*                则需要对偏移进行 + 1，以便得到高8位值
*
* RegType：指明要取得偏移的寄存器，取值可见枚举RegType
*/
// 获得寄存器的偏移
// 16和32位和8位低字节均使用32位enum，8位高字节使用20以上的数
int CInterpretHandler::GetRegisterOffset(int RegType)
{
	if( RegType < 0 )
		return RegType;
	int offset = m_RegisterIdx[RegType]*4;
	if ( RegType >= 20 )// 判断是否取寄存器高8位
		offset++;//8位高字节的地方
	return offset;
}
//初始化
BOOL CInterpretHandler::Init()
{
	RandListIdx(m_RegisterIdx,REGCOUNT);	//打乱寄存器偏移的索引

	return TRUE;
}
/**
* 为指定虚拟机指令table设置所需操作数
*
* table: 包含某个虚拟机指令与汇编指令相关信息
* asmtext：保存生成的汇编代码
* len: asmtext缓存的长度
*/
//设置参数
void CInterpretHandler::SetArg(VMTable* table,char* asmtext,int len)
{
	// 操作数不超过3个
	for(int i = 0; i < 3; i++)
	{
		// 欲处理操作数超限制，则跳出
		// OperandNum: 操作数个数
		if( i >= table->OperandNum )
			break;
		// 以下将生成：mov EAX, [esp + (0 | 4 | 8)]
		// 所以相当于将堆栈中压入的操作数复制到EAX寄存器中以作为虚拟机的操作数使用
		sprintf_s(asmtext,len,"%smov %s,[esp+%02x]\n",asmtext,ArgReg[i],i*4);//将堆栈中的数据交给参数寄存器
	}
}

/**
* 将指定虚拟机指令table的操作数恢复到堆栈中(操作数一般都被压入栈)
*
* table: 包含某个虚拟机指令与汇编指令相关信息
* asmtext：保存生成的汇编代码
* len: asmtext缓存的长度
*/
//恢复参数
void CInterpretHandler::RestoreArg(VMTable* table,char* asmtext,int len)
{
	//将结果保存回堆栈
	for(int i = 0; i < 3; i++)
	{
		if( i >= table->OperandNum )
			break;

		sprintf_s(asmtext,len,"%smov [esp+%02x],%s\n",asmtext,i*4,ArgReg[i]);
	}
}

/**
* 将VMContext上下文保存的EFlag寄存器值恢复到EFlag寄存器中
*
* asmtext：保存恢复所需的汇编代码
* len: asmtext缓存的长度
*/
//恢复标志
void CInterpretHandler::RestoreFlag(char* asmtext,int len)
{
	sprintf_s(asmtext,len,"%sPush [edi+0x%02x]\nPopfd\n",asmtext,GetRegisterOffset(RT_EFlag));//恢复标志
}
/**
* 将EFlag寄存器值保存到VMContext上下文的EFlag中
*
* asmtext：保存恢复所需的汇编代码
* len: asmtext缓存的长度
*/
//保存标志
void CInterpretHandler::SaveFlag(char* asmtext,int len)
{
	sprintf_s(asmtext,len,"%sPushfd\nPop [edi+0x%02x]\n",asmtext,GetRegisterOffset(RT_EFlag));//保存标志
}

/**
* 根据指定虚拟机指令-汇编指令信息table为虚拟机指令生成相应的汇编代码
*
* table: 包含某个虚拟机指令与汇编指令相关信息
* asmtext：保存生成的汇编代码
* len: asmtext缓存的长度
*
* 返回值：成功返回TRUE，否则FALSE
*/
//根据结构声称ASM字符串
BOOL CInterpretHandler::InterpretASMStr(VMTable* table,char* asmtext,int len)
{
	// 检测参数的合法性
	if( strcmp(table->VMInstrName,"") == 0 || asmtext == NULL ) return FALSE;
	// 清0 asmtext缓存
	memset(asmtext,0,len);
	// 为虚拟机指令设置操作数
	SetArg(table,asmtext,len);//设置参数
	// 某些汇编指令在执行时，需要用到某些寄存器的值来控制其执行，那么，对于相应
	// 的虚拟指令，当然也是需要，所以，现在需要恢复所需寄存器的值了

	// 为虚拟机指令设置所需的寄存器
	//恢复需要的寄存器
	for(int i = 0; i < 4; i++)
	{ 
		// 判断虚拟机指令是否需要用 i 寄存器
		if( table->NeedReg[i] != NONE && table->NeedReg[i] < 14 )
		{
			// edi -> 指向VMContext

			// 由于虚拟机将所有寄存器的值保存在VMContext中，所以，
			// 现在将保存的值复制到所需寄存器中以便指令使用
			sprintf_s(asmtext,len,"%smov %s,[edi+0x%02x]\n",asmtext,
				vregname[2][table->NeedReg[i]],GetRegisterOffset(table->NeedReg[i]));
		}
	}
	// 为vBegin指令生成汇编代码
	BOOL After = FALSE;
	if( _stricmp(table->strInstruction,"vBegin") == 0 )
	{
		After = InterpretvBegin(table,asmtext,len);
	}
	else if (_stricmp(table->strInstruction, "vtoReal") == 0) // 为vtoReal指令生成汇编代码，vtoReal用于跳转至真实指令
	{
		After = InterpretvtoReal(table,asmtext,len);
	}
///////////////////////////////////////////////////////////////////////////////
	else if( _stricmp(table->strInstruction,"push") == 0 )
	{
		After = InterpretPush(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"pop") == 0 )
	{
		After = InterpretPop(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"pushfd") == 0 )
	{
		After = InterpretPushfd(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"popfd") == 0 )
	{
		After = InterpretPopfd(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"pushad") == 0 )
	{
		After = InterpretPushad(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"popad") == 0 )
	{
		After = InterpretPopad(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"enter") == 0 )
	{
		After = InterpretEnter(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"leave") == 0 )
	{
		After = InterpretLeave(table,asmtext,len);
	}
	/////////////////////////////////////////////////////////////////////////////////
	// 为jcxz指令生成相应的汇编代码
	// 注：jcxz指令，该指令实现当寄存器CX的值等于0时转移到标号,否则顺序执行
	else if( _stricmp(table->strInstruction,"jcxz") == 0 )
	{
		// 先将VMContext上下文的EFlag寄存器恢复到EFlag标志寄存器
		RestoreFlag(asmtext,len);
		After = InterpretJCXZ(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"jmp") == 0 )
	{
		After = InterpretJMP(table,asmtext,len);
	}
	else if( table->strInstruction[0] == 'J' || table->strInstruction[0] == 'j' )//不是jcxz和jmp,就是条件跳转了
	{
		RestoreFlag(asmtext,len);
		After = InterpretJCC(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"loope") == 0 )
	{
		RestoreFlag(asmtext,len);
		After = InterpretLoope(table,asmtext,len);
		SaveFlag(asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"retn") == 0 )
	{
		After = InterpretRetn(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"call") == 0 )
	{
		After = InterpretCall(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"SaveEsp") == 0 )
	{
		After = InterpretSaveEsp(table,asmtext,len);
	}
	else if( _stricmp(table->strInstruction,"RestoreEsp") == 0 )
	{
		After = InterpretRestoreEsp(table,asmtext,len);
	}
	else
	{
		After = CommonInstruction(table,asmtext,len);
	}

	// 某些指令执行完成后，将会影响到某些寄存器的值，这里所做的，就是将
	// 指令执行后改变的寄存器值保存回VMContext上下文中的各个寄存器中
	//sprintf_s(asmtext,len,"%smov [edi+%02x],ebp\n",asmtext,GetRegisterOffset(RT_Esp));//将ebp(即真正esp)保存到esp的地址
	//保存寄存器的值
	for(int i = 0; i < 3; i++)
	{
		// 如果需保存i寄存器，即i寄存器被改变了
		if( table->SaveReg[i] != NONE )
		{
			sprintf_s(asmtext,len,"%smov [edi+0x%02x],%s\n",asmtext,
				GetRegisterOffset(table->NeedReg[i]),vregname[2][table->NeedReg[i]]);
		}
	}

	if( After )
	{
		RestoreArg(table,asmtext,len);
	}

	// 将最终为指令生成的汇编代码转为大写
	_strupr_s(asmtext,ASMTEXTLEN);
	return TRUE;
}
/**
* 根据操作数的位数bit，获取相应的修饰符
*
* bit: 操作数的位数
* scalestr：用于返回得到的修饰符
*
* 返回值：返回操作数大小索引：
*                8位: 0
*                16位: 1
*                32位：2
*/
int GetScalestr(int bit,OUT char* scalestr)
{
	int sizeidx = 0;
	if( bit == 8 )
	{
		sizeidx = 0;
		strcpy_s(scalestr,6,"byte");
	}
	else if( bit == 16 )
	{
		sizeidx = 1;
		strcpy_s(scalestr,6,"word");
	}
	else if( bit == 32 )
	{
		sizeidx = 2;
		strcpy_s(scalestr,6,"dword");
	}
	return sizeidx;
}

/**
* 为普通指令生成相应的汇编代码
*
* table: 存放某个普通指令的信息，根据这些信息可以为相应虚拟机生成汇编代码
* asmtext: 保存生成的汇编代码
* len：asmtext缓存的长度
*
* 返回值：只返回TRUE
*/
BOOL CInterpretHandler::CommonInstruction(VMTable* table,char* asmtext,int len)
{
	char scalestr[6] = {0};
	int sizeidx = 0;
	// 处理指令的操作数
	char stroperand[1024] = {0};
	for(int i = 0; i < 3; i++)
	{
		// 操作数超限制
		if( i >= table->OperandNum )
			break;
		sizeidx = GetScalestr(table->bitnum[i],scalestr);// 获取i操作数的修饰符
		// 如果i操作数是一个内存数
		if( table->optype[i] == MEMTYPE )//内存数
		{	//操作数顺序eax,ecx,edx
			sprintf_s(stroperand,1024,"%s%s ptr %s[%s],",stroperand,scalestr,GetSegStr(table->Segment),vregname[2][i]);//得到参数
		}
		else//立即数和寄存器
		{
			sprintf_s(stroperand,1024,"%s%s,",stroperand,vregname[sizeidx][i]);//得到参数
		}
	}
	// 去掉最后操作数的逗号
	if( table->OperandNum > 0)
		stroperand[strlen(stroperand)-1] = '\0';//去掉逗号

	RestoreFlag(asmtext,len);
	// 执行指令
	sprintf_s(asmtext,len,"%s%s %s\n",asmtext,table->strInstruction,stroperand);//真正执行的指令
	SaveFlag(asmtext,len);
	return TRUE;
}

// 获得段前缀
char* CInterpretHandler::GetSegStr(int Segment)
{
	static char segstr[10] = "";
	memset(segstr,0,10);
	if( Segment == SEG_FS )
		strcpy_s(segstr,10,"fs:");
	else if( Segment == SEG_GS )
		strcpy_s(segstr,10,"gs:");
	return segstr;
}
// 首先执行的指令
BOOL CInterpretHandler::InterpretvBegin(VMTable* table,char* asmtext,int len)
{
	//popfd
	//pop ebp
	//pop edi
	//pop esi
	//pop edx
	//pop ecx
	//pop eax
	int s_reg[8] = {RT_EFlag,RT_Ebp,RT_Edi,RT_Esi,RT_Edx,RT_Ecx,RT_Ebx,RT_Eax};

	// 弹出寄存器
	for(int i = 0; i < 8;i++)
	{
		sprintf_s(asmtext,len,"%smov eax,dword ptr [ebp]\n",asmtext);
		sprintf_s(asmtext,len,"%smov [edi+%02X],eax\n",asmtext,GetRegisterOffset(s_reg[i]));
		
		sprintf_s(asmtext,len,"%sadd ebp,4\n",asmtext);
	}
	// 释放伪地址堆栈
	sprintf_s(asmtext,len,"%sadd ebp,4\n",asmtext);
	sprintf_s(asmtext,len,"%smov [edi+%02x],ebp\n",asmtext,GetRegisterOffset(RT_Esp));//将ebp(即真正esp)保存到esp的地址

	return FALSE;
}

// 跳转到真实指令
BOOL CInterpretHandler::InterpretvtoReal(VMTable* table,char* asmtext,int len)
{
	int s_reg[9] = {RT_Esp,RT_EFlag,RT_Ebp,RT_Edi,RT_Esi,RT_Edx,RT_Ecx,RT_Ebx,RT_Eax};
	//之前有一个pushimm32 xxxx地址

	//把这个值放如真实堆栈
	sprintf_s(asmtext,len,"%smov eax,dword ptr [esi]\n",asmtext);//释放4字节堆栈
	sprintf_s(asmtext,len,"%sadd esi,4\n",asmtext);
	sprintf_s(asmtext,len,"%ssub ebp,4\n",asmtext);
	sprintf_s(asmtext,len,"%smov dword ptr [ebp],eax\n",asmtext);//eax,第1个参数

	sprintf_s(asmtext,len,"%smov [edi+%02x],ebp\n",asmtext,GetRegisterOffset(RT_Esp));//将ebp(即真正esp)保存到esp的地址

	// 弹出寄存器
	for(int i = 0; i < 9;i++)
	{
		sprintf_s(asmtext,len,"%spush [edi+%02X]\n",asmtext,GetRegisterOffset(s_reg[i]));
	}
	sprintf_s(asmtext,len,"%spop eax\n",asmtext);
	sprintf_s(asmtext,len,"%spop ebx\n",asmtext);
	sprintf_s(asmtext,len,"%spop ecx\n",asmtext);
	sprintf_s(asmtext,len,"%spop edx\n",asmtext);
	sprintf_s(asmtext,len,"%spop esi\n",asmtext);
	sprintf_s(asmtext,len,"%spop edi\n",asmtext);
	sprintf_s(asmtext,len,"%spop ebp\n",asmtext);
	sprintf_s(asmtext,len,"%spopfd\n",asmtext);
	sprintf_s(asmtext,len,"%spop esp\n",asmtext);
	//返回
	sprintf_s(asmtext,len,"%sretn\n",asmtext);
	return FALSE;
}
//		mov		eax,dword ptr [esp]
//		sub		ebp,4
//		mov		eax,[eax]
//		mov		word ptr [ebp],ax
//解释push
BOOL CInterpretHandler::InterpretPush(VMTable* table,char* asmtext,int len)
{
	char scalestr[6] = {0};
	int sizeidx = 0;
	if( table->bitnum[0] == 8 )
	{
		sizeidx = 0;
		strcpy_s(scalestr,6,"byte");
	}
	else if( table->bitnum[0] == 16 )
	{
		sizeidx = 1;
		strcpy_s(scalestr,6,"word");
	}
	else if( table->bitnum[0] == 32 )
	{
		sizeidx = 2;
		strcpy_s(scalestr,6,"dword");
	}

	sprintf_s(asmtext,len,"%ssub ebp,%d\n",asmtext,table->bitnum[0] / 8);//从堆栈中腾出空间
	if( table->optype[0] == MEMTYPE )
	{
		sprintf_s(asmtext,len,"%smov %s,%s ptr %s[eax]\n",asmtext,vregname[sizeidx][RT_Eax],scalestr,GetSegStr(table->Segment));//得到内存数的值
	}
	sprintf_s(asmtext,len,"%smov %s ptr [ebp],%s\n",asmtext,scalestr,vregname[sizeidx][RT_Eax]);
	return TRUE;
}
//		mov		eax,dword ptr [ebp]
//		add		ebp,4
//		mov		eax,[eax]
//解释pop
BOOL CInterpretHandler::InterpretPop(VMTable* table,char* asmtext,int len)
{
	char scalestr[6] = {0};
	int sizeidx = 0;
	if( table->bitnum[0] == 8 )
	{
		sizeidx = 0;
		strcpy_s(scalestr,6,"byte");
	}
	else if( table->bitnum[0] == 16 )
	{
		sizeidx = 1;
		strcpy_s(scalestr,6,"word");
	}
	else if( table->bitnum[0] == 32 )
	{
		sizeidx = 2;
		strcpy_s(scalestr,6,"dword");
	}
	sprintf_s(asmtext,len,"%smov %s,%s ptr [ebp]\n",asmtext,vregname[sizeidx][RT_Ecx],scalestr);//得到堆栈的值
	sprintf_s(asmtext,len,"%sadd ebp,%d\n",asmtext,table->bitnum[0] / 8);//释放4字节堆栈
	if( table->optype[0] == MEMTYPE )
	{
		sprintf_s(asmtext,len,"%smov %s,%s ptr %s[eax]\n",asmtext,vregname[sizeidx][RT_Eax],scalestr,GetSegStr(table->Segment));//得到内存数的值
	}
	sprintf_s(asmtext,len,"%smov eax,ecx\n",asmtext);//将值给mem(只有mem，因为无论是reg还是mem在vm中都是内存地址)
	return TRUE;
}

//解释pushfd
BOOL CInterpretHandler::InterpretPushfd(VMTable* table,char* asmtext,int len)
{
	sprintf_s(asmtext,len,"%ssub ebp,4\n",asmtext);//从堆栈中取得参数
	sprintf_s(asmtext,len,"%smov eax,[edi+%02x]\n",asmtext,GetRegisterOffset(RT_EFlag));//得到EFL
	sprintf_s(asmtext,len,"%smov dword ptr [ebp],eax\n",asmtext);//赋值EFL寄存器
	return TRUE;
}
//解释popfd
BOOL CInterpretHandler::InterpretPopfd(VMTable* table,char* asmtext,int len)
{
	sprintf_s(asmtext,len,"%smov eax,dword ptr [ebp]\n",asmtext);//赋值EFL寄存器
	sprintf_s(asmtext,len,"%sadd ebp,4\n",asmtext);//释放堆栈
	sprintf_s(asmtext,len,"%smov [edi+%02x],eax\n",asmtext,GetRegisterOffset(RT_EFlag));//设置EFL
	return TRUE;
}


// 解释pushad
BOOL CInterpretHandler::InterpretPushad(VMTable* table,char* asmtext,int len)
{
	for(int i = 0 ; i < 8;i++)
	{
		sprintf_s(asmtext,len,"%ssub ebp,4\n",asmtext);//从堆栈中取得参数
		sprintf_s(asmtext,len,"%smov eax,[edi+%02x]\n",asmtext,GetRegisterOffset(i));//i = enum RegType
		sprintf_s(asmtext,len,"%smov dword ptr [ebp],eax\n",asmtext);//push
	}
	return TRUE;
}

// 解释popad
BOOL CInterpretHandler::InterpretPopad(VMTable* table,char* asmtext,int len)
{
	for(int i = 8 ; i > 0;i--)
	{
		sprintf_s(asmtext,len,"%smov eax,dword ptr [ebp]\n",asmtext);//
		sprintf_s(asmtext,len,"%sadd ebp,4\n",asmtext);//释放堆栈
		sprintf_s(asmtext,len,"%smov [edi+%02x],eax\n",asmtext,GetRegisterOffset(i));//i = enum RegType
	}
	return TRUE;
}

// 解释enter
//push ebp
//mov ebp,esp
//sub esp,(L - 1) * 4 ; L > 0才有这步操作，用来存储嵌套的L - 1个子过程的栈框架指针（注意是指针）
//push ebp ; 当前过程的栈框架指针
//sub esp,N
BOOL CInterpretHandler::InterpretEnter(VMTable* table,char* asmtext,int len)
{
	//push ebp
	sprintf_s(asmtext,len,"%smov edx,[edi+%02x]\n",asmtext,GetRegisterOffset(RT_Ebp));//
	sprintf_s(asmtext,len,"%ssub ebp,4\n",asmtext);//
	sprintf_s(asmtext,len,"%smov dword ptr [ebp],edx\n",asmtext);//push ebp

	//mov ebp,esp
	sprintf_s(asmtext,len,"%smov edx,ebp\n",asmtext);//
	sprintf_s(asmtext,len,"%smov [edi+%02x],edx\n",asmtext,GetRegisterOffset(RT_Esp));//
	
	//sub esp,(L - 1) * 4 ; L > 0才有这步操作
	sprintf_s(asmtext,len,"%smov edx,ebp\n",asmtext);//
	sprintf_s(asmtext,len,"%slea ecx,[ecx*4]\n",asmtext);//ecx = 第2个参数
	sprintf_s(asmtext,len,"%ssub edx,ecx\n",asmtext);//不管是不是0都减去
	sprintf_s(asmtext,len,"%ssub edx,4\n",asmtext);//再减1个4
	sprintf_s(asmtext,len,"%stest ecx,ecx\n",asmtext);//测试是否为0
	sprintf_s(asmtext,len,"%scmovne ebp,edx\n",asmtext);//如果不为0就赋值减去后的结果

	//push ebp
	sprintf_s(asmtext,len,"%smov edx,[edi+%02x]\n",asmtext,GetRegisterOffset(RT_Ebp));//
	sprintf_s(asmtext,len,"%ssub ebp,4\n",asmtext);//
	sprintf_s(asmtext,len,"%smov dword ptr [ebp],edx\n",asmtext);//push ebp

	//sub esp,N
	sprintf_s(asmtext,len,"%ssub ebp,eax\n",asmtext);//eax,第1个参数
	return TRUE;
}

// 解释leave
// mov ebp,esp
// pop ebp
BOOL CInterpretHandler::InterpretLeave(VMTable* table,char* asmtext,int len)
{
	//mov ebp,esp
	sprintf_s(asmtext,len,"%smov edx,ebp\n",asmtext);//
	sprintf_s(asmtext,len,"%smov [edi+%02x],edx\n",asmtext,GetRegisterOffset(RT_Esp));//
	//pop ebp
	sprintf_s(asmtext,len,"%smov eax,[ebp]\n",asmtext);//
	sprintf_s(asmtext,len,"%sadd ebp,4\n",asmtext);//释放堆栈
	sprintf_s(asmtext,len,"%smov [edi+%02x],eax\n",asmtext,GetRegisterOffset(RT_Ebp));//
	return TRUE;
}
///////////////////////////////////////////////////
// 解释jmp
BOOL CInterpretHandler::InterpretJMP(VMTable* table,char* asmtext,int len)
{
	sprintf_s(asmtext,len,"%smov esi,eax\n",asmtext);//
	sprintf_s(asmtext,len,"%sadd esp,4\n",asmtext);//
	return FALSE;
}
// 解释jcxz\jecxz
BOOL CInterpretHandler::InterpretJCXZ(VMTable* table,char* asmtext,int len)
{
	sprintf_s(asmtext,len,"%stest ecx,ecx\n",asmtext);//
	sprintf_s(asmtext,len,"%sCMOVZ esi,eax\n",asmtext);//eax为第1个参数
	sprintf_s(asmtext,len,"%sadd esp,4\n",asmtext);//
	return FALSE;
}	
// 解释jcc
BOOL CInterpretHandler::InterpretJCC(VMTable* table,char* asmtext,int len)
{
	char strPostfix[16] = {0};//条件后缀
	strcpy_s(strPostfix,16,&table->strInstruction[1]);
	sprintf_s(asmtext,len,"%scmov%s esi,[esp]\n",asmtext,strPostfix);//
	sprintf_s(asmtext,len,"%sadd esp,4\n",asmtext);//
	return FALSE;
}
// 解释loope
BOOL CInterpretHandler::InterpretLoope(VMTable* table,char* asmtext,int len)
{
	sprintf_s(asmtext,len,"%spushfd\n",asmtext);//
	sprintf_s(asmtext,len,"%stest ecx,ecx\n",asmtext);//
	sprintf_s(asmtext,len,"%scmovne edx,eax\n",asmtext);//eax为第1个参数
	sprintf_s(asmtext,len,"%spopfd\n",asmtext);//
	sprintf_s(asmtext,len,"%scmovne edx, esi\n",asmtext);//
	sprintf_s(asmtext,len,"%scmove edx,eax\n",asmtext);//eax为第1个参数
	sprintf_s(asmtext,len,"%sadd ebp,4\n",asmtext);//释放堆栈
	return FALSE;
}


// 解释返回
BOOL CInterpretHandler::InterpretRetn(VMTable* table,char* asmtext,int len)
{
	int s_reg[9] = {RT_Esp,RT_EFlag,RT_Ebp,RT_Edi,RT_Esi,RT_Edx,RT_Ecx,RT_Ebx,RT_Eax};

	if( table->OperandNum == 1 )//retn xxx
	{
		sprintf_s(asmtext,len,"%smov edx,ebp\n",asmtext);
		sprintf_s(asmtext,len,"%sadd ebp,eax\n",asmtext);//retn xx,先放上去
		sprintf_s(asmtext,len,"%smov [ebp],edx\n",asmtext);//再PUSH进retn地址
		sprintf_s(asmtext,len,"%smov [edi+%02x],ebp\n",asmtext,GetRegisterOffset(RT_Esp));//将ebp(即真正esp)保存到esp的地址
	}
	// 弹出寄存器
	for(int i = 0; i < 9;i++)
	{
		sprintf_s(asmtext,len,"%spush [edi+%02X]\n",asmtext,GetRegisterOffset(s_reg[i]));
	}
	sprintf_s(asmtext,len,"%spop eax\n",asmtext);
	sprintf_s(asmtext,len,"%spop ebx\n",asmtext);
	sprintf_s(asmtext,len,"%spop ecx\n",asmtext);
	sprintf_s(asmtext,len,"%spop edx\n",asmtext);
	sprintf_s(asmtext,len,"%spop esi\n",asmtext);
	sprintf_s(asmtext,len,"%spop edi\n",asmtext);
	sprintf_s(asmtext,len,"%spop ebp\n",asmtext);
	sprintf_s(asmtext,len,"%spopfd\n",asmtext);
	sprintf_s(asmtext,len,"%spop esp\n",asmtext);

	//返回
	sprintf_s(asmtext,len,"%sretn\n",asmtext);
	return FALSE;
}
// 解释子调用
BOOL CInterpretHandler::InterpretCall(VMTable* table,char* asmtext,int len)
{
	int s_reg[9] = {RT_Esp,RT_EFlag,RT_Ebp,RT_Edi,RT_Esi,RT_Edx,RT_Ecx,RT_Ebx,RT_Eax};
	
	sprintf_s(asmtext,len,"%smov edx,[esi]\n",asmtext);
	sprintf_s(asmtext,len,"%ssub ebp,4\n",asmtext);//4字节空间
	sprintf_s(asmtext,len,"%smov [ebp],edx\n",asmtext);//返回地址
	sprintf_s(asmtext,len,"%ssub ebp,4\n",asmtext);//4字节空间
	if( table->optype[0] == MEMTYPE )
	{
		sprintf_s(asmtext,len,"%smov eax,dword ptr [eax]\n",asmtext);//得到内存数的值
	}
	sprintf_s(asmtext,len,"%smov [ebp],eax\n",asmtext);//CALL的地址 eax = 第一个操作数

	sprintf_s(asmtext,len,"%smov [edi+%02x],ebp\n",asmtext,GetRegisterOffset(RT_Esp));//将ebp(即真正esp)保存到esp的地址

	// 弹出寄存器
	for(int i = 0; i < 9;i++)
	{
		sprintf_s(asmtext,len,"%spush [edi+%02X]\n",asmtext,GetRegisterOffset(s_reg[i]));
	}
	sprintf_s(asmtext,len,"%spop eax\n",asmtext);
	sprintf_s(asmtext,len,"%spop ebx\n",asmtext);
	sprintf_s(asmtext,len,"%spop ecx\n",asmtext);
	sprintf_s(asmtext,len,"%spop edx\n",asmtext);
	sprintf_s(asmtext,len,"%spop esi\n",asmtext);
	sprintf_s(asmtext,len,"%spop edi\n",asmtext);
	sprintf_s(asmtext,len,"%spop ebp\n",asmtext);
	sprintf_s(asmtext,len,"%spopfd\n",asmtext);
	sprintf_s(asmtext,len,"%spop esp\n",asmtext);

	//返回
	sprintf_s(asmtext,len,"%sretn\n",asmtext);

	return FALSE;//不恢复了
}
// 解释保护堆栈Handler
BOOL CInterpretHandler::InterpretSaveEsp(VMTable* table,char* asmtext,int len)
{
	sprintf_s(asmtext,len,"%smov [edi+%02x],ebp\n",asmtext,GetRegisterOffset(RT_Esp));//将ebp(即真正esp)保存到esp的地址
	return FALSE;
}
// 解释恢复堆栈Handler
BOOL CInterpretHandler::InterpretRestoreEsp(VMTable* table,char* asmtext,int len)
{
	sprintf_s(asmtext,len,"%smov ebp,[edi+%02x]\n",asmtext,GetRegisterOffset(RT_Esp));//将vmesp的值恢复到ebp(即真正esp)
	return FALSE;
}  