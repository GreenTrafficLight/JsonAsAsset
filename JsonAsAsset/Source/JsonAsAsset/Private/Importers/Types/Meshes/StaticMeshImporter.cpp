// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/Types/Meshes/StaticMeshImporter.h"

#include "Dom/JsonObject.h"
#include "Utilities/AssetUtilities.h"
#include "Utilities/EngineUtilities.h"

#include "RawMesh.h"
#include "MeshUtilities.h"

#include "Serialization/MemoryReader.h"

bool UStaticMeshImporter::ImportStaticMesh(UStaticMesh*& OutStaticMesh, TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties)  {
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

		if (StaticMesh) {
			OutStaticMesh = StaticMesh;
			return true;
		}
	}

	return false;
}

bool UStaticMeshImporter::ImportSkeletalMesh(USkeletalMesh*& OutSkeletalMesh, TArray<uint8>& Data) {
	const TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

	USkeletalMesh* SkeletalMesh = nullptr;
	SkeletalMesh = FindObject<USkeletalMesh>(Package, *AssetName);

	if (!SkeletalMesh) {
		TObjectPtr<USkeleton> Object;
		LoadObject(&Properties->GetObjectField(TEXT("Skeleton")), Object);

		if (!Object.IsValid()) {
			return false;
		}
		
		USkeleton* Skeleton = Object.Get();
		FReferenceSkeleton RefSkeleton = Skeleton->GetReferenceSkeleton();

		SkeletalMesh = NewObject<USkeletalMesh>(OutermostPkg, *AssetName, RF_Public | RF_Standalone);
		SkeletalMesh->Skeleton = Skeleton;
		SkeletalMesh->RefSkeleton = RefSkeleton;

		const TArray<TSharedPtr<FJsonValue>> LODsObject = JsonObject->GetArrayField(TEXT("LODModels"));
		int32 NumLODs = LODsObject.Num();

		FSkeletalMeshResource* ImportedResource = SkeletalMesh->GetImportedResource();
		ImportedResource->LODModels.Empty();

		FMemoryReader Reader(Data, false);

		for (int32 lodIndex = 0; lodIndex < 1; lodIndex++) {
			const TSharedPtr<FJsonObject> LODObject = LODsObject[lodIndex]->AsObject();
			const int32 NumTexCoords = LODObject->GetIntegerField(TEXT("NumTexCoords"));

			new(ImportedResource->LODModels) FStaticLODModel();
			FStaticLODModel& LODModel = ImportedResource->LODModels[0];
			LODModel.NumTexCoords = NumTexCoords;
			//LODModel.Sections.Empty();


			uint32 IndexCount = 0;
			Reader << IndexCount;
			uint32 VertexCount = 0;
			Reader << VertexCount;

			TArray<uint32> Indices;
			for (uint32 i = 0; i < IndexCount; i++) {
				uint32 Indice;
				Reader << Indice;
				Indices.Add(Indice);
			}

			TArray<FVertInfluence> Influences;
			TArray<FMeshWedge> Wedges;
			TArray<FMeshFace> Faces;
			TArray<int32> PointToOriginalMap;

			TArray<FVector> Positions;
			TArray<FVector4> Normals;
			TArray<FVector4> Tangents;
			TArray<FVector2D> UVs;
			TArray<float> BoneWeights;
			TArray<uint16> BoneIndices;
			for (uint32 i = 0; i < VertexCount; i++) {
				FVector Position;
				FVector4 Normal;
				FVector4 Tangent;
				FVector2D UV;
				uint8 NumBones;

				Reader << Position;
				Positions.Add(Position);
				Reader << Normal;
				Normals.Add(Normal);
				Reader << Tangent;
				Tangents.Add(Tangent);
				Reader << UV;
				UVs.Add(UV);
				
				Reader << NumBones;
				for (uint32 j = 0; j < NumBones; j++) {
					FVertInfluence Inf;
					Inf.VertIndex = i;
					Reader << Inf.Weight;
					Reader << Inf.BoneIndex;
					Influences.Add(Inf);
				}

				FMeshWedge W;
				W.iVertex = i;
				W.UVs[0] = UVs[i];
				Wedges.Add(W);
			}

			for (int32 i = 0; i < Indices.Num(); i += 3)
			{
				FMeshFace Face;
				Face.iWedge[0] = Indices[i + 0];
				Face.iWedge[1] = Indices[i + 1];
				Face.iWedge[2] = Indices[i + 2];
				Face.MeshMaterialIndex = 0;
				Faces.Add(Face);
			}

			for (int32 i = 0; i < Positions.Num(); i++)
				PointToOriginalMap.Add(i);

			TArray<FText> WarnMessages;
			TArray<FName> WarnNames;
			//LODModel.NumTexCoords = 1;

			IMeshUtilities::MeshBuildOptions BuildOptions;
			BuildOptions.bRemoveDegenerateTriangles = true;

			MeshUtilities.BuildSkeletalMesh(
				LODModel,
				RefSkeleton,
				Influences,
				Wedges,
				Faces,
				Positions,
				PointToOriginalMap,
				BuildOptions,
				&WarnMessages,
				&WarnNames
			);

			/*const TArray<TSharedPtr<FJsonValue>> SectionsObject = LODObject->GetArrayField(TEXT("Sections"));
			int32 NumSections = SectionsObject.Num();

			for (int32 SectionIndex = 0; SectionIndex < 1; SectionIndex++) {
				const TSharedPtr<FJsonObject> SectionData = SectionsObject[SectionIndex]->AsObject();
				int32 BaseIndex = SectionData->GetIntegerField(TEXT("BaseIndex"));
				int32 NumTriangles = SectionData->GetIntegerField(TEXT("NumTriangles"));
				int32 BaseVertexIndex = SectionData->GetIntegerField(TEXT("BaseVertexIndex"));
				int32 NumVertices = SectionData->GetIntegerField(TEXT("NumVertices"));
				int32 MaxBoneInfluences = SectionData->GetIntegerField(TEXT("MaxBoneInfluences"));

				FSkelMeshSection& Section = *new(LODModel.Sections) FSkelMeshSection();
				Section.BaseIndex = BaseIndex;
				Section.NumTriangles = NumTriangles;
				Section.BaseVertexIndex = BaseVertexIndex;
				Section.NumVertices = NumVertices;

				for (int32 i = BaseIndex; i < BaseIndex + NumTriangles; i++)
				{
					for (int32 j = 0; j < 3; j++) {
						int32 vertexIndex = Indices[i * 3 + j];

						FSoftSkinVertex NewVertex;
						NewVertex.Position = Positions[vertexIndex];
						NewVertex.UVs[0] = UVs[vertexIndex];
						NewVertex.TangentX = FVector(Tangents[vertexIndex].X, Tangents[vertexIndex].Y, Tangents[vertexIndex].Z);
						NewVertex.TangentZ = FVector(Normals[vertexIndex].X, Normals[vertexIndex].Y, Normals[vertexIndex].Z);
						NewVertex.Color = FColor::White;

						for (int32 k = 0; k < MaxBoneInfluences; k++) {
							NewVertex.InfluenceBones[k] = BoneIndices[vertexIndex];
							NewVertex.InfluenceWeights[k] = BoneWeights[vertexIndex];
						}

						Section.SoftVertices.Add(NewVertex);
					}
				}

				LODModel.MultiSizeIndexContainer.CreateIndexBuffer(sizeof(uint32));
				LODModel.MultiSizeIndexContainer.CopyIndexBuffer(Indices);
			}*/
		}
	}

	if (SkeletalMesh) {
		OutSkeletalMesh = SkeletalMesh;
		return true;
	}

	return false;
}

bool UStaticMeshImporter::ImportStaticMesh_Data(UStaticMesh* InStaticMesh, const TSharedPtr<FJsonObject>& Properties) const  {
	if (InStaticMesh == nullptr) return false;

	GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(Properties->GetObjectField("Properties"),
		{
			"BodySetup",
			"NavCollision",
			"RenderData"
	}), InStaticMesh);

	return true;
}