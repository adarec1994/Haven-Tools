///////////////////////////////////////////////////////////////////////  
//  SpeedTreeKeyUtility.h
//
//  *** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with 
//  the terms of that agreement.
//
//      Copyright (c) 2003-2007 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com
//
//  *** Release version 4.1 ***

// dimhotepus: Add missed SpeedTreeKeyUtility basic implementation.

#pragma once

#include <string>

class SpeedTreeKeyUtility
{
public:
	static bool KeyIsValid(const std::string&, std::string&)
	{
		return true;
	}
};