// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once

#include <Features/IModularFeature.h>

/**
 * An interface for modular feature implementations that decide which scheme we use for URIs.
 * If you've got a project with multiple branches, you can implement this to differentiate the URI scheme
 * by branch.
 */
struct IHermesUriSchemeProvider : IModularFeature
{
	virtual ~IHermesUriSchemeProvider()
	{
	}

	// IModularFeature name -- use this to register an implementation of IHermesUrlSchemeProvider.
	static FName GetModularFeatureName()
	{
		static FName HermesUriSchemeProviderFeatureName(TEXT("HermesUriSchemeProvider"));
		return HermesUriSchemeProviderFeatureName;
	}

	virtual TOptional<FString> GetPreferredScheme() = 0;
};
