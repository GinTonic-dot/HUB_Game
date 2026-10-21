#pragma once

#include "CoreMinimal.h"
#include "HubBuildingType.h"
#include "HubBuildingData.generated.h"

USTRUCT(BlueprintType)
struct SEMESTERPROJEKT_API FHubBuildingData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	EHubBuildingType BuildingType = EHubBuildingType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	int32 BaseCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	int32 CostIncreasePerLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	int32 MaxLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	TSubclassOf<AActor> BuildingActorClass = nullptr;
};