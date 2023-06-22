#pragma once

// Items here may only be renamed, not moved / deleted / replaced
enum GameId_
{
	GameId_T7,
	GameId_T8,
	GameId_TTT2,
	GameId_TREV,
	GameId_T6,
	GameId_T5,
};

namespace GameVersions
{
	namespace T8
	{
		enum
		{
			CLOSED_NETWORK_TEST,
		};
	}

	namespace T7
	{
		enum
		{
			NONE = 0,

			FROM_TTT2,
			FROM_TREV,
			FROM_T6,
			FROM_T5,
		};
	}

	namespace TTT2
	{
		enum
		{
			RPCS3_BLES01702_0100 = 0,
			RPCS3_BLES01702_0103 = 1,
		};
	}
};