#pragma once

namespace SADebugHelper
{
	static void Print(const FString& Msg, FColor DisplayColor = FColor::MakeRandomColor(), float DisplayTime = 7.0f, int32 InKey = -1)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(InKey, DisplayTime, DisplayColor, Msg);
		}
	}
}