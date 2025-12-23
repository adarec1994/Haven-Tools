/**********************************************************************************************************
Dragon Age Origins Tools
Author: Eshme  http://social.bioware.com/project/2336/

latest edit:
13 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Include Script
DAOMaterialImport.ms
Material Import functions
************************************************************************************************************
*/

	struct DAOMaterialStruct (
		TypeString,
		SemanticString,
		Map_Diffuse,
		Map_Normal,
		Map_Specular,
		Map_Emissive,
		Map_Tint,
		Map_AgeDiffuse,
		Map_AgeNormal,
		Map_CorneaNormal,
		Map_Distortion,
		Map_DistortionModifier,
		Map_DistortionVolume,
		Map_Emotions0,
		Map_Emotions1,
		Map_EmotionsNormal,
		Map_Tattoo,
		Map_SkyA,
		Map_SkyB,
		Map_PerlinNoise,
		Map_Fresnel,
		Map_BrowStubble,
		Map_BrowStubbleNormal,
		Map_TintNoise,
		Map_Noise,
		Map_CelestialBody,
		Map_Packed,
		Map_Relief,
		Map_Gore,
		--Map_RMPDecal,
		Map_Heraldry,
		Map_Beckmann,
		Map_WaterMask,
		Map_Decal
	)
	
	struct DAOSemanticStruct ( --??
		Name,
		DisplayName,
		RequirePosition,
		RequireTexCoord0,
		RequireTexCoord1,
		RequireNormal,
		RequireBiNormal,
		RequireTangent,
		RequireColor,
		RequireBlendWeight,
		RequireBlendIndices
	)
	
	struct MAOParseStruct (
		fn toElement Stream Element = (skipToString Stream ("<"+Element)),
		fn toAttribute Stream Attribute = (skipToString Stream (Attribute+"=\"")),
		fn ReadAttribute Stream = (readDelimitedString Stream "\""),
		fn ReadElement Stream = (
			local EndString = readDelimitedString Stream ">"
			local Value
			for i = 1 to EndString.count do
			(
				if EndString[i] == "/" then return ""
			)
			Value = readDelimitedString Stream "</"
			skiptoString Stream (">")
			Value
		),
		fn getNextElement Stream = (
			skipToString Stream ("<")
			local OldPos = FilePos Stream
			local Element = readDelimitedString Stream (">")
			--format ("E  "+Element as string+"Pos:  "+OldPos as string+"\n")
			if Element[1] == "/" or Element == "" then return undefined
			seek Stream OldPos
			Element = readDelimitedString Stream (" ")
		),
		fn ConvertVector4f InString = (
			local OutStrings = filterString InString (" ")
			local Out = #()
			for i = 1 to OutStrings.count do
			(
				if OutStrings[i] != "" do append Out (OutStrings[i] as float)
			)
			Out
		)
	)
	
	fn ImportMaterial MeshObj MaterialObj ApplyMat:true= (
	
		local MAOStream
		local DAOMat = DAOMaterialStruct()
		local MAOParse = MAOParseStruct()
		local Element
		local AttributeName
		local AttributeValue
		local Mat
		local TypeIDs
		local SemanticIDs
		
		local MatInstalled = (DAOMaterial != undefined)	--checks if class is present
		
		if MatInstalled then
		(
			Mat = DAOMaterial Name:MaterialObj
			TypeIDs = GetMatTypeIDs()		--collect translation tables, ID <-> StringID
			SemanticIDs = GetMatSemanticIDs()
		)
		else
		(
			Mat = standardMaterial Name:MaterialObj shadertype:1
		)
		
		MAOFile = SearchFile DAOTools.ImportPaths (getFilenamePath FileStates.MAOFileName) (MaterialObj+".mao") "MAO"
		if MAOFile != undefined then
		(
			MAOStream = OpenFile MAOFile mode:"rt"
			FileStates.MAOFileName = MAOFile
			
			--***************************
			--Run 1 ,get material type
			--***************************
			seek MAOStream 0
			MAOParse.toElement MAOStream "MaterialObject"
			while (Element = MAOParse.getNextElement MAOStream) != undefined do	--Get next Element within MaterialObject
			(
				if (EqualsCaseLess Element "Material") then
				(
					MAOParse.toAttribute MAOStream "Name"
					DAOMat.TypeString = (MAOParse.ReadAttribute MAOStream)
				)
				if (EqualsCaseLess Element "DefaultSemantic") then
				(
					MAOParse.toAttribute MAOStream "Name"
					DAOMat.SemanticString = (MAOParse.ReadAttribute MAOStream)
				)
				MAOParse.ReadElement MAOStream	--Close that Element
			)
			
			--*****************************************
			--Set default Material Values based on Type
			--Only Dragon Age Material
			--******************************************
			if MatInstalled then
			(
				--Store Type and Semantic ID's
				if DAOMat.TypeString != undefined then Mat.MatTypeID = GetDAOMatIDByString TypeIDs DAOMat.TypeString
				if DAOMat.SemanticString != undefined then
				(
					Mat.MatSemanticID = GetDAOMatIDByString SemanticIDs DAOMat.SemanticString
				)
				else
				(	--Setting default semantic ID's, if it wasnt read (shouldnt happen?)
					Mat.MatSemanticID = case Mat.MatTypeID of
					(
						1: 1
						2: 4
						3: 10
						4: 1
						8: 1
						9: 1
						11: 1
						13: 54
						14: 1
						16: 1
						18: 1
						19: 1
						20: 1
					)
				)
				--Unsupported Formats temporary override. Prints a warning that it cannot be imported properly.
				if (finditem #(5,6,7,10,12,15,17) Mat.MatTypeID) > 0 then
				(
					MessageBox ("Materialtype from Object: \""+((GetDAOMatStringByID TypeIDs Mat.MatTypeID) as string)+"\" is unsupported.\nWill assign Default Prop.") title:"Model Import"
					Mat.MatTypeID = 14	--Prop material
					Mat.MatSemanticID = 1	--default semantic
				)
				
				--Now writing parameters
				Mat.ParamPropsInit = false	--this flag must be unset so they're not reinitialized again later
				
				case Mat.MatTypeID of	--setting defaults first. Overwrite with MAO values afterwards ,if present
				(
					1:(		--"Black.mat"
						Mat.eng_vWireframeColor = [0,255,0] as color
					)
					2:(		--Canopy
						Mat.mml_vFalloffParams1 = 1.5
						Mat.mml_vFalloffParams2 = 1.5
						Mat.mml_vFalloffParams3 = 1.0
						Mat.mml_vFalloffParams4 = 0.00
						Mat.mml_fSpecularReflectionMult = 1.0
						Mat.mml_fOpacityMultiplier = 1.0
						Mat.mml_fGoreThreshold = 0.0
						Mat.mml_vTintColor = (color 0 0 0)
						Mat.eng_vWireframeColor = (color 255 255 255)
					)
					3:(		--Character
						Mat.mml_vFalloffParams1 = 1.5
						Mat.mml_vFalloffParams2 = 1.5
						Mat.mml_vFalloffParams3 = 1.0
						Mat.mml_vFalloffParams4 = 0.00
						Mat.mml_fSpecularReflectionMult = 1.0
						Mat.mml_fOpacityMultiplier = 1.0
						Mat.mml_fGoreThreshold = 0.0
						Mat.mml_vTintColor = (color 0 0 0)
						Mat.eng_vWireframeColor = (color 255 255 255)
						--Mat.mml_fRMPInvertFresnel = 0.0
						--Mat.mml_fRMPFresnelFalloff = 0.0
						--Mat.mml_vRMPTintColor = (color 0 0 0)
						--Mat.mml_fRMPAlpha = 0.0
						--Mat.mml_vRMPVariationTint = (color 255 255 255)
					)
					4:(		--Cloth.mat
						Mat.mml_vFalloffParams1 = 1.5
						Mat.mml_vFalloffParams2 = 1.5
						Mat.mml_vFalloffParams3 = 1.0
						Mat.mml_vFalloffParams4 = 0.00
						Mat.mml_fSpecularReflectionMult = 1.0
						Mat.mml_fOpacityMultiplier = 1.0
						Mat.mml_fGoreThreshold = 0.0
						Mat.mml_vTintColor = (color 0 0 0)
						Mat.eng_vWireframeColor = (color 0 0 255)
					)
					5:(
					)
					6:(
					)
					7:(
					)
					8:(		--Eye.mat
						Mat.mml_vEyeSpecularParameters_CSP = 30.0
						Mat.mml_vEyeSpecularParameters_CSM = 0.75
						Mat.mml_vEyeSpecularParameters_SSP = 75.0
						Mat.mml_vEyeSpecularParameters_SSM = 0.75
						Mat.mml_fOpacityMultiplier = 1.0
						Mat.eng_vWireframeColor = (color 0 0 255)
					)
					9:(		--Face.mat
						Mat.mml_vFalloffParams1 = 1.0
						Mat.mml_vFalloffParams2 = 2.0
						Mat.mml_vFalloffParams3 = 0.0
						Mat.mml_vFalloffParams4 = 0.0
						Mat.mml_fOpacityMultiplier = 1.0
						Mat.g_fSpecularMult = 0.2
						Mat.g_fLipSpecularBoost = 0.0
						Mat.g_fRimPower = 0.0
						Mat.mml_fGoreThreshold = 0.0
						Mat.mml_vTintColor = (color 0 0 0)
						Mat.eng_vWireframeColor = (color 179 179 179)
						Mat.g_vAmbientMult = (color 0 0 0)
					)
					10:(
					)
					11:(	--Hairalpha.mat
						Mat.mml_vHairParameters_PSM = 1.0
						Mat.mml_vHairParameters_PSP = 200.0
						Mat.mml_vHairParameters_SSP = 30.0
						Mat.mml_vHairParameters_NTTF = 10.0
						Mat.mml_fOpacityMultiplier = 1.0
						Mat.mml_vTintColor = (color 0 0 0)
						Mat.eng_vWireframeColor = (color 179 179 230)
					)
					12:(
					)
					13:(	--Lava.mat
						Mat.mml_fLavaBrightness = 1
						Mat.mml_fLavaContrast = 1
						Mat.mml_vLavaTint = [255,102,0] as color
						Mat.mml_vUVScrollSpeed_1U = 0.05
						Mat.mml_vUVScrollSpeed_1V = 0.05
						Mat.mml_vUVScrollSpeed_2U = 0.05
						Mat.mml_vUVScrollSpeed_2V = 0.05
						Mat.mml_vMaskUVScrollSpeed_U = 0.05
						Mat.mml_vMaskUVScrollSpeed_V = 0.05
					)
					14:(	--Prop.mat
						Mat.mml_fOpacityMultiplier = 1.0
						Mat.mml_fSpecularReflectionMult = 1.0
						Mat.mml_fSpecularPower = 50.0
						Mat.mml_vFalloffParams1 = 1.0
						Mat.mml_vFalloffParams2 = 2.0
						Mat.mml_vFalloffParams3 = 0.0
						Mat.mml_vFalloffParams4 = 0.0
						Mat.mml_vTintColor = (color 0 0 0)
						Mat.eng_vWireframeColor = (color 127 51 51)
					)
					15:(
					)
					16:(	--Static.mat
						Mat.mml_vFalloffParams1 = 1.5
						Mat.mml_vFalloffParams2 = 1.5
						Mat.mml_vFalloffParams3 = 0.0
						Mat.mml_vFalloffParams4 = 0.0
						Mat.mml_fSpecularReflectionMult = 1.0
						Mat.mml_fOpacityMultiplier = 1.0
						Mat.mml_fReliefAspect = 1.0
						Mat.mml_vTintColor = (color 0 0 0)
						Mat.eng_vWireframeColor = (color 0 255 0)
						Mat.mml_iSoundMaterialType = 0
					)
					17:(
					)
					18:(	--water.mat
						Mat.mml_fdistortion_fade_distance	= 50.0
						Mat.mml_fdistortion_fade_multiplier	= 0.0
						Mat.mml_fdistortion_invert			= 1.0	--true
						Mat.mml_fdistortion_magnitude		= 5.0
		
						Mat.Water_Wave1_Height				= 0.2
						Mat.Water_Wave2_Height				= 0.2
						Mat.Water_Wave3_Height				= 0.3
						Mat.Water_MinVertexColorAlpha		= 0.1
						Mat.Water_SpecularFalloff			= 1.0
						Mat.Water_SpecularMultiplier		= 1.0
						Mat.Water_SunlightSpecularPower		= 50.0
						Mat.Water_OpacityFalloff			= 1.0
		
						Mat.Water_Flow1_Direction			= 295.0
						Mat.Water_Flow1_Speed				= 0.072
						Mat.Water_Flow1_TilingU				= 1.0
						Mat.Water_Flow1_TilingV				= 1.0
						Mat.Water_Flow2_Direction			= 235.0
						Mat.Water_Flow2_Speed				= 0.045
						Mat.Water_Flow2_TilingU				= 1.0
						Mat.Water_Flow2_TilingV				= 1.0
		
						Mat.Water_EdgeMask_Direction		= 270.0
						Mat.Water_EdgeMask_Speed			= 0.023
						Mat.Water_EdgeMask_TilingU			= 1.0
						Mat.Water_EdgeMask_TilingV			= 1.0
						
						Mat.Water_Wave1_Direction			= 270.0
						Mat.Water_Wave1_Speed				= 0.023
						Mat.Water_Wave1_Tiling				= 0.7
						Mat.Water_Wave2_Direction			= 315.0
						Mat.Water_Wave2_Speed				= 0.047
						Mat.Water_Wave2_Tiling				= 0.7
						Mat.Water_Wave3_Direction			= 180.0
						Mat.Water_Wave3_Speed				= 0.014
						Mat.Water_Wave3_Tiling				= 0.7
					)
					19:(	--weapon.mat
						Mat.mml_vFalloffParams1 = 1.5
						Mat.mml_vFalloffParams2 = 1.5
						Mat.mml_vFalloffParams3 = 10.0
						Mat.mml_vFalloffParams4 = 0.65
						Mat.mml_fOpacityMultiplier = 1.0
						Mat.mml_fSpecularReflectionMult = 1.0
						Mat.mml_fGoreThreshold = 0.0
						Mat.mml_vTintColor = (color 0 0 0)
						Mat.eng_vWireframeColor = (color 51 130 51 0)
					)
					20:(	--weapontrail.mat
						Mat.mml_fdistortion_fade_distance = 50.0
						Mat.mml_fdistortion_fade_multiplier = 0.0
						Mat.mml_fdistortion_invert = 1.0
						Mat.mml_fdistortion_magnitude = 5.0
					)
				)
			)
			
			--******************************
			--Run 2 ,get all the other stuff
			--******************************
			seek MAOStream 0
			MAOParse.toElement MAOStream "MaterialObject"
			while (Element = MAOParse.getNextElement MAOStream) != undefined do	--Get next Element within MaterialObject
			(
				if (EqualsCaseLess Element "Texture") then
				(
					MAOParse.toAttribute MAOStream "Name"
					AttributeName = (MAOParse.ReadAttribute MAOStream)
					MAOParse.toAttribute MAOStream "ResName"
					AttributeValue = (MAOParse.ReadAttribute MAOStream)
					
					local TextureFile
					if not (EqualsCaseLess AttributeName "mml_tLightmap") then
					(
						TextureFile = SearchFile DAOTools.ImportPaths (getFilenamePath FileStates.DDSFileName) AttributeValue "DDS"
					)
					if TextureFile != undefined then
					(
						AttributeValue = TextureFile
						FileStates.DDSFileName = TextureFile
					)
					
					--Temporarily store the filenames to the local "DAOMat" here
					--instead of "Mat" directly. Applying to Mat later.
					--Cause Mat can be different material types which are treated differently.
					if (EqualsCaseLess AttributeName "mml_tLightmap") then ()
					if (EqualsCaseLess AttributeName "mml_tDiffuse") then DAOMat.Map_Diffuse = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tNormalMap") then DAOMat.Map_Normal = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tSpecularMask") then DAOMat.Map_Specular = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tTintMask") then DAOMat.Map_Tint = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tFresnelMask") then DAOMat.Map_Fresnel = AttributeValue
					if (EqualsCaseLess AttributeName "PerlinNoise") then DAOMat.Map_PerlinNoise = AttributeValue
					if (EqualsCaseLess AttributeName "SkyTextureA") then DAOMat.Map_SkyA = AttributeValue
					if (EqualsCaseLess AttributeName "SkyTextureB") then DAOMat.Map_SkyB = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tDistortion") then DAOMat.Map_Distortion = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tDistortionModifiers") then DAOMat.Map_DistortionModifier = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tDistortionVolume") then DAOMat.Map_DistortionVolume = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tEmissiveMask") then DAOMat.Map_Emissive = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tCorneaNormalMap") then DAOMat.Map_CorneaNormal = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tAgeNormalMap") then DAOMat.Map_AgeNormal = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tBrowStubble") then DAOMat.Map_BrowStubble = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tBrowStubbleNormal") then DAOMat.Map_BrowStubbleNormal = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tEmotionsMask0") then DAOMat.Map_Emotions0 = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tEmotionsMask1") then DAOMat.Map_Emotions1 = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tEmotionsNormal") then DAOMat.Map_EmotionsNormal = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tAgeDiffuseMap") then DAOMat.Map_AgeDiffuse = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tTattooMask") then DAOMat.Map_Tattoo = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tPackedTexture") then DAOMat.Map_Packed = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tTintNoise") then DAOMat.Map_TintNoise = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tNoise") then DAOMat.Map_Noise = AttributeValue
					if (EqualsCaseLess AttributeName "CelestialBody") then DAOMat.Map_CelestialBody = AttributeValue
					if (EqualsCaseLess AttributeName "mat_tNormalMap") then DAOMat.Map_Normal = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tReliefMap") then DAOMat.Map_Relief = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tGoreMap") then DAOMat.Map_Gore = AttributeValue
					--if (EqualsCaseLess AttributeName "mml_tRMPDecal") then DAOMat.Map_RMPDecal = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tHeraldryMap") then DAOMat.Map_Heraldry = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tBeckmann") then DAOMat.Map_Beckmann = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tWaterMask") then DAOMat.Map_WaterMask = AttributeValue
					if (EqualsCaseLess AttributeName "mml_tDecal") then DAOMat.Map_Decal = AttributeValue
				)
				if (EqualsCaseLess Element "Vector4f") then
				(
					MAOParse.toAttribute MAOStream "Name"
					AttributeName = (MAOParse.ReadAttribute MAOStream)
					MAOParse.toAttribute MAOStream "value"
					AttributeValue = MAOParse.ConvertVector4f (MAOParse.ReadAttribute MAOStream)
						
					if MatInstalled then
					(
						if (EqualsCaseLess AttributeName "mml_vFalloffParams") then
						(
							Mat.mml_vFalloffParams4 = AttributeValue[1]
							Mat.mml_vFalloffParams3 = AttributeValue[2]
							Mat.mml_vFalloffParams2 = AttributeValue[3]
							Mat.mml_vFalloffParams1 = AttributeValue[4]
						)
						if (EqualsCaseLess AttributeName "mml_vEyeSpecularParameters") then
						(
							Mat.mml_vEyeSpecularParameters_CSP = AttributeValue[1]
							Mat.mml_vEyeSpecularParameters_CSM = AttributeValue[2]
							Mat.mml_vEyeSpecularParameters_SSP = AttributeValue[3]
							Mat.mml_vEyeSpecularParameters_SSM = AttributeValue[4]
						)
						if (EqualsCaseLess AttributeName "mml_vHairParameters") then
						(
							Mat.mml_vHairParameters_PSM = AttributeValue[1]
							Mat.mml_vHairParameters_PSP = AttributeValue[2]
							Mat.mml_vHairParameters_SSP = AttributeValue[3]
							Mat.mml_vHairParameters_NTTF = AttributeValue[4]
						)
						if (EqualsCaseLess AttributeName "mml_vLavaTint") then
						(
							Mat.mml_vLavaTint = [(AttributeValue[1] * 255),(AttributeValue[2] * 255),(AttributeValue[3] * 255)] as color
						)
						if (EqualsCaseLess AttributeName "mml_vUVScrollSpeed") then
						(
							Mat.mml_vUVScrollSpeed_1U = AttributeValue[1]
							Mat.mml_vUVScrollSpeed_1V = AttributeValue[2]
							Mat.mml_vUVScrollSpeed_2U = AttributeValue[3]
							Mat.mml_vUVScrollSpeed_2V = AttributeValue[4]
						)
						if (EqualsCaseLess AttributeName "mml_vMaskUVScrollSpeed") then
						(
							Mat.mml_vMaskUVScrollSpeed_U = AttributeValue[1]
							Mat.mml_vMaskUVScrollSpeed_V = AttributeValue[2]
						)
						if (EqualsCaseLess AttributeName "mml_vTintColor") then
						(
							Mat.mml_vTintColor = [(AttributeValue[1] * 255),(AttributeValue[2] * 255),(AttributeValue[3] * 255)] as color
						)
						if (EqualsCaseLess AttributeName "eng_vWireframeColor") then
						(
							Mat.eng_vWireframeColor = [(AttributeValue[1] * 255),(AttributeValue[2] * 255),(AttributeValue[3] * 255)] as color
						)
						if (EqualsCaseLess AttributeName "g_vAmbientMult") then
						(
							Mat.g_vAmbientMult = [(AttributeValue[1] * 255),(AttributeValue[2] * 255),(AttributeValue[3] * 255)] as color
						)
						if (EqualsCaseLess AttributeName "mml_vVSHWaterAlphaMaskParams") then
						(
							Mat.Water_EdgeMask_Direction	= acos(dot [0,1,0] (normalize [AttributeValue[1],AttributeValue[2],0]))
							Mat.Water_EdgeMask_Speed		= length [AttributeValue[1],AttributeValue[2]]
							Mat.Water_EdgeMask_TilingU		= AttributeValue[3]
							Mat.Water_EdgeMask_TilingV		= AttributeValue[4]
						)
					)
				)
				if (EqualsCaseLess Element "Vector4fArray") then
				(
					MAOParse.toAttribute MAOStream "Name"
					AttributeName = (MAOParse.ReadAttribute MAOStream)
					MAOParse.toAttribute MAOStream "value"
					AttributeValue = MAOParse.ConvertVector4f (MAOParse.ReadAttribute MAOStream)
						
					if MatInstalled then
					(
						if (EqualsCaseLess AttributeName "mat_vPSHWaterParams") then	--4x2 Array
						(
							Mat.Water_Wave1_Height			= AttributeValue[1]
							Mat.Water_Wave2_Height			= AttributeValue[2]
							Mat.Water_Wave3_Height			= AttributeValue[3]
							Mat.Water_MinVertexColorAlpha	= AttributeValue[4]
							Mat.Water_SpecularFalloff		= AttributeValue[5]
							Mat.Water_SpecularMultiplier	= AttributeValue[6]
							Mat.Water_SunlightSpecularPower	= AttributeValue[7]
							Mat.Water_OpacityFalloff		= AttributeValue[8]
						)
						if (EqualsCaseLess AttributeName "mat_vVSHWaterParams") then	--4x3 Array
						(
							Mat.Water_Wave1_Direction	= acos(dot [0,1,0] (normalize [AttributeValue[1],AttributeValue[2],0]))
							Mat.Water_Wave1_Speed		= length [AttributeValue[1],AttributeValue[2]]
							Mat.Water_Wave1_Tiling		= AttributeValue[3]
							Mat.Water_Wave2_Direction	= acos(dot [0,1,0] (normalize [AttributeValue[5],AttributeValue[6],0]))
							Mat.Water_Wave2_Speed		= length [AttributeValue[5],AttributeValue[6]]
							Mat.Water_Wave2_Tiling		= AttributeValue[7]
							Mat.Water_Wave3_Direction	= acos(dot [0,1,0] (normalize [AttributeValue[9],AttributeValue[10],0]))
							Mat.Water_Wave3_Speed		= length [AttributeValue[9],AttributeValue[10]]
							Mat.Water_Wave3_Tiling		= AttributeValue[11]
						)
						if (EqualsCaseLess AttributeName "mml_vVSHWaterDiffuseParams") then	--4x2 Array
						(
							Mat.Water_Flow1_Direction	= acos(dot [0,1,0] (normalize [AttributeValue[1],AttributeValue[2],0]))
							Mat.Water_Flow1_Speed		= length [AttributeValue[1],AttributeValue[2]]
							Mat.Water_Flow1_TilingU		= AttributeValue[3]
							Mat.Water_Flow1_TilingV		= AttributeValue[4]
							Mat.Water_Flow2_Direction	= acos(dot [0,1,0] (normalize [AttributeValue[5],AttributeValue[6],0]))
							Mat.Water_Flow2_Speed		= length [AttributeValue[5],AttributeValue[6]]
							Mat.Water_Flow2_TilingU		= AttributeValue[7]
							Mat.Water_Flow2_TilingV		= AttributeValue[8]
						)
					)
				)
				if (EqualsCaseLess Element "Float") then
				(
					MAOParse.toAttribute MAOStream "Name"
					AttributeName = (MAOParse.ReadAttribute MAOStream)
					MAOParse.toAttribute MAOStream "value"
					AttributeValue = (MAOParse.ReadAttribute MAOStream) as float
						
					if MatInstalled then
					(
						if (EqualsCaseLess AttributeName "mml_fSpecularReflectionMult") then Mat.mml_fSpecularReflectionMult = AttributeValue
						if (EqualsCaseLess AttributeName "mml_fOpacityMultiplier") then Mat.mml_fOpacityMultiplier = AttributeValue
						if (EqualsCaseLess AttributeName "mml_fReliefAspect") then Mat.mml_fReliefAspect = AttributeValue
						if (EqualsCaseLess AttributeName "mml_fGoreThreshold") then Mat.mml_fGoreThreshold = AttributeValue
						if (EqualsCaseLess AttributeName "mml_fdistortion_fade_distance") then Mat.mml_fdistortion_fade_distance = AttributeValue
						if (EqualsCaseLess AttributeName "mml_fdistortion_fade_multiplier") then Mat.mml_fdistortion_fade_multiplier = AttributeValue
						if (EqualsCaseLess AttributeName "mml_fdistortion_invert") then Mat.mml_fdistortion_invert = AttributeValue
						if (EqualsCaseLess AttributeName "mml_fdistortion_magnitude") then Mat.mml_fdistortion_magnitude = AttributeValue
						if (EqualsCaseLess AttributeName "g_fLipSpecularBoost") then Mat.g_fLipSpecularBoost = AttributeValue
						if (EqualsCaseLess AttributeName "g_fRimPower") then Mat.g_fRimPower = AttributeValue
						if (EqualsCaseLess AttributeName "g_fSpecularMult") then Mat.g_fSpecularMult = AttributeValue
						if (EqualsCaseLess AttributeName "mml_fSpecularPower") then Mat.mml_fSpecularPower = AttributeValue
						if (EqualsCaseLess AttributeName "mml_fLavaBrightness") then Mat.mml_fLavaBrightness = AttributeValue
						if (EqualsCaseLess AttributeName "mml_fLavaContrast") then Mat.mml_fLavaContrast = AttributeValue
					)
				)
				if (EqualsCaseLess Element "SoundType") then
				(
					MAOParse.toAttribute MAOStream "Name"
					AttributeName = (MAOParse.ReadAttribute MAOStream)
					MAOParse.toAttribute MAOStream "value"
					AttributeValue = (MAOParse.ReadAttribute MAOStream) as integer
					
					if MatInstalled then
					(
						if (EqualsCaseLess AttributeName "mml_iSoundMaterialType") then Mat.mml_iSoundMaterialType = AttributeValue
					)
				)
				MAOParse.ReadElement MAOStream	--Close that Element
			)
			
			--********************************************
			--Store the Maps in the Material
			--Standard, or Dragon Age Material if present
			--*******************************************
			if MatInstalled then
			(
				if DAOMat.Map_Diffuse != undefined then Mat.Map_Diffuse = bitmapTexture filename:DAOMat.Map_Diffuse
				if DAOMat.Map_Diffuse != undefined then showTextureMap Mat Mat.Map_Diffuse true
				
				if DAOMat.Map_Normal				!= undefined then Mat.Map_Normal		= bitmapTexture filename:DAOMat.Map_Normal
				if DAOMat.Map_Specular				!= undefined then Mat.Map_Specular		= bitmapTexture filename:DAOMat.Map_Specular
				if DAOMat.Map_Emissive				!= undefined then Mat.Map_Emissive		= bitmapTexture filename:DAOMat.Map_Emissive
				if DAOMat.Map_Tint					!= undefined then Mat.Map_Tint			= bitmapTexture filename:DAOMat.Map_Tint
				if DAOMat.Map_AgeDiffuse			!= undefined then Mat.Map_AgeDiffuse	= bitmapTexture filename:DAOMat.Map_AgeDiffuse
				if DAOMat.Map_AgeNormal				!= undefined then Mat.Map_AgeNormal		= bitmapTexture filename:DAOMat.Map_AgeNormal
				if DAOMat.Map_CorneaNormal			!= undefined then Mat.Map_CorneaNormal	= bitmapTexture filename:DAOMat.Map_CorneaNormal
				if DAOMat.Map_Distortion			!= undefined then Mat.Map_Distortion	= bitmapTexture filename:DAOMat.Map_Distortion
				if DAOMat.Map_DistortionModifier	!= undefined then Mat.Map_DistortionModifier	= bitmapTexture filename:DAOMat.Map_DistortionModifier
				if DAOMat.Map_DistortionVolume		!= undefined then Mat.Map_DistortionVolume		= bitmapTexture filename:DAOMat.Map_DistortionVolume
				if DAOMat.Map_Emotions0				!= undefined then Mat.Map_Emotions0		= bitmapTexture filename:DAOMat.Map_Emotions0
				if DAOMat.Map_Emotions1				!= undefined then Mat.Map_Emotions1		= bitmapTexture filename:DAOMat.Map_Emotions1
				if DAOMat.Map_EmotionsNormal		!= undefined then Mat.Map_EmotionsNormal		= bitmapTexture filename:DAOMat.Map_EmotionsNormal
				if DAOMat.Map_Tattoo				!= undefined then Mat.Map_Tattoo		= bitmapTexture filename:DAOMat.Map_Tattoo
				if DAOMat.Map_SkyA					!= undefined then Mat.Map_SkyA			= bitmapTexture filename:DAOMat.Map_SkyA
				if DAOMat.Map_SkyB					!= undefined then Mat.Map_SkyB			= bitmapTexture filename:DAOMat.Map_SkyB
				if DAOMat.Map_PerlinNoise			!= undefined then Mat.Map_PerlinNoise	= bitmapTexture filename:DAOMat.Map_PerlinNoise
				if DAOMat.Map_Fresnel				!= undefined then Mat.Map_Fresnel		= bitmapTexture filename:DAOMat.Map_Fresnel
				if DAOMat.Map_BrowStubble			!= undefined then Mat.Map_BrowStubble	= bitmapTexture filename:DAOMat.Map_BrowStubble
				if DAOMat.Map_BrowStubbleNormal		!= undefined then Mat.Map_BrowStubbleNormal = bitmapTexture filename:DAOMat.Map_BrowStubbleNormal
				if DAOMat.Map_TintNoise				!= undefined then Mat.Map_TintNoise		= bitmapTexture filename:DAOMat.Map_TintNoise
				if DAOMat.Map_Noise					!= undefined then Mat.Map_Noise			= bitmapTexture filename:DAOMat.Map_Noise
				if DAOMat.Map_CelestialBody			!= undefined then Mat.Map_CelestialBody	= bitmapTexture filename:DAOMat.Map_CelestialBody
				if DAOMat.Map_Packed				!= undefined then Mat.Map_Packed		= bitmapTexture filename:DAOMat.Map_Packed
				if DAOMat.Map_Relief				!= undefined then Mat.Map_Relief		= bitmapTexture filename:DAOMat.Map_Relief
				if DAOMat.Map_Gore					!= undefined then Mat.Map_Gore			= bitmapTexture filename:DAOMat.Map_Gore
				--if DAOMat.Map_RMPDecal			!= undefined then Mat.Map_RMPDecal		= bitmapTexture filename:DAOMat.Map_RMPDecal
				if DAOMat.Map_Heraldry				!= undefined then Mat.Map_Heraldry		= bitmapTexture filename:DAOMat.Map_Heraldry
				if DAOMat.Map_Beckmann				!= undefined then Mat.Map_Beckmann		= bitmapTexture filename:DAOMat.Map_Beckmann
				if DAOMat.Map_WaterMask				!= undefined then Mat.Map_WaterMask		= bitmapTexture filename:DAOMat.Map_WaterMask
				if DAOMat.Map_Decal					!= undefined then Mat.Map_Decal			= bitmapTexture filename:DAOMat.Map_Decal
			)
			else
			(
				if DAOMat.Map_Diffuse != undefined then Mat.diffuseMap = bitmapTexture filename:DAOMat.Map_Diffuse name:"Diffuse"
				if DAOMat.Map_Diffuse != undefined then showTextureMap Mat Mat.diffuseMap true
				
				if DAOMat.Map_Normal != undefined then Mat.BumpMap = bitmapTexture filename:DAOMat.Map_Normal name:"Normal"
				if DAOMat.Map_Specular != undefined then Mat.SpecularMap = bitmapTexture filename:DAOMat.Map_Specular name:"Specular"
			)
			close MAOStream
		)
		else MessageBox "MAO File not found, Skipping.." title:"MAO Import"
		
		--********************
		--Apply Material
		--*******************
		
		if ApplyMat == true then MeshObj.material = Mat
		if ApplyMat == false then Mat
	)
