// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once

#include "HermesServer.h"
#include "HermesUriSchemeProvider.h"

#include <CoreMinimal.h>

namespace Hermes
{
	/**
	* Filter out any invalid characters to generate a valid scheme from Input.
	*
	* @param Input an arbitrary string that'll be sanitized
	*/
	static FORCEINLINE TOptional<FString> SanitizeScheme(FString Input)
	{
		// Lowercase all the characters, and strip out any characters outside of a-z, 0-9, period, dash, and plus.
		// See RFC3986's definition of a legal URI scheme (https://datatracker.ietf.org/doc/html/rfc3986)
		const int32 SchemeLength = Input.Len();
		TCHAR* SchemeData = Input.GetCharArray().GetData();
		int32 DstIndex = 0;
		for (int32 SrcIndex = 0; SrcIndex < SchemeLength; ++SrcIndex)
		{
			const TCHAR Character = FChar::ToLower(SchemeData[SrcIndex]);
			if (FChar::IsAlnum(Character) || Character == TEXT('.') || Character == TEXT('-') || Character == TEXT('+'))
			{
				SchemeData[DstIndex++] = Character;
			}
		}

		// Shrink the scheme down to only include the characters that were legal
		Input.LeftInline(DstIndex);

		// The first character can only be an alphabetic character, so strip off any non-alphabetic ones
		while (Input.Len() != 0 && !FChar::IsAlpha(Input[0]))
		{
			Input.RemoveAt(0, 1, false);
		}

		if (Input.Len() > 0)
		{
			return Input;
		}

		return TOptional<FString>();
	}
}
