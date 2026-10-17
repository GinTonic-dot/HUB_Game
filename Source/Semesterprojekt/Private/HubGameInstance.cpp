#include "HubGameInstance.h"

int32 UHubGameInstance::GetBuildingLevel(EHubBuildingType BuildingType) const
{
	if (const int32* FoundLevel = BuildingLevels.Find(BuildingType))
	{
		return *FoundLevel;
	}

	return 0;
}

int32 UHubGameInstance::GetBuildingCost(const FHubBuildingData& BuildingData) const
{
	const int32 CurrentLevel = GetBuildingLevel(BuildingData.BuildingType);

	return BuildingData.BaseCost
		+ (CurrentLevel * BuildingData.CostIncreasePerLevel);
}

bool UHubGameInstance::CanPurchaseBuilding(const FHubBuildingData& BuildingData) const
{
	const int32 CurrentLevel = GetBuildingLevel(BuildingData.BuildingType);

	if (CurrentLevel >= BuildingData.MaxLevel)
	{
		return false;
	}

	const int32 Cost = GetBuildingCost(BuildingData);

	return TotalResources >= Cost;
}

bool UHubGameInstance::PurchaseBuilding(const FHubBuildingData& BuildingData)
{
	if (!CanPurchaseBuilding(BuildingData))
	{
		return false;
	}

	const int32 Cost = GetBuildingCost(BuildingData);
	const int32 CurrentLevel = GetBuildingLevel(BuildingData.BuildingType);

	TotalResources -= Cost;

	BuildingLevels.FindOrAdd(BuildingData.BuildingType) = CurrentLevel + 1;

	return true;
}