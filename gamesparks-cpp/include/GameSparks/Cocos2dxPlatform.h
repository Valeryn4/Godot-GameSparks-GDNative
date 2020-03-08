// Copyright 2015 GameSparks Ltd 2015, Inc. All Rights Reserved.

#ifndef Cocos2dxPlatform_h__
#define Cocos2dxPlatform_h__

#pragma once

#include "./IGSPlatform.h"

#ifndef CC_TARGET_PLATFORM
#	error "This file is meant for inclusion in Cocos2d-x projects only. Please make sure, that this file is included AFTER the Cocos2d-x headers."
#endif /* CC_TARGET_PLATFORM */

namespace GameSparks
{
	namespace Core
	{
		//! IGSPlatform implementation for [Cocos2d-x](http://www.cocos2d-x.org/).
		class Cocos2dxPlatform : public IGSPlatform
		{
			public:
				/*!
					Construct a Cocos2dxPlatform with the given credentials

					@param usePreviewServer true if you want to use the preview server, false if you want to use the production server
					@param verboseLogging true for verbose logging, false for less verbose logging
				*/
				Cocos2dxPlatform(const gsstl::string& apikey, const gsstl::string& secret, bool usePreviewServer, bool verboseLogging=false)
				: IGSPlatform(apikey, secret, usePreviewServer, verboseLogging)
				{}

				//! Gets a unique identifier for the device
				/*virtual gsstl::string GetDeviceId() const
				{
					// TODO: implement
					return "NOT-IMPLEMENTED";
				}*/

				//! Will be used in analytics reports
				gsstl::string GetSDK() const override
				{
					return "GameSparks SDK C++ 1.0 for Cocos2d-x";
				}

				//! Will be used in analytics reports
				gsstl::string GetDeviceType() const override
				{
					// TODO: implement
					return "Cocos2d-x";
				}

				//! Receives debugging information from the API
				void DebugMsg(const gsstl::string& message) const override
				{
					cocos2d::log("[GameSparks]: %s", message.c_str());
				}

				//! If you return a non-empty string, it will be added as "engine" to the deviceStats object reported to the server for analytics.
				gsstl::string GetEngine() const override { return "Cocos2d-x"; }

				//! If you return a non-empty string, it will be added as "engine.version" to the deviceStats object reported to the server for analytics.
				gsstl::string GetEngineVersion() const override { return cocos2d::cocos2dVersion(); }
		};
	}
}
#endif // Cocos2dxPlatform_h__
