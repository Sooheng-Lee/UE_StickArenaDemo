#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"

namespace SADebugHelper
{
	// Editor viewports and PIE only; editor binaries launched with -game are excluded too.
	inline bool IsEnabled()
	{
#if WITH_EDITOR
		return GIsEditor;
#else
		return false;
#endif
	}

	static void Print(const FString& Msg, FColor DisplayColor = FColor::MakeRandomColor(), float DisplayTime = 7.0f, int32 InKey = -1)
	{
#if WITH_EDITOR
		if (IsEnabled() && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(InKey, DisplayTime, DisplayColor, Msg);
			UE_LOG(LogTemp, Display, TEXT("%s"), *Msg);
		}
#endif
	}
}
