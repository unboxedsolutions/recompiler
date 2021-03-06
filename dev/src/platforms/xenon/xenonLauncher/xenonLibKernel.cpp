#include "build.h"
#include "xenonLibUtils.h" 
#include "xenonLibNatives.h"
#include "xenonThread.h"
#include <mutex>
#include "xenonPlatform.h"

//---------------------------------------------------------------------------

uint64 __fastcall XboxKernel_KeEnableFpuExceptions( uint64 ip, cpu::CpuRegs& regs )
{
	const uint32 enabled = (const uint32) regs.R3;
	GLog.Log( "KeEnableFpuExceptions(%d)", enabled );
	RETURN_ARG(0);
}

uint64 __fastcall XboxKernel_KeQueryPerformanceFrequency( uint64 ip, cpu::CpuRegs& regs )
{
	__int64 ret = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&ret);
	//ret *= 10;
	RETURN_ARG(ret); // english
}

uint64 __fastcall XboxKernel_XGetLanguage( uint64 ip, cpu::CpuRegs& regs )
{
	GLog.Log("XGetLanguage: english returned");
	RETURN_ARG(1); // english
}

uint64 __fastcall XboxKernel_XGetAVPack( uint64 ip, cpu::CpuRegs& regs )
{
	GLog.Log("XGetAVPack: returning USA/Canada");
	RETURN_ARG(0x010000);
}

uint64 __fastcall XboxKernel_XamLoaderTerminateTitle( uint64 ip, cpu::CpuRegs& regs )
{
	GLog.Log("XamLoaderTerminateTitle: called");
	ULONG_PTR args[1] = { 0 }; // exit code
	RaiseException(cpu::EXCEPTION_TERMINATE_PROCESS, EXCEPTION_NONCONTINUABLE, 1, &args[0] );
	RETURN();
}

