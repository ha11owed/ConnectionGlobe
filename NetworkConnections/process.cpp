#include "process.h"
#include "common.h"
#include "Psapi.h"

#pragma comment(lib, "Psapi.lib")

#define buffSize (4 * 1024)

std::wstring GetProcessFileName(unsigned long pid)
{
	std::wstring ret;
	HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (h != 0)
	{
		TCHAR buffer[buffSize];
		size_t n = GetProcessImageFileName(h, buffer, buffSize);
		if (n > 0)
		{
			ret.assign(buffer, n);
		}
		CloseHandle(h);
	}
	return ret;
}
