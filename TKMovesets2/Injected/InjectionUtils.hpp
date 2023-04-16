#pragma once

#include <string>
#include <windows.h>

namespace InjectionUtils
{
	// Returns the module address of the given module name inside of the current process the DLL is injected in
	uint64_t GetSelfModuleAddress(std::string moduleName);
}
