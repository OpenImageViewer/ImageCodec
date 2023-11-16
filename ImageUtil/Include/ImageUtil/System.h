#pragma once
#include <cstdint>

namespace IMUtil
{
	class System
	{
	public:
		static uint32_t GetIdealNumThreadsForMemoryOperations();
	};
}