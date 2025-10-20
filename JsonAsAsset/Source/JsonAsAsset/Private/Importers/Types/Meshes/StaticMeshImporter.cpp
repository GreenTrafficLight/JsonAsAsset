// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Types/Meshes/StaticMeshImporter.h"

#include "Dom/JsonObject.h"
#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"

#include "RawMesh.h"

#include "Serialization/MemoryReader.h"

bool UStaticMeshImporter::ImportStaticMesh(UStaticMesh*& OutStaticMesh, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const  {
	UStaticMesh* StaticMesh = nullptr;
	StaticMesh = FindObject<UStaticMesh>(Package, *AssetName);

	if (!StaticMesh) {
		StaticMesh = NewObject<UStaticMesh>(OutermostPkg, *AssetName, RF_Public | RF_Standalone);

		const TSharedPtr<FJsonObject> RenderDataObject = Properties->GetObjectField("RenderData");
		const TArray<TSharedPtr<FJsonValue>> LODsObject = RenderDataObject->GetArrayField(TEXT("LODs"));
		int32 NumLODs = LODsObject.Num();

		FMemoryReader Reader(Data, false);

		// TO DO : Handle LODs by putting a loop here
		for (int32 lodIndex = 0; lodIndex < 1; lodIndex++) {
			const TSharedPtr<FJsonObject> LODObject = LODsObject[lodIndex]->AsObject();

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

			const int32 Stride = 13;
			TArray<float> RawVertexData;
			RawVertexData.SetNum(VertexCount * Stride);
			Reader.Serialize(RawVertexData.GetData(), RawVertexData.Num() * sizeof(float));

			TArray<FVector> Positions;
			TArray<FVector4> Normals;
			TArray<FVector4> Tangents;
			TArray<FVector2D> UVs;
			Positions.SetNum(VertexCount);
			Normals.SetNum(VertexCount);
			Tangents.SetNum(VertexCount);
			UVs.SetNum(VertexCount);

			for (uint32 i = 0; i < VertexCount; ++i)
			{
				int32 Offset = i * Stride;
				Positions[i] = FVector(
					RawVertexData[Offset + 0],
					RawVertexData[Offset + 1],
					RawVertexData[Offset + 2]);

				Normals[i] = FVector4(
					RawVertexData[Offset + 3],
					RawVertexData[Offset + 4],
					RawVertexData[Offset + 5],
					RawVertexData[Offset + 6]);

				Tangents[i] = FVector4(
					RawVertexData[Offset + 7],
					RawVertexData[Offset + 8],
					RawVertexData[Offset + 9],
					RawVertexData[Offset + 10]);

				UVs[i] = FVector2D(
					RawVertexData[Offset + 11],
					RawVertexData[Offset + 12]);
			}

			const TArray<TSharedPtr<FJsonValue>> SectionsObject = LODObject->GetArrayField(TEXT("Sections"));
			int32 NumSections = SectionsObject.Num();

			new(StaticMesh->SourceModels) FStaticMeshSourceModel();
			FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[lodIndex];

			FRawMesh RawMesh;
			SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

			RawMesh.VertexPositions = Positions;

			for (int32 SectionIndex = 0; SectionIndex < 1; SectionIndex++) {
				StaticMesh->StaticMaterials.Add(FStaticMaterial());

				const TSharedPtr<FJsonObject> Section = SectionsObject[SectionIndex]->AsObject();
				int32 MaterialIndex = Section->GetIntegerField(TEXT("MaterialIndex"));
				int32 FirstIndex = Section->GetIntegerField(TEXT("FirstIndex"));
				int32 NumTriangles = Section->GetIntegerField(TEXT("NumTriangles"));

				for (int32 i = FirstIndex; i < FirstIndex + NumTriangles; i++)
				{
					for (int32 j = 0; j < 3; j++) {
						int32 vertexIndex = Indices[i * 3 + j];

						RawMesh.WedgeIndices.Add(vertexIndex);

						RawMesh.WedgeTexCoords[0].Add(UVs[vertexIndex]);

						RawMesh.WedgeTangentX.Add(FVector(Tangents[vertexIndex].X, Tangents[vertexIndex].Y, Tangents[vertexIndex].Z));
						RawMesh.WedgeTangentZ.Add(FVector(Normals[vertexIndex].X, Normals[vertexIndex].Y, Normals[vertexIndex].Z));

						RawMesh.WedgeColors.Add(FColor::White);
					}

					RawMesh.FaceMaterialIndices.Add(MaterialIndex);
					RawMesh.FaceSmoothingMasks.Add(1);
				}
			}

			SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
		}

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
	}

	return false;
}

bool UStaticMeshImporter::ImportStaticMesh_Data(UStaticMesh* InStaticMesh, const TSharedPtr<FJsonObject>& Properties) const  {
	if (InStaticMesh == nullptr) return false;

	// READ LODS

	return true;
}