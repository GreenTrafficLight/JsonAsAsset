/* Copyright JsonAsAsset Contributors 2024-2025 */

#pragma once

struct FExportData {
	FExportData(const FName Type, const FName Outer, const TSharedPtr<FJsonObject>& Json) {
		this->Type = Type;
		this->Outer = Outer;
		this->Json = Json.Get();
	}

	FExportData(const FString& Type, const FString& Outer, const TSharedPtr<FJsonObject>& Json) {
		this->Type = FName(*Type);
		this->Outer = FName(*Outer);
		this->Json = Json.Get();
	}

	FExportData(const FString& Type, const FString& Outer, FJsonObject* Json) {
		this->Type = FName(*Type);
		this->Outer = FName(*Outer);
		this->Json = Json;
	}

	FName Type;
	FName Outer;
	FJsonObject* Json;
};

/* Conversion Functions ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
inline FVector ObjectToVector(const FJsonObject* Object) {
	return FVector(Object->GetNumberField(TEXT("X")), Object->GetNumberField(TEXT("Y")), Object->GetNumberField(TEXT("Z")));
}

#if ENGINE_UE5
inline FVector3f ObjectToVector3F(const FJsonObject* Object) {
	return FVector3f(Object->GetNumberField(TEXT("X")), Object->GetNumberField(TEXT("Y")), Object->GetNumberField(TEXT("Z")));
}
#else

inline FVector ObjectToVector3F(const FJsonObject* Object) {
	return FVector(Object->GetNumberField(TEXT("X")), Object->GetNumberField(TEXT("Y")), Object->GetNumberField(TEXT("Z")));
}
#endif

inline FRotator ObjectToRotator(const FJsonObject* Object) {
	return FRotator(Object->GetNumberField(TEXT("Pitch")), Object->GetNumberField(TEXT("Yaw")), Object->GetNumberField(TEXT("Roll")));
}

inline FLinearColor ObjectToLinearColor(const FJsonObject* Object) {
	return FLinearColor(Object->GetNumberField(TEXT("R")), Object->GetNumberField(TEXT("G")), Object->GetNumberField(TEXT("B")), Object->GetNumberField(TEXT("A")));
}

inline FRichCurveKey ObjectToRichCurveKey(const TSharedPtr<FJsonObject>& Object) {
	FString InterpMode = Object->GetStringField("InterpMode");

	UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("ERichCurveInterpMode"), true);
	ERichCurveInterpMode Mode = RCIM_Linear;

	if (EnumPtr)
	{
		int64 Value = EnumPtr->GetValueByName(FName(*InterpMode));
		if (Value != INDEX_NONE)
		{
			Mode = static_cast<ERichCurveInterpMode>(Value);
		}
	}

	return FRichCurveKey(
		Object->GetNumberField("Time"),
		Object->GetNumberField("Value"),
		Object->GetNumberField("ArriveTangent"),
		Object->GetNumberField("LeaveTangent"),
		Mode
	);
}