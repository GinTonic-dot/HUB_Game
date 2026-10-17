#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Data/HubBuildingType.h"
#include "Data/HubBuildingData.h"
#include "HubGameInstance.generated.h"

UCLASS()
class SEMESTERPROJEKT_API UHubGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
	int32 TotalResources = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
	int32 LastRunResources = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings")
	TMap<EHubBuildingType, int32> BuildingLevels;


	UFUNCTION(BlueprintPure, Category = "Buildings")
	int32 GetBuildingLevel(EHubBuildingType BuildingType) const;

	UFUNCTION(BlueprintPure, Category = "Buildings")
	int32 GetBuildingCost(const FHubBuildingData& BuildingData) const;

	UFUNCTION(BlueprintPure, Category = "Buildings")
	bool CanPurchaseBuilding(const FHubBuildingData& BuildingData) const;
	
	UFUNCTION(BlueprintCallable, Category = "Buildings")
	bool PurchaseBuilding(const FHubBuildingData& BuildingData);
};