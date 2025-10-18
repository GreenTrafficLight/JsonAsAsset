// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Types/Meshes/StaticMeshImporter.h"

#include "Dom/JsonObject.h"
#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"

#include "RawMesh.h"

#include "Serialization/MemoryReader.h"

#include "Importers/Importer.h"

bool UStaticMeshImporter::ImportStaticMesh(UStaticMesh*& OutStaticMesh, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const  {
	const TSharedPtr<FJsonObject> RenderDataObject = Properties->GetObjectField("RenderData");
	const TArray<TSharedPtr<FJsonValue>> LODsObject = RenderDataObject->GetArrayField(TEXT("LODs"));
	
	UE_LOG(LogTemp, Log, TEXT("AAAAAAAAAAAAAAAAA"));

	FMemoryReader Reader(Data, false);

	// TO DO : Handle LODs by putting a loop here
	const TSharedPtr<FJsonObject> LODObject = LODsObject[0]->AsObject();

	uint32 IndexCount = 0;
	Reader << IndexCount;
	uint32 VertexCount = 0;
	Reader << VertexCount;

	TArray<uint16> RawIndices;
	RawIndices.SetNum(IndexCount);
	Reader.Serialize(RawIndices.GetData(), IndexCount * sizeof(uint16));

	TArray<int32> Indices;
	Indices.Reserve(RawIndices.Num());

	for (uint16 Idx : RawIndices)
	{
		Indices.Add(static_cast<int32>(Idx));
	}


	TArray<float> RawVertexData;
	RawVertexData.SetNum(VertexCount * 3);
	Reader.Serialize(RawVertexData.GetData(), RawVertexData.Num() * sizeof(float));

	TArray<FVector> VertexData;
	VertexData.SetNum(VertexCount);
	for (uint32 i = 0; i < VertexCount; ++i)
	{
		VertexData[i].X = RawVertexData[i * 3 + 0];
		VertexData[i].Y = RawVertexData[i * 3 + 1];
		VertexData[i].Z = RawVertexData[i * 3 + 2];
	}
	
	const TArray<TSharedPtr<FJsonValue>> SectionsObject = LODObject->GetArrayField(TEXT("Sections"));

	// TO DO : Handle sections
	const TSharedPtr<FJsonObject> Section = SectionsObject[0]->AsObject();
	int32 MaterialIndex = Section->GetIntegerField(TEXT("MaterialIndex"));
	int32 FirstIndex = Section->GetIntegerField(TEXT("FirstIndex"));
	int32 NumTriangles = Section->GetIntegerField(TEXT("NumTriangles")) * 3;

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(OutermostPkg, *FileName, RF_Public | RF_Standalone);
	StaticMesh->StaticMaterials.Add(FStaticMaterial());

	new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];

	FRawMesh RawMesh;
	SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

	///
	/* EXAMPLE WORKING CODE
	// --- 1. Vertices ---
	RawMesh.VertexPositions.Add(FVector(0, 0, 0));
	RawMesh.VertexPositions.Add(FVector(100, 0, 0));
	RawMesh.VertexPositions.Add(FVector(0, 100, 0));

	// --- 2. Indices (1 triangle = 3 wedges) ---
	RawMesh.WedgeIndices.Add(0);
	RawMesh.WedgeIndices.Add(1);
	RawMesh.WedgeIndices.Add(2);

	// --- 3. UVs ---
	RawMesh.WedgeTexCoords[0].Add(FVector2D(0.f, 0.f));
	RawMesh.WedgeTexCoords[0].Add(FVector2D(1.f, 0.f));
	RawMesh.WedgeTexCoords[0].Add(FVector2D(0.f, 1.f));

	// --- 4. Tangents & normals (must match wedge count = 3) ---
	for (int32 i = 0; i < 3; ++i)
	{
		RawMesh.WedgeTangentX.Add(FVector(1, 0, 0));
		RawMesh.WedgeTangentY.Add(FVector(0, 1, 0));
		RawMesh.WedgeTangentZ.Add(FVector(0, 0, 1));
	}

	// --- 5. Face material indices and smoothing masks (one per face = one triangle) ---
	RawMesh.FaceMaterialIndices.Add(0);
	RawMesh.FaceSmoothingMasks.Add(0);
	*/
	///

	///
	/* EXAMPLE WORKING CODE
	TArray<FVector> CubeVertices = {
		FVector(-50, -50, -50),
		FVector(-50,  50, -50),
		FVector(50,  50, -50),
		FVector(50, -50, -50),
		FVector(-50, -50, 50),
		FVector(-50,  50, 50),
		FVector(50,  50, 50),
		FVector(50, -50, 50)
	};

	// Define cube triangles
	TArray<int32> CubeIndices = {
		0,1,2, 0,2,3,  // Bottom
		4,6,5, 4,7,6,  // Top
		0,4,5, 0,5,1,  // Left
		3,2,6, 3,6,7,  // Right
		1,5,6, 1,6,2,  // Front
		0,3,7, 0,7,4   // Back
	};

	RawMesh.VertexPositions = CubeVertices;

	for (int32 i = 0; i < CubeIndices.Num(); i += 3)
	{
		RawMesh.WedgeIndices.Add(CubeIndices[i]);
		RawMesh.WedgeIndices.Add(CubeIndices[i + 1]);
		RawMesh.WedgeIndices.Add(CubeIndices[i + 2]);

		RawMesh.FaceMaterialIndices.Add(0);
		RawMesh.FaceSmoothingMasks.Add(0);
	}

	for (int32 i = 0; i < CubeIndices.Num(); ++i)
	{
		RawMesh.WedgeTangentX.Add(FVector::ZeroVector);
		RawMesh.WedgeTangentY.Add(FVector::ZeroVector);
		RawMesh.WedgeTangentZ.Add(FVector::UpVector);
		RawMesh.WedgeTexCoords[0].Add(FVector2D::ZeroVector);
		RawMesh.WedgeColors.Add(FColor::White);
	}
	*/
	///

	RawMesh.VertexPositions = VertexData;

	for (int32 i = FirstIndex; i < FirstIndex + NumTriangles; i += 3)
	{
		RawMesh.WedgeIndices.Add(Indices[i + 0]);
		RawMesh.WedgeIndices.Add(Indices[i + 1]);
		RawMesh.WedgeIndices.Add(Indices[i + 2]);

		// Add dummy UVs/normals/colors for each wedge
		RawMesh.WedgeTexCoords[0].Add(FVector2D::ZeroVector);
		RawMesh.WedgeTexCoords[0].Add(FVector2D::ZeroVector);
		RawMesh.WedgeTexCoords[0].Add(FVector2D::ZeroVector);

		RawMesh.WedgeTangentX.Add(FVector::ForwardVector);
		RawMesh.WedgeTangentX.Add(FVector::ForwardVector);
		RawMesh.WedgeTangentX.Add(FVector::ForwardVector);

		RawMesh.WedgeTangentZ.Add(FVector::UpVector);
		RawMesh.WedgeTangentZ.Add(FVector::UpVector);
		RawMesh.WedgeTangentZ.Add(FVector::UpVector);

		RawMesh.WedgeColors.Add(FColor::White);
		RawMesh.WedgeColors.Add(FColor::White);
		RawMesh.WedgeColors.Add(FColor::White);

		RawMesh.FaceMaterialIndices.Add(MaterialIndex);
		RawMesh.FaceSmoothingMasks.Add(0);
	}

	SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);

	TArray<FText> BuildErrors;
	StaticMesh->Build(false, &BuildErrors);

	if (BuildErrors.Num() > 0)
	{
		for (const FText& Err : BuildErrors)
		{
			UE_LOG(LogTemp, Warning, TEXT("Build error: %s"), *Err.ToString());
		}
	}


	ImportStaticMesh_Data(StaticMesh, Properties);

	UE_LOG(LogTemp, Log, TEXT("TTTTTTTTTTTTTTTTT"));

	if (StaticMesh) {
		OutStaticMesh = StaticMesh;
		return true;
	}

	return false;
}

bool UStaticMeshImporter::ImportStaticMesh_Data(UStaticMesh* InStaticMesh, const TSharedPtr<FJsonObject>& Properties) const  {
	if (InStaticMesh == nullptr) return false;

	// READ LODS

	return true;
}