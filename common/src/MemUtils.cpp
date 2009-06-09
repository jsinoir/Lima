#include "MemUtils.h"
#include "Exceptions.h"

#include <sys/sysinfo.h>
#include <limits.h>
#include <unistd.h>

using namespace lima;

void lima::GetSystemMem(int& mem_unit, int& system_mem)
{
	if (mem_unit < 0)
		throw LIMA_HW_EXC(InvalidValue, "Invalid mem_unit value");

        struct sysinfo s_info;
	if (sysinfo(&s_info) < 0)
		throw LIMA_HW_EXC(Error, "Error calling sysinfo");

        long long tot_mem = s_info.totalram;
	tot_mem *= s_info.mem_unit;

	if (mem_unit == 0) 
		mem_unit = s_info.mem_unit;

	long long huge_blocks = tot_mem / mem_unit;
	if (huge_blocks > INT_MAX)
		throw LIMA_HW_EXC(Error, "Too much memory to be described "
				  "with the given mem_unit");
	system_mem = huge_blocks;
}

void lima::GetPageSize(int& page_size)
{
	page_size = getpagesize();
}
