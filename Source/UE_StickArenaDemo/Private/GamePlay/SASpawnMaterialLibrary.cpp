#include "GamePlay/SASpawnMaterialLibrary.h"
#include "Materials/Material.h"

void USASpawnMaterialLibrary::EnableDissolveInputs(UMaterial* Material)
{
#if WITH_EDITOR
	if (Material && Material->GetEditorOnlyData())
	{
		Material->GetEditorOnlyData()->OpacityMask.UseConstant = false;
		Material->GetEditorOnlyData()->EmissiveColor.UseConstant = false;
		Material->MarkPackageDirty();
	}
#endif
}
