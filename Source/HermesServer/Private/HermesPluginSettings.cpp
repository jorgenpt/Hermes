// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "HermesPluginSettings.h"

#include <Misc/App.h>

UHermesPluginSettings::UHermesPluginSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultUriScheme = FApp::GetProjectName();

	// Lowercase all the characters, and strip out any characters outside of a-z, 0-9, period, dash, and plus.
	// See RFC3986's definition of a legal URI scheme (https://datatracker.ietf.org/doc/html/rfc3986)
	const int32 SchemeLength = DefaultUriScheme.Len();
	TCHAR* SchemeData = DefaultUriScheme.GetCharArray().GetData();
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
	DefaultUriScheme.LeftInline(DstIndex);

	// The first character can only be an alphabetic character, so strip off any non-alphabetic ones
	while (DefaultUriScheme.Len() != 0 && !FChar::IsAlpha(DefaultUriScheme[0]))
	{
		DefaultUriScheme.RemoveAt(0, 1, false);
	}

	if (DefaultUriScheme.Len() == 0)
	{
		DefaultUriScheme = TEXT("hue4");
	}
}
