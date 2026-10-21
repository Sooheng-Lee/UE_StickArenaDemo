#include "Widget/SAMinimapData.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"
#endif

void USAMinimapData::PrepareForCapture(UWorld* World)
{
#if WITH_EDITOR
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	if (World) World->SendAllEndOfFrameUpdates();
	FlushRenderingCommands();
#endif
}