uint64 __fastcall XboxKernel_XamShowMessageBoxUIEx( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_RtlInitAnsiString( uint64 ip, cpu::CpuRegs& regs )
{
	xnative::X_ANSI_STRING* ansiString = GetPointer<xnative::X_ANSI_STRING>( regs.R3 );
	const char* srcString = GetPointer<char>( regs.R4 );

	uint32 len = (uint32)( srcString ? strlen(srcString) : 0 );
	if (len >= 65534)
		len = 65534;

	ansiString->Length = ToPlatform((uint16)(len));
	ansiString->MaximumLength = ToPlatform((uint16)(len+1));
	ansiString->BufferPtr = ToPlatform((uint32)srcString);

	RETURN();
}

template< typename T >
static inline uint32 ExtractSetting(T value, cpu::CpuRegs& regs )
{
	void* outputBuf = GetPointer<void>(regs.R5);
	const uint32 outputBufSize = (uint32) regs.R6;
	uint16* settingSizePtr = GetPointer<uint16>(regs.R7);

	// no buffer, extract size
	if (!outputBuf)
	{
		// no size ptr :(
		if (!settingSizePtr)
			RETURN_ARG(ERROR_INVALID_DATA);

		// output data size (watch for endianess)
		*settingSizePtr = ToPlatform((uint16)sizeof(T));
		RETURN_ARG(0);
	}

	// buffer to small
	if (outputBufSize < sizeof(T))
		RETURN_ARG(ERROR_INVALID_DATA);

	// output value
	memcpy(outputBuf, &value, sizeof(T));

	// output size
	if (settingSizePtr)
		*settingSizePtr = ToPlatform((uint16)sizeof(T));

	// valid
	RETURN_ARG(0);
}

// HRESULT __stdcall ExGetXConfigSetting(u16 categoryNum, u16 settingNum, void* outputBuff, s32 outputBuffSize, u16* settingSize);
uint64 __fastcall XboxKernel_ExGetXConfigSetting( uint64 ip, cpu::CpuRegs& regs )
{
	const uint16 categoryNum = (uint16) regs.R3;
	const uint16 settingNum = (uint16) regs.R4;
	void* outputBuf = GetPointer<void>(regs.R5);
	const uint32 outputBufSize = (uint32) regs.R6;
	uint16* settingSizePtr = GetPointer<uint16>(regs.R7);
	GLog.Log( "XConfig: cat(%d), setting(%d), buf(0x%08X), bufsize(%d), sizePtr(0x%08X)",
		categoryNum, settingNum, (uint32)outputBuf, outputBufSize, (uint32)settingSizePtr);

	// invalid category num
	if (categoryNum >= xnative::XCONFIG_CATEGORY_MAX)
	{
		GLog.Log( "XConfig: invalid category id(%d)" );
		RETURN_ARG(-1);
	}

	// category type
	GLog.Log( "XConfig: Category type '%s' (%d)", xnative::XConfigNames[categoryNum], categoryNum );

	// extract the setting
	switch (categoryNum)
	{
		case xnative::XCONFIG_SECURED_CATEGORY:
		{
			switch (settingNum)
			{
				//case XCONFIG_SECURED_DATA:
				case xnative::XCONFIG_SECURED_MAC_ADDRESS: return ExtractSetting( xnative::XGetConfigSecuredSettings().MACAddress, regs );
				case xnative::XCONFIG_SECURED_AV_REGION: return ExtractSetting( ToPlatform(xnative::XGetConfigSecuredSettings().AVRegion), regs );
				case xnative::XCONFIG_SECURED_GAME_REGION: return ExtractSetting( ToPlatform(xnative::XGetConfigSecuredSettings().GameRegion), regs );
				case xnative::XCONFIG_SECURED_DVD_REGION: return ExtractSetting( ToPlatform(xnative::XGetConfigSecuredSettings().DVDRegion), regs );
				case xnative::XCONFIG_SECURED_RESET_KEY: return ExtractSetting( ToPlatform(xnative::XGetConfigSecuredSettings().ResetKey), regs );
				case xnative::XCONFIG_SECURED_SYSTEM_FLAGS: return ExtractSetting( ToPlatform(xnative::XGetConfigSecuredSettings().SystemFlags), regs );
				case xnative::XCONFIG_SECURED_POWER_MODE: return ExtractSetting( xnative::XGetConfigSecuredSettings().PowerMode, regs );
				case xnative::XCONFIG_SECURED_ONLINE_NETWORK_ID: return ExtractSetting( xnative::XGetConfigSecuredSettings().OnlineNetworkID, regs );
				case xnative::XCONFIG_SECURED_POWER_VCS_CONTROL: return ExtractSetting( xnative::XGetConfigSecuredSettings().PowerVcsControl, regs );
				default: break;
			}

			break;
		}

		case xnative::XCONFIG_USER_CATEGORY:
		{
			switch (settingNum)
			{
				case xnative::XCONFIG_USER_VIDEO_FLAGS: return ExtractSetting( (uint32)0x7, regs );
			}
		}

		default:
			break;
	}

	// invalid config settings
	RETURN_ARG(ERROR_INVALID_DATA);
}

// BOOL XexCheckExecutablePrivilege(DWORD priviledge);
uint64 __fastcall XboxKernel_XexCheckExecutablePrivilege( uint64 ip, cpu::CpuRegs& regs )
{
	const uint32 mask = (const uint32) regs.R3;
	GLog.Log("Privledge check: 0x%08X", mask);
	RETURN_ARG(1);
}

uint64 __fastcall XboxKernel_DbgPrint( uint64 ip, cpu::CpuRegs& regs )
{
	// input string
	const char* txt = GetPointer<const char>(regs.R3);
	if ( txt )
	{
		const char* cur = txt;
		const char* end = txt + strlen(txt);;

		// output string
		char buffer[ 4096 ];
		char* write = buffer;
		char* writeEnd = buffer + sizeof(buffer) - 1;

		// format values
		const TReg* curReg = &regs.R4;
		while ( cur < end )
		{
			const char ch = *cur++;

			if (ch == '%')
			{
				// skip the numbers (for now)
				while (*cur >= '0' && *cur <= '9' )
					++cur;

				// get the code				
				const char code = *cur++;
				switch ( code )
				{
					// just %
					case '%':
					{
						Append(write, writeEnd, '%');
						break;
					}

					// address
					case 'x':
					case 'X':
					{
						const uint64 val = *curReg;
						AppendHex(write, writeEnd, val);
						++curReg;
						break;
					}

					// string
					case 's':
					case 'S':
					{
						const char* str = (const char*)( *curReg & 0xFFFFFFFF );
						AppendString(write, writeEnd, str);
						++curReg;
						break;
					}
				}
			}
			else
			{
				Append(write,writeEnd,ch);
			}
		}

		// end of string
		Append(write, writeEnd,0);

		// print the generated buffer
		GLog.Log( "DbgPrint: %s", buffer );
	}

	RETURN();
}

uint64 __fastcall XboxKernel___C_specific_handler( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_RtlImageXexHeaderField( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_HalReturnToFirmware( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}
////#pragma optimize( "", off )
uint64 __fastcall XboxKernel_NtAllocateVirtualMemory( uint64 ip, cpu::CpuRegs& regs )
{
	uint32* basePtr = GetPointer<uint32>(regs.R3);
	void* base = (void*)_byteswap_ulong(*basePtr);
	uint32* sizePtr = GetPointer<uint32>(regs.R4);
	uint32 size = _byteswap_ulong((uint32) *sizePtr);
	uint64 allocType = regs.R5;
	uint64 protect = regs.R6;

	char* commitFlag = (allocType & xnative::XMEM_COMMIT) ? "COMIT " : "";
	char* reserveFlag = (allocType & xnative::XMEM_RESERVE) ? "RESERVE " : "";
	GLog.Log( "NtAllocateVirtualMemory(size=%d, base=0x%08X) %s%s", (uint32)size, (uint32)base, reserveFlag, commitFlag );
	/*GLog.Log( "  *BasePtr = 0x%08X", (uint32)basePtr);
	GLog.Log( "  *Base = 0x%08X", (uint32)base);
	GLog.Log( "  *SizePtr = 0x%08X", (uint32)sizePtr);
	GLog.Log( "  *Size = 0x%08X", (uint32)size);
	GLog.Log( "  *AllocFlag = %08X", allocType);
	if ( allocType & xnative::XMEM_COMMIT ) GLog.Log( "  *AllocFlag = MEM_COMMIT" );
	if ( allocType & xnative::XMEM_RESERVE ) GLog.Log( "  *AllocFlag = MEM_RESERVE" );
	if ( allocType & xnative::XMEM_DECOMMIT ) GLog.Log( "  *AllocFlag = MEM_DECOMMIT" );
	if ( allocType & xnative::XMEM_RELEASE ) GLog.Log( "  *AllocFlag = MEM_RELEASE" );
	if ( allocType & xnative::XMEM_FREE ) GLog.Log( "  *AllocFlag = MEM_FREE" );
	if ( allocType & xnative::XMEM_PRIVATE ) GLog.Log( "  *AllocFlag = MEM_PRIVATE" );
	if ( allocType & xnative::XMEM_RESET ) GLog.Log( "  *AllocFlag = MEM_RESET" );
	if ( allocType & xnative::XMEM_TOP_DOWN ) GLog.Log( "  *AllocFlag = MEM_TOP_DOWN" );
	if ( allocType & xnative::XMEM_NOZERO ) GLog.Log( "  *AllocFlag = MEM_NOZERO" );
	if ( allocType & xnative::XMEM_LARGE_PAGES ) GLog.Log( "  *AllocFlag = MEM_LARGE_PAGES" );
	if ( allocType & xnative::XMEM_HEAP ) GLog.Log( "  *AllocFlag = MEM_HEAP" );
	if ( allocType & xnative::XMEM_16MB_PAGES ) GLog.Log( "  *AllocFlag = MEM_16MB_PAGES" );
	GLog.Log( "  *Protect = %08X", protect);
	if ( protect & xnative::XPAGE_NOACCESS ) GLog.Log( "  *Protect = PAGE_NOACCESS" );
	if ( protect & xnative::XPAGE_READONLY ) GLog.Log( "  *Protect = PAGE_READONLY" );
	if ( protect & xnative::XPAGE_READWRITE ) GLog.Log( "  *Protect = PAGE_READWRITE" );
	if ( protect & xnative::XPAGE_WRITECOPY ) GLog.Log( "  *Protect = PAGE_WRITECOPY" );
	if ( protect & xnative::XPAGE_EXECUTE ) GLog.Log( "  *Protect = PAGE_EXECUTE" );
	if ( protect & xnative::XPAGE_EXECUTE_READ ) GLog.Log( "  *Protect = PAGE_EXECUTE_READ" );
	if ( protect & xnative::XPAGE_EXECUTE_READWRITE ) GLog.Log( "  *Protect = PAGE_EXECUTE_READWRITE" );
	if ( protect & xnative::XPAGE_EXECUTE_WRITECOPY ) GLog.Log( "  *Protect = PAGE_EXECUTE_WRITECOPY" );
	if ( protect & xnative::XPAGE_GUARD ) GLog.Log( "  *Protect = PAGE_GUARD" );
	if ( protect & xnative::XPAGE_NOCACHE ) GLog.Log( "  *Protect = PAGE_NOCACHE" );
	if ( protect & xnative::XPAGE_WRITECOMBINE ) GLog.Log( "  *Protect = PAGE_WRITECOMBINE" );
	if ( protect & xnative::XPAGE_USER_READONLY ) GLog.Log( "  *Protect = PAGE_USER_READONLY" );
	if ( protect & xnative::XPAGE_USER_READWRITE ) GLog.Log( "  *Protect = PAGE_USER_READWRITE" );*/
	
	// invalid type
	if (allocType & xnative::XMEM_RESERVE && (base != NULL))
	{
		GLog.Err("VMEM: Trying to reserve predefined memory region. Not supported.");
		regs.R3 = ((DWORD   )0xC0000005L);
		*basePtr = NULL;
		RETURN();
	}

	// translate allocation flags
	uint32 realAllocFlags = 0;
	if ( allocType & xnative::XMEM_COMMIT ) realAllocFlags |= native::IMemory::eAlloc_Commit;
	if ( allocType & xnative::XMEM_RESERVE )  realAllocFlags |= native::IMemory::eAlloc_Reserve;
	if ( allocType & xnative::XMEM_DECOMMIT ) realAllocFlags |= native::IMemory::eAlloc_Decomit;
	if ( allocType & xnative::XMEM_RELEASE )  realAllocFlags |= native::IMemory::eAlloc_Release;
	if ( allocType & xnative::XMEM_LARGE_PAGES )  realAllocFlags |= native::IMemory::eAlloc_Page64K;
	if ( allocType & xnative::XMEM_16MB_PAGES )  realAllocFlags |= native::IMemory::eAlloc_Page16MB;

	// access flags
	uint32 realAccessFlags = 0;
	if ( protect & xnative::XPAGE_READONLY ) realAccessFlags |= native::IMemory::eFlags_ReadOnly;
	if ( protect & xnative::XPAGE_READWRITE ) realAccessFlags |= native::IMemory::eFlags_ReadWrite;
	if ( protect & xnative::XPAGE_NOCACHE ) realAccessFlags |= native::IMemory::eFlags_NotCached;
	if ( protect & xnative::XPAGE_WRITECOMBINE ) realAccessFlags |= native::IMemory::eFlags_WriteCombine;

	// determine aligned size
	uint32 pageAlignment = 4096;
	if ( allocType & xnative::XMEM_LARGE_PAGES ) pageAlignment = 64 << 10;
	else if ( allocType & xnative::XMEM_16MB_PAGES ) pageAlignment = 16 << 20;

	// align size
	uint32 alignedSize = (size + (pageAlignment-1)) & ~(pageAlignment-1);
	if ( alignedSize != size )
	{
		GLog.Warn("VMEM: Aligned memory allocation %08X->%08X.", size, alignedSize);
	}

	// allocate memory from system
	void* allocated = GPlatform.GetMemory().VirtualAlloc(base,alignedSize,realAllocFlags,realAccessFlags);
	if (!allocated)
	{
		GLog.Err("VMEM: Allocation failed.");
		regs.R3 = ((DWORD   )0xC0000005L);
		*basePtr = NULL;
		RETURN();
	}

	// save allocated memory & size
	*basePtr = _byteswap_ulong((uint32)allocated);	
	*sizePtr = _byteswap_ulong((uint32)alignedSize);

	// allocation was OK
	regs.R3 = 0;
	RETURN();
}

uint64 __fastcall XboxKernel_KeBugCheckEx( uint64 ip, cpu::CpuRegs& regs )
{
	GLog.Err( "BugCheck: Encountered unrecoverable system exception: %d (%016LLXX, %016LLXX, %016LLXX, %016LLXX)",
		(uint32)(regs.R3), regs.R4, regs.R5, regs.R6, regs.R7 );
	RETURN();
}

uint32 GCurrentProcessType = xnative::X_PROCTYPE_USER;

uint64 __fastcall XboxKernel_KeGetCurrentProcessType( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_ARG(GCurrentProcessType); // ?
}

uint64 __fastcall XboxKernel_KeSetCurrentProcessType( uint64 ip, cpu::CpuRegs& regs )
{
	const uint32 type = (const uint32) regs.R3;
	DEBUG_CHECK( type >= 0 && type <= 2 );
	GCurrentProcessType = type;
	RETURN();
}

uint64 __fastcall XboxKernel_NtFreeVirtualMemory( uint64 ip, cpu::CpuRegs& regs )
{
	void** basePtr = GetPointer<void*>(regs.R3);
	void* base = (void*)_byteswap_ulong((uint32) *basePtr);
	uint32* sizePtr = GetPointer<uint32>(regs.R4);
	uint32 size = _byteswap_ulong((uint32) *sizePtr);
	uint64 freeType = regs.R5;
	uint64 protect = regs.R6;

	char* decoimtFlag = (freeType & xnative::XMEM_DECOMMIT) ? "DECOMIT " : "";
	char* releaseFlag = (freeType & xnative::XMEM_RELEASE) ? "RELEASE " : "";
	GLog.Log( "NtFreeVirtualMemory(size=%d, base=0x%08X, %s%s)", (uint32)size, (uint32)base, decoimtFlag, releaseFlag );

	if (!base)
		return xnative::X_STATUS_MEMORY_NOT_ALLOCATED;

	// translate allocation flags
	uint32 realFreeFlags = 0;
	if ( freeType & xnative::XMEM_COMMIT ) realFreeFlags |= native::IMemory::eAlloc_Commit;
	if ( freeType & xnative::XMEM_RESERVE )  realFreeFlags |= native::IMemory::eAlloc_Reserve;
	if ( freeType & xnative::XMEM_DECOMMIT ) realFreeFlags |= native::IMemory::eAlloc_Decomit;
	if ( freeType & xnative::XMEM_RELEASE )  realFreeFlags |= native::IMemory::eAlloc_Release;

	uint32 freedSize = 0;
	if ( !GPlatform.GetMemory().VirtualFree(base, size, realFreeFlags, freedSize) )
		return xnative::X_STATUS_UNSUCCESSFUL;

	if ( sizePtr )
		*sizePtr = _byteswap_ulong( freedSize );

	RETURN_ARG(0);
}

uint64 __fastcall XboxKernel_RtlCompareMemoryUlong( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_NtQueryVirtualMemory( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_RtlRaiseException( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

struct ErrorLookupTable
{
	uint32 base_code;
	uint32 count;
	const uint32* entries;
};

const uint32 error_table_0x00000103[] =
{
	0x000003E5,  // 0x00000103
	0,           //
	0x000000EA,  // 0x00000105
	0x00000514,  // 0x00000106
	0x00000515,  // 0x00000107
	0,           //
	0,           //
	0,           //
	0,           //
	0x000003FE,  // 0x0000010C
	0x00000516,  // 0x0000010D
};

const uint32 error_table_0x40000002[] =
{
	0x00000057,  // 0x40000002
	0,           //
	0,           //
	0,           //
	0x00000517,  // 0x40000006
	0,           //
	0x00000460,  // 0x40000008
	0x000003F6,  // 0x40000009
	0,           //
	0,           //
	0x00000461,  // 0x4000000C
	0x00000518,  // 0x4000000D
};

const uint32 error_table_0x40020056[] =
{
	0x00000720,  // 0x40020056
};

const uint32 error_table_0x400200AF[] =
{
	0x00000779,  // 0x400200AF
};

const uint32 error_table_0x80000001[] =
{
	0x80000001,  // 0x80000001
	0x000003E6,  // 0x80000002
	0x80000003,  // 0x80000003
	0x80000004,  // 0x80000004
	0x000000EA,  // 0x80000005
	0x00000012,  // 0x80000006
	0,           //
	0,           //
	0,           //
	0,           //
	0x0000056F,  // 0x8000000B
	0,           //
	0x0000012B,  // 0x8000000D
	0x0000001C,  // 0x8000000E
	0x00000015,  // 0x8000000F
	0x00000015,  // 0x80000010
	0x000000AA,  // 0x80000011
	0x00000103,  // 0x80000012
	0x000000FE,  // 0x80000013
	0x000000FF,  // 0x80000014
	0x000000FF,  // 0x80000015
	0x00000456,  // 0x80000016
	0,           //
	0,           //
	0,           //
	0x00000103,  // 0x8000001A
	0x0000044D,  // 0x8000001B
	0x00000456,  // 0x8000001C
	0x00000457,  // 0x8000001D
	0x0000044C,  // 0x8000001E
	0x0000044E,  // 0x8000001F
	0,           //
	0x0000044F,  // 0x80000021
	0x00000450,  // 0x80000022
	0,           //
	0,           //
	0x00000962,  // 0x80000025
};

const uint32 error_table_0x80000288[] =
{
	0x0000048D,  // 0x80000288
	0x0000048E,  // 0x80000289
};

const uint32 error_table_0x80090300[] =
{
	0x000005AA,  // 0x80090300
	0x00000006,  // 0x80090301
	0x00000001,  // 0x80090302
	0x00000035,  // 0x80090303
	0x0000054F,  // 0x80090304
	0x00000554,  // 0x80090305
	0x00000120,  // 0x80090306
	0x00000554,  // 0x80090307
	0x00000057,  // 0x80090308
	0x00000057,  // 0x80090309
	0x00000032,  // 0x8009030A
	0x00000558,  // 0x8009030B
	0x0000052E,  // 0x8009030C
	0x00000057,  // 0x8009030D
	0x00000520,  // 0x8009030E
	0x00000005,  // 0x8009030F
	0x00000005,  // 0x80090310
	0x0000051F,  // 0x80090311
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000554,  // 0x80090316
	0,           //
	0x000006F8,  // 0x80090318
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000057,  // 0x80090320
	0x0000007A,  // 0x80090321
	0x00000574,  // 0x80090322
	0,           //
	0,           //
	0x000006FE,  // 0x80090325
	0x00000057,  // 0x80090326
	0x00000057,  // 0x80090327
	0x00000532,  // 0x80090328
	0x00001770,  // 0x80090329
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x00001771,  // 0x80090330
	0x00000001,  // 0x80090331
};

const uint32 error_table_0xC0000001[] =
{
	0x0000001F,  // 0xC0000001
	0x00000001,  // 0xC0000002
	0x00000057,  // 0xC0000003
	0x00000018,  // 0xC0000004
	0x000003E6,  // 0xC0000005
	0x000003E7,  // 0xC0000006
	0x000005AE,  // 0xC0000007
	0x00000006,  // 0xC0000008
	0x000003E9,  // 0xC0000009
	0x000000C1,  // 0xC000000A
	0x00000057,  // 0xC000000B
	0,           //
	0x00000057,  // 0xC000000D
	0x00000002,  // 0xC000000E
	0x00000002,  // 0xC000000F
	0x00000001,  // 0xC0000010
	0x00000026,  // 0xC0000011
	0x00000022,  // 0xC0000012
	0x00000015,  // 0xC0000013
	0x000006F9,  // 0xC0000014
	0x0000001B,  // 0xC0000015
	0x000000EA,  // 0xC0000016
	0x00000008,  // 0xC0000017
	0x000001E7,  // 0xC0000018
	0x000001E7,  // 0xC0000019
	0x00000057,  // 0xC000001A
	0x00000057,  // 0xC000001B
	0x00000001,  // 0xC000001C
	0xC000001D,  // 0xC000001D
	0x00000005,  // 0xC000001E
	0x00000005,  // 0xC000001F
	0x000000C1,  // 0xC0000020
	0x00000005,  // 0xC0000021
	0x00000005,  // 0xC0000022
	0x0000007A,  // 0xC0000023
	0x00000006,  // 0xC0000024
	0xC0000025,  // 0xC0000025
	0xC0000026,  // 0xC0000026
	0,           //
	0,           //
	0,           //
	0x0000009E,  // 0xC000002A
	0xC000002B,  // 0xC000002B
	0x000001E7,  // 0xC000002C
	0x000001E7,  // 0xC000002D
	0,           //
	0,           //
	0x00000057,  // 0xC0000030
	0,           //
	0x00000571,  // 0xC0000032
	0x0000007B,  // 0xC0000033
	0x00000002,  // 0xC0000034
	0x000000B7,  // 0xC0000035
	0,           //
	0x00000006,  // 0xC0000037
	0,           //
	0x000000A1,  // 0xC0000039
	0x00000003,  // 0xC000003A
	0x000000A1,  // 0xC000003B
	0x0000045D,  // 0xC000003C
	0x0000045D,  // 0xC000003D
	0x00000017,  // 0xC000003E
	0x00000017,  // 0xC000003F
	0x00000008,  // 0xC0000040
	0x00000005,  // 0xC0000041
	0x00000006,  // 0xC0000042
	0x00000020,  // 0xC0000043
	0x00000718,  // 0xC0000044
	0x00000057,  // 0xC0000045
	0x00000120,  // 0xC0000046
	0x0000012A,  // 0xC0000047
	0x00000057,  // 0xC0000048
	0x00000057,  // 0xC0000049
	0x0000009C,  // 0xC000004A
	0x00000005,  // 0xC000004B
	0x00000057,  // 0xC000004C
	0x00000057,  // 0xC000004D
	0x00000057,  // 0xC000004E
	0x0000011A,  // 0xC000004F
	0x000000FF,  // 0xC0000050
	0x00000570,  // 0xC0000051
	0x00000570,  // 0xC0000052
	0x00000570,  // 0xC0000053
	0x00000021,  // 0xC0000054
	0x00000021,  // 0xC0000055
	0x00000005,  // 0xC0000056
	0x00000032,  // 0xC0000057
	0x00000519,  // 0xC0000058
	0x0000051A,  // 0xC0000059
	0x0000051B,  // 0xC000005A
	0x0000051C,  // 0xC000005B
	0x0000051D,  // 0xC000005C
	0x0000051E,  // 0xC000005D
	0x0000051F,  // 0xC000005E
	0x00000520,  // 0xC000005F
	0x00000521,  // 0xC0000060
	0x00000522,  // 0xC0000061
	0x00000523,  // 0xC0000062
	0x00000524,  // 0xC0000063
	0x00000525,  // 0xC0000064
	0x00000526,  // 0xC0000065
	0x00000527,  // 0xC0000066
	0x00000528,  // 0xC0000067
	0x00000529,  // 0xC0000068
	0x0000052A,  // 0xC0000069
	0x00000056,  // 0xC000006A
	0x0000052C,  // 0xC000006B
	0x0000052D,  // 0xC000006C
	0x0000052E,  // 0xC000006D
	0x0000052F,  // 0xC000006E
	0x00000530,  // 0xC000006F
	0x00000531,  // 0xC0000070
	0x00000532,  // 0xC0000071
	0x00000533,  // 0xC0000072
	0x00000534,  // 0xC0000073
	0x00000535,  // 0xC0000074
	0x00000536,  // 0xC0000075
	0x00000537,  // 0xC0000076
	0x00000538,  // 0xC0000077
	0x00000539,  // 0xC0000078
	0x0000053A,  // 0xC0000079
	0x0000007F,  // 0xC000007A
	0x000000C1,  // 0xC000007B
	0x000003F0,  // 0xC000007C
	0x0000053C,  // 0xC000007D
	0x0000009E,  // 0xC000007E
	0x00000070,  // 0xC000007F
	0x0000053D,  // 0xC0000080
	0x0000053E,  // 0xC0000081
	0x00000044,  // 0xC0000082
	0x00000103,  // 0xC0000083
	0x0000053F,  // 0xC0000084
	0x00000103,  // 0xC0000085
	0x0000009A,  // 0xC0000086
	0x0000000E,  // 0xC0000087
	0x000001E7,  // 0xC0000088
	0x00000714,  // 0xC0000089
	0x00000715,  // 0xC000008A
	0x00000716,  // 0xC000008B
	0xC000008C,  // 0xC000008C
	0xC000008D,  // 0xC000008D
	0xC000008E,  // 0xC000008E
	0xC000008F,  // 0xC000008F
	0xC0000090,  // 0xC0000090
	0xC0000091,  // 0xC0000091
	0xC0000092,  // 0xC0000092
	0xC0000093,  // 0xC0000093
	0xC0000094,  // 0xC0000094
	0x00000216,  // 0xC0000095
	0xC0000096,  // 0xC0000096
	0x00000008,  // 0xC0000097
	0x000003EE,  // 0xC0000098
	0x00000540,  // 0xC0000099
	0x000005AA,  // 0xC000009A
	0x00000003,  // 0xC000009B
	0x00000017,  // 0xC000009C
	0x0000048F,  // 0xC000009D
	0x00000015,  // 0xC000009E
	0x000001E7,  // 0xC000009F
	0x000001E7,  // 0xC00000A0
	0x000005AD,  // 0xC00000A1
	0x00000013,  // 0xC00000A2
	0x00000015,  // 0xC00000A3
	0x00000541,  // 0xC00000A4
	0x00000542,  // 0xC00000A5
	0x00000543,  // 0xC00000A6
	0x00000544,  // 0xC00000A7
	0x00000545,  // 0xC00000A8
	0x00000057,  // 0xC00000A9
	0,           //
	0x000000E7,  // 0xC00000AB
	0x000000E7,  // 0xC00000AC
	0x000000E6,  // 0xC00000AD
	0x000000E7,  // 0xC00000AE
	0x00000001,  // 0xC00000AF
	0x000000E9,  // 0xC00000B0
	0x000000E8,  // 0xC00000B1
	0x00000217,  // 0xC00000B2
	0x00000218,  // 0xC00000B3
	0x000000E6,  // 0xC00000B4
	0x00000079,  // 0xC00000B5
	0x00000026,  // 0xC00000B6
	0,           //
	0,           //
	0,           //
	0x00000005,  // 0xC00000BA
	0x00000032,  // 0xC00000BB
	0x00000033,  // 0xC00000BC
	0x00000034,  // 0xC00000BD
	0x00000035,  // 0xC00000BE
	0x00000036,  // 0xC00000BF
	0x00000037,  // 0xC00000C0
	0x00000038,  // 0xC00000C1
	0x00000039,  // 0xC00000C2
	0x0000003A,  // 0xC00000C3
	0x0000003B,  // 0xC00000C4
	0x0000003C,  // 0xC00000C5
	0x0000003D,  // 0xC00000C6
	0x0000003E,  // 0xC00000C7
	0x0000003F,  // 0xC00000C8
	0x00000040,  // 0xC00000C9
	0x00000041,  // 0xC00000CA
	0x00000042,  // 0xC00000CB
	0x00000043,  // 0xC00000CC
	0x00000044,  // 0xC00000CD
	0x00000045,  // 0xC00000CE
	0x00000046,  // 0xC00000CF
	0x00000047,  // 0xC00000D0
	0x00000048,  // 0xC00000D1
	0x00000058,  // 0xC00000D2
	0,           //
	0x00000011,  // 0xC00000D4
	0x00000005,  // 0xC00000D5
	0x000000F0,  // 0xC00000D6
	0x00000546,  // 0xC00000D7
	0,           //
	0x000000E8,  // 0xC00000D9
	0x00000547,  // 0xC00000DA
	0,           //
	0x00000548,  // 0xC00000DC
	0x00000549,  // 0xC00000DD
	0x0000054A,  // 0xC00000DE
	0x0000054B,  // 0xC00000DF
	0x0000054C,  // 0xC00000E0
	0x0000054D,  // 0xC00000E1
	0x0000012C,  // 0xC00000E2
	0x0000012D,  // 0xC00000E3
	0x0000054E,  // 0xC00000E4
	0x0000054F,  // 0xC00000E5
	0x00000550,  // 0xC00000E6
	0x00000551,  // 0xC00000E7
	0x000006F8,  // 0xC00000E8
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000552,  // 0xC00000ED
	0x00000553,  // 0xC00000EE
	0x00000057,  // 0xC00000EF
	0x00000057,  // 0xC00000F0
	0x00000057,  // 0xC00000F1
	0x00000057,  // 0xC00000F2
	0x00000057,  // 0xC00000F3
	0x00000057,  // 0xC00000F4
	0x00000057,  // 0xC00000F5
	0x00000057,  // 0xC00000F6
	0x00000057,  // 0xC00000F7
	0x00000057,  // 0xC00000F8
	0x00000057,  // 0xC00000F9
	0x00000057,  // 0xC00000FA
	0x00000003,  // 0xC00000FB
	0,           //
	0x000003E9,  // 0xC00000FD
	0x00000554,  // 0xC00000FE
	0,           //
	0x000000CB,  // 0xC0000100
	0x00000091,  // 0xC0000101
	0x00000570,  // 0xC0000102
	0x0000010B,  // 0xC0000103
	0x00000555,  // 0xC0000104
	0x00000556,  // 0xC0000105
	0x000000CE,  // 0xC0000106
	0x00000961,  // 0xC0000107
	0x00000964,  // 0xC0000108
	0x0000013D,  // 0xC0000109
	0x00000005,  // 0xC000010A
	0x00000557,  // 0xC000010B
	0,           //
	0x00000558,  // 0xC000010D
	0x00000420,  // 0xC000010E
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x000005A4,  // 0xC0000117
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x000003EE,  // 0xC000011E
	0x00000004,  // 0xC000011F
	0x000003E3,  // 0xC0000120
	0x00000005,  // 0xC0000121
	0x000004BA,  // 0xC0000122
	0x00000005,  // 0xC0000123
	0x0000055B,  // 0xC0000124
	0x0000055C,  // 0xC0000125
	0x0000055D,  // 0xC0000126
	0x0000055E,  // 0xC0000127
	0x00000006,  // 0xC0000128
	0,           //
	0,           //
	0x0000055F,  // 0xC000012B
	0,           //
	0x000005AF,  // 0xC000012D
	0,           //
	0,           //
	0x000000C1,  // 0xC0000130
	0,           //
	0,           //
	0x00000576,  // 0xC0000133
	0,           //
	0x0000007E,  // 0xC0000135
	0,           //
	0,           //
	0x000000B6,  // 0xC0000138
	0x0000007F,  // 0xC0000139
	0,           //
	0x00000040,  // 0xC000013B
	0x00000040,  // 0xC000013C
	0x00000033,  // 0xC000013D
	0x0000003B,  // 0xC000013E
	0x0000003B,  // 0xC000013F
	0x0000003B,  // 0xC0000140
	0x0000003B,  // 0xC0000141
	0x0000045A,  // 0xC0000142
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x0000007C,  // 0xC0000148
	0x00000056,  // 0xC0000149
	0,           //
	0x0000006D,  // 0xC000014B
	0x000003F1,  // 0xC000014C
	0x000003F8,  // 0xC000014D
	0,           //
	0x000003ED,  // 0xC000014F
	0x0000045E,  // 0xC0000150
	0x00000560,  // 0xC0000151
	0x00000561,  // 0xC0000152
	0x00000562,  // 0xC0000153
	0x00000563,  // 0xC0000154
	0x00000564,  // 0xC0000155
	0x00000565,  // 0xC0000156
	0x00000566,  // 0xC0000157
	0x00000567,  // 0xC0000158
	0x000003EF,  // 0xC0000159
	0x00000568,  // 0xC000015A
	0x00000569,  // 0xC000015B
	0x000003F9,  // 0xC000015C
	0x0000056A,  // 0xC000015D
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000459,  // 0xC0000162
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000466,  // 0xC0000169
	0x00000467,  // 0xC000016A
	0x00000468,  // 0xC000016B
	0x0000045F,  // 0xC000016C
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000451,  // 0xC0000172
	0x00000452,  // 0xC0000173
	0x00000453,  // 0xC0000174
	0x00000454,  // 0xC0000175
	0x00000455,  // 0xC0000176
	0x00000469,  // 0xC0000177
	0x00000458,  // 0xC0000178
	0,           //
	0x0000056B,  // 0xC000017A
	0x0000056C,  // 0xC000017B
	0x000003FA,  // 0xC000017C
	0x000003FB,  // 0xC000017D
	0x0000056D,  // 0xC000017E
	0x0000056E,  // 0xC000017F
	0x000003FC,  // 0xC0000180
	0x000003FD,  // 0xC0000181
	0x00000057,  // 0xC0000182
	0x0000045D,  // 0xC0000183
	0x00000016,  // 0xC0000184
	0x0000045D,  // 0xC0000185
	0x0000045D,  // 0xC0000186
	0,           //
	0x000005DE,  // 0xC0000188
	0x00000013,  // 0xC0000189
	0x000006FA,  // 0xC000018A
	0x000006FB,  // 0xC000018B
	0x000006FC,  // 0xC000018C
	0x000006FD,  // 0xC000018D
	0x000005DC,  // 0xC000018E
	0x000005DD,  // 0xC000018F
	0x000006FE,  // 0xC0000190
	0,           //
	0x00000700,  // 0xC0000192
	0x00000701,  // 0xC0000193
	0x0000046B,  // 0xC0000194
	0x000004C3,  // 0xC0000195
	0x000004C4,  // 0xC0000196
	0x000005DF,  // 0xC0000197
	0x0000070F,  // 0xC0000198
	0x00000710,  // 0xC0000199
	0x00000711,  // 0xC000019A
	0x00000712,  // 0xC000019B
};

const uint32 error_table_0xC0000202[] =
{
	0x00000572,  // 0xC0000202
	0x0000003B,  // 0xC0000203
	0x00000717,  // 0xC0000204
	0x0000046A,  // 0xC0000205
	0x000006F8,  // 0xC0000206
	0x000004BE,  // 0xC0000207
	0x000004BE,  // 0xC0000208
	0x00000044,  // 0xC0000209
	0x00000034,  // 0xC000020A
	0x00000040,  // 0xC000020B
	0x00000040,  // 0xC000020C
	0x00000040,  // 0xC000020D
	0x00000044,  // 0xC000020E
	0x0000003B,  // 0xC000020F
	0x0000003B,  // 0xC0000210
	0x0000003B,  // 0xC0000211
	0x0000003B,  // 0xC0000212
	0x0000003B,  // 0xC0000213
	0x0000003B,  // 0xC0000214
	0x0000003B,  // 0xC0000215
	0x00000032,  // 0xC0000216
	0x00000032,  // 0xC0000217
	0,           //
	0,           //
	0,           //
	0,           //
	0x000017E6,  // 0xC000021C
	0,           //
	0,           //
	0,           //
	0x0000046C,  // 0xC0000220
	0x000000C1,  // 0xC0000221
	0,           //
	0,           //
	0x00000773,  // 0xC0000224
	0x00000490,  // 0xC0000225
	0,           //
	0,           //
	0,           //
	0,           //
	0xC000022A,  // 0xC000022A
	0xC000022B,  // 0xC000022B
	0,           //
	0x000004D5,  // 0xC000022D
	0,           //
	0,           //
	0x00000492,  // 0xC0000230
	0,           //
	0,           //
	0x00000774,  // 0xC0000233
	0x00000775,  // 0xC0000234
	0x00000006,  // 0xC0000235
	0x000004C9,  // 0xC0000236
	0x000004CA,  // 0xC0000237
	0x000004CB,  // 0xC0000238
	0x000004CC,  // 0xC0000239
	0x000004CD,  // 0xC000023A
	0x000004CE,  // 0xC000023B
	0x000004CF,  // 0xC000023C
	0x000004D0,  // 0xC000023D
	0x000004D1,  // 0xC000023E
	0x000004D2,  // 0xC000023F
	0x000004D3,  // 0xC0000240
	0x000004D4,  // 0xC0000241
	0,           //
	0x000004C8,  // 0xC0000243
	0,           //
	0,           //
	0x000004D6,  // 0xC0000246
	0x000004D7,  // 0xC0000247
	0x000004D8,  // 0xC0000248
	0x000000C1,  // 0xC0000249
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x0000054F,  // 0xC0000253
	0,           //
	0,           //
	0,           //
	0x000004D0,  // 0xC0000257
	0,           //
	0x00000573,  // 0xC0000259
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000422,  // 0xC000025E
	0,           //
	0,           //
	0,           //
	0x000000B6,  // 0xC0000262
	0x0000007F,  // 0xC0000263
	0x00000120,  // 0xC0000264
	0x00000476,  // 0xC0000265
	0,           //
	0x000010FE,  // 0xC0000267
	0,           //
	0,           //
	0,           //
	0,           //
	0x000007D1,  // 0xC000026C
	0x000004B1,  // 0xC000026D
	0x00000015,  // 0xC000026E
	0,           //
	0,           //
	0,           //
	0x00000491,  // 0xC0000272
	0,           //
	0,           //
	0x00001126,  // 0xC0000275
	0x00001129,  // 0xC0000276
	0x0000112A,  // 0xC0000277
	0x00001128,  // 0xC0000278
	0x00000780,  // 0xC0000279
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000781,  // 0xC0000280
	0x000000A1,  // 0xC0000281
	0,           //
	0x00000488,  // 0xC0000283
	0x00000489,  // 0xC0000284
	0x0000048A,  // 0xC0000285
	0x0000048B,  // 0xC0000286
	0x0000048C,  // 0xC0000287
	0,           //
	0,           //
	0x00000005,  // 0xC000028A
	0x00000005,  // 0xC000028B
	0,           //
	0x00000005,  // 0xC000028D
	0x00000005,  // 0xC000028E
	0x00000005,  // 0xC000028F
	0x00000005,  // 0xC0000290
	0x00001777,  // 0xC0000291
	0x00001778,  // 0xC0000292
	0x00001772,  // 0xC0000293
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000001,  // 0xC000029C
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x00001127,  // 0xC00002B2
	0,           //
	0,           //
	0,           //
	0x00000651,  // 0xC00002B6
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000575,  // 0xC00002C3
	0,           //
	0x000003E6,  // 0xC00002C5
	0,           //
	0,           //
	0,           //
	0,           //
	0x000010E8,  // 0xC00002CA
	0,           //
	0x000004E3,  // 0xC00002CC
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000052,  // 0xC00002EA
};

const uint32 error_table_0xC0020001[] =
{
	0x000006A4,  // 0xC0020001
	0x000006A5,  // 0xC0020002
	0x00000006,  // 0xC0020003
	0x000006A7,  // 0xC0020004
	0x000006A8,  // 0xC0020005
	0x000006A9,  // 0xC0020006
	0x000006AA,  // 0xC0020007
	0x000006AB,  // 0xC0020008
	0x000006AC,  // 0xC0020009
	0x000006AD,  // 0xC002000A
	0x000006AE,  // 0xC002000B
	0x000006AF,  // 0xC002000C
	0x000006B0,  // 0xC002000D
	0x000006B1,  // 0xC002000E
	0x000006B2,  // 0xC002000F
	0x000006B3,  // 0xC0020010
	0x000006B4,  // 0xC0020011
	0x000006B5,  // 0xC0020012
	0x000006B6,  // 0xC0020013
	0x000006B7,  // 0xC0020014
	0x000006B8,  // 0xC0020015
	0x000006B9,  // 0xC0020016
	0x000006BA,  // 0xC0020017
	0x000006BB,  // 0xC0020018
	0x000006BC,  // 0xC0020019
	0x000006BD,  // 0xC002001A
	0x000006BE,  // 0xC002001B
	0x000006BF,  // 0xC002001C
	0x000006C0,  // 0xC002001D
	0,           //
	0x000006C2,  // 0xC002001F
	0,           //
	0x000006C4,  // 0xC0020021
	0x000006C5,  // 0xC0020022
	0x000006C6,  // 0xC0020023
	0x000006C7,  // 0xC0020024
	0x000006C8,  // 0xC0020025
	0x000006C9,  // 0xC0020026
	0,           //
	0x000006CB,  // 0xC0020028
	0x000006CC,  // 0xC0020029
	0x000006CD,  // 0xC002002A
	0x000006CE,  // 0xC002002B
	0x000006CF,  // 0xC002002C
	0x000006D0,  // 0xC002002D
	0x000006D1,  // 0xC002002E
	0x000006D2,  // 0xC002002F
	0x000006D3,  // 0xC0020030
	0x000006D4,  // 0xC0020031
	0x000006D5,  // 0xC0020032
	0x000006D6,  // 0xC0020033
	0x000006D7,  // 0xC0020034
	0x000006D8,  // 0xC0020035
	0x000006D9,  // 0xC0020036
	0x000006DA,  // 0xC0020037
	0x000006DB,  // 0xC0020038
	0x000006DC,  // 0xC0020039
	0x000006DD,  // 0xC002003A
	0x000006DE,  // 0xC002003B
	0x000006DF,  // 0xC002003C
	0x000006E0,  // 0xC002003D
	0x000006E1,  // 0xC002003E
	0x000006E2,  // 0xC002003F
	0x000006E3,  // 0xC0020040
	0x000006E4,  // 0xC0020041
	0x000006E5,  // 0xC0020042
	0x000006E6,  // 0xC0020043
	0x000006E7,  // 0xC0020044
	0x000006E8,  // 0xC0020045
	0x000006E9,  // 0xC0020046
	0x000006EA,  // 0xC0020047
	0x000006EB,  // 0xC0020048
	0x000006FF,  // 0xC0020049
	0x0000070E,  // 0xC002004A
	0x0000076A,  // 0xC002004B
	0x0000076B,  // 0xC002004C
	0x0000076C,  // 0xC002004D
	0,           //
	0x00000719,  // 0xC002004F
	0x0000071A,  // 0xC0020050
	0x0000071B,  // 0xC0020051
	0x0000071C,  // 0xC0020052
	0x0000071D,  // 0xC0020053
	0x0000071E,  // 0xC0020054
	0x0000071F,  // 0xC0020055
	0,           //
	0x00000721,  // 0xC0020057
	0x00000722,  // 0xC0020058
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x0000077A,  // 0xC0020062
	0x0000077B,  // 0xC0020063
};

const uint32 error_table_0xC0030001[] =
{
	0x000006EC,  // 0xC0030001
	0x000006ED,  // 0xC0030002
	0x000006EE,  // 0xC0030003
	0x00000006,  // 0xC0030004
	0x00000006,  // 0xC0030005
	0x000006F1,  // 0xC0030006
	0x000006F2,  // 0xC0030007
	0x000006F3,  // 0xC0030008
	0x000006F4,  // 0xC0030009
	0x000006F5,  // 0xC003000A
	0x000006F6,  // 0xC003000B
	0x000006F7,  // 0xC003000C
};

const uint32 error_table_0xC0030059[] =
{
	0x00000723,  // 0xC0030059
	0x00000724,  // 0xC003005A
	0x00000725,  // 0xC003005B
	0x00000726,  // 0xC003005C
	0,           //
	0x00000728,  // 0xC003005E
	0x0000077C,  // 0xC003005F
	0x0000077D,  // 0xC0030060
	0x0000077E,  // 0xC0030061
};

const uint32 error_table_0xC0050003[] =
{
	0x0000045D,  // 0xC0050003
	0x00000456,  // 0xC0050004
};

const uint32 error_table_0xC0980001[] =
{
	0x00000037,  // 0xC0980001
	0x00000037,  // 0xC0980002
	0,           //
	0,           //
	0,           //
	0,           //
	0,           //
	0x00000037,  // 0xC0980008
};

#define MAKE_ENTRY(x) { x, sizeof(error_table_##x) / sizeof(error_table_##x[0]), error_table_##x }
const ErrorLookupTable error_tables[] =
{
	MAKE_ENTRY(0x00000103), 
	MAKE_ENTRY(0x40000002), 
	MAKE_ENTRY(0x40020056),
	MAKE_ENTRY(0x400200AF), 
	MAKE_ENTRY(0x80000001), 
	MAKE_ENTRY(0x80000288),
	MAKE_ENTRY(0x80090300),
	MAKE_ENTRY(0xC0000001), 
	MAKE_ENTRY(0xC0000202),
	MAKE_ENTRY(0xC0020001), 
	MAKE_ENTRY(0xC0030001), 
	MAKE_ENTRY(0xC0030059),
	MAKE_ENTRY(0xC0050003), 
	MAKE_ENTRY(0xC0980001),
	{ 0, 0, nullptr },
};
#undef MAKE_ENTRY

uint64 __fastcall XboxKernel_RtlNtStatusToDosError( uint64 ip, cpu::CpuRegs& regs )
{
	uint32 status = (const uint32) regs.R3;
	GLog.Log("RtlNtStatusToDosError(%04X)", status);

	if (!status || (status & 0x20000000))
	{
	    RETURN_ARG(0);
	}
	else if ((status & 0xF0000000) == 0xD0000000)
	{
		// High bit doesn't matter.
		status &= ~0x30000000;
	}

	// scan error tables
	auto error_table = &error_tables[0];
	while (error_table->base_code)
	{
		if (status < error_table->base_code)
			break;

		auto index = status - error_table->base_code;
		if (index < error_table->count)
		{
			uint32 result = error_table->entries[index];
			if (!result)
				break;

			GLog.Log("RtlNtStatusToDosError found %X", result);
			RETURN_ARG(result);
		}
		++error_table;
	}

	if ((status >> 16) == 0xC001)
		return status & 0xFFFF;
	
	RETURN_ARG(317);
}

uint64 __fastcall XboxKernel_KeBugCheck( uint64 ip, cpu::CpuRegs& regs )
{
	GLog.Err( "BugCheck: Encountered unrecoverable system exception: %d",
		(uint32)(regs.R3) );

	RETURN();
}

uint64 __fastcall XboxKernel_RtlFreeAnsiString( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_RtlUnicodeStringToAnsiString( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_RtlInitUnicodeString( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_NtDuplicateObject( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_ObDereferenceObject( uint64 ip, cpu::CpuRegs& regs )
{
	const uint32 objectPtr = (const uint32) regs.R3;
	GLog.Log("ObDereferenceObject(%08Xh)", objectPtr);

	// Check if a dummy value from ObReferenceObjectByHandle.
	if (!objectPtr)
	{
		RETURN_ARG(0);
	}

	// Get the kernel object
	auto* object = GPlatform.GetKernel().ResolveObject( (void*) objectPtr, xenon::NativeKernelObjectType::Unknown );
	if (!object)
	{
		GLog.Err( "Kernel: Pointer %08Xh does not match any kernel object", (uint32)objectPtr );
		RETURN_ARG(0);
	}

	// object cannot be dereferenced
	if (!object->IsRefCounted())
	{
		GLog.Err( "Kernel: Called dereference on object '%s' which is not refcounted (pointer %08Xh)", object->GetName(), (uint32)objectPtr );
		RETURN_ARG(0);
	}

	// dereference the object
	auto* refCountedObject = static_cast<xenon::IKernelObjectRefCounted*>(object);
	refCountedObject->Release();
	RETURN_ARG(0);
}

uint64 __fastcall XboxKernel_ObReferenceObjectByHandle( uint64 ip, cpu::CpuRegs& regs )
{
	const uint32 handle = (const uint32) regs.R3;
	const uint32 objectTypePtr = (const uint32) regs.R4;
	const uint32 outObjectPtr = (const uint32) regs.R5;

	// resolve handle to a kernel object
	auto* object = GPlatform.GetKernel().ResolveHandle( handle, xenon::KernelObjectType::Unknown );
	if ( !object )
	{
		GLog.Warn( "Unresolved object handle: %08Xh", handle );
		RETURN_ARG( xnative::X_ERROR_BAD_ARGUMENTS );
	}

	// object is not refcounted
	if ( !object->IsRefCounted() )
	{
		GLog.Err( "Kernel object is not refcounted!" );
	}

	// add additional reference
	auto* refCountedObject = static_cast<xenon::IKernelObjectRefCounted*>(object);
	refCountedObject->AddRef();

	// resolve native data
	uint32 nativeAddr = 0;;
	if ( objectTypePtr == 0xD017BEEF ) // ExSemaphoreObjectType
	{
		GLog.Err( "Unsupported case" );
		nativeAddr = 0xDEADF00D;
	}
	else if ( objectTypePtr == 0xD01BBEEF ) // ExThreadObjectType
	{
		nativeAddr = static_cast< xenon::KernelThread* >( object )->GetMemoryBlock().GetThreadDataAddr();
	}
	else 
	{
		if ( object->GetType() == xenon::KernelObjectType::Thread )
		{
			// a thread ? if so, get the internal thread data
			nativeAddr = static_cast< xenon::KernelThread* >(object)->GetMemoryBlock().GetThreadDataAddr();
		}
		else
		{
			GLog.Warn( "Object '%s', handle %08Xh has undefined object data", object->GetName(), handle );
			nativeAddr = 0xDEADF00D;
		}
	}

	// save the shit
    if ( outObjectPtr )
	{
		mem::storeAddr<uint32>( outObjectPtr, nativeAddr );
    }

	RETURN_ARG( 0 );
}

uint64 __fastcall XboxKernel_ObCreateSymbolicLink( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_ObDeleteSymbolicLink( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_ExRegisterTitleTerminateNotification( uint64 ip, cpu::CpuRegs& regs )
{
	const uint32 ptr = (uint32) regs.R3;
	const uint32 create = (uint32) regs.R4;
	const uint32 routine =  _byteswap_ulong( ((uint32*)ptr)[0] );
	const uint32 priority = _byteswap_ulong( ((uint32*)ptr)[1] );
	
	GLog.Log( "ExRegisterTitleTerminateNotification, routine=0x%08Xh, prio=%d", routine, priority );
	RETURN_ARG(0);
}

// copied from http://msdn.microsoft.com/en-us/library/ff552263
uint64 __fastcall XboxKernel_RtlFillMemoryUlong( uint64 ip, cpu::CpuRegs& regs )
{
	// _Out_  PVOID Destination,
	// _In_   SIZE_T Length,
	// _In_   ULONG Pattern

	const uint32 destPtr = (const uint32) regs.R3;
	const uint32 size = (const uint32) regs.R4;
	const uint32 pattern = (const uint32) regs.R5;

	const uint32 count = size >> 2;
	const uint32 swappedPattern = _byteswap_ulong( pattern );

	uint32* mem = (uint32*) destPtr;
	for ( uint32 i=0; i<count; ++i )
	{
		mem[i] = swappedPattern;
	}

	RETURN();
}

uint64 __fastcall XboxKernel_KeInitializeDpc( uint64 ip, cpu::CpuRegs& regs )
{
	const uint32 dpcPtr = (const uint32) regs.R3;
	const uint32 addr = (const uint32) regs.R4;
	const uint32 context = (const uint32) regs.R4;

	GLog.Log( "KeIKeInitializeDpc(0x%08X, 0x%08X, 0x%08X)", dpcPtr, addr, context );

	// KDPC (maybe) 0x18 bytes?
	const uint32 type = 19;
	const uint32 importance = 0;
	const uint32 unkown = 0;

	mem::storeAddr<uint32>( dpcPtr+0, (type << 24) | (importance << 16) | (unkown) );
	mem::storeAddr<uint32>( dpcPtr+4, 0);  // flink
	mem::storeAddr<uint32>( dpcPtr+8, 0);  // blink
	mem::storeAddr<uint32>( dpcPtr+12, addr);
	mem::storeAddr<uint32>( dpcPtr+16, context);
	mem::storeAddr<uint32>( dpcPtr+20, 0);  // arg1
	mem::storeAddr<uint32>( dpcPtr+24, 0);  // arg2

	RETURN();
}

uint64 __fastcall XboxKernel_KeInsertQueueDpc( uint64 ip, cpu::CpuRegs& regs )
{
	const uint32 dpcPtr = (const uint32) regs.R3;
	const uint32 arg1 = (const uint32) regs.R4;
	const uint32 arg2 = (const uint32) regs.R5;
	
	GLog.Err("KeInsertQueueDpc(0x%08X, 0x%08X, 0x%08X)", dpcPtr, arg1, arg2 );

	const uint32 listEntryPtr = dpcPtr + 4;

	// Lock dispatcher.
	auto& dispatcher = GPlatform.GetKernel().GetDPCDispatcher();
	dispatcher.Lock();

	// If already in a queue, abort.
	auto* dpcList = dispatcher.GetList();
	if ( dpcList->IsQueued( listEntryPtr ) )
	{
		dispatcher.Unlock();
		RETURN_ARG(0);
	}

	// Prep DPC.
	mem::storeAddr<uint32>( dpcPtr + 20, arg1 );
	mem::storeAddr<uint32>( dpcPtr + 24, arg2 );

	dpcList->Insert( listEntryPtr );
	dispatcher.Unlock();

	RETURN_ARG(1);
}

uint64 __fastcall XboxKernel_KeRemoveQueueDpc( uint64 ip, cpu::CpuRegs& regs )
{
	const uint32 dpcPtr = (const uint32) regs.R3;

	GLog.Err("KeRemoveQueueDpc(0x%p)", dpcPtr);

	bool result = false;

	uint32 listentryPtr = dpcPtr + 4;

	auto& dispatcher = GPlatform.GetKernel().GetDPCDispatcher();
	dispatcher.Lock();

	auto dpcList = dispatcher.GetList();
	if ( dpcList->IsQueued( listentryPtr ) )
	{
		dpcList->Remove( listentryPtr );
		result = true;
	}

	dispatcher.Unlock();

	RETURN_ARG(result ? 1 : 0);
}

static std::mutex GGlobalListMutex;

// http://www.nirsoft.net/kernel_struct/vista/SLIST_HEADER.html
uint64 __fastcall XboxKernel_InterlockedPopEntrySList( uint64 ip, cpu::CpuRegs& regs )
{
	const uint32 plistPtr = (const uint32) regs.R3;

	GLog.Log( "InterlockedPopEntrySList(0x%p)", plistPtr );
	
	std::lock_guard<std::mutex> lock( GGlobalListMutex );

	uint32 first = mem::loadAddr<uint32>( plistPtr );
	if (first == 0)
	{
		RETURN_ARG(0);
	}

	uint32 second = mem::loadAddr<uint32>( first );
	mem::storeAddr< uint32 >( plistPtr, second );

	RETURN_ARG(first);
}

uint64 __fastcall XboxKernel_KeLockL2( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_KeUnlockL2( uint64 ip, cpu::CpuRegs& regs )
{
	RETURN_DEFAULT();
}

uint64 __fastcall XboxKernel_MmQueryAddressProtect( uint64 ip, cpu::CpuRegs& regs )
{
	uint32 address = (uint32)( regs.R3 );

	uint32 protectFlags = GPlatform.GetMemory().GetVirtualMemoryProtectFlags( (void*) address );
	RETURN_ARG( protectFlags );
}

uint64 __fastcall XboxKernel_MmSetAddressProtect( uint64 ip, cpu::CpuRegs& regs )
{
	uint32 address = (uint32)( regs.R3 );
	uint32 size = (uint32)( regs.R4 );
	uint32 protectFlags = (uint32)( regs.R5 ) & 0xF;

	GPlatform.GetMemory().VirtualProtect( (void*) address, size, protectFlags );
	RETURN_ARG( 0 );
}

uint64 __fastcall XboxKernel_MmQueryAllocationSize( uint64 ip, cpu::CpuRegs& regs )
{
	uint32 address = (uint32)( regs.R3 );
	uint32 size = GPlatform.GetMemory().GetVirtualMemoryAllocationSize( (void*) address );
	RETURN_ARG( size );
}

void XboxKernel_Interrupt20(uint64 ip, cpu::CpuRegs& regs)
{

}

void XboxKernel_Interrupt22(uint64 ip, cpu::CpuRegs& regs)
{

}

void RegisterXboxKernel(runtime::Symbols& symbols)
{
	#define REGISTER(x) symbols.RegisterFunction(#x, (runtime::TBlockFunc) &XboxKernel_##x);
	REGISTER(XGetLanguage);
	REGISTER(XGetAVPack);

	REGISTER(DbgPrint);
	REGISTER(KeBugCheckEx);
	REGISTER(KeBugCheck);

	REGISTER(NtAllocateVirtualMemory);
	REGISTER(NtFreeVirtualMemory);
	REGISTER(NtQueryVirtualMemory);

	REGISTER(KeQueryPerformanceFrequency);

	REGISTER(RtlInitUnicodeString);
	REGISTER(RtlInitAnsiString);
	REGISTER(RtlFreeAnsiString);
	REGISTER(RtlUnicodeStringToAnsiString);

	REGISTER(RtlFillMemoryUlong);

	REGISTER(XamLoaderTerminateTitle);
	REGISTER(XamShowMessageBoxUIEx);

	REGISTER(NtDuplicateObject);
	REGISTER(ObDereferenceObject);
	REGISTER(ObReferenceObjectByHandle);
	REGISTER(ObDeleteSymbolicLink);
	REGISTER(ObCreateSymbolicLink);

	REGISTER(ExGetXConfigSetting);
	REGISTER(XexCheckExecutablePrivilege);
	REGISTER(__C_specific_handler);
	REGISTER(RtlImageXexHeaderField);
	REGISTER(HalReturnToFirmware);
	REGISTER(KeGetCurrentProcessType);
	REGISTER(RtlCompareMemoryUlong);
	REGISTER(RtlRaiseException);
	REGISTER(RtlNtStatusToDosError);
	REGISTER(ExRegisterTitleTerminateNotification);
	REGISTER(KeEnableFpuExceptions);

	REGISTER(KeInitializeDpc);
	REGISTER(KeInsertQueueDpc);
	REGISTER(KeRemoveQueueDpc);
	REGISTER(InterlockedPopEntrySList);

	REGISTER(KeLockL2);
	REGISTER(KeUnlockL2);

	REGISTER(KeSetCurrentProcessType);

	REGISTER( MmQueryAddressProtect );
	REGISTER( MmSetAddressProtect );
	REGISTER( MmQueryAllocationSize );

	symbols.RegisterInterrupt(20, (runtime::TInterruptFunc) &XboxKernel_Interrupt20);
	symbols.RegisterInterrupt(22, (runtime::TInterruptFunc) &XboxKernel_Interrupt22);

	#undef REGISTER
}