#pragma once

#include "CoreMinimal.h"
#include "HubBuildingType.generated.h"

UENUM(BlueprintType)
enum class EHubBuildingType : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	Telescope UMETA(DisplayName = "Telescope"),
	AmmunitionStation UMETA(DisplayName = "Ammunition Station"),
	ArmorPlateStation UMETA(DisplayName = "Armor Plate Station"),
	BoosterStation UMETA(DisplayName = "Booster Station"),
	PlatformStation UMETA(DisplayName = "Platform Station")
};