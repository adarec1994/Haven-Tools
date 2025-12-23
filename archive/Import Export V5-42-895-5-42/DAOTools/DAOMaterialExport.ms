/**********************************************************************************************************
Dragon Age Origins Tools
Author: Eshme  http://social.bioware.com/project/2336/

last edited:
13 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Include Script
DAOMaterialExport.ms
Material Export Functions for Model Export
************************************************************************************************************
*/

	fn GetFileFromBitmap Map Default = (
		local Out = ""
		if Map != undefined and Classof Map == BitmapTexture then
		(
			Out = (GetFilenameFile Map.Filename) + ".DDS"
		)
		else
		(
			Out = Default + ".DDS"
		)
		Out
	)
	
	fn MatchID Semantic InArray = (
		--************************************************************************
		--Simple funtion to spare lot of if-then, when comparing lot of numbers
		--************************************************************************
		for item in InArray do if (item == Semantic) then return true
		false
	)
	
	fn ExportMAO = (
		--********************************************************
		--Exporting one or more MAO's based on mat ref's stored in
		--DAOModelExVars.MaterialObjects
		--********************************************************
		local Mat
		local MatName
		local TypeID
		local SemID
		local Stream
		local ListDone = #()
		local TypeIDs = GetMatTypeIDs()
		local SemanticIDs = GetMatSemanticIDs()
		
		for Item in DAOModelExVars.MaterialObjects where ((ClassOf Item[2] == DAOMaterial) and (finditem ListDone Item[2] == 0)) do
		(
			--**************************************************************************
			--Item[1] is the Node , Item[2] the Material
			--ListDone saves the Materials done by their Name, in Max these are unique
			--************************************************************************
			Mat = Item[2]
			append ListDone Mat
			MatName = FixOutputName Mat.Name #()	--removes spaces essentially
			
			TypeID = Mat.MatTypeID
			SemID = Mat.MatSemanticID
			
			Stream = DAOTools.FOPen (DAOTools.OutputPath + MatName + ".mao")
			if Stream == undefined then
			(
				MessageBox ("Error creating output MAO file.\n"+(DAOTools.OutputPath + MatName + ".mao\n\nSkipping..")) title:"Model Export"
			)
			else
			(
				format "<?xml version=\"1.0\" ?>\n" to:Stream
				format "<!-- Created with Eshme's DAO Model and Animation Import Export Tool Version: % -->\n" DAOTools.Version to:Stream
				format "<!-- Visit my Page at http://social.bioware.com/project/2336/  -->\n" to:Stream
				format "<!-- On : % -->\n\n" (localtime) to:Stream
				
				format "<MaterialObject Name=\"%\">\n" MatName to:Stream
				format "<Material Name=\"%\"></Material>\n" (GetDAOMatStringByID TypeIDs Mat.MatTypeID) to:Stream
				format "<DefaultSemantic Name=\"%\"></DefaultSemantic>\n" (GetDAOMatStringByID SemanticIDs Mat.MatSemanticID) to:Stream
				
				try ( case TypeID of
				(
					1:(		--Black.mat
						if (MatchID SemID #(111)) then	--Wireframe
						(
							format "<Vector4f Name=\"eng_vWireframeColor\" value=\"% % % 1.0\"/>\n" (Mat.eng_vWireframeColor.r / 255.0) (Mat.eng_vWireframeColor.g / 255.0) (Mat.eng_vWireframeColor.b / 255.0) to:Stream
						)
					)
					2:(		--Canopy.mat
						if (MatchID SemID #(4,5,20,117,112)) then	--Default LOD1-3 , Debug , PSSM
						(
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "Default_Missing") to:Stream
						)
						if (MatchID SemID #(4,117)) then	--Default LOD1 , Debug
						(
							format "<Texture Name=\"mml_tNormalMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Normal "Default_FlatNM") to:Stream
						)
						if (MatchID SemID #(4,5,20,117)) then	--Default LOD1-3 , Debug
						(
							format "<Texture Name=\"mml_tSpecularMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Specular "Default_SpecularMask") to:Stream
							format "<Texture Name=\"mml_tTintMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Tint "Default_Black") to:Stream
							format "<Texture Name=\"mml_tGoreMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Gore "Aur_WhiteVol") to:Stream
							--format "<Texture Name=\"mml_tRMPDecal\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_RMPDecal "Default_Black") to:Stream
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
							format "<Float Name=\"mml_fSpecularReflectionMult\" value=\"%\"></Float>\n" (Mat.mml_fSpecularReflectionMult) to:Stream
							format "<Vector4f Name=\"mml_vTintColor\" value=\"% % % 0.0\"/>\n" ((Mat.mml_vTintColor.r / 255.0)) ((Mat.mml_vTintColor.g / 255.0)) ((Mat.mml_vTintColor.b / 255.0)) to:Stream
							format "<Float Name=\"mml_fGoreThreshold\" value=\"%\"></Float>\n" (Mat.mml_fGoreThreshold) to:Stream
						)
						if (MatchID SemID #(111)) then	--Wireframe
						(
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
							format "<Vector4f Name=\"eng_vWireframeColor\" value=\"% % % 1.0\"/>\n" ((Mat.eng_vWireframeColor.r / 255.0)) ((Mat.eng_vWireframeColor.g / 255.0)) ((Mat.eng_vWireframeColor.b / 255.0)) to:Stream
						)
					)
					3:(		--Character.mat
						if (MatchID SemID #(4,5,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,117)) then
						(	--nearly all semantics + debug
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "Default_Missing") to:Stream
							format "<Texture Name=\"mml_tSpecularMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Specular "Default_SpecularMask") to:Stream
							format "<Texture Name=\"mml_tGoreMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Gore "Aur_WhiteVol") to:Stream
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
							format "<Float Name=\"mml_fSpecularReflectionMult\" value=\"%\"></Float>\n" (Mat.mml_fSpecularReflectionMult) to:Stream
							format "<Vector4f Name=\"mml_vTintColor\" value=\"% % % 0.0\"/>\n" ((Mat.mml_vTintColor.r / 255.0)) ((Mat.mml_vTintColor.g / 255.0)) ((Mat.mml_vTintColor.b / 255.0)) to:Stream
							format "<Float Name=\"mml_fGoreThreshold\" value=\"%\"></Float>\n" (Mat.mml_fGoreThreshold) to:Stream
						)
						if (MatchID SemID #(4,5,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,53,117)) then
						(	--Tinting Semantics + debug
							format "<Texture Name=\"mml_tTintMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Tint "Default_Black") to:Stream
						)	--nearly all semantics
						/*
						if (MatchID SemID #(4,5,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53)) then
						(
							format "<Texture Name=\"mml_tRMPDecal\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_RMPDecal "Default_Black") to:Stream
							format "<Float Name=\"mml_fRMPInvertFresnel\" value=\"%\"></Float>\n" (Mat.mml_fRMPInvertFresnel) to:Stream
							format "<Float Name=\"mml_fRMPFresnelFalloff\" value=\"%\"></Float>\n" (Mat.mml_fRMPFresnelFalloff) to:Stream
							format "<Float Name=\"mml_fRMPAlpha\" value=\"%\"></Float>\n" (Mat.mml_fRMPAlpha) to:Stream
							format "<Vector4f Name=\"mml_vRMPTintColor\" value=\"% % % 0.0\"/>\n" ((Mat.mml_vRMPTintColor.r / 255.0)) ((Mat.mml_vRMPTintColor.g / 255.0)) ((Mat.mml_vRMPTintColor.b / 255.0)) to:Stream
							format "<Vector4f Name=\"mml_vRMPVariationTint\" value=\"% % % 1.0\"/>\n" ((Mat.mml_vRMPVariationTint.r / 255.0)) ((Mat.mml_vRMPVariationTint.g / 255.0)) ((Mat.mml_vRMPVariationTint.b / 255.0)) to:Stream
						)
						*/
						if (MatchID SemID #(4,7,10,11,13,15,16,18,21,22,24,26,27,29,32,33,35,37,38,40,43,44,46,49,50,52,53,117)) then		--only LOD1 here
						(
							format "<Texture Name=\"mml_tNormalMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Normal "Default_FlatNM") to:Stream
						)
						if (MatchID SemID #(13,14,15,16,17,24,25,26,27,28,35,36,37,38,39,46,47,48,49,50,51)) then					--Emissive semantics
						(
							format "<Texture Name=\"mml_tEmissiveMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Emissive "Default_Black") to:Stream
						)
						if (MatchID SemID #(111)) then		--Wireframe
						(
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
							format "<Vector4f Name=\"eng_vWireframeColor\" value=\"% % % 1.0\"/>\n" ((Mat.eng_vWireframeColor.r / 255.0)) ((Mat.eng_vWireframeColor.g / 255.0)) ((Mat.eng_vWireframeColor.b / 255.0)) to:Stream
						)
					)
					4:(		--Cloth.mat
						if (MatchID SemID #(1,2)) then	--Default, Blend
						(
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "Default_Missing") to:Stream
							format "<Texture Name=\"mml_tSpecularMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Specular "Default_SpecularMask") to:Stream
							format "<Texture Name=\"mml_tTintMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Tint "Default_Black") to:Stream
							format "<Texture Name=\"mml_tHeraldryMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Heraldry "Default_Black") to:Stream
							format "<Texture Name=\"mml_tGoreMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Gore "Aur_WhiteVol") to:Stream
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
							format "<Float Name=\"mml_fSpecularReflectionMult\" value=\"%\"></Float>\n" (Mat.mml_fSpecularReflectionMult) to:Stream
							format "<Vector4f Name=\"mml_vTintColor\" value=\"% % % 0.0\"/>\n" ((Mat.mml_vTintColor.r / 255.0)) ((Mat.mml_vTintColor.g / 255.0)) ((Mat.mml_vTintColor.b / 255.0)) to:Stream
							format "<Float Name=\"mml_fGoreThreshold\" value=\"%\"></Float>\n" (Mat.mml_fGoreThreshold) to:Stream
						)
						if (MatchID SemID #(111)) then	--Wireframe
						(
							format "<Vector4f Name=\"eng_vWireframeColor\" value=\"% % % 1.0\"/>\n" ((Mat.eng_vWireframeColor.r / 255.0)) ((Mat.eng_vWireframeColor.g / 255.0)) ((Mat.eng_vWireframeColor.b / 255.0)) to:Stream
						)
					)
					8:(		--Eye.mat
						if (MatchID SemID #(1,117)) then	--Default,Debug
						(
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "Default_Missing") to:Stream
							format "<Texture Name=\"mml_tNormalMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Normal "Default_FlatNM") to:Stream
							format "<Texture Name=\"mml_tTintMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Tint "Default_Black") to:Stream
							format "<Texture Name=\"mml_tCorneaNormalMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_CorneaNormal "Default_FlatNM") to:Stream
							format "<Vector4f Name=\"mml_vEyeSpecularParameters\" value=\"% % % %\"/>\n" (Mat.mml_vEyeSpecularParameters_CSP) (Mat.mml_vEyeSpecularParameters_CSM) (Mat.mml_vEyeSpecularParameters_SSP) (Mat.mml_vEyeSpecularParameters_SSM) to:Stream
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
						)
						if (MatchID SemID #(69)) then	--Eyelash
						(
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "Default_Missing") to:Stream
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
						)
						if (MatchID SemID #(111)) then	--Wireframe
						(
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
							format "<Vector4f Name=\"eng_vWireframeColor\" value=\"% % % 1.0\"/>\n" ((Mat.eng_vWireframeColor.r / 255.0)) ((Mat.eng_vWireframeColor.g / 255.0)) ((Mat.eng_vWireframeColor.b / 255.0)) to:Stream
						)
					)
					9:(		--Face.mat
						if (MatchID SemID #(1,56,57)) then	--Default. All LODs
						(
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "Default_Missing") to:Stream
							format "<Texture Name=\"mml_tNormalMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Normal "Default_FlatNM") to:Stream
							format "<Texture Name=\"mml_tTintMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Tint "Default_Black") to:Stream
							format "<Texture Name=\"mml_tAgeNormalMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_AgeNormal "Default_FlatNM") to:Stream
							format "<Texture Name=\"mml_tEmotionsNormal\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_EmotionsNormal "Default_FlatNM") to:Stream
							format "<Texture Name=\"mml_tAgeDiffuseMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_AgeDiffuse "Default_Black") to:Stream
							format "<Texture Name=\"mml_tEmotionsMask0\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Emotions0 "Default_Black") to:Stream
							format "<Texture Name=\"mml_tEmotionsMask1\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Emotions1 "Default_Black") to:Stream
							format "<Texture Name=\"mml_tBrowStubble\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_BrowStubble "Default_Black") to:Stream
							format "<Texture Name=\"mml_tBrowStubbleNormal\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_BrowStubbleNormal "Default_FlatNM") to:Stream
							format "<Texture Name=\"mml_tTattooMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Tattoo "Default_Black") to:Stream
							format "<Texture Name=\"mml_tGoreMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Gore "Aur_WhiteVol") to:Stream --Gore_Default
							format "<Texture Name=\"mml_tBeckmann\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Beckmann "Beckmann") to:Stream
							
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
							format "<Vector4f Name=\"g_vAmbientMult\" value=\"% % % 0.0\"/>\n" (Mat.g_vAmbientMult.r / 255.0) (Mat.g_vAmbientMult.g / 255.0) (Mat.g_vAmbientMult.b / 255.0) to:Stream
							format "<Float Name=\"g_fSpecularMult\" value=\"%\"></Float>\n" (Mat.g_fSpecularMult) to:Stream
							format "<Float Name=\"g_fLipSpecularBoost\" value=\"%\"></Float>\n" (Mat.g_fLipSpecularBoost) to:Stream
							format "<Float Name=\"g_fRimPower\" value=\"%\"></Float>\n" (Mat.g_fRimPower) to:Stream
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
							format "<Float Name=\"mml_fGoreThreshold\" value=\"%\"></Float>\n" (Mat.mml_fGoreThreshold) to:Stream
							format "<Vector4f Name=\"mml_vTintColor\" value=\"% % % 0.0\"/>\n" ((Mat.mml_vTintColor.r / 255.0)) ((Mat.mml_vTintColor.g / 255.0)) ((Mat.mml_vTintColor.b / 255.0)) to:Stream
						)
						if (MatchID SemID #(111)) then	--Wireframe
						(
							format "<Vector4f Name=\"eng_vWireframeColor\" value=\"% % % 1.0\"/>\n" ((Mat.eng_vWireframeColor.r / 255.0)) ((Mat.eng_vWireframeColor.g / 255.0)) ((Mat.eng_vWireframeColor.b / 255.0)) to:Stream
						)
					)
					11:(	--Hairalpha.mat
						if (MatchID SemID #(1,56,57,118,117,113)) then	--LOD1,LOD2,LOD3,Overdraw,Debug,Depthwrite
						(
							format "<Texture Name=\"mml_tPackedTexture\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Packed "Default_Missing") to:Stream
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
						)
						if (MatchID SemID #(1,56,57,117)) then	--LOD1,LOD2,LOD3,Debug
						(
							format "<Vector4f Name=\"mml_vTintColor\" value=\"% % % 0.0\"/>\n" ((Mat.mml_vTintColor.r / 255.0)) ((Mat.mml_vTintColor.g / 255.0)) ((Mat.mml_vTintColor.b / 255.0)) to:Stream
							format "<Vector4f Name=\"mml_vHairParameters\" value=\"% % % %\"/>\n" (Mat.mml_vHairParameters_PSM) (Mat.mml_vHairParameters_PSP) (Mat.mml_vHairParameters_SSP) (Mat.mml_vHairParameters_NTTF) to:Stream
						)
						if (MatchID SemID #(1,56,117)) then	--LOD1,LOD2,Debug
						(
							format "<Texture Name=\"mml_tTintNoise\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_TintNoise "Default_Missing") to:Stream
						)
						if (MatchID SemID #(111)) then	--Wireframe
						(
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
							format "<Vector4f Name=\"eng_vWireframeColor\" value=\"% % % 1.0\"/>\n" ((Mat.eng_vWireframeColor.r / 255.0)) ((Mat.eng_vWireframeColor.g / 255.0)) ((Mat.eng_vWireframeColor.b / 255.0)) to:Stream
						)
					)
					13:(	--Lava.mat
						if (MatchID SemID #(54,55)) then	--Diffuse,Volume (all)
						(
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "Default_Missing") to:Stream
							format "<Texture Name=\"mml_tNoise\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Noise "Default_Missing") to:Stream
							format "<Float Name=\"mml_fLavaBrightness\" value=\"%\"></Float>\n" (Mat.mml_fLavaBrightness) to:Stream
							format "<Float Name=\"mml_fLavaContrast\" value=\"%\"></Float>\n" (Mat.mml_fLavaContrast) to:Stream
							format "<Vector4f Name=\"mml_vLavaTint\" value=\"% % % 1.0\"/>\n" ((Mat.mml_vLavaTint.r / 255.0)) ((Mat.mml_vLavaTint.g / 255.0)) ((Mat.mml_vLavaTint.b / 255.0)) to:Stream
							format "<Vector4f Name=\"mml_vUVScrollSpeed\" value=\"% % % %\"/>\n" (Mat.mml_vUVScrollSpeed_1U) (Mat.mml_vUVScrollSpeed_1V) (Mat.mml_vUVScrollSpeed_2U) (Mat.mml_vUVScrollSpeed_2V) to:Stream
							format "<Vector4f Name=\"mml_vMaskUVScrollSpeed\" value=\"% % 0 0\"/>\n" (Mat.mml_vMaskUVScrollSpeed_U) (Mat.mml_vMaskUVScrollSpeed_V) to:Stream
						)
					)
					14:(	--Prop.mat
						if (MatchID SemID #(1,2,3)) then	--Default, Punch, Blend
						(
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "Default_Missing") to:Stream
							format "<Texture Name=\"mml_tNormalMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Normal "Default_FlatNM") to:Stream
							format "<Texture Name=\"mml_tSpecularMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Specular "Default_SpecularMask") to:Stream
							format "<Texture Name=\"mml_tEmissiveMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Emissive "Default_Black") to:Stream
							format "<Vector4f Name=\"mml_vTintColor\" value=\"% % % 0.0\"/>\n" ((Mat.mml_vTintColor.r / 255.0)) ((Mat.mml_vTintColor.g / 255.0)) ((Mat.mml_vTintColor.b / 255.0)) to:Stream
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
							format "<Float Name=\"mml_fSpecularReflectionMult\" value=\"%\"></Float>\n" (Mat.mml_fSpecularReflectionMult) to:Stream
						)
						if (MatchID SemID #(1,2,3,111)) then	--Default, Punch, Blend, Wireframe
						(
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
						)
						if (MatchID SemID #(111)) then	--Wireframe
						(
							format "<Float Name=\"mml_fSpecularPower\" value=\"%\"></Float>\n" (Mat.mml_fSpecularPower) to:Stream
							format "<Vector4f Name=\"eng_vWireframeColor\" value=\"% % % 1.0\"/>\n" ((Mat.eng_vWireframeColor.r / 255.0)) ((Mat.eng_vWireframeColor.g / 255.0)) ((Mat.eng_vWireframeColor.b / 255.0)) to:Stream
						)
					)
					16:(	--Static.mat
						if (MatchID SemID #(1,75,76,77,78,57,79,117)) then	--Default, LOD1Relief, LOD1Blend, LOD1BlendFlat, LOD1Punch, LOD3, LOD3Punch, Debug
						(
							format "<Texture Name=\"mml_tLightmap\" ResName=\"LM.dds\" RequiresID=\"true\"></Texture>\n" to:Stream
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "Default_Missing") to:Stream
							format "<Texture Name=\"mml_tNormalMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Normal "Default_FlatNM") to:Stream
							format "<Texture Name=\"mml_tSpecularMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Specular "Default_SpecularMask") to:Stream
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
							format "<Float Name=\"mml_fSpecularReflectionMult\" value=\"%\"></Float>\n" (Mat.mml_fSpecularReflectionMult) to:Stream
							format "<Vector4f Name=\"mml_vTintColor\" value=\"% % % 0.0\"/>\n" ((Mat.mml_vTintColor.r / 255.0)) ((Mat.mml_vTintColor.g / 255.0)) ((Mat.mml_vTintColor.b / 255.0)) to:Stream
						)
						if (MatchID SemID #(75)) then	--LOD1Relief
						(
							format "<Texture Name=\"mml_tReliefMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Relief "Default_Gray50") to:Stream
							format "<Float Name=\"mml_fReliefAspect\" value=\"%\"></Float>\n" (Mat.mml_fReliefAspect) to:Stream
						)
						if (MatchID SemID #(1,75,76,77,78,57,79)) then	--Default, LOD1Relief, LOD1Blend, LOD1BlendFlat, LOD1Punch, LOD3, LOD3Punch
						(
							format "<SoundType Name=\"mml_iSoundMaterialType\" value=\"%\"></SoundType>\n" (Mat.mml_iSoundMaterialType) to:Stream
						)
						if (MatchID SemID #(111)) then	--Wireframe
						(
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
							format "<Vector4f Name=\"eng_vWireframeColor\" value=\"% % % 1.0\"/>\n" ((Mat.eng_vWireframeColor.r / 255.0)) ((Mat.eng_vWireframeColor.g / 255.0)) ((Mat.eng_vWireframeColor.b / 255.0)) to:Stream
						)
					)
					18:(	--Water.mat
						if (MatchID SemID #(1,108,107,109,110)) then	--default, all
						(
							format "<Texture Name=\"mml_tNormalMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Normal "Default_FlatNM") to:Stream
						)
						if (MatchID SemID #(108,109)) then	--flowing water, complex
						(
							format "<Texture Name=\"mml_tDistortion\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Distortion "Default_Missing.dds") to:Stream
							format "<Texture Name=\"mml_tDistortionModifiers\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_DistortionModifier "Default_White") to:Stream
							format "<Texture Name=\"mml_tWaterMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_WaterMask "Default_White.dds") to:Stream
							format "<Texture Name=\"mml_tDecal\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Decal "Default_Missing.dds") to:Stream
						)
						if (MatchID SemID #(1,108,107,109,110)) then	--default, all
						(
							local Arr = #()	--temporary, for better visibility
							Arr[1] = Mat.Water_Wave1_Height
							Arr[2] = Mat.Water_Wave2_Height
							Arr[3] = Mat.Water_Wave3_Height
							Arr[4] = Mat.Water_MinVertexColorAlpha
							Arr[5] = Mat.Water_SpecularFalloff
							Arr[6] = Mat.Water_SpecularMultiplier
							Arr[7] = Mat.Water_SunlightSpecularPower
							Arr[8] = Mat.Water_OpacityFalloff
							format "<Vector4fArray Name=\"mat_vPSHWaterParams\" value=\"% % % % % % % %\"/>\n" Arr[1] Arr[2] Arr[3] Arr[4] Arr[5] Arr[6] Arr[7] Arr[8] to:Stream
							Arr[1] = sin(Mat.Water_Wave1_Direction) * Mat.Water_Wave1_Speed
							Arr[2] = cos(Mat.Water_Wave1_Direction) * Mat.Water_Wave1_Speed
							Arr[3] = Mat.Water_Wave1_Tiling
							Arr[5] = sin(Mat.Water_Wave2_Direction) * Mat.Water_Wave2_Speed
							Arr[6] = cos(Mat.Water_Wave2_Direction) * Mat.Water_Wave2_Speed
							Arr[7] = Mat.Water_Wave2_Tiling
							Arr[9] = sin(Mat.Water_Wave3_Direction) * Mat.Water_Wave3_Speed
							Arr[10] = cos(Mat.Water_Wave3_Direction) * Mat.Water_Wave3_Speed
							Arr[11] = Mat.Water_Wave3_Tiling
							format "<Vector4fArray Name=\"mat_vVSHWaterParams\" value=\"% % % 0.0 % % % 0.0 % % % 0.0\"/>\n" Arr[1] Arr[2] Arr[3] Arr[5] Arr[6] Arr[7] Arr[9] Arr[10] Arr[11] to:Stream
						)
						if (MatchID SemID #(108,109)) then	--flowing water, complex
						(
							local Arr = #()	--temporary, for better visibility
							format "<Float Name=\"mml_fdistortion_fade_distance\" value=\"%\"></Float>\n" (Mat.mml_fdistortion_fade_distance) to:Stream
							format "<Float Name=\"mml_fdistortion_fade_multiplier\" value=\"%\"></Float>\n" (Mat.mml_fdistortion_fade_multiplier) to:Stream
							format "<Float Name=\"mml_fdistortion_invert\" value=\"%\"></Float>\n" (Mat.mml_fdistortion_invert) to:Stream
							format "<Float Name=\"mml_fdistortion_magnitude\" value=\"%\"></Float>\n" (Mat.mml_fdistortion_magnitude) to:Stream
							
							Arr[1] = sin(Mat.Water_EdgeMask_Direction) * Mat.Water_EdgeMask_Speed
							Arr[2] = cos(Mat.Water_EdgeMask_Direction) * Mat.Water_EdgeMask_Speed
							Arr[3] = Mat.Water_EdgeMask_TilingU
							Arr[4] = Mat.Water_EdgeMask_TilingV
							format "<Vector4f Name=\"mml_vVSHWaterAlphaMaskParams\" value=\"% % % %\"/>\n" Arr[1] Arr[2] Arr[3] Arr[4] to:Stream
							Arr[1] = sin(Mat.Water_Flow1_Direction) * Mat.Water_Flow1_Speed
							Arr[2] = cos(Mat.Water_Flow1_Direction) * Mat.Water_Flow1_Speed
							Arr[3] = Mat.Water_Flow1_TilingU
							Arr[4] = Mat.Water_Flow1_TilingV
							Arr[5] = sin(Mat.Water_Flow2_Direction) * Mat.Water_Flow2_Speed
							Arr[6] = cos(Mat.Water_Flow2_Direction) * Mat.Water_Flow2_Speed
							Arr[7] = Mat.Water_Flow2_TilingU
							Arr[8] = Mat.Water_Flow2_TilingV
							format "<Vector4fArray Name=\"mml_vVSHWaterDiffuseParams\" value=\"% % % % % % % %\"/>\n" Arr[1] Arr[2] Arr[3] Arr[4] Arr[5] Arr[6] Arr[7] Arr[8] to:Stream
						)
					)
					19:(	--Weapon.mat
						if (MatchID SemID #(1)) then	--default
						(
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "Default_Missing") to:Stream
							format "<Texture Name=\"mml_tNormalMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Normal "Default_FlatNM") to:Stream
							format "<Texture Name=\"mml_tSpecularMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Specular "Default_SpecularMask") to:Stream
							format "<Texture Name=\"mml_tEmissiveMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Emissive "Default_Black") to:Stream
							format "<Texture Name=\"mml_tTintMask\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Tint "Default_Black") to:Stream
							format "<Texture Name=\"mml_tGoreMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Gore "Aur_WhiteVol") to:Stream
							--format "<Texture Name=\"mml_tRMPDecal\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_RMPDecal "Default_Black") to:Stream
							format "<Texture Name=\"mml_tHeraldryMap\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Heraldry "Default_Black") to:Stream
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
							format "<Float Name=\"mml_fOpacityMultiplier\" value=\"%\"></Float>\n" (Mat.mml_fOpacityMultiplier) to:Stream
							format "<Float Name=\"mml_fSpecularReflectionMult\" value=\"%\"></Float>\n" (Mat.mml_fSpecularReflectionMult) to:Stream
							format "<Vector4f Name=\"mml_vTintColor\" value=\"% % % 0.0\"/>\n" ((Mat.mml_vTintColor.r / 255.0)) ((Mat.mml_vTintColor.g / 255.0)) ((Mat.mml_vTintColor.b / 255.0)) to:Stream
							format "<Float Name=\"mml_fGoreThreshold\" value=\"%\"></Float>\n" (Mat.mml_fGoreThreshold) to:Stream
						)
						if (MatchID SemID #(111)) then	--wireframe
						(
							format "<Vector4f Name=\"mml_vFalloffParams\" value=\"% % % %\"/>\n" (Mat.mml_vFalloffParams4) (Mat.mml_vFalloffParams3) (Mat.mml_vFalloffParams2) (Mat.mml_vFalloffParams1) to:Stream
							format "<Vector4f Name=\"eng_vWireframeColor\" value=\"% % % 1.0\"/>\n" ((Mat.eng_vWireframeColor.r / 255.0)) ((Mat.eng_vWireframeColor.g / 255.0)) ((Mat.eng_vWireframeColor.b / 255.0)) to:Stream
						)
					)
					20:(	--Weapontrail.mat
						if (MatchID SemID #(1)) then	--default
						(
							format "<Texture Name=\"mml_tLightmap\" ResName=\"LM.dds\" RequiresID=\"true\"></Texture>\n" to:Stream
							format "<Texture Name=\"mml_tDiffuse\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_Diffuse "weapontrail_streaks") to:Stream
							format "<Texture Name=\"mml_tDistortionModifiers\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_DistortionModifier "Default_White") to:Stream
							format "<Texture Name=\"mml_tDistortionVolume\" ResName=\"%\"></Texture>\n" (GetFileFromBitmap Mat.Map_DistortionVolume "Aur_WhiteVol") to:Stream
							format "<Float Name=\"mml_fdistortion_fade_distance\" value=\"%\"></Float>\n" (Mat.mml_fdistortion_fade_distance) to:Stream
							format "<Float Name=\"mml_fdistortion_fade_multiplier\" value=\"%\"></Float>\n" (Mat.mml_fdistortion_fade_multiplier) to:Stream
							format "<Float Name=\"mml_fdistortion_invert\" value=\"%\"></Float>\n" (Mat.mml_fdistortion_invert) to:Stream
							format "<Float Name=\"mml_fdistortion_magnitude\" value=\"%\"></Float>\n" (Mat.mml_fdistortion_magnitude) to:Stream
						)
					)
				) catch (MessageBox ("Error reading from Material of Object: \""+Item[1].name+"\" Material: \""+MatName + ".mao\"\nCheck Dragon Age Material is up to date, valid, or reapply.") title:"Model Export")
				
				format "</MaterialObject>\n" to:Stream
				
				--**************
				--Closing..
				--**************
				if not(DAOTools.isgmax) then flush Stream
				DAOTools.Fclose Stream
			)
		)
	)