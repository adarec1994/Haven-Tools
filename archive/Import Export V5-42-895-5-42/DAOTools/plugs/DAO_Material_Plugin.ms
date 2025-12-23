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
Material Plugin
DAOMaterialPlugin.ms
Dragon Age Material
************************************************************************************************************
*/
(
	struct DAOMatSemanticStruct (
		ID,					--ID to handle Semantics in the properties rollouts, via ID rather than appearance in dropdownlist
		SemanticDisplay,	--This text displayed in the dropdownlist, for this semantic
		LOD1Semantic,		--One "Semantic" actually holds up to 3 LOD semantics, this is IDs rather than strings to avoid case sense problems
		LOD2Semantic,		--ID Tables are in "DAOcommon.ms"
		LOD3Semantic
	)

	struct DAOMatTypeStruct (
		TypeDisplay,		--Material types ,this text is displayed to the user
		Type,				--And this is corresponding ID ,translates into string ie:"Static.mat"
		Semantics = #(),	--Holding Semantics ,array #(struct DAOMatSemanticStruct)
		SemanticsExt = #(),	--Holding Advanced or debug Semantics, or whatever these are. array #(struct DAOMatSemanticStruct)
		DefaultSem			--The Default Semantic Index.
	)
	
	struct DAOMatIniStruct (		--Standalone struct of global stuff for the Plugin
		MatTypes = #(),				--Holds the Material Types available. array #(struct DAOMatTypeStruct)
		RollUpStates = #(true,false,false),		--Rollup states for the 3 Rollouts
		CurrentPath = "C:\\",		--Recent Path, when browsing Maps via the "..." button for Gmax users
		fn CheckMap Map = (			--Checks if Maps are Bitmaps and exportable, prints Warning to the user
			if (Classof Map) != Bitmaptexture and Map != undefined then
			(
				Messagebox ("Make sure you apply a Bitmap, containing a proper Filename before you export a MAO Material File") title:"Material Warning"
			)
		),
		fn MatchID TheID InArray = (
			for item in InArray do if (item == TheID) then return true
			false
		)
	)
	
	fn DAOMatInitialize = (			--Initializes the global "DAOMatIni"
		local Temp
		global DAOMatIni = DAOMatIniStruct()
		
		Temp = DAOMatTypeStruct TypeDisplay:"Black" Type:1 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default Black Mat" LOD1Semantic:1)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:2 SemanticDisplay:"Wireframe" LOD1Semantic:111)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Canopy" Type:2 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default (Wild Sylvan)[Punchy][Mul Tint][Addv Blend]" LOD1Semantic:4 LOD2Semantic:5 LOD3Semantic:20)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:2 SemanticDisplay:"Wireframe" LOD1Semantic:111)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:3 SemanticDisplay:"Debug" LOD1Semantic:117)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:4 SemanticDisplay:"PSSM" LOD1Semantic:112)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:5 SemanticDisplay:"DepthWrite" LOD1Semantic:113)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:6 SemanticDisplay:"DepthWriteDebug" LOD1Semantic:114)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Character" Type:3 DefaultSem:3
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"[Additive Tint][Alpha Blending][No Emissive]" LOD1Semantic:10 LOD2Semantic:12 LOD3Semantic:9)
		append Temp.Semantics (DAOMatSemanticStruct ID:2 SemanticDisplay:"[Additive Tint][Alpha Blending][Use Emissive]" LOD1Semantic:15 LOD2Semantic:17 LOD3Semantic:9)
		append Temp.Semantics (DAOMatSemanticStruct ID:3 SemanticDisplay:"[Additive Tint][No Alpha][No Emissive]" LOD1Semantic:7 LOD2Semantic:8 LOD3Semantic:9)
		append Temp.Semantics (DAOMatSemanticStruct ID:4 SemanticDisplay:"[Additive Tint][No Alpha][Use Emissive]" LOD1Semantic:13 LOD2Semantic:14 LOD3Semantic:9)
		append Temp.Semantics (DAOMatSemanticStruct ID:5 SemanticDisplay:"[Additive Tint][Punchthrough][No Emissive]" LOD1Semantic:11 LOD2Semantic:12 LOD3Semantic:9)
		append Temp.Semantics (DAOMatSemanticStruct ID:6 SemanticDisplay:"[Additive Tint][Punchthrough][Use Emissive]" LOD1Semantic:16 LOD2Semantic:17 LOD3Semantic:9)
		append Temp.Semantics (DAOMatSemanticStruct ID:7 SemanticDisplay:"[Armor w/ SkinTint][Alpha Blending][No Emissive]" LOD1Semantic:32 LOD2Semantic:34 LOD3Semantic:31)
		append Temp.Semantics (DAOMatSemanticStruct ID:8 SemanticDisplay:"[Armor w/ SkinTint][Alpha Blending][Use Emissive]" LOD1Semantic:37 LOD2Semantic:39 LOD3Semantic:31)
		append Temp.Semantics (DAOMatSemanticStruct ID:9 SemanticDisplay:"[Armor w/ SkinTint][No Alpha][No Emissive]" LOD1Semantic:29 LOD2Semantic:30 LOD3Semantic:31)
		append Temp.Semantics (DAOMatSemanticStruct ID:10 SemanticDisplay:"[Armor w/ SkinTint][No Alpha][Use Emissive]" LOD1Semantic:35 LOD2Semantic:36 LOD3Semantic:31)
		append Temp.Semantics (DAOMatSemanticStruct ID:11 SemanticDisplay:"[Armor w/ SkinTint][Punchthrough][No Emissive]" LOD1Semantic:33 LOD2Semantic:34 LOD3Semantic:31)
		append Temp.Semantics (DAOMatSemanticStruct ID:12 SemanticDisplay:"[Armor w/ SkinTint][Punchthrough][Use Emissive]" LOD1Semantic:38 LOD2Semantic:39 LOD3Semantic:31)
		append Temp.Semantics (DAOMatSemanticStruct ID:13 SemanticDisplay:"[Multiply Tint][Alpha Blending][No Emissive]" LOD1Semantic:21 LOD2Semantic:23 LOD3Semantic:20)
		append Temp.Semantics (DAOMatSemanticStruct ID:14 SemanticDisplay:"[Multiply Tint][Alpha Blending][Use Emissive]" LOD1Semantic:26 LOD2Semantic:28 LOD3Semantic:20)
		append Temp.Semantics (DAOMatSemanticStruct ID:15 SemanticDisplay:"[Multiply Tint][No Alpha][No Emissive]" LOD1Semantic:18 LOD2Semantic:19 LOD3Semantic:20)
		append Temp.Semantics (DAOMatSemanticStruct ID:16 SemanticDisplay:"[Multiply Tint][No Alpha][Use Emissive]" LOD1Semantic:24 LOD2Semantic:25 LOD3Semantic:20)
		append Temp.Semantics (DAOMatSemanticStruct ID:17 SemanticDisplay:"[Multiply Tint][Punchthrough][No Emissive]" LOD1Semantic:22 LOD2Semantic:23 LOD3Semantic:20)
		append Temp.Semantics (DAOMatSemanticStruct ID:18 SemanticDisplay:"[Multiply Tint][Punchthrough][Use Emissive]" LOD1Semantic:27 LOD2Semantic:28 LOD3Semantic:20)
		append Temp.Semantics (DAOMatSemanticStruct ID:19 SemanticDisplay:"[No Tint][Alpha Blending][No Emissive]" LOD1Semantic:43 LOD2Semantic:45 LOD3Semantic:42)
		append Temp.Semantics (DAOMatSemanticStruct ID:20 SemanticDisplay:"[No Tint][Alpha Blending][Use Emissive]" LOD1Semantic:49 LOD2Semantic:51 LOD3Semantic:42)
		append Temp.Semantics (DAOMatSemanticStruct ID:21 SemanticDisplay:"[No Tint][No Alpha][No Emissive]" LOD1Semantic:40 LOD2Semantic:41 LOD3Semantic:42)
		append Temp.Semantics (DAOMatSemanticStruct ID:22 SemanticDisplay:"[No Tint][No Alpha][Use Emissive]" LOD1Semantic:46 LOD2Semantic:47 LOD3Semantic:48)
		append Temp.Semantics (DAOMatSemanticStruct ID:23 SemanticDisplay:"[No Tint][Punchthrough][No Emissive]" LOD1Semantic:44 LOD2Semantic:45 LOD3Semantic:42)
		append Temp.Semantics (DAOMatSemanticStruct ID:24 SemanticDisplay:"[No Tint][Punchthrough][Use Emissive]" LOD1Semantic:50 LOD2Semantic:51 LOD3Semantic:42)
		append Temp.Semantics (DAOMatSemanticStruct ID:25 SemanticDisplay:"Sylvan[Mul Tint][Punch][No Emiss][Addv Blend]" LOD1Semantic:4 LOD2Semantic:5 LOD3Semantic:20)
		append Temp.Semantics (DAOMatSemanticStruct ID:26 SemanticDisplay:"Transparent Body[Mul Tint][Alpha Blend][No Emiss]" LOD1Semantic:53 LOD2Semantic:23 LOD3Semantic:20)
		append Temp.Semantics (DAOMatSemanticStruct ID:27 SemanticDisplay:"Transparent Body[No Tint][Alpha Blend][No Emiss]" LOD1Semantic:52 LOD2Semantic:45 LOD3Semantic:42)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:28 SemanticDisplay:"Debug" LOD1Semantic:117)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:29 SemanticDisplay:"Wireframe" LOD1Semantic:111)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:30 SemanticDisplay:"DepthWriteDebug" LOD1Semantic:114)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:31 SemanticDisplay:"DepthWrite" LOD1Semantic:113)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:32 SemanticDisplay:"DepthWriteLowLOD" LOD1Semantic:115)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:33 SemanticDisplay:"OpaqueDepthWrite" LOD1Semantic:116)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:34 SemanticDisplay:"PSSM" LOD1Semantic:112)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Cloth" Type:4 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default (Opaque,Cloth,2 sided)" LOD1Semantic:1)
		append Temp.Semantics (DAOMatSemanticStruct ID:2 SemanticDisplay:"Blend (Transparent,Cloth,2 sided)" LOD1Semantic:3)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:3 SemanticDisplay:"Wireframe" LOD1Semantic:111)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:4 SemanticDisplay:"DepthWrite" LOD1Semantic:113)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:5 SemanticDisplay:"DepthWriteDebug" LOD1Semantic:114)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Eye" Type:8 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default (Opaque,Character,eye)" LOD1Semantic:1)
		append Temp.Semantics (DAOMatSemanticStruct ID:2 SemanticDisplay:"Eyelash (Transparent,Character,eye)" LOD1Semantic:69)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:3 SemanticDisplay:"Wireframe (Opaque)" LOD1Semantic:111)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:4 SemanticDisplay:"Debug" LOD1Semantic:117)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:5 SemanticDisplay:"DepthWrite" LOD1Semantic:113)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Face" Type:9 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default (Face,Character)" LOD1Semantic:1 LOD2Semantic:56 LOD3Semantic:57)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:2 SemanticDisplay:"Wireframe (Character)" LOD1Semantic:111)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:3 SemanticDisplay:"PSSM" LOD1Semantic:112)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:4 SemanticDisplay:"DepthWrite" LOD1Semantic:113)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:5 SemanticDisplay:"DepthWriteLowLOD" LOD1Semantic:115)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:6 SemanticDisplay:"DepthWriteDebug" LOD1Semantic:114)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Hair" Type:11 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default (MultipassHair,Character)" LOD1Semantic:1 LOD2Semantic:56 LOD3Semantic:57)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:2 SemanticDisplay:"Wireframe (Transparent,PunchThrough)" LOD1Semantic:111)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:3 SemanticDisplay:"Overdraw (Transparent,Hair,Character)" LOD1Semantic:118)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:4 SemanticDisplay:"Debug (Transparent,Hair,Character)" LOD1Semantic:117)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:5 SemanticDisplay:"DepthWriteDebug" LOD1Semantic:114)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:6 SemanticDisplay:"DepthWrite" LOD1Semantic:113)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:7 SemanticDisplay:"PSSM" LOD1Semantic:112)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Lava" Type:13 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Diffuse (Opaque)" LOD1Semantic:54)
		append Temp.Semantics (DAOMatSemanticStruct ID:2 SemanticDisplay:"Volume (Opaque)" LOD1Semantic:55)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Prop Default" Type:14 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default (Opaque,Prop)" LOD1Semantic:1)
		append Temp.Semantics (DAOMatSemanticStruct ID:2 SemanticDisplay:"Punch (Opaque,Prop,PunchThrough)" LOD1Semantic:2)
		append Temp.Semantics (DAOMatSemanticStruct ID:3 SemanticDisplay:"Blend (Transparent,Prop)" LOD1Semantic:3)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:4 SemanticDisplay:"Wireframe (Opaque)" LOD1Semantic:111)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:5 SemanticDisplay:"PSSM" LOD1Semantic:112)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:6 SemanticDisplay:"DepthWrite" LOD1Semantic:113)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:7 SemanticDisplay:"DepthWriteDebug" LOD1Semantic:114)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Static (lightmappable)" Type:16 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default (Opaque)" LOD1Semantic:1)
		append Temp.Semantics (DAOMatSemanticStruct ID:2 SemanticDisplay:"LOD1Relief (Opaque,Relief)" LOD1Semantic:75)
		append Temp.Semantics (DAOMatSemanticStruct ID:3 SemanticDisplay:"LOD1Blend (Transparent)" LOD1Semantic:76)
		append Temp.Semantics (DAOMatSemanticStruct ID:4 SemanticDisplay:"LOD1BlendFlat (Transparent,BlendFlat)" LOD1Semantic:77)
		append Temp.Semantics (DAOMatSemanticStruct ID:5 SemanticDisplay:"LOD1Punch (PunchThrough)" LOD1Semantic:78)
		append Temp.Semantics (DAOMatSemanticStruct ID:6 SemanticDisplay:"LOD3 (Opaque)" LOD3Semantic:57)
		append Temp.Semantics (DAOMatSemanticStruct ID:7 SemanticDisplay:"LOD3Punch (PunchThrough)" LOD3Semantic:79)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:8 SemanticDisplay:"Debug (Opaque)" LOD1Semantic:117)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:9 SemanticDisplay:"Wireframe" LOD1Semantic:111)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:10 SemanticDisplay:"PSSM" LOD1Semantic:112)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:11 SemanticDisplay:"DepthWrite" LOD1Semantic:113)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:12 SemanticDisplay:"DepthWriteDebug" LOD1Semantic:114)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Water" Type:18 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default Water" LOD1Semantic:1)
		append Temp.Semantics (DAOMatSemanticStruct ID:2 SemanticDisplay:"Flowing Water" LOD1Semantic:108)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:3 SemanticDisplay:"NoSpec" LOD1Semantic:107)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:4 SemanticDisplay:"ToolsetProbeFlowingWater" LOD1Semantic:109)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:5 SemanticDisplay:"ToolsetWater" LOD1Semantic:110)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Weapon" Type:19 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default (Opaque,Weapon,Character)" LOD1Semantic:1)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:2 SemanticDisplay:"Wireframe (Opaque)" LOD1Semantic:111)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:3 SemanticDisplay:"PSSM" LOD1Semantic:112)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:4 SemanticDisplay:"DepthWrite" LOD1Semantic:113)
		append Temp.SemanticsExt (DAOMatSemanticStruct ID:5 SemanticDisplay:"DepthWriteDebug" LOD1Semantic:114)
		append DAOMatIni.MatTypes Temp
		
		Temp = DAOMatTypeStruct TypeDisplay:"Weapontrail" Type:20 DefaultSem:1
		append Temp.Semantics (DAOMatSemanticStruct ID:1 SemanticDisplay:"Default (Distortion_Diffuse)" LOD1Semantic:1)
		append DAOMatIni.MatTypes Temp
	)
	DAOMatInitialize()
)

plugin material DAOMaterial
name:"Dragon Age Material"
classID:#(5690245,3547892)
extends:Standard replaceUI:true version:1
(
	parameters main rollout:MatTypeRollout
	(
		MatTypeID animatable:false type:#integer default:0
		MatSemanticID animatable:false type:#integer default:0
	)
	
	parameters matprops rollout:MatPropertyRollout
	(
		--Flag to prevent undefined params when material is first created
		ParamPropsInit animatable:false type:#boolean default:true
		
		--shared Parameters, for all Material Types. Typically only some are used per Material
		mml_vFalloffParams1 animatable:false type:#float
		mml_vFalloffParams2 animatable:false type:#float
		mml_vFalloffParams3 animatable:false type:#float
		mml_vFalloffParams4 animatable:false type:#float
		
		mml_vEyeSpecularParameters_CSP animatable:false type:#float
		mml_vEyeSpecularParameters_CSM animatable:false type:#float
		mml_vEyeSpecularParameters_SSP animatable:false type:#float
		mml_vEyeSpecularParameters_SSM animatable:false type:#float
		
		mml_vHairParameters_PSM animatable:false type:#float
		mml_vHairParameters_PSP animatable:false type:#float
		mml_vHairParameters_SSP animatable:false type:#float
		mml_vHairParameters_NTTF animatable:false type:#float
		
		mml_fSpecularReflectionMult	animatable:false type:#float
		mml_fSpecularPower			animatable:false type:#float
		mml_fOpacityMultiplier		animatable:false type:#float
		mml_vTintColor				animatable:false type:#color
		eng_vWireframeColor			animatable:false type:#color
		mml_iSoundMaterialType		animatable:false type:#integer
		mml_fReliefAspect			animatable:false type:#float
		mml_fGoreThreshold			animatable:false type:#float
		g_vAmbientMult				animatable:false type:#color
		g_fLipSpecularBoost			animatable:false type:#float
		g_fRimPower					animatable:false type:#float
		g_fSpecularMult				animatable:false type:#float
		
		mml_fLavaBrightness			animatable:false type:#float
		mml_fLavaContrast			animatable:false type:#float
		mml_vLavaTint				animatable:false type:#color
		
		mml_vUVScrollSpeed_1U		animatable:false type:#float
		mml_vUVScrollSpeed_1V		animatable:false type:#float
		mml_vUVScrollSpeed_2U		animatable:false type:#float
		mml_vUVScrollSpeed_2V		animatable:false type:#float
		mml_vMaskUVScrollSpeed_U	animatable:false type:#float
		mml_vMaskUVScrollSpeed_V	animatable:false type:#float
		
		mml_fdistortion_fade_distance	animatable:false type:#float
		mml_fdistortion_fade_multiplier	animatable:false type:#float
		mml_fdistortion_invert			animatable:false type:#float
		mml_fdistortion_magnitude		animatable:false type:#float
		
		Water_Wave1_Height			animatable:false type:#float	--translates into MAO "mat_vPSHWaterParams" Array4x2 (1 thru 8)
		Water_Wave2_Height			animatable:false type:#float
		Water_Wave3_Height			animatable:false type:#float
		Water_MinVertexColorAlpha	animatable:false type:#float
		Water_SpecularFalloff		animatable:false type:#float
		Water_SpecularMultiplier	animatable:false type:#float
		Water_SunlightSpecularPower	animatable:false type:#float
		Water_OpacityFalloff		animatable:false type:#float
		
		Water_Flow1_Direction	animatable:false type:#float		--stored in the MAO as "mml_vVSHWaterDiffuseParams" Vector4f x 2
		Water_Flow1_Speed		animatable:false type:#float
		Water_Flow1_TilingU		animatable:false type:#float
		Water_Flow1_TilingV		animatable:false type:#float
		Water_Flow2_Direction	animatable:false type:#float
		Water_Flow2_Speed		animatable:false type:#float
		Water_Flow2_TilingU		animatable:false type:#float
		Water_Flow2_TilingV		animatable:false type:#float
		
		Water_EdgeMask_Direction	animatable:false type:#float	--those stored in the MAO as "mml_vVSHWaterAlphaMaskParams" Vector4f
		Water_EdgeMask_Speed		animatable:false type:#float
		Water_EdgeMask_TilingU		animatable:false type:#float
		Water_EdgeMask_TilingV		animatable:false type:#float
		
		Water_Wave1_Direction	animatable:false type:#float	--stored in the MAO as "mat_vVSHWaterParams" Vector4f x3
		Water_Wave1_Speed		animatable:false type:#float
		Water_Wave1_Tiling		animatable:false type:#float
		Water_Wave2_Direction	animatable:false type:#float
		Water_Wave2_Speed		animatable:false type:#float
		Water_Wave2_Tiling		animatable:false type:#float
		Water_Wave3_Direction	animatable:false type:#float
		Water_Wave3_Speed		animatable:false type:#float
		Water_Wave3_Tiling		animatable:false type:#float
		
		
		--The following 4 are not connected to the MAO export, instead they are used in MSH export
		--Mat_UseColorChannel		animatable:false type:#boolean default:false
		--Mat_UseAlphaChannel		animatable:false type:#boolean default:false
		--Mat_VertexColor			animatable:false type:#color default:(color 127 159 135)
		--Mat_VertexAlpha			animatable:false type:#float default:0.5
	)
	
	parameters Maps rollout:MatMapsRollout
	(
		--Maps for all materials ,shared
		Map_Diffuse animatable:false type:#TextureMap ui:mbtn1
		Map_Normal animatable:false type:#TextureMap ui:mbtn2
		Map_Specular animatable:false type:#TextureMap ui:mbtn3
		Map_Emissive animatable:false type:#TextureMap ui:mbtn4
		Map_Tint animatable:false type:#TextureMap ui:mbtn5
		Map_AgeDiffuse animatable:false type:#TextureMap ui:mbtn6
		Map_AgeNormal animatable:false type:#TextureMap ui:mbtn7
		Map_CorneaNormal animatable:false type:#TextureMap ui:mbtn8
		Map_Distortion animatable:false type:#TextureMap ui:mbtn9
		Map_DistortionModifier animatable:false type:#TextureMap ui:mbtn10
		Map_DistortionVolume animatable:false type:#TextureMap ui:mbtn11
		Map_Emotions0 animatable:false type:#TextureMap ui:mbtn12
		Map_Emotions1 animatable:false type:#TextureMap ui:mbtn13
		Map_EmotionsNormal animatable:false type:#TextureMap ui:mbtn14
		Map_Tattoo animatable:false type:#TextureMap ui:mbtn15
		Map_SkyA animatable:false type:#TextureMap ui:mbtn16
		Map_SkyB animatable:false type:#TextureMap ui:mbtn17
		Map_PerlinNoise animatable:false type:#TextureMap ui:mbtn18
		Map_Fresnel animatable:false type:#TextureMap ui:mbtn19
		Map_BrowStubble animatable:false type:#TextureMap ui:mbtn20
		Map_BrowStubbleNormal animatable:false type:#TextureMap ui:mbtn21
		Map_TintNoise animatable:false type:#TextureMap ui:mbtn22
		Map_Noise animatable:false type:#TextureMap ui:mbtn23
		Map_CelestialBody animatable:false type:#TextureMap ui:mbtn24
		Map_Packed animatable:false type:#TextureMap ui:mbtn25
		Map_Relief animatable:false type:#TextureMap ui:mbtn26
		Map_Gore animatable:false type:#TextureMap ui:mbtn27
		Map_Heraldry animatable:false type:#TextureMap ui:mbtn28
		Map_Beckmann animatable:false type:#TextureMap ui:mbtn29
		Map_WaterMask animatable:false type:#TextureMap ui:mbtn30
		Map_Decal animatable:false type:#TextureMap ui:mbtn31
		
		
		on Map_Diffuse set tex do (DAOMatIni.CheckMap tex ;delegate.DiffuseMap = tex ;delegate.DiffuseMapAmount = 100 ;delegate.DiffuseMapEnable = true)
		on Map_Normal set tex do (DAOMatIni.CheckMap tex ;delegate.BumpMap = tex ;delegate.BumpMapAmount = 100 ;delegate.BumpMapEnable = true)
		on Map_Specular set tex do (DAOMatIni.CheckMap tex ;delegate.SpecularMap = tex ;delegate.SpecularMapAmount = 100 ;delegate.SpecularMapEnable = true)
		on Map_Emissive set tex do (DAOMatIni.CheckMap tex ;delegate.selfIllumMap = tex ;delegate.selfIllumMapAmount = 100 ;delegate.selfIllumMapEnable = true)
		on Map_Tint set tex do (DAOMatIni.CheckMap tex)
		on Map_AgeDiffuse set tex do (DAOMatIni.CheckMap tex)
		on Map_AgeNormal set tex do (DAOMatIni.CheckMap tex)
		on Map_CorneaNormal set tex do (DAOMatIni.CheckMap tex)
		on Map_Distortion set tex do (DAOMatIni.CheckMap tex)
		on Map_DistortionModifier set tex do (DAOMatIni.CheckMap tex)
		on Map_DistortionVolume set tex do (DAOMatIni.CheckMap tex)
		on Map_Emotions0 set tex do (DAOMatIni.CheckMap tex)
		on Map_Emotions1 set tex do (DAOMatIni.CheckMap tex)
		on Map_EmotionsNormal set tex do (DAOMatIni.CheckMap tex)
		on Map_Tattoo set tex do (DAOMatIni.CheckMap tex)
		on Map_SkyA set tex do (DAOMatIni.CheckMap tex)
		on Map_SkyB set tex do (DAOMatIni.CheckMap tex)
		on Map_PerlinNoise set tex do (DAOMatIni.CheckMap tex)
		on Map_Fresnel set tex do (DAOMatIni.CheckMap tex)
		on Map_BrowStubble set tex do (DAOMatIni.CheckMap tex)
		on Map_BrowStubbleNormal set tex do (DAOMatIni.CheckMap tex)
		on Map_TintNoise set tex do (DAOMatIni.CheckMap tex)
		on Map_Noise set tex do (DAOMatIni.CheckMap tex)
		on Map_CelestialBody set tex do (DAOMatIni.CheckMap tex)
		on Map_Packed set tex do (DAOMatIni.CheckMap tex)
		on Map_Relief set tex do (DAOMatIni.CheckMap tex)
		on Map_Gore set tex do (DAOMatIni.CheckMap tex)
		on Map_Heraldry set tex do (DAOMatIni.CheckMap tex)
		on Map_Beckmann set tex do (DAOMatIni.CheckMap tex)
		on Map_WaterMask set tex do (DAOMatIni.CheckMap tex)
		on Map_Decal set tex do (DAOMatIni.CheckMap tex)
		--on Map_RMPDecal set tex do (DAOMatIni.CheckMap tex)
	)
	
	rollout MatMapsRollout "Texture Maps" width:330 height:310
	(
		mapButton mbtn1 "none" pos:[125,5] width:155 height:18 ;button btn1 "..." pos:[282,5] width:18 height:18 ;label lbl1 "Diffuse Map . . . . . . . . . ." pos:[3,5] width:120 height:13 ;button btn_c1 "C" pos:[302,5] width:18 height:18
		mapButton mbtn2 "none" pos:[125,25] width:155 height:18 ;button btn2 "..." pos:[282,25] width:18 height:18 ;label lbl2 "Normal Map . . . . . . . . . . . . ." pos:[3,25] width:120 height:13 ;button btn_c2 "C" pos:[302,25] width:18 height:18
		mapButton mbtn3 "none" pos:[125,45] width:155 height:18 ;button btn3 "..." pos:[282,45] width:18 height:18 ;label lbl3 "Specular Mask . . . . . . . . . . . . ." pos:[3,45] width:120 height:13 ;button btn_c3 "C" pos:[302,45] width:18 height:18
		mapButton mbtn4 "none" pos:[125,65] width:155 height:18 ;button btn4 "..." pos:[282,65] width:18 height:18 ;label lbl4 "Emissive Mask . . . . . . . . . . . . ." pos:[3,65] width:120 height:13 ;button btn_c4 "C" pos:[302,65] width:18 height:18
		mapButton mbtn5 "none" pos:[125,85] width:155 height:18 ;button btn5 "..." pos:[282,85] width:18 height:18 ;label lbl5 "Tint Mask . . . . . . . . . . . . . . ." pos:[3,85] width:120 height:13 ;button btn_c5 "C" pos:[302,85] width:18 height:18
		mapButton mbtn6 "none" pos:[125,105] width:155 height:18 ;button btn6 "..." pos:[282,105] width:18 height:18 ;label lbl6 "Age Diffuse Map . . . . . . . . . . ." pos:[3,105] width:120 height:13 ;button btn_c6 "C" pos:[302,105] width:18 height:18
		mapButton mbtn7 "none" pos:[125,125] width:155 height:18 ;button btn7 "..." pos:[282,125] width:18 height:18 ;label lbl7 "Age Normal Map . . . . . . . . . . . ." pos:[3,125] width:120 height:13 ;button btn_c7 "C" pos:[302,125] width:18 height:18
		mapButton mbtn8 "none" pos:[125,145] width:155 height:18 ;button btn8 "..." pos:[282,145] width:18 height:18 ;label lbl8 "Cornea Normal Map . . . . . . . . ." pos:[3,145] width:120 height:13 ;button btn_c8 "C" pos:[302,145] width:18 height:18
		mapButton mbtn9 "none" pos:[125,165] width:155 height:18 ;button btn9 "..." pos:[282,165] width:18 height:18 ;label lbl9 "Distortion Map . . . . . . . . . . . ." pos:[3,165] width:120 height:13 ;button btn_c9 "C" pos:[302,165] width:18 height:18
		mapButton mbtn10 "none" pos:[125,185] width:155 height:18 ;button btn10 "..." pos:[282,185] width:18 height:18 ;label lbl10 "Distortion Modifier Map . . . . . . ." pos:[3,185] width:120 height:13 ;button btn_c10 "C" pos:[302,185] width:18 height:18
		mapButton mbtn11 "none" pos:[125,205] width:155 height:18 ;button btn11 "..." pos:[282,205] width:18 height:18 ;label lbl11 "Distortion Volume Map . . . . . . ." pos:[3,205] width:120 height:13 ;button btn_c11 "C" pos:[302,205] width:18 height:18
		mapButton mbtn12 "none" pos:[125,225] width:155 height:18 ;button btn12 "..." pos:[282,225] width:18 height:18 ;label lbl12 "Emotions Mask 0 . . . . . . . . . ." pos:[3,225] width:120 height:13 ;button btn_c12 "C" pos:[302,225] width:18 height:18
		mapButton mbtn13 "none" pos:[125,245] width:155 height:18 ;button btn13 "..." pos:[282,245] width:18 height:18 ;label lbl13 "Emotions Mask 1 . . . . . . . . . ." pos:[3,245] width:120 height:13 ;button btn_c13 "C" pos:[302,245] width:18 height:18
		mapButton mbtn14 "none" pos:[125,265] width:155 height:18 ;button btn14 "..." pos:[282,265] width:18 height:18 ;label lbl14 "Emotions Normal Map . . . . . . ." pos:[3,265] width:120 height:13 ;button btn_c14 "C" pos:[302,265] width:18 height:18
		mapButton mbtn15 "none" pos:[125,285] width:155 height:18 ;button btn15 "..." pos:[282,285] width:18 height:18 ;label lbl15 "Tattoo Mask . . . . . . . . . . . ." pos:[3,285] width:120 height:13 ;button btn_c15 "C" pos:[302,285] width:18 height:18
		mapButton mbtn16 "none" pos:[125,305] width:155 height:18 ;button btn16 "..." pos:[282,305] width:18 height:18 ;label lbl16 "Sky Texture A . . . . . . . . . . ." pos:[3,305] width:120 height:13 ;button btn_c16 "C" pos:[302,305] width:18 height:18
		mapButton mbtn17 "none" pos:[125,325] width:155 height:18 ;button btn17 "..." pos:[282,325] width:18 height:18 ;label lbl17 "Sky Texture B . . . . . . . . . . ." pos:[3,325] width:120 height:13 ;button btn_c17 "C" pos:[302,325] width:18 height:18
		mapButton mbtn18 "none" pos:[125,345] width:155 height:18 ;button btn18 "..." pos:[282,345] width:18 height:18 ;label lbl18 "Perlin Noise Map . . . . . . . . . ." pos:[3,345] width:120 height:13 ;button btn_c18 "C" pos:[302,345] width:18 height:18
		mapButton mbtn19 "none" pos:[125,365] width:155 height:18 ;button btn19 "..." pos:[282,365] width:18 height:18 ;label lbl19 "Fresnel Mask . . . . . . . . . . . . ." pos:[3,365] width:120 height:13 ;button btn_c19 "C" pos:[302,365] width:18 height:18
		mapButton mbtn20 "none" pos:[125,385] width:155 height:18 ;button btn20 "..." pos:[282,385] width:18 height:18 ;label lbl20 "Brow Stubble Map . . . . . . . . ." pos:[3,385] width:120 height:13 ;button btn_c20 "C" pos:[302,385] width:18 height:18
		mapButton mbtn21 "none" pos:[125,405] width:155 height:18 ;button btn21 "..." pos:[282,405] width:18 height:18 ;label lbl21 "Brow Stubble Normal Map . . . . ." pos:[3,405] width:120 height:13 ;button btn_c21 "C" pos:[302,405] width:18 height:18
		mapButton mbtn22 "none" pos:[125,425] width:155 height:18 ;button btn22 "..." pos:[282,425] width:18 height:18 ;label lbl22 "Tint Noise Map . . . . . . . . ." pos:[3,425] width:120 height:13 ;button btn_c22 "C" pos:[302,425] width:18 height:18
		mapButton mbtn23 "none" pos:[125,445] width:155 height:18 ;button btn23 "..." pos:[282,445] width:18 height:18 ;label lbl23 "Noise Map . . . . . . . . . . ." pos:[3,445] width:120 height:13 ;button btn_c23 "C" pos:[302,445] width:18 height:18
		mapButton mbtn24 "none" pos:[125,465] width:155 height:18 ;button btn24 "..." pos:[282,465] width:18 height:18 ;label lbl24 "Celestial Body Map . . . . . . ." pos:[3,465] width:120 height:13 ;button btn_c24 "C" pos:[302,465] width:18 height:18
		mapButton mbtn25 "none" pos:[125,485] width:155 height:18 ;button btn25 "..." pos:[282,485] width:18 height:18 ;label lbl25 "Packed Texture . . . . . . . . . ." pos:[3,485] width:120 height:13 ;button btn_c25 "C" pos:[302,485] width:18 height:18
		mapButton mbtn26 "none" pos:[125,505] width:155 height:18 ;button btn26 "..." pos:[282,505] width:18 height:18 ;label lbl26 "Relief Map . . . . . . . . . . . ." pos:[3,505] width:120 height:13 ;button btn_c26 "C" pos:[302,505] width:18 height:18
		mapButton mbtn27 "none" pos:[125,525] width:155 height:18 ;button btn27 "..." pos:[282,525] width:18 height:18 ;label lbl27 "Gore Map . . . . . . . . . . . ." pos:[3,525] width:120 height:13 ;button btn_c27 "C" pos:[302,525] width:18 height:18
		mapButton mbtn28 "none" pos:[125,545] width:155 height:18 ;button btn28 "..." pos:[282,545] width:18 height:18 ;label lbl28 "Heraldry Map . . . . . . . . ." pos:[3,545] width:120 height:13 ;button btn_c28 "C" pos:[302,545] width:18 height:18
		mapButton mbtn29 "none" pos:[125,565] width:155 height:18 ;button btn29 "..." pos:[282,565] width:18 height:18 ;label lbl29 "Beckmann Map . . . . . . . . ." pos:[3,565] width:120 height:13 ;button btn_c29 "C" pos:[302,565] width:18 height:18
		mapButton mbtn30 "none" pos:[125,585] width:155 height:18 ;button btn30 "..." pos:[282,585] width:18 height:18 ;label lbl30 "Water Edge . . . . . . . . ." pos:[3,585] width:120 height:13 ;button btn_c30 "C" pos:[302,585] width:18 height:18
		mapButton mbtn31 "none" pos:[125,605] width:155 height:18 ;button btn31 "..." pos:[282,605] width:18 height:18 ;label lbl31 "Foam Decal . . . . . . . . ." pos:[3,605] width:120 height:13 ;button btn_c31 "C" pos:[302,605] width:18 height:18
		
		local CurrentType
		local CurrentSem
		local CurrentLOD
		local TexFile
		local CurrentPath = "C:\\"
		
		--local CtrlCheckboxes = #(chk1,chk2,chk3,chk4,chk5,chk6,chk7,chk8,chk9,chk10,chk11,chk12,chk13,chk14,chk15,chk16,chk17,chk18,chk19,chk20,chk21,chk22,chk23,chk24,chk25,chk26,chk27,chk28,chk29,chk30,chk31)
		local CtrlLabels = #(lbl1,lbl2,lbl3,lbl4,lbl5,lbl6,lbl7,lbl8,lbl9,lbl10,lbl11,lbl12,lbl13,lbl14,lbl15,lbl16,lbl17,lbl18,lbl19,lbl20,lbl21,lbl22,lbl23,lbl24,lbl25,lbl26,lbl27,lbl28,lbl29,lbl30,lbl31)
		local CtrlMapButtons = #(mbtn1,mbtn2,mbtn3,mbtn4,mbtn5,mbtn6,mbtn7,mbtn8,mbtn9,mbtn10,mbtn11,mbtn12,mbtn13,mbtn14,mbtn15,mbtn16,mbtn17,mbtn18,mbtn19,mbtn20,mbtn21,mbtn22,mbtn23,mbtn24,mbtn25,mbtn26,mbtn27,mbtn28,mbtn29,mbtn30,mbtn31)
		local CtrlButtons = #(btn1,btn2,btn3,btn4,btn5,btn6,btn7,btn8,btn9,btn10,btn11,btn12,btn13,btn14,btn15,btn16,btn17,btn18,btn19,btn20,btn21,btn22,btn23,btn24,btn25,btn26,btn27,btn28,btn29,btn30,btn31)
		local CtrlCButtons = #(btn_c1,btn_c2,btn_c3,btn_c4,btn_c5,btn_c6,btn_c7,btn_c8,btn_c9,btn_c10,btn_c11,btn_c12,btn_c13,btn_c14,btn_c15,btn_c16,btn_c17,btn_c18,btn_c19,btn_c20,btn_c21,btn_c22,btn_c23,btn_c24,btn_c25,btn_c26,btn_c27,btn_c28,btn_c29,btn_c30,btn_c31)
		
		fn HideElements Excluding = (
			--*********************************************************************
			--Hides all texture map control elements ,except those listed in Excluding
			--Sorts the maps ,disabled at the bottom, enabled maps on top
			--********************************************************************
			local Height = 5
			local Heights = #()
			
			for i = 1 to Excluding.count do
			(
				if Heights[(Excluding[i])] == undefined then
				(
					Heights[(Excluding[i])] = Height
					Height += 20
				)
			)
			for i = 1 to CtrlMapButtons.count where ((finditem Excluding i) == 0) do
			(
				Heights[i] = Height
				Height += 20
			)
			
			for i = 1 to CtrlMapButtons.count do
			(
				--CtrlCheckboxes[i].pos = [CtrlCheckboxes[i].pos.x,Heights[i]+1]
				CtrlMapButtons[i].pos = [CtrlMapButtons[i].pos.x,Heights[i]]
				CtrlButtons[i].pos = [CtrlButtons[i].pos.x,Heights[i]]
				CtrlLabels[i].pos = [CtrlLabels[i].pos.x,Heights[i]]
				CtrlCButtons[i].pos = [CtrlCButtons[i].pos.x,Heights[i]]
				
				CtrlMapButtons[i].enabled = CtrlButtons[i].enabled = CtrlLabels[i].enabled = CtrlCButtons[i].enabled = ((finditem Excluding i) > 0)
			)
		)
		
		fn UpdateDialog = (
			--***************************************************************
			--Updates texture rollout, hiding mapbuttons not supported by mat
			--****************************************************************
			local DontHideThis = #()
			
			case CurrentType of
			(
				1:(		--Black.mat	(no maps)
				)
				2:(		--Canopy.mat
					if (DAOMatIni.MatchID CurrentSem #(1,3,4)) then join DontHideThis #(1)
					if (CurrentSem == 1 and CurrentLOD == 1) or CurrentSem == 3 then join DontHideThis #(1,2,3,5,27) --,28)
					if (DAOMatIni.MatchID CurrentSem #(1,3)) then join DontHideThis #(1,3,5,27)--,28)	
				)
				3:(		--Character.mat
					if (DAOMatIni.MatchID CurrentSem #(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28)) then
					(
						if CurrentLOD == 1 then join DontHideThis #(1,2)	--Diff,Norm
						if CurrentSem == 28 then join DontHideThis #(1,2)	--Diff,Norm
						join DontHideThis #(1,3,27)						--Diff,Spec,Gore
					)
					if (DAOMatIni.MatchID CurrentSem #(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,25,26,28)) then join DontHideThis #(5)		--Tint
					if (DAOMatIni.MatchID CurrentSem #(2,4,6,8,10,12,14,16,18,20,24)) and CurrentLOD != 3 then join DontHideThis #(4)	--Emissive
					if (DAOMatIni.MatchID CurrentSem #(22)) then join DontHideThis #(4)		--Emissive
				)
				4:(		--Cloth.Mat
					if (DAOMatIni.MatchID CurrentSem #(1,2)) then join DontHideThis #(1,3,5,27,28)
				)
				8:(		--Eye.mat
					if (DAOMatIni.MatchID CurrentSem #(1,4)) then join DontHideThis #(1,2,5,8)
					if (DAOMatIni.MatchID CurrentSem #(2)) then join DontHideThis #(1)
				)
				9:(		--Face.mat
					if (DAOMatIni.MatchID CurrentSem #(1)) then join DontHideThis #(1,2,5,6,7,12,13,14,15,20,21,27,29)
				)
				11:(	--Hairalpha.mat
					if (DAOMatIni.MatchID CurrentSem #(1,4)) then
					(
						if CurrentLOD == 1 or CurrentLOD == 2 then join DontHideThis #(25,22)
						if CurrentLOD == 3 then join DontHideThis #(25)
					)
					if (DAOMatIni.MatchID CurrentSem #(3,6)) then join DontHideThis #(25)
				)
				13:(	--Lava.mat
					if (DAOMatIni.MatchID CurrentSem #(1,2)) then join DontHideThis #(1,23)
				)
				14:(	--Prop.mat
					if (DAOMatIni.MatchID CurrentSem #(1,2,3)) then join DontHideThis #(1,2,3,4)
				)
				16:(	--Static.mat
					if (DAOMatIni.MatchID CurrentSem #(1,2,3,4,5,6,7,8)) then join DontHideThis #(1,2,3)
					if (DAOMatIni.MatchID CurrentSem #(2)) then join DontHideThis #(1,2,3,26)
				)
				18:(	--Water.mat
					if (DAOMatIni.MatchID CurrentSem #(1,3,5)) then join DontHideThis #(2)
					if (DAOMatIni.MatchID CurrentSem #(2,4)) then join DontHideThis #(2,9,10,30,31)
				)
				19:(	--Weapon.mat
					if (DAOMatIni.MatchID CurrentSem #(1)) then join DontHideThis #(1,2,3,4,5,27,28) --,28,29)
				)
				20:(	--Weapontrail.mat
					join DontHideThis #(1,10,11)
				)
			)
			HideElements DontHideThis
		)
		
		--********************************************************************
		--This is default open file query to allow GMax Users select DDS Files
		--via the "..." button
		--********************************************************************
		fn QueryMap = (
			TexFile = (getOpenFileName caption:"Select an DDS File" filename:(DAOMatIni.CurrentPath) types:"DDS files (*.dds)|*.dds|All Files (*.*)|*.*|")
			if TexFile != undefined then DAOMatIni.CurrentPath = getFilenamePath TexFile
		)
		on btn1 pressed do (QueryMap() ;if TexFile != undefined then Map_Diffuse = bitmapTexture filename:(TexFile))
		on btn2 pressed do (QueryMap() ;if TexFile != undefined then Map_Normal = bitmapTexture filename:(TexFile))
		on btn3 pressed do (QueryMap() ;if TexFile != undefined then Map_Specular = bitmapTexture filename:(TexFile))
		on btn4 pressed do (QueryMap() ;if TexFile != undefined then Map_Emissive = bitmapTexture filename:(TexFile))
		on btn5 pressed do (QueryMap() ;if TexFile != undefined then Map_Tint = bitmapTexture filename:(TexFile))
		on btn6 pressed do (QueryMap() ;if TexFile != undefined then Map_AgeDiffuse = bitmapTexture filename:(TexFile))
		on btn7 pressed do (QueryMap() ;if TexFile != undefined then Map_AgeNormal = bitmapTexture filename:(TexFile))
		on btn8 pressed do (QueryMap() ;if TexFile != undefined then Map_CorneaNormal = bitmapTexture filename:(TexFile))
		on btn9 pressed do (QueryMap() ;if TexFile != undefined then Map_Distortion = bitmapTexture filename:(TexFile))
		on btn10 pressed do (QueryMap() ;if TexFile != undefined then Map_DistortionModifier = bitmapTexture filename:(TexFile))
		on btn11 pressed do (QueryMap() ;if TexFile != undefined then Map_DistortionVolume = bitmapTexture filename:(TexFile))
		on btn12 pressed do (QueryMap() ;if TexFile != undefined then Map_Emotions0 = bitmapTexture filename:(TexFile))
		on btn13 pressed do (QueryMap() ;if TexFile != undefined then Map_Emotions1 = bitmapTexture filename:(TexFile))
		on btn14 pressed do (QueryMap() ;if TexFile != undefined then Map_EmotionsNormal = bitmapTexture filename:(TexFile))
		on btn15 pressed do (QueryMap() ;if TexFile != undefined then Map_Tattoo = bitmapTexture filename:(TexFile))
		on btn16 pressed do (QueryMap() ;if TexFile != undefined then Map_SkyA = bitmapTexture filename:(TexFile))
		on btn17 pressed do (QueryMap() ;if TexFile != undefined then Map_SkyB = bitmapTexture filename:(TexFile))
		on btn18 pressed do (QueryMap() ;if TexFile != undefined then Map_PerlinNoise = bitmapTexture filename:(TexFile))
		on btn19 pressed do (QueryMap() ;if TexFile != undefined then Map_Fresnel = bitmapTexture filename:(TexFile))
		on btn20 pressed do (QueryMap() ;if TexFile != undefined then Map_BrowStubble = bitmapTexture filename:(TexFile))
		on btn21 pressed do (QueryMap() ;if TexFile != undefined then Map_BrowStubbleNormal = bitmapTexture filename:(TexFile))
		on btn22 pressed do (QueryMap() ;if TexFile != undefined then Map_TintNoise = bitmapTexture filename:(TexFile))
		on btn23 pressed do (QueryMap() ;if TexFile != undefined then Map_Noise = bitmapTexture filename:(TexFile))
		on btn24 pressed do (QueryMap() ;if TexFile != undefined then Map_CelestialBody = bitmapTexture filename:(TexFile))
		on btn25 pressed do (QueryMap() ;if TexFile != undefined then Map_Packed = bitmapTexture filename:(TexFile))
		on btn26 pressed do (QueryMap() ;if TexFile != undefined then Map_Relief = bitmapTexture filename:(TexFile))
		on btn27 pressed do (QueryMap() ;if TexFile != undefined then Map_Gore = bitmapTexture filename:(TexFile))
		on btn28 pressed do (QueryMap() ;if TexFile != undefined then Map_Heraldry = bitmapTexture filename:(TexFile))
		on btn29 pressed do (QueryMap() ;if TexFile != undefined then Map_Beckmann = bitmapTexture filename:(TexFile))
		on btn30 pressed do (QueryMap() ;if TexFile != undefined then Map_WaterMask = bitmapTexture filename:(TexFile))
		on btn31 pressed do (QueryMap() ;if TexFile != undefined then Map_Decal = bitmapTexture filename:(TexFile))
		--on btn29 pressed do (QueryMap() ;if TexFile != undefined then Map_RMPDecal = bitmapTexture filename:(TexFile))
		
		
		--********************************************************************
		--"C" button handler, this button removes the map from the mapbutton.
		--Its for Gmax, where a remove function isnt available otherwise
		--********************************************************************
		on btn_c1 pressed do Map_Diffuse = undefined
		on btn_c2 pressed do Map_Normal = undefined
		on btn_c3 pressed do Map_Specular = undefined
		on btn_c4 pressed do Map_Emissive = undefined
		on btn_c5 pressed do Map_Tint = undefined
		on btn_c6 pressed do Map_AgeDiffuse = undefined
		on btn_c7 pressed do Map_AgeNormal = undefined
		on btn_c8 pressed do Map_CorneaNormal = undefined
		on btn_c9 pressed do Map_Distortion = undefined
		on btn_c10 pressed do Map_DistortionModifier = undefined
		on btn_c11 pressed do Map_DistortionVolume = undefined
		on btn_c12 pressed do Map_Emotions0 = undefined
		on btn_c13 pressed do Map_Emotions1 = undefined
		on btn_c14 pressed do Map_EmotionsNormal = undefined
		on btn_c15 pressed do Map_Tattoo = undefined
		on btn_c16 pressed do Map_SkyA = undefined
		on btn_c17 pressed do Map_SkyB = undefined
		on btn_c18 pressed do Map_PerlinNoise = undefined
		on btn_c19 pressed do Map_Fresnel = undefined
		on btn_c20 pressed do Map_BrowStubble = undefined
		on btn_c21 pressed do Map_BrowStubbleNormal = undefined
		on btn_c22 pressed do Map_TintNoise = undefined
		on btn_c23 pressed do Map_Noise = undefined
		on btn_c24 pressed do Map_CelestialBody = undefined
		on btn_c25 pressed do Map_Packed = undefined
		on btn_c26 pressed do Map_Relief = undefined
		on btn_c27 pressed do Map_Gore = undefined
		on btn_c28 pressed do Map_Heraldry = undefined
		on btn_c29 pressed do Map_Beckmann = undefined
		on btn_c30 pressed do Map_WaterMask = undefined
		on btn_c31 pressed do Map_Decal = undefined
		
		on MatMapsRollout rolledup state do DAOMatIni.RollUpStates[3] = state
		on MatMapsRollout open do ()
	)
	
	rollout MatPropertyRollout "Material Properties" width:347 height:375
	(
		--controls are shared by all materials
		GroupBox grp1 "" pos:[4,1000] width:216 height:65	--1000 to lenghten the rollout, shows glitches otherwise
		GroupBox grp2 "" pos:[4,70] width:216 height:65
		GroupBox grp3 "" pos:[4,136] width:216 height:102
		GroupBox grp4 "" pos:[4,136] width:216 height:102
		GroupBox grp5 "" pos:[4,136] width:216 height:102
		GroupBox grp6 "" pos:[4,136] width:216 height:102
		GroupBox grp7 "" pos:[4,136] width:216 height:102
		--GroupBox grp8 "" pos:[4,136] width:216 height:102
		
		label lbl1 "" pos:[15,25] width:130 height:13
		label lbl2 "" pos:[15,46] width:130 height:13
		label lbl3 "" pos:[15,90] width:130 height:13
		label lbl4 "" pos:[15,110] width:130 height:13
		label lbl5 "" pos:[15,175] width:130 height:13
		label lbl6 "" pos:[15,155] width:130 height:13
		label lbl7 "" pos:[15,195] width:130 height:13
		label lbl8 "" pos:[15,215] width:130 height:13
		label lbl9 "" pos:[15,215] width:130 height:13
		label lbl10 "" pos:[15,215] width:130 height:13
		label lbl11 "" pos:[15,215] width:130 height:13
		label lbl12 "" pos:[15,215] width:130 height:13
		label lbl13 "" pos:[15,215] width:130 height:13
		label lbl14 "" pos:[15,215] width:130 height:13
		label lbl15 "" pos:[15,215] width:130 height:13
		label lbl16 "" pos:[15,215] width:130 height:13
		label lbl17 "" pos:[15,215] width:130 height:13
		label lbl18 "" pos:[15,215] width:130 height:13
		label lbl19 "" pos:[15,215] width:130 height:13
		label lbl20 "" pos:[15,215] width:130 height:13
		label lbl21 "" pos:[15,215] width:130 height:13
		label lbl22 "" pos:[15,215] width:130 height:13
		label lbl23 "" pos:[15,215] width:130 height:13
		label lbl24 "" pos:[15,215] width:130 height:13
		label lbl25 "" pos:[15,215] width:130 height:13
		label lbl26 "" pos:[15,215] width:130 height:13
		label lbl27 "" pos:[15,215] width:130 height:13
		label lbl28 "" pos:[15,215] width:130 height:13
		label lbl29 "" pos:[15,215] width:130 height:13
		label lbl30 "" pos:[15,815] width:130 height:13
		label lbl31 "" pos:[15,815] width:130 height:13
		label lbl32 "" pos:[15,815] width:130 height:13
		label lbl33 "" pos:[15,815] width:130 height:13
		
		label lbl_long "" pos:[15,215] width:300 height:50
		
		spinner spn1 "" pos:[139,25] width:63 height:16
		spinner spn2 "" pos:[139,46] width:63 height:16
		spinner spn3 "" pos:[139,90] width:63 height:16
		spinner spn4 "" pos:[139,110] width:63 height:16
		spinner spn5 "" pos:[139,175] width:63 height:16
		spinner spn6 "" pos:[139,155] width:63 height:16
		spinner spn7 "" pos:[139,195] width:63 height:16
		spinner spn8 "" pos:[139,195] width:63 height:16
		spinner spn9 "" pos:[139,195] width:63 height:16
		spinner spn10 "" pos:[139,195] width:63 height:16
		spinner spn11 "" pos:[139,195] width:63 height:16
		spinner spn12 "" pos:[139,195] width:63 height:16
		spinner spn13 "" pos:[139,195] width:63 height:16
		spinner spn14 "" pos:[139,195] width:63 height:16
		spinner spn15 "" pos:[139,195] width:63 height:16
		spinner spn16 "" pos:[139,195] width:63 height:16
		spinner spn17 "" pos:[139,195] width:63 height:16
		spinner spn18 "" pos:[139,195] width:63 height:16
		spinner spn19 "" pos:[139,195] width:63 height:16
		spinner spn20 "" pos:[139,195] width:63 height:16
		spinner spn21 "" pos:[139,195] width:63 height:16
		spinner spn22 "" pos:[139,195] width:63 height:16
		spinner spn23 "" pos:[139,195] width:63 height:16
		spinner spn24 "" pos:[139,195] width:63 height:16
		spinner spn25 "" pos:[139,195] width:63 height:16
		spinner spn26 "" pos:[139,195] width:63 height:16
		spinner spn27 "" pos:[139,195] width:63 height:16
		spinner spn28 "" pos:[139,195] width:63 height:16
		spinner spn29 "" pos:[139,195] width:63 height:16
		spinner spn30 "" pos:[139,195] width:63 height:16
		spinner spn31 "" pos:[139,195] width:63 height:16
		spinner spn32 "" pos:[139,195] width:63 height:16
		--spinner spn33 "" pos:[139,195] width:63 height:16
		
		colorPicker cp1 "" pos:[1,1] width:63 height:16
		colorPicker cp2 "" pos:[1,1] width:63 height:16
		colorPicker cp3 "" pos:[1,1] width:63 height:16
		colorPicker cp4 "" pos:[1,1] width:63 height:16
		
		checkbox chk1 "" pos:[15,25] width:13 height:13
		
		--radiobuttons rdo1 labels:#("Use Vertex Color Channel","Set Color Value") pos:[1,1] width:100 columns:1
		--radiobuttons rdo2 labels:#("Use Vertex Alpha Channel","Set Alpha Value") pos:[1,1] width:100 columns:1
		
		dropdownList ddl1 "" pos:[14,24] width:148 height:21
		button btn1 "Reset to Defaults" pos:[36,246] width:137 height:21
		
		local CurrentType
		local CurrentSem
		local CurrentLOD
		local ControlElements = #(	grp1,grp2,grp3,grp4,grp5,grp6,grp7,\
									lbl1,lbl2,lbl3,lbl4,lbl5,lbl6,lbl7,lbl8,lbl9,lbl10,\
									lbl11,lbl12,lbl13,lbl14,lbl15,lbl16,lbl17,lbl18,lbl19,lbl20,\
									lbl21,lbl22,lbl23,lbl24,lbl25,lbl26,lbl27,lbl28,lbl29,lbl30,\
									lbl31,lbl32,lbl33,lbl_long,\
									spn1,spn2,spn3,spn4,spn5,spn6,spn7,spn8,spn9,spn10,\
									spn11,spn12,spn13,spn14,spn15,spn16,spn17,spn18,spn19,spn20,\
									spn21,spn22,spn23,spn24,spn25,spn26,spn27,spn28,spn29,spn30,\
									spn31,spn32,\
									cp1,cp2,cp3,cp4,\
									chk1,ddl1,btn1)
		
		fn HideElements Excluding = (
			--**********************************************************************
			--Hides Properties UI Controls
			--**********************************************************************
			for Element in ControlElements do if (finditem Excluding Element) == 0 then Element.pos = [500,0]
		)
		
		fn WriteParams = (
			--********************************************************************
			--called permanently ,each time one tampers with the UI to sync params
			--Storing the values of the UI controls that are currently displayed.
			--********************************************************************
			animate off ( ParamPropsInit = false )
			animate off case CurrentType of
			(
				1:(		--Black.mat
					eng_vWireframeColor = cp2.color
					eng_vWireframeColorChanged = true
				)
				2:(		--Canopy.mat
					mml_vFalloffParams1 = spn1.value
					mml_vFalloffParams2 = spn2.value
					mml_vFalloffParams3 = spn3.value
					mml_vFalloffParams4 = spn4.value
					mml_fOpacityMultiplier = spn5.value
					mml_fSpecularReflectionMult = spn6.value
					mml_fGoreThreshold = spn7.value
					mml_vTintColor = cp1.color
					eng_vWireframeColor = cp2.color
				)
				3:(		--Character.mat
					mml_vFalloffParams1 = spn1.value
					mml_vFalloffParams2 = spn2.value
					mml_vFalloffParams3 = spn3.value
					mml_vFalloffParams4 = spn4.value
					mml_fOpacityMultiplier = spn5.value
					mml_fSpecularReflectionMult = spn6.value
					mml_fGoreThreshold = spn7.value
					mml_vTintColor = cp1.color
					eng_vWireframeColor = cp2.color
				)
				4:(		--Cloth.mat
					mml_vFalloffParams1 = spn1.value
					mml_vFalloffParams2 = spn2.value
					mml_vFalloffParams3 = spn3.value
					mml_vFalloffParams4 = spn4.value
					mml_fOpacityMultiplier = spn5.value
					mml_fSpecularReflectionMult = spn6.value
					mml_fGoreThreshold = spn7.value
					mml_vTintColor = cp1.color
					eng_vWireframeColor = cp2.color
				)
				8:(		--Eye.mat
					mml_vEyeSpecularParameters_CSP = spn1.value
					mml_vEyeSpecularParameters_CSM = spn2.value
					mml_vEyeSpecularParameters_SSP = spn3.value
					mml_vEyeSpecularParameters_SSM = spn4.value
					mml_fOpacityMultiplier = spn5.value
					eng_vWireframeColor = cp2.color
				)
				9:(		--Face.mat
					mml_vFalloffParams1 = spn1.value
					mml_vFalloffParams2 = spn2.value
					mml_vFalloffParams3 = spn3.value
					mml_vFalloffParams4 = spn4.value
					mml_fOpacityMultiplier = spn5.value
					g_fSpecularMult = spn6.value
					g_fLipSpecularBoost = spn7.value
					g_fRimPower = spn8.value
					mml_fGoreThreshold = spn9.value
					mml_vTintColor = cp1.color
					eng_vWireframeColor = cp2.color
					g_vAmbientMult = cp3.color
				)
				11:(	--Hairalpha.mat
					mml_vHairParameters_PSM = spn1.value
					mml_vHairParameters_PSP = spn2.value
					mml_vHairParameters_SSP = spn3.value
					mml_vHairParameters_NTTF = spn4.value
					mml_fOpacityMultiplier = spn5.value
					mml_vTintColor = cp1.color
					eng_vWireframeColor = cp2.color
				)
				13:(	--Lava.mat
					mml_vUVScrollSpeed_1U = spn1.value
					mml_vUVScrollSpeed_1V = spn2.value
					mml_vUVScrollSpeed_2U = spn3.value
					mml_vUVScrollSpeed_2V = spn4.value
					mml_vMaskUVScrollSpeed_U = spn5.value
					mml_vMaskUVScrollSpeed_V = spn6.value
					mml_fLavaBrightness = spn7.value
					mml_fLavaContrast = spn8.value
					mml_vLavaTint = cp1.color
				)
				14:(	--Prop.mat
					mml_vFalloffParams1 = spn1.value
					mml_vFalloffParams2 = spn2.value
					mml_vFalloffParams3 = spn3.value
					mml_vFalloffParams4 = spn4.value
					mml_fOpacityMultiplier = spn5.value
					mml_fSpecularReflectionMult = spn6.value
					mml_fSpecularPower = spn7.value
					mml_vTintColor = cp1.color
					eng_vWireframeColor = cp2.color
				)
				16:(	--Static.mat
					mml_vFalloffParams1 = spn1.value
					mml_vFalloffParams2 = spn2.value
					mml_vFalloffParams3 = spn3.value
					mml_vFalloffParams4 = spn4.value
					mml_fOpacityMultiplier = spn5.value
					mml_fSpecularReflectionMult = spn6.value
					mml_fReliefAspect = spn7.value
					mml_vTintColor = cp1.color
					eng_vWireframeColor = cp2.color
					mml_iSoundMaterialType = ddl1.selection - 1
				)
				18:(	--Water.mat
					mml_fdistortion_fade_distance	= spn30.value
					mml_fdistortion_fade_multiplier	= spn31.value
					mml_fdistortion_invert			= (if chk1.state == true then 1.0 else 0.0)
					mml_fdistortion_magnitude		= spn32.value
		
					Water_Wave1_Height				= spn7.value
					Water_Wave2_Height				= spn11.value
					Water_Wave3_Height				= spn15.value
					Water_MinVertexColorAlpha		= spn1.value
					Water_SpecularFalloff			= spn3.value
					Water_SpecularMultiplier		= spn4.value
					Water_SunlightSpecularPower		= spn5.value
					Water_OpacityFalloff			= spn2.value
		
					Water_Flow1_Direction			= spn22.value
					Water_Flow1_Speed				= spn23.value
					Water_Flow1_TilingU				= spn24.value
					Water_Flow1_TilingV				= spn25.value
					Water_Flow2_Direction			= spn26.value
					Water_Flow2_Speed				= spn27.value
					Water_Flow2_TilingU				= spn28.value
					Water_Flow2_TilingV				= spn29.value
		
					Water_EdgeMask_Direction		= spn18.value
					Water_EdgeMask_Speed			= spn19.value
					Water_EdgeMask_TilingU			= spn20.value
					Water_EdgeMask_TilingV			= spn21.value
						
					Water_Wave1_Direction			= spn6.value
					Water_Wave1_Speed				= spn8.value
					Water_Wave1_Tiling				= spn9.value
					Water_Wave2_Direction			= spn10.value
					Water_Wave2_Speed				= spn12.value
					Water_Wave2_Tiling				= spn13.value
					Water_Wave3_Direction			= spn14.value
					Water_Wave3_Speed				= spn16.value
					Water_Wave3_Tiling				= spn17.value
					
					--Mat_UseColorChannel				= (if rdo1.state == 1 then true else false)
					--Mat_UseAlphaChannel				= (if rdo2.state == 1 then true else false)
					--Mat_VertexColor					= cp1.color
					--Mat_VertexAlpha					= spn33.value
				)
				19:(	--Weapon.mat
					mml_vFalloffParams1 = spn1.value
					mml_vFalloffParams2 = spn2.value
					mml_vFalloffParams3 = spn3.value
					mml_vFalloffParams4 = spn4.value
					mml_fOpacityMultiplier = spn5.value
					mml_fSpecularReflectionMult = spn6.value
					mml_fGoreThreshold = spn7.value
					mml_vTintColor = cp1.color
					eng_vWireframeColor = cp2.color
				)
				20:(	--Weapontrail.mat
					mml_fdistortion_fade_distance = spn1.value
					mml_fdistortion_fade_multiplier = spn2.value
					mml_fdistortion_invert = (if chk1.state == true then 1.0 else 0.0)
					mml_fdistortion_magnitude = spn4.value
				)
			)
		)
		
		fn UpdateDialog Reset:false = (
			--******************************************************************************************************
			--Major Properties Display function
			--Aligns Controls based on Material (Type and Semantic), Sets Control Values
			--
			--Each Material Type uses only a fraction of the available Controls based on the Material requirmeents.
			--******************************************************************************************************
			local DontHideThis = #()
			local RollUpState = DAOMatIni.RollUpStates[2]
			animate off case CurrentType of
			(
				1:(		--Black.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Black Material Properties"
					if CurrentSem == 1 then
					(
						MatPropertyRollout.height = 50
						grp3.pos = [4,4] ;grp3.caption = "None" ;grp3.height = 30
						join DontHideThis #(grp3)
					)
					if CurrentSem == 2 then
					(
						MatPropertyRollout.height = 85
						
						grp3.pos = [4,4] ;grp3.caption = "Various" ;grp3.height = 40
						lbl1.pos = [15,22] ;lbl1.caption = "Wireframe Color"
						cp2.pos = [149,22] ;cp2.caption = "" ;cp2.color = (color 0 255 0)
						btn1.pos = [36,56]
						join DontHideThis #(grp3,lbl1,cp2,btn1)
					)
					if Reset == true or ParamPropsInit == true then
					(	--DEFAULT
						eng_vWireframeColor = (color 0 255 0)
					)
					cp1.color = eng_vWireframeColor
				)
				2:(		--Canopy.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Canopy Material Properties"
					if CurrentSem == 1 or CurrentSem == 2 or CurrentSem == 3 then
					(
						grp1.pos = [4,4] ;grp1.caption = "Specular Falloff Parameters" ;grp1.height = 65
						lbl1.pos = [15,25] ;lbl1.caption = "Falloff Multiplier"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [0.0,2.0,1.0]
						lbl2.pos = [15,46] ;lbl2.caption = "Falloff Width"
						spn2.pos = [200,46] ;spn2.caption = "" ;spn2.range = [0.0,2.0,2.0]
						
						grp2.pos = [4,70] ;grp2.caption = "Tactical Cam Highlight Mask Parameters" ;grp2.height = 65
						lbl3.pos = [15,90] ;lbl3.caption = "Highlight Multiplier"
						spn3.pos = [200,90] ;spn3.caption = "" ;spn3.range = [0.0,20.0,0.0]
						lbl4.pos = [15,110] ;lbl4.caption = "Highlight Width"
						spn4.pos = [200,110] ;spn4.caption = "" ;spn4.range = [0.0,2.0,0.0]
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,grp2,lbl3,spn3,lbl4,spn4)
					)
					if CurrentSem == 1 or CurrentSem == 3 then
					(
						MatPropertyRollout.height = 275
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 102
						lbl5.pos = [15,155] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,155] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						lbl6.pos = [15,175] ;lbl6.caption = "Specular Multiplier"
						spn6.pos = [200,175] ;spn6.caption = "" ;spn6.range = [0.0,2.0,1.0]
						lbl7.pos = [15,195] ;lbl7.caption = "Gore Threshold"
						spn7.pos = [200,195] ;spn7.caption = "" ;spn7.range = [0.0,2.0,1.0]
						lbl8.pos = [15,215] ;lbl8.caption = "Tint Override Color"
						cp1.pos = [149,215] ;cp1.caption = "" ;cp1.color = (color 0 0 0)
						btn1.pos = [36,246]
						
						join DontHideThis #(grp3,lbl5,spn5,lbl6,spn6,lbl7,spn7,lbl8,cp1,btn1)
					)
					if CurrentSem == 2 then
					(
						MatPropertyRollout.height = 210
						
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 40
						lbl5.pos = [15,155] ;lbl5.caption = "Wireframe Color"
						cp2.pos = [149,155] ;cp2.caption = "" ;cp2.color = (color 127 51 51)
						btn1.pos = [36,186]
						
						join DontHideThis #(grp3,lbl5,cp2,btn1)
					)
					if (DAOMatIni.MatchID CurrentSem #(4,5,6)) then
					(
						MatPropertyRollout.height = 50
						grp3.pos = [4,4] ;grp3.caption = "None" ;grp3.height = 30
						join DontHideThis #(grp3)
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Canopy
						mml_vFalloffParams1 = 1.5
						mml_vFalloffParams2 = 1.5
						mml_vFalloffParams3 = 1.0
						mml_vFalloffParams4 = 0.00
						mml_fSpecularReflectionMult = 1.0
						mml_fOpacityMultiplier = 1.0
						mml_fGoreThreshold = 0.0
						mml_vTintColor = (color 0 0 0)
						eng_vWireframeColor = (color 255 255 255)
					)
					spn1.value = mml_vFalloffParams1
					spn2.value = mml_vFalloffParams2
					spn3.value = mml_vFalloffParams3
					spn4.value = mml_vFalloffParams4
					spn5.value = mml_fOpacityMultiplier
					spn6.value = mml_fSpecularReflectionMult
					spn7.value = mml_fGoreThreshold
					cp1.color = mml_vTintColor
					cp2.color = eng_vWireframeColor
				)
				3:(		--Character.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Character Material Properties"
					if (DAOMatIni.MatchID CurrentSem #(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29)) then
					(
						grp1.pos = [4,4] ;grp1.caption = "Specular Falloff Parameters" ;grp1.height = 65
						lbl1.pos = [15,25] ;lbl1.caption = "Falloff Multiplier"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [0.0,2.0,1.0]
						lbl2.pos = [15,46] ;lbl2.caption = "Falloff Width"
						spn2.pos = [200,46] ;spn2.caption = "" ;spn2.range = [0.0,2.0,2.0]
						
						grp2.pos = [4,70] ;grp2.caption = "Tactical Cam Highlight Mask Parameters" ;grp2.height = 65
						lbl3.pos = [15,90] ;lbl3.caption = "Highlight Multiplier"
						spn3.pos = [200,90] ;spn3.caption = "" ;spn3.range = [0.0,20.0,0.0]
						lbl4.pos = [15,110] ;lbl4.caption = "Highlight Width"
						spn4.pos = [200,110] ;spn4.caption = "" ;spn4.range = [0.0,2.0,0.0]
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,grp2,lbl3,spn3,lbl4,spn4)
					)
					if (DAOMatIni.MatchID CurrentSem #(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28)) then
					(
						--if (DAOMatIni.MatchID CurrentSem #(28)) then MatPropertyRollout.height = 275
						MatPropertyRollout.height = 275
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 102
						lbl5.pos = [15,155] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,155] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						lbl6.pos = [15,175] ;lbl6.caption = "Specular Multiplier"
						spn6.pos = [200,175] ;spn6.caption = "" ;spn6.range = [0.0,2.0,1.0]
						lbl7.pos = [15,195] ;lbl7.caption = "Gore Threshold"
						spn7.pos = [200,195] ;spn7.caption = "" ;spn7.range = [0.0,2.0,1.0]
						lbl8.pos = [15,215] ;lbl8.caption = "Tint Override Color"
						cp1.pos = [149,215] ;cp1.caption = "" ;cp1.color = (color 0 0 0)
						btn1.pos = [36,246]
						
						join DontHideThis #(grp3,lbl5,spn5,lbl6,spn6,lbl7,spn7,lbl8,cp1,btn1)
					)
					if (DAOMatIni.MatchID CurrentSem #(29)) then
					(
						MatPropertyRollout.height = 210
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 40
						lbl5.pos = [15,155] ;lbl5.caption = "Wireframe Color"
						cp2.pos = [149,155] ;cp2.caption = "" ;cp2.color = (color 127 51 51)
						btn1.pos = [36,186]
						join DontHideThis #(grp3,lbl5,cp2,btn1)
					)
					if (DAOMatIni.MatchID CurrentSem #(30,31,32,33,34)) then
					(
						MatPropertyRollout.height = 50
						grp3.pos = [4,4] ;grp3.caption = "None" ;grp3.height = 30
						join DontHideThis #(grp3)
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Character setting
						mml_vFalloffParams1 = 1.5
						mml_vFalloffParams2 = 1.5
						mml_vFalloffParams3 = 1.0
						mml_vFalloffParams4 = 0.00
						mml_fSpecularReflectionMult = 1.0
						mml_fOpacityMultiplier = 1.0
						mml_fGoreThreshold = 0.0
						mml_vTintColor = (color 0 0 0)
						eng_vWireframeColor = (color 255 255 255)
					)
					spn1.value = mml_vFalloffParams1
					spn2.value = mml_vFalloffParams2
					spn3.value = mml_vFalloffParams3
					spn4.value = mml_vFalloffParams4
					spn5.value = mml_fOpacityMultiplier
					spn6.value = mml_fSpecularReflectionMult
					spn7.value = mml_fGoreThreshold
					cp1.color = mml_vTintColor
					cp2.color = eng_vWireframeColor
				)
				4:(		--Cloth.mat
					if CurrentSem == 1 or CurrentSem == 2 then
					(
						MatPropertyRollout.height = 275
						
						grp1.pos = [4,4] ;grp1.caption = "Specular Falloff Parameters" ;grp1.height = 65
						lbl1.pos = [15,25] ;lbl1.caption = "Falloff Multiplier"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [0.0,2.0,1.0]
						lbl2.pos = [15,46] ;lbl2.caption = "Falloff Width"
						spn2.pos = [200,46] ;spn2.caption = "" ;spn2.range = [0.0,2.0,2.0]
						
						grp2.pos = [4,70] ;grp2.caption = "Tactical Cam Highlight Mask Parameters" ;grp2.height = 65
						lbl3.pos = [15,90] ;lbl3.caption = "Highlight Multiplier"
						spn3.pos = [200,90] ;spn3.caption = "" ;spn3.range = [0.0,20.0,0.0]
						lbl4.pos = [15,110] ;lbl4.caption = "Highlight Width"
						spn4.pos = [200,110] ;spn4.caption = "" ;spn4.range = [0.0,2.0,0.0]
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,grp2,lbl3,spn3,lbl4,spn4)
						
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 102
						lbl5.pos = [15,155] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,155] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						lbl6.pos = [15,175] ;lbl6.caption = "Specular Multiplier"
						spn6.pos = [200,175] ;spn6.caption = "" ;spn6.range = [0.0,2.0,1.0]
						lbl7.pos = [15,195] ;lbl7.caption = "Gore Threshold"
						spn7.pos = [200,195] ;spn7.caption = "" ;spn7.range = [0.0,2.0,1.0]
						lbl8.pos = [15,215] ;lbl8.caption = "Tint Override Color"
						cp1.pos = [149,215] ;cp1.caption = "" ;cp1.color = (color 0 0 0)
						btn1.pos = [36,246]
						
						join DontHideThis #(grp3,lbl5,spn5,lbl6,spn6,lbl7,spn7,lbl8,cp1,btn1)
					)
					if CurrentSem == 3 then
					(
						MatPropertyRollout.height = 85
						
						grp3.pos = [4,4] ;grp3.caption = "Various" ;grp3.height = 40
						lbl5.pos = [15,22] ;lbl5.caption = "Wireframe Color"
						cp2.pos = [149,22] ;cp2.caption = "" ;cp2.color = (color 0 255 0)
						btn1.pos = [36,56]
						
						join DontHideThis #(grp3,lbl5,cp2,btn1)
					)
					if (DAOMatIni.MatchID CurrentSem #(4,5)) then
					(
						MatPropertyRollout.height = 50
						grp3.pos = [4,4] ;grp3.caption = "None" ;grp3.height = 30
						join DontHideThis #(grp3)
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Cloth
						mml_vFalloffParams1 = 1.5
						mml_vFalloffParams2 = 1.5
						mml_vFalloffParams3 = 1.0
						mml_vFalloffParams4 = 0.00
						mml_fSpecularReflectionMult = 1.0
						mml_fOpacityMultiplier = 1.0
						mml_fGoreThreshold = 0.0
						mml_vTintColor = (color 0 0 0)
						eng_vWireframeColor = (color 0 0 255)
					)
					spn1.value = mml_vFalloffParams1
					spn2.value = mml_vFalloffParams2
					spn3.value = mml_vFalloffParams3
					spn4.value = mml_vFalloffParams4
					spn5.value = mml_fOpacityMultiplier
					spn6.value = mml_fSpecularReflectionMult
					spn7.value = mml_fGoreThreshold
					cp1.color = mml_vTintColor
					cp2.color = eng_vWireframeColor
				)
				8:(		--Eye.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Eye Material Properties"
					if CurrentSem == 1 or CurrentSem == 4 then	--default ,debug
					(
						MatPropertyRollout.height = 190
						grp1.pos = [4,4] ;grp1.caption = "Eye Parameters" ;grp1.height = 105
						lbl1.pos = [15,25] ;lbl1.caption = "Cornea Specular Power"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [1.0,500.0,1.0]
						lbl2.pos = [15,45] ;lbl2.caption = "Cornea Specular Mask"
						spn2.pos = [200,45] ;spn2.caption = "" ;spn2.range = [0.0,1.0,1.0]
						lbl3.pos = [15,65] ;lbl3.caption = "Sclera Specular Power"
						spn3.pos = [200,65] ;spn3.caption = "" ;spn3.range = [1.0,500.0,0.0]
						lbl4.pos = [15,85] ;lbl4.caption = "Sclera Specular Mask"
						spn4.pos = [200,85] ;spn4.caption = "" ;spn4.range = [0.0,1.0,1.0]
						
						grp3.pos = [4,110] ;grp3.caption = "Various" ;grp3.height = 42
						lbl5.pos = [15,128] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,128] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						btn1.pos = [36,160]
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,lbl3,spn3,lbl4,spn4,grp3,lbl5,spn5,btn1)
					)
					if CurrentSem == 2 then	--Eyelash
					(
						MatPropertyRollout.height = 80
						grp3.pos = [4,4] ;grp3.caption = "Various" ;grp3.height = 42
						lbl5.pos = [15,22] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,22] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						btn1.pos = [36,50]
						join DontHideThis #(grp3,lbl5,spn5,btn1)
					)
					if CurrentSem == 3 then	--Wireframe
					(
						MatPropertyRollout.height = 100
						
						grp3.pos = [4,4] ;grp3.caption = "Various" ;grp3.height = 60
						lbl5.pos = [15,22] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,22] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						lbl8.pos = [15,42] ;lbl8.caption = "Wireframe Color"
						cp2.pos = [149,42] ;cp2.caption = "" ;cp2.color = (color 127 51 51)
						btn1.pos = [36,70]
						
						join DontHideThis #(grp3,lbl5,spn5,lbl8,cp2,btn1)
					)
					if CurrentSem == 5 then	--depthwrite
					(
						MatPropertyRollout.height = 50
						grp3.pos = [4,4] ;grp3.caption = "None" ;grp3.height = 30
						
						join DontHideThis #(grp3)
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Eye
						mml_vEyeSpecularParameters_CSP = 30.0
						mml_vEyeSpecularParameters_CSM = 0.75
						mml_vEyeSpecularParameters_SSP = 75.0
						mml_vEyeSpecularParameters_SSM = 0.75
						mml_fOpacityMultiplier = 1.0
						eng_vWireframeColor = (color 0 0 255)
					)
					spn1.value = mml_vEyeSpecularParameters_CSP
					spn2.value = mml_vEyeSpecularParameters_CSM
					spn3.value = mml_vEyeSpecularParameters_SSP
					spn4.value = mml_vEyeSpecularParameters_SSM
					spn5.value = mml_fOpacityMultiplier
					cp2.color = eng_vWireframeColor
				)
				9:(		--Face.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Face Material Properties"
					if CurrentSem == 1 then	--default
					(
						MatPropertyRollout.height = 380
						grp1.pos = [4,4] ;grp1.caption = "Specular Falloff Parameters" ;grp1.height = 65
						lbl1.pos = [15,25] ;lbl1.caption = "Falloff Multiplier"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [0.0,2.0,1.0]
						lbl2.pos = [15,46] ;lbl2.caption = "Falloff Width"
						spn2.pos = [200,46] ;spn2.caption = "" ;spn2.range = [0.0,2.0,2.0]
						
						grp2.pos = [4,70] ;grp2.caption = "Tactical Cam Highlight Mask Parameters" ;grp2.height = 65
						lbl3.pos = [15,90] ;lbl3.caption = "Highlight Multiplier"
						spn3.pos = [200,90] ;spn3.caption = "" ;spn3.range = [0.0,20.0,0.0]
						lbl4.pos = [15,110] ;lbl4.caption = "Highlight Width"
						spn4.pos = [200,110] ;spn4.caption = "" ;spn4.range = [0.0,2.0,0.0]
						
						grp3.pos = [4,136] ;grp3.caption = "Ambient Multiplier Params" ;grp3.height = 42
						lbl10.pos = [15,153] ;lbl10.caption = "Ambient Multiplier"
						cp3.pos = [149,153] ;cp3.caption = "" ;cp3.color = (color 0 0 0)
						
						grp4.pos = [4,178] ;grp4.caption = "Light Parameters" ;grp4.height = 86
						lbl6.pos = [15,199] ;lbl6.caption = "Specular Multiplier"
						spn6.pos = [200,199] ;spn6.caption = "" ;spn6.range = [0.0,2.0,1.0]
						lbl7.pos = [15,220] ;lbl7.caption = "Lip Specular Boost"
						spn7.pos = [200,220] ;spn7.caption = "" ;spn7.range = [0.0,1.0,1.0]
						lbl8.pos = [15,241] ;lbl8.caption = "Rim Power"
						spn8.pos = [200,241] ;spn8.caption = "" ;spn8.range = [0.0,30.0,1.0]
						
						grp5.pos = [4,264] ;grp5.caption = "Various" ;grp5.height = 86
						lbl5.pos = [15,285] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,285] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						lbl9.pos = [15,306] ;lbl9.caption = "Gore Threshold"
						spn9.pos = [200,306] ;spn9.caption = "" ;spn9.range = [0.0,2.0,0.0]
						lbl11.pos = [15,327] ;lbl11.caption = "Tint Override Color"
						cp1.pos = [149,327] ;cp1.caption = "" ;cp1.color = (color 0 0 0)
						
						btn1.pos = [36,355]
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,grp2,lbl3,spn3,lbl4,spn4,grp3,lbl10,cp3,grp4,lbl6,spn6,lbl7,spn7,lbl8,spn8,grp5,lbl5,spn5,lbl9,spn9,lbl11,cp1,btn1)
					)
					if CurrentSem == 2 then	--Wireframe
					(
						MatPropertyRollout.height = 80
						
						grp3.pos = [4,4] ;grp3.caption = "Various" ;grp3.height = 40
						lbl8.pos = [15,22] ;lbl8.caption = "Wireframe Color"
						cp2.pos = [149,22] ;cp2.caption = "" ;cp2.color = (color 127 51 51)
						btn1.pos = [36,50]
						
						join DontHideThis #(grp3,lbl8,cp2,btn1)
					)
					if (DAOMatIni.MatchID CurrentSem #(3,4,5,6)) then	--rest
					(
						MatPropertyRollout.height = 50
						grp3.pos = [4,4] ;grp3.caption = "None" ;grp3.height = 30
						
						join DontHideThis #(grp3)
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Face
						mml_vFalloffParams1 = 1.0
						mml_vFalloffParams2 = 2.0
						mml_vFalloffParams3 = 0.0
						mml_vFalloffParams4 = 0.0
						mml_fOpacityMultiplier = 1.0
						g_fSpecularMult = 0.2
						g_fLipSpecularBoost = 0.0
						g_fRimPower = 0.0
						mml_fGoreThreshold = 0.0
						mml_vTintColor = (color 0 0 0)
						eng_vWireframeColor = (color 179 179 179)
						g_vAmbientMult = (color 0 0 0)
					)
					spn1.value = mml_vFalloffParams1
					spn2.value = mml_vFalloffParams2
					spn3.value = mml_vFalloffParams3
					spn4.value = mml_vFalloffParams4
					spn5.value = mml_fOpacityMultiplier
					spn6.value = g_fSpecularMult
					spn7.value = g_fLipSpecularBoost
					spn8.value = g_fRimPower
					spn9.value = mml_fGoreThreshold
					cp1.color = mml_vTintColor
					cp2.color = eng_vWireframeColor
					cp3.color = g_vAmbientMult
				)
				11:(	--Hairalpha.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Hair Material Properties"
					if CurrentSem == 1 or CurrentSem == 4 then
					(
						MatPropertyRollout.height = 205
						grp1.pos = [4,4] ;grp1.caption = "Hair Parameters" ;grp1.height = 105
						lbl1.pos = [15,25] ;lbl1.caption = "Primary specular mask"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [1.0,1.0,1.0]
						lbl2.pos = [15,45] ;lbl2.caption = "Primary specular power"
						spn2.pos = [200,45] ;spn2.caption = "" ;spn2.range = [1.0,500.0,1.0]
						lbl3.pos = [15,65] ;lbl3.caption = "Secondary specular power"
						spn3.pos = [200,65] ;spn3.caption = "" ;spn3.range = [1.0,255.0,1.0]
						lbl4.pos = [15,85] ;lbl4.caption = "Noise texture tile factor"
						spn4.pos = [200,85] ;spn4.caption = "" ;spn4.range = [1.0,25.0,1.0]
						
						grp3.pos = [4,110] ;grp3.caption = "Various" ;grp3.height = 62
						lbl5.pos = [15,128] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,128] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						lbl8.pos = [15,148] ;lbl8.caption = "Tint Override Color"
						cp1.pos = [149,148] ;cp1.caption = "" ;cp1.color = (color 0 0 0)
						btn1.pos = [36,180]
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,lbl3,spn3,lbl4,spn4,grp3,lbl5,spn5,lbl8,cp1,btn1)
					)
					if CurrentSem == 2 then	--Wireframe
					(
						MatPropertyRollout.height = 100
						
						grp3.pos = [4,4] ;grp3.caption = "Various" ;grp3.height = 60
						lbl5.pos = [15,22] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,22] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						lbl8.pos = [15,42] ;lbl8.caption = "Wireframe Color"
						cp2.pos = [149,42] ;cp2.caption = "" ;cp2.color = (color 127 51 51)
						btn1.pos = [36,70]
						
						join DontHideThis #(grp3,lbl5,spn5,lbl8,cp2,btn1)
					)
					if CurrentSem == 3 or CurrentSem == 6 then	--Overdraw ,Depthwrite
					(
						MatPropertyRollout.height = 80
						
						grp3.pos = [4,4] ;grp3.caption = "Various" ;grp3.height = 40
						lbl5.pos = [15,22] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,22] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						btn1.pos = [36,50]
						
						join DontHideThis #(grp3,lbl5,spn5,btn1)
					)
					if CurrentSem == 5 or CurrentSem == 7 then	--depthwritedebug ,pssm
					(
						MatPropertyRollout.height = 50
						grp3.pos = [4,4] ;grp3.caption = "None" ;grp3.height = 30
						
						join DontHideThis #(grp3)
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Hair
						mml_vHairParameters_PSM = 1.0
						mml_vHairParameters_PSP = 200.0
						mml_vHairParameters_SSP = 30.0
						mml_vHairParameters_NTTF = 10.0
						mml_fOpacityMultiplier = 1.0
						mml_vTintColor = (color 0 0 0)
						eng_vWireframeColor = (color 179 179 230)
					)
					spn1.value = mml_vHairParameters_PSM
					spn2.value = mml_vHairParameters_PSP
					spn3.value = mml_vHairParameters_SSP
					spn4.value = mml_vHairParameters_NTTF
					spn5.value = mml_fOpacityMultiplier
					cp1.color = mml_vTintColor
					cp2.color = eng_vWireframeColor
				)
				13:(	--Lava.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Lava Material Properties"
					if CurrentSem == 1 or CurrentSem == 2 then
					(
						MatPropertyRollout.height = 270
						grp1.pos = [4,4] ;grp1.caption = "Scroll Texture" ;grp1.height = 145
						lbl1.pos = [15,25] ;lbl1.caption = "Layer1 U scrollspeed m/s"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [-100000.0,100000.0,1.0]
						lbl2.pos = [15,45] ;lbl2.caption = "Layer1 V scrollspeed m/s"
						spn2.pos = [200,45] ;spn2.caption = "" ;spn2.range = [-100000.0,100000.0,2.0]
						lbl3.pos = [15,65] ;lbl3.caption = "Layer2 U scrollspeed m/s"
						spn3.pos = [200,65] ;spn3.caption = "" ;spn3.range = [-100000.0,100000.0,0.0]
						lbl4.pos = [15,85] ;lbl4.caption = "Layer2 V scrollspeed m/s"
						spn4.pos = [200,85] ;spn4.caption = "" ;spn4.range = [-100000.0,100000.0,0.0]
						lbl5.pos = [15,105] ;lbl5.caption = "Noise U scrollspeed m/s"
						spn5.pos = [200,105] ;spn5.caption = "" ;spn5.range = [-100000.0,100000.0,0.0]
						lbl6.pos = [15,125] ;lbl6.caption = "Noise V scrollspeed m/s"
						spn6.pos = [200,125] ;spn6.caption = "" ;spn6.range = [-100000.0,100000.0,0.0]
						
						grp2.pos = [4,150] ;grp2.caption = "Lava Parameters" ;grp2.height = 85
						lbl7.pos = [15,171] ;lbl7.caption = "Lava Brightness"
						spn7.pos = [200,171] ;spn7.caption = "" ;spn7.range = [0.0,20.0,1.0]
						lbl8.pos = [15,191] ;lbl8.caption = "Lava Contrast"
						spn8.pos = [200,191] ;spn8.caption = "" ;spn8.range = [0.0,10.0,1.0]
						lbl9.pos = [15,211] ;lbl9.caption = "Lava Tint Color"
						cp1.pos = [149,211] ;cp1.caption = "" ;cp1.color = (color 0 0 0)
						
						btn1.pos = [36,240]
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,lbl3,spn3,lbl4,spn4,lbl5,spn5,lbl6,spn6,grp2,lbl7,spn7,lbl8,spn8,lbl9,cp1,btn1)
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Lava
						mml_fLavaBrightness = 1
						mml_fLavaContrast = 1
						mml_vLavaTint = [255,102,0] as color
						mml_vUVScrollSpeed_1U = 0.05
						mml_vUVScrollSpeed_1V = 0.05
						mml_vUVScrollSpeed_2U = 0.05
						mml_vUVScrollSpeed_2V = 0.05
						mml_vMaskUVScrollSpeed_U = 0.05
						mml_vMaskUVScrollSpeed_V = 0.05
					)
					spn1.value = mml_vUVScrollSpeed_1U
					spn2.value = mml_vUVScrollSpeed_1V
					spn3.value = mml_vUVScrollSpeed_2U
					spn4.value = mml_vUVScrollSpeed_2V
					spn5.value = mml_vMaskUVScrollSpeed_U
					spn6.value = mml_vMaskUVScrollSpeed_V
					spn7.value = mml_fLavaBrightness
					spn8.value = mml_fLavaContrast
					cp1.color = mml_vLavaTint
				)
				14:(	--Prop.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Prop Material Properties"
					if CurrentSem == 1 or CurrentSem == 2 or CurrentSem == 3 or CurrentSem == 4 then
					(
						grp1.pos = [4,4] ;grp1.caption = "Specular Falloff Parameters" ;grp1.height = 65
						lbl1.pos = [15,25] ;lbl1.caption = "Falloff Multiplier"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [0.0,2.0,1.0]
						lbl2.pos = [15,46] ;lbl2.caption = "Falloff Width"
						spn2.pos = [200,46] ;spn2.caption = "" ;spn2.range = [0.0,2.0,2.0]
						
						grp2.pos = [4,70] ;grp2.caption = "Tactical Cam Highlight Mask Parameters" ;grp2.height = 65
						lbl3.pos = [15,90] ;lbl3.caption = "Highlight Multiplier"
						spn3.pos = [200,90] ;spn3.caption = "" ;spn3.range = [0.0,20.0,0.0]
						lbl4.pos = [15,110] ;lbl4.caption = "Highlight Width"
						spn4.pos = [200,110] ;spn4.caption = "" ;spn4.range = [0.0,2.0,0.0]
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,grp2,lbl3,spn3,lbl4,spn4)
					)
					if CurrentSem == 1 or CurrentSem == 2 or CurrentSem == 3 then
					(
						MatPropertyRollout.height = 255
						
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 82
						lbl5.pos = [15,155] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,155] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						lbl6.pos = [15,175] ;lbl6.caption = "Specular Multiplier"
						spn6.pos = [200,175] ;spn6.caption = "" ;spn6.range = [0.0,2.0,1.0]
						lbl8.pos = [15,195] ;lbl8.caption = "Tint Override Color"
						cp1.pos = [149,195] ;cp1.caption = "" ;cp1.color = (color 0 0 0)
						btn1.pos = [36,226]
						
						join DontHideThis #(grp3,lbl5,spn5,lbl6,spn6,lbl8,cp1,btn1)
					)
					if CurrentSem == 4 then
					(
						MatPropertyRollout.height = 230
						
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 60
						lbl7.pos = [15,155] ;lbl7.caption = "Specular Power"
						spn7.pos = [200,155] ;spn7.caption = "" ;spn7.range = [0.0,500.0,50.0]
						lbl8.pos = [15,175] ;lbl8.caption = "Wireframe Color"
						cp2.pos = [149,175] ;cp2.caption = "" ;cp2.color = (color 127 51 51)
						btn1.pos = [36,206]
						
						join DontHideThis #(grp3,lbl7,spn7,lbl8,cp2,btn1)
					)
					if CurrentSem == 5 or CurrentSem == 6 or CurrentSem == 7 then
					(
						MatPropertyRollout.height = 50
						grp3.pos = [4,4] ;grp3.caption = "None" ;grp3.height = 30
						
						join DontHideThis #(grp3)
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Prop
						mml_vFalloffParams1 = 1.0
						mml_vFalloffParams2 = 2.0
						mml_vFalloffParams3 = 0.0
						mml_vFalloffParams4 = 0.0
						mml_fSpecularReflectionMult = 1.0
						mml_fOpacityMultiplier = 1.0
						mml_vTintColor = (color 0 0 0)
						eng_vWireframeColor = (color 127 51 51)
					)
					spn1.value = mml_vFalloffParams1
					spn2.value = mml_vFalloffParams2
					spn3.value = mml_vFalloffParams3
					spn4.value = mml_vFalloffParams4
					spn5.value = mml_fOpacityMultiplier
					spn6.value = mml_fSpecularReflectionMult
					cp1.color = mml_vTintColor
					cp2.color = eng_vWireframeColor
				)
				16:(	--Static.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Static Material Properties"
					if CurrentSem >= 1 and CurrentSem <= 8 then
					(
						grp1.pos = [4,4] ;grp1.caption = "Specular Falloff Parameters" ;grp1.height = 65
						lbl1.pos = [15,25] ;lbl1.caption = "Falloff Multiplier"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [0.0,2.0,1.5]
						lbl2.pos = [15,46] ;lbl2.caption = "Falloff Width"
						spn2.pos = [200,46] ;spn2.caption = "" ;spn2.range = [0.0,2.0,1.5]
						
						grp2.pos = [4,70] ;grp2.caption = "Tactical Cam Highlight Mask Parameters" ;grp2.height = 65
						lbl3.pos = [15,90] ;lbl3.caption = "Highlight Multiplier"
						spn3.pos = [200,90] ;spn3.caption = "" ;spn3.range = [0.0,20.0,0.0]
						lbl4.pos = [15,110] ;lbl4.caption = "Highlight Width"
						spn4.pos = [200,110] ;spn4.caption = "" ;spn4.range = [0.0,2.0,0.0]
						
						lbl5.pos = [15,155] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,155] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						lbl6.pos = [15,175] ;lbl6.caption = "Specular Multiplier"
						spn6.pos = [200,175] ;spn6.caption = "" ;spn6.range = [0.0,2.0,1.0]
						lbl8.pos = [15,195] ;lbl8.caption = "Tint Override Color"
						cp1.pos = [149,195] ;cp1.caption = "" ;cp1.color = (color 0 0 0)
						ddl1.items = #("Unknown","Wood","Sand","Leaves","Grass","Stone","Dirt","Puddle","Water","Mud","Ice","Snow","Hollow Wood","Metal","Gravel","Carpet")
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,grp2,lbl3,spn3,lbl4,spn4,lbl5,spn5,lbl6,spn6,lbl8,cp1)
					)
					if CurrentSem == 1 or CurrentSem == 3 or CurrentSem == 4 or CurrentSem == 5 or CurrentSem == 6 or CurrentSem == 7 then
					(
						MatPropertyRollout.height = 300
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 82
						grp4.pos = [4,220] ;grp4.caption = "Sound Material Type" ;grp4.height = 47
						ddl1.pos = [15,240]
						btn1.pos = [36,270]
						join DontHideThis #(grp3,grp4,ddl1,btn1)
					)
					if CurrentSem == 2 then
					(
						MatPropertyRollout.height = 320
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 102
						lbl7.pos = [15,215] ;lbl7.caption = "Relief Aspect Ratio"
						spn7.pos = [200,215] ;spn7.caption = "" ;spn7.range = [0.0,2.0,1.0]
						grp4.pos = [4,240] ;grp4.caption = "Sound Material Type" ;grp4.height = 47
						ddl1.pos = [10,260]
						btn1.pos = [36,290]
						join DontHideThis #(grp3,lbl7,spn7,ddl1,btn1,grp4)
					)
					if CurrentSem == 8 then
					(
						MatPropertyRollout.height = 255
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 82
						btn1.pos = [36,226]
						join DontHideThis #(btn1,grp3)
					)
					if CurrentSem == 9 then
					(
						MatPropertyRollout.height = 85
						
						grp3.pos = [4,4] ;grp3.caption = "Various" ;grp3.height = 40
						lbl8.pos = [15,22] ;lbl8.caption = "Wireframe Color"
						cp2.pos = [149,22] ;cp2.caption = "" ;cp2.color = (color 0 255 0)
						btn1.pos = [36,56]
						join DontHideThis #(grp3,lbl8,cp2,btn1)
					)
					if CurrentSem == 10 or CurrentSem == 11 or CurrentSem == 12 then
					(
						MatPropertyRollout.height = 50
						grp3.pos = [4,4] ;grp3.caption = "None" ;grp3.height = 30
						join DontHideThis #(grp3)
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Static
						mml_vFalloffParams1 = 1.5
						mml_vFalloffParams2 = 1.5
						mml_vFalloffParams3 = 0.0
						mml_vFalloffParams4 = 0.0
						mml_fSpecularReflectionMult = 1.0
						mml_fOpacityMultiplier = 1.0
						mml_fReliefAspect = 1.0
						mml_vTintColor = (color 0 0 0)
						eng_vWireframeColor = (color 0 255 0)
						mml_iSoundMaterialType = 0
					)
					spn1.value = mml_vFalloffParams1
					spn2.value = mml_vFalloffParams2
					spn3.value = mml_vFalloffParams3
					spn4.value = mml_vFalloffParams4
					spn5.value = mml_fOpacityMultiplier
					spn6.value = mml_fSpecularReflectionMult
					spn7.value = mml_fReliefAspect
					cp1.color = mml_vTintColor
					cp2.color = eng_vWireframeColor
					ddl1.selection = mml_iSoundMaterialType + 1
				)
				18:(	--Water.mat
					if CurrentSem == 1 or CurrentSem == 2 or CurrentSem == 3 or CurrentSem == 4 or CurrentSem == 5 then
					(
						--grp8.pos = [4,4] ;grp8.caption = "Water Color and Transparency" ;grp8.height = 120
						--rdo1.pos = [15,20]
						--cp1.pos = [149,36]
						--rdo2.pos = [15,58]
						--spn33.pos = [200,74] ;spn33.caption = "" ;spn33.range = [0.0,1.0,0.5]
						--lbl1.pos = [15,100] ;lbl1.caption = "Minimum Alpha Value"
						--spn1.pos = [200,100] ;spn1.caption = "" ;spn1.range = [0.0,1.0,0.1]
						--join DontHideThis #(grp8,rdo1,rdo2,cp1,spn33,lbl1,spn1)
						
						grp1.pos = [4,4] ;grp1.caption = "Water Parameters" ;grp1.height = 127
						lbl1.pos = [15,25] ;lbl1.caption = "Min Vertex Color Alpha"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [0.0,1.0,0.1]
						lbl2.pos = [15,46] ;lbl2.caption = "Opacity Falloff"
						spn2.pos = [200,46] ;spn2.caption = "" ;spn2.range = [0.01,2.0,1.0]
						lbl3.pos = [15,67] ;lbl3.caption = "Specular Falloff"
						spn3.pos = [200,67] ;spn3.caption = "" ;spn3.range = [1.0,5.0,1.0]
						lbl4.pos = [15,88] ;lbl4.caption = "Specular Multiplier"
						spn4.pos = [200,88] ;spn4.caption = "" ;spn4.range = [0.0,2.0,1.0]
						lbl5.pos = [15,109] ;lbl5.caption = "Sunlight Specular Power"
						spn5.pos = [200,109] ;spn5.caption = "" ;spn5.range = [10.0,500.0,50.0]
						join DontHideThis #(grp1,lbl1,lbl2,lbl3,lbl4,lbl5,spn1,spn2,spn3,spn4,spn5)
						
						grp2.pos = [4,132] ;grp2.caption = "Wave1" ;grp2.height = 102
						lbl6.pos = [15,150] ;lbl6.caption = "Wave1 Direction"
						spn6.pos = [200,150] ;spn6.caption = "" ;spn6.range = [0.0,359.0,270.0]
						lbl7.pos = [15,171] ;lbl7.caption = "Wave1 Height"
						spn7.pos = [200,171] ;spn7.caption = "" ;spn7.range = [0.0,1.0,0.2]
						lbl8.pos = [15,192] ;lbl8.caption = "Wave1 Speed (m/s)"
						spn8.pos = [200,192] ;spn8.caption = "" ;spn8.range = [0.0,0.5,0.023]
						lbl9.pos = [15,213] ;lbl9.caption = "Wave1 Tiling"
						spn9.pos = [200,213] ;spn9.caption = "" ;spn9.range = [0.0,2.0,0.7]
						join DontHideThis #(grp2,lbl6,spn6,lbl7,spn7,lbl8,spn8,lbl9,spn9)
						
						grp3.pos = [4,235] ;grp3.caption = "Wave2" ;grp3.height = 102
						lbl10.pos = [15,253] ;lbl10.caption = "Wave2 Direction"
						spn10.pos = [200,253] ;spn10.caption = "" ;spn10.range = [0.0,359.0,315.0]
						lbl11.pos = [15,274] ;lbl11.caption = "Wave2 Height"
						spn11.pos = [200,274] ;spn11.caption = "" ;spn11.range = [0.0,1.0,0.2]
						lbl12.pos = [15,295] ;lbl12.caption = "Wave2 Speed (m/s)"
						spn12.pos = [200,295] ;spn12.caption = "" ;spn12.range = [0.0,0.5,0.047]
						lbl13.pos = [15,316] ;lbl13.caption = "Wave2 Tiling"
						spn13.pos = [200,316] ;spn13.caption = "" ;spn13.range = [0.0,10000.0,0.7]
						join DontHideThis #(grp3,lbl10,spn10,lbl11,spn11,lbl12,spn12,lbl13,spn13)
						
						grp4.pos = [4,338] ;grp4.caption = "Wave3" ;grp4.height = 102
						lbl14.pos = [15,356] ;lbl14.caption = "Wave3 Direction"
						spn14.pos = [200,356] ;spn14.caption = "" ;spn14.range = [0.0,359.0,180.0]
						lbl15.pos = [15,377] ;lbl15.caption = "Wave3 Height"
						spn15.pos = [200,377] ;spn15.caption = "" ;spn15.range = [0.0,1.0,0.3]
						lbl16.pos = [15,398] ;lbl16.caption = "Wave3 Speed (m/s)"
						spn16.pos = [200,398] ;spn16.caption = "" ;spn16.range = [0.0,0.5,0.014]
						lbl17.pos = [15,419] ;lbl17.caption = "Wave3 Tiling"
						spn17.pos = [200,419] ;spn17.caption = "" ;spn17.range = [0.0,10000.0,0.7]
						join DontHideThis #(grp4,lbl14,spn14,lbl15,spn15,lbl16,spn16,lbl17,spn17)
					)
					if CurrentSem == 1 or CurrentSem == 3 or CurrentSem == 5 then
					(
						btn1.pos = [36,445]
						join DontHideThis #(btn1)
						MatPropertyRollout.height = 470	
					)
					if CurrentSem == 2 or CurrentSem == 4 then
					(
						grp5.pos = [4,441] ;grp5.caption = "Water Edge" ;grp5.height = 102
						lbl18.pos = [15,459] ;lbl18.caption = "Edge Mask Direction"
						spn18.pos = [200,459] ;spn18.caption = "" ;spn18.range = [0.0,359.0,270.0]
						lbl19.pos = [15,480] ;lbl19.caption = "Edge Mask Speed (m/s)"
						spn19.pos = [200,480] ;spn19.caption = "" ;spn19.range = [0.0,0.5,0.023]
						lbl20.pos = [15,501] ;lbl20.caption = "Edge Mask Tiling U"
						spn20.pos = [200,501] ;spn20.caption = "" ;spn20.range = [0.0,10000.0,1.0]
						lbl21.pos = [15,522] ;lbl21.caption = "Edge Mask Tiling V"
						spn21.pos = [200,522] ;spn21.caption = "" ;spn21.range = [0.0,10000.0,1.0]
						join DontHideThis #(grp5,lbl18,spn18,lbl19,spn19,lbl20,spn20,lbl21,spn21)
						
						grp6.pos = [4,544] ;grp6.caption = "Water Flow" ;grp6.height = 186
						lbl22.pos = [15,562] ;lbl22.caption = "Flow Layer1 Direction"
						spn22.pos = [200,562] ;spn22.caption = "" ;spn22.range = [0.0,359.0,295.0]
						lbl23.pos = [15,583] ;lbl23.caption = "Flow Layer1 Speed (m/s)"
						spn23.pos = [200,583] ;spn23.caption = "" ;spn23.range = [0.0,0.5,0.072]
						lbl24.pos = [15,604] ;lbl24.caption = "Flow Layer1 Tiling U"
						spn24.pos = [200,604] ;spn24.caption = "" ;spn24.range = [0.0,10000.0,1.0]
						lbl25.pos = [15,625] ;lbl25.caption = "Flow Layer1 Tiling V"
						spn25.pos = [200,625] ;spn25.caption = "" ;spn25.range = [0.0,10000.0,1.0]
						lbl26.pos = [15,646] ;lbl26.caption = "Flow Layer2 Direction"
						spn26.pos = [200,646] ;spn26.caption = "" ;spn26.range = [0.0,359.0,235.0]
						lbl27.pos = [15,667] ;lbl27.caption = "Flow Layer2 Speed (m/s)"
						spn27.pos = [200,667] ;spn27.caption = "" ;spn27.range = [0.0,0.5,0.045]
						lbl28.pos = [15,688] ;lbl28.caption = "Flow Layer2 Tiling U"
						spn28.pos = [200,688] ;spn28.caption = "" ;spn20.range = [0.0,10000.0,1.0]
						lbl29.pos = [15,709] ;lbl29.caption = "Flow Layer2 Tiling V"
						spn29.pos = [200,709] ;spn29.caption = "" ;spn29.range = [0.0,10000.0,1.0]
						join DontHideThis #(grp6,lbl22,spn22,lbl23,spn23,lbl24,spn24,lbl25,spn25,lbl26,spn26,lbl27,spn27,lbl28,spn28,lbl29,spn29)
						
						grp7.pos = [4,731] ;grp7.caption = "Distortion Parameters" ;grp7.height = 102
						lbl30.pos = [15,749] ;lbl30.caption = "Distortion Fade Distance"
						spn30.pos = [200,749] ;spn30.caption = "" ;spn30.range = [0.01,1000.0,50.0]
						lbl31.pos = [15,770] ;lbl31.caption = "Distortion Fade Multiplier"
						spn31.pos = [200,770] ;spn31.caption = "" ;spn31.range = [0.00,1.00,0.0]
						lbl32.pos = [15,791] ;lbl32.caption = "Distortion Invert"
						chk1.pos = [150,791] ;chk1.caption = ""
						lbl33.pos = [15,812] ;lbl33.caption = "Distortion Magnitude"
						spn32.pos = [200,812] ;spn32.caption = "" ;spn32.range = [0.0,10.0,5.0]
						join DontHideThis #(grp7,lbl30,spn30,lbl31,spn31,lbl32,chk1,lbl33,spn32)
						
						btn1.pos = [36,840]
						join DontHideThis #(btn1)
						MatPropertyRollout.height = 870	
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Water	(taken from Toolset initial values)
						mml_fdistortion_fade_distance	= 50.0
						mml_fdistortion_fade_multiplier	= 0.0
						mml_fdistortion_invert			= 1.0	--true
						mml_fdistortion_magnitude		= 5.0
		
						Water_Wave1_Height				= 0.2
						Water_Wave2_Height				= 0.2
						Water_Wave3_Height				= 0.3
						Water_MinVertexColorAlpha		= 0.1
						Water_SpecularFalloff			= 1.0
						Water_SpecularMultiplier		= 1.0
						Water_SunlightSpecularPower		= 50.0
						Water_OpacityFalloff			= 1.0
		
						Water_Flow1_Direction			= 295.0
						Water_Flow1_Speed				= 0.072
						Water_Flow1_TilingU				= 1.0
						Water_Flow1_TilingV				= 1.0
						Water_Flow2_Direction			= 235.0
						Water_Flow2_Speed				= 0.045
						Water_Flow2_TilingU				= 1.0
						Water_Flow2_TilingV				= 1.0
		
						Water_EdgeMask_Direction		= 270.0
						Water_EdgeMask_Speed			= 0.023
						Water_EdgeMask_TilingU			= 1.0
						Water_EdgeMask_TilingV			= 1.0
						
						Water_Wave1_Direction			= 270.0
						Water_Wave1_Speed				= 0.023
						Water_Wave1_Tiling				= 0.7
						Water_Wave2_Direction			= 315.0
						Water_Wave2_Speed				= 0.047
						Water_Wave2_Tiling				= 0.7
						Water_Wave3_Direction			= 180.0
						Water_Wave3_Speed				= 0.014
						Water_Wave3_Tiling				= 0.7
						
						--Mat_UseColorChannel				= false
						--Mat_UseAlphaChannel				= false
						--Mat_VertexColor					= (color 127 177 127)
						--Mat_VertexAlpha					= 0.5
					)
					spn30.value	= mml_fdistortion_fade_distance
					spn31.value	= mml_fdistortion_fade_multiplier
					chk1.checked	= (if mml_fdistortion_invert == 1.0 then true else false)
					spn32.value	= mml_fdistortion_magnitude
		
					spn7.value	= Water_Wave1_Height
					spn11.value	= Water_Wave2_Height
					spn15.value	= Water_Wave3_Height
					spn1.value	= Water_MinVertexColorAlpha
					spn3.value	= Water_SpecularFalloff
					spn4.value	= Water_SpecularMultiplier
					spn5.value	= Water_SunlightSpecularPower
					spn2.value	= Water_OpacityFalloff
		
					spn22.value	= Water_Flow1_Direction
					spn23.value	= Water_Flow1_Speed
					spn24.value	= Water_Flow1_TilingU
					spn25.value	= Water_Flow1_TilingV
					spn26.value	= Water_Flow2_Direction
					spn27.value	= Water_Flow2_Speed
					spn28.value	= Water_Flow2_TilingU
					spn29.value	= Water_Flow2_TilingV
		
					spn18.value	= Water_EdgeMask_Direction
					spn19.value	= Water_EdgeMask_Speed
					spn20.value	= Water_EdgeMask_TilingU
					spn21.value	= Water_EdgeMask_TilingV
						
					spn6.value	= Water_Wave1_Direction
					spn8.value	= Water_Wave1_Speed
					spn9.value	= Water_Wave1_Tiling
					spn10.value	= Water_Wave2_Direction
					spn12.value	= Water_Wave2_Speed
					spn13.value	= Water_Wave2_Tiling
					spn14.value	= Water_Wave3_Direction
					spn16.value	= Water_Wave3_Speed
					spn17.value	= Water_Wave3_Tiling
					
					--rdo1.state	= (if Mat_UseColorChannel == true then 1 else 2)
					--rdo2.state	= (if Mat_UseAlphaChannel == true then 1 else 2)
					--cp1.color	= Mat_VertexColor
					--spn33.value	= Mat_VertexAlpha
					
					--cp1.enabled		= (if Mat_UseColorChannel == true then false else true)
					--spn33.enabled	= (if Mat_UseAlphaChannel == true then false else true)
				)
				19:(	--Weapon.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Weapon Material Properties"
					if CurrentSem == 1 or CurrentSem == 2 then
					(
						grp1.pos = [4,4] ;grp1.caption = "Specular Falloff Parameters" ;grp1.height = 65
						lbl1.pos = [15,25] ;lbl1.caption = "Falloff Multiplier"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [0.0,2.0,1.0]
						lbl2.pos = [15,46] ;lbl2.caption = "Falloff Width"
						spn2.pos = [200,46] ;spn2.caption = "" ;spn2.range = [0.0,2.0,2.0]
						
						grp2.pos = [4,70] ;grp2.caption = "Tactical Cam Highlight Mask Parameters" ;grp2.height = 65
						lbl3.pos = [15,90] ;lbl3.caption = "Highlight Multiplier"
						spn3.pos = [200,90] ;spn3.caption = "" ;spn3.range = [0.0,20.0,0.0]
						lbl4.pos = [15,110] ;lbl4.caption = "Highlight Width"
						spn4.pos = [200,110] ;spn4.caption = "" ;spn4.range = [0.0,2.0,0.0]
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,grp2,lbl3,spn3,lbl4,spn4)
					)
					if CurrentSem == 1 then
					(
						MatPropertyRollout.height = 275
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 102
						lbl5.pos = [15,155] ;lbl5.caption = "Opacity Multiplier"
						spn5.pos = [200,155] ;spn5.caption = "" ;spn5.range = [0.0,2.0,1.0]
						lbl6.pos = [15,175] ;lbl6.caption = "Specular Multiplier"
						spn6.pos = [200,175] ;spn6.caption = "" ;spn6.range = [0.0,2.0,1.0]
						lbl7.pos = [15,195] ;lbl7.caption = "Gore Threshold"
						spn7.pos = [200,195] ;spn7.caption = "" ;spn7.range = [0.0,2.0,1.0]
						lbl8.pos = [15,215] ;lbl8.caption = "Tint Override Color"
						cp1.pos = [149,215] ;cp1.caption = "" ;cp1.color = (color 0 0 0)
						btn1.pos = [36,246]
						
						join DontHideThis #(grp3,lbl5,spn5,lbl6,spn6,lbl7,spn7,lbl8,cp1,btn1)
					)
					if CurrentSem == 2 then
					(
						MatPropertyRollout.height = 210
						
						grp3.pos = [4,136] ;grp3.caption = "Various" ;grp3.height = 40
						lbl8.pos = [15,155] ;lbl8.caption = "Wireframe Color"
						cp2.pos = [149,155] ;cp2.caption = "" ;cp2.color = (color 127 51 51)
						btn1.pos = [36,186]
						
						join DontHideThis #(grp3,lbl8,cp2,btn1)
					)
					if CurrentSem == 3 or CurrentSem == 4 or CurrentSem == 5 then
					(
						MatPropertyRollout.height = 50
						grp3.pos = [4,4] ;grp3.caption = "None" ;grp3.height = 30
						join DontHideThis #(grp3)
					)
					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Weapon
						mml_vFalloffParams1 = 1.5
						mml_vFalloffParams2 = 1.5
						mml_vFalloffParams3 = 10.0
						mml_vFalloffParams4 = 0.65
						mml_fOpacityMultiplier = 1.0
						mml_fSpecularReflectionMult = 1.0
						mml_fGoreThreshold = 0.0
						mml_vTintColor = (color 0 0 0)
						eng_vWireframeColor = (color 51 130 51 0)
					)
					spn1.value = mml_vFalloffParams1
					spn2.value = mml_vFalloffParams2
					spn3.value = mml_vFalloffParams3
					spn4.value = mml_vFalloffParams4
					spn5.value = mml_fOpacityMultiplier
					spn6.value = mml_fSpecularReflectionMult
					spn7.value = mml_fGoreThreshold
					cp1.color = mml_vTintColor
					cp2.color = eng_vWireframeColor
				)
				20:(	--Weapontrail.mat
					--if (maxversion())[1] >= 5100 then MatPropertyRollout.title = "Weapontrail Material Properties"
					if CurrentSem == 1 then
					(
						MatPropertyRollout.height = 170
						grp1.pos = [4,4] ;grp1.caption = "Distortion Parameters" ;grp1.height = 105
						lbl1.pos = [15,25] ;lbl1.caption = "Distortion Fade Distance"
						spn1.pos = [200,25] ;spn1.caption = "" ;spn1.range = [0.01,1000.0,1.0]
						lbl2.pos = [15,46] ;lbl2.caption = "Distortion Fade Multiplier"
						spn2.pos = [200,46] ;spn2.caption = "" ;spn2.range = [0.00,1.00,1.0]
						lbl3.pos = [15,67] ;lbl3.caption = "Distortion Invert"
						chk1.pos = [150,67] ;chk1.caption = ""
						--spn3.pos = [200,67] ;spn3.caption = "" ;spn3.range = [0.0,1.0,1.0]
						lbl4.pos = [15,88] ;lbl4.caption = "Distortion Magnitude"
						spn4.pos = [200,88] ;spn4.caption = "" ;spn4.range = [0.0,10.0,0.0]
						
						btn1.pos = [36,146]
						
						lbl_long.pos = [5,116]
						lbl_long.caption = "Weapontrails Material cant be assigned, and must be picked from Weapontrails Properties instead."
						
						join DontHideThis #(grp1,lbl1,spn1,lbl2,spn2,lbl3,chk1,lbl4,spn4,btn1,lbl_long)
					)

					if Reset == true or ParamPropsInit == true then animate off
					(	--DEFAULT Weapontrail (taken from weapontrail_streaks.mao)
						mml_fdistortion_fade_distance = 25.0
						mml_fdistortion_fade_multiplier = 0.0
						mml_fdistortion_invert = 1.0
						mml_fdistortion_magnitude = 3.0
					)
					spn1.value = mml_fdistortion_fade_distance
					spn2.value = mml_fdistortion_fade_multiplier
					chk1.checked = (if mml_fdistortion_invert == 1.0 then true else false)
					spn4.value = mml_fdistortion_magnitude
				)
			)
			HideElements DontHideThis	--hiding all elements (except..)
			WriteParams()	--ensures syncing , should be thou
			DAOMatIni.RollUpStates[2] = RollUpState
		)

		on spn1 changed value do WriteParams()
		on spn2 changed value do WriteParams()
		on spn3 changed value do WriteParams()
		on spn4 changed value do WriteParams()
		on spn5 changed value do WriteParams()
		on spn6 changed value do WriteParams()
		on spn7 changed value do WriteParams()
		on spn8 changed value do WriteParams()
		on spn9 changed value do WriteParams()
		on spn10 changed value do WriteParams()
		on spn11 changed value do WriteParams()
		on spn12 changed value do WriteParams()
		on spn13 changed value do WriteParams()
		on spn14 changed value do WriteParams()
		on spn15 changed value do WriteParams()
		on spn16 changed value do WriteParams()
		on spn17 changed value do WriteParams()
		on spn18 changed value do WriteParams()
		on spn19 changed value do WriteParams()
		on spn20 changed value do WriteParams()
		on spn21 changed value do WriteParams()
		on spn22 changed value do WriteParams()
		on spn23 changed value do WriteParams()
		on spn24 changed value do WriteParams()
		on spn25 changed value do WriteParams()
		on spn26 changed value do WriteParams()
		on spn27 changed value do WriteParams()
		on spn28 changed value do WriteParams()
		on spn29 changed value do WriteParams()
		on spn30 changed value do WriteParams()
		on spn31 changed value do WriteParams()
		on spn32 changed value do WriteParams()
		--on spn33 changed value do WriteParams()
		on cp1 changed value do WriteParams()
		on cp2 changed value do WriteParams()
		on cp3 changed value do WriteParams()
		on cp4 changed value do WriteParams()
		on ddl1 selected sel do WriteParams()
		on chk1 changed state do WriteParams()
		--on rdo1 changed state do
		--(
		--	if CurrentType == 18 then cp1.enabled = (if state == 1 then false else true)
		--	WriteParams()
		--)
		--on rdo2 changed state do
		--(
		--	if CurrentType == 18 then spn33.enabled = (if state == 1 then false else true)
		--	WriteParams()
		--)
		
		on btn1 pressed do UpdateDialog Reset:true
		
		on MatPropertyRollout rolledup state do DAOMatIni.RollUpStates[2] = state
		
		on MatPropertyRollout open do () --UpdateDialog()
	)
	
	rollout MatTypeRollout "Basic Dragon Age Material Type" width:386 height:162 --rolledup:(not DAOMatIni.RollUpStates[1])
	(
		dropdownList ddl_types "" pos:[14,24] width:148 height:21
		GroupBox grp1 "Material Type" pos:[4,4] width:318 height:49
		
		GroupBox grp_semantics "Render Semantics" pos:[4,54] width:318 height:65
		dropdownList ddl_semantics "" pos:[14,73] width:251 height:21
		checkbox chk_debug "enable Debug Semantics" pos:[14,100] width:150 height:15
		
		radioButtons rdo_lod "" pos:[269,66] width:51 height:48 columns:1 labels:#("LOD1", "LOD2", "LOD3")
		
		local TypeHasChanged = false	--when type is switched, make a value reset on properties rollout
		local CurrTypeSelection
		
		fn FillSemanticList Ext:false KeepSelect:false Select:0 = (
			local SetSelection
			local ItemCollection = for item in DAOMatIni.MatTypes[ddl_types.selection].Semantics collect item.SemanticDisplay
			if Ext == true then for item in DAOMatIni.MatTypes[ddl_types.selection].SemanticsExt do append ItemCollection item.SemanticDisplay
			
			animate off
			(
				if KeepSelect == false then
				(
					if Select == 0 then SetSelection = DAOMatIni.MatTypes[ddl_types.selection].DefaultSem else SetSelection = Select
				)	
				else SetSelection = ddl_semantics.selection
				
				if SetSelection > ItemCollection.count then SetSelection = DAOMatIni.MatTypes[ddl_types.selection].DefaultSem
				if SetSelection > ItemCollection.count then SetSelection = 1
				
				ddl_semantics.items = ItemCollection
				ddl_semantics.selection = SetSelection
			)
		)
		
		fn UpdateDebugBox = (
			animate off
			(
				if DAOMatIni.MatTypes[ddl_types.selection].SemanticsExt.count == 0 then
				(
					chk_debug.enabled = chk_debug.state = false
				)
				else chk_debug.enabled = true
			)
		)
		
		fn UpdateLODButtons KeepSelect:false = (
			local ItemCollection = #()
			animate off
			(
				for item in DAOMatIni.MatTypes[ddl_types.selection].Semantics do append ItemCollection item
				for item in DAOMatIni.MatTypes[ddl_types.selection].SemanticsExt do append ItemCollection item
				
				local LOD1 = (ItemCollection[ddl_semantics.selection].LOD1Semantic != undefined)
				local LOD2 = (ItemCollection[ddl_semantics.selection].LOD2Semantic != undefined)
				local LOD3 = (ItemCollection[ddl_semantics.selection].LOD3Semantic != undefined)
				local LODS = 0
				if LOD1 then LODS += 1 ;if LOD2 then LODS += 1 ;if LOD3 then LODS += 1
				if LODS == 1 then rdo_lod.enabled = false else rdo_lod.enabled = true
				
				if KeepSelect == true then
				(
					if rdo_lod.state == 1 and LOD1 == false then KeepSelect = false
					if rdo_lod.state == 2 and LOD2 == false then KeepSelect = false
					if rdo_lod.state == 3 and LOD3 == false then KeepSelect = false
				)
				if KeepSelect == false then
				(
					if LOD1 != true then
					(
						if LOD2 != true then
						(
							if LOD3 != true then rdo_lod.state = 1 else rdo_lod.state = 3
						)
						else rdo_lod.state = 2
					)
					else rdo_lod.state = 1
				)
			)
		)
		
		fn WriteParams = (
			local ItemCollection = #()
			for item in DAOMatIni.MatTypes[ddl_types.selection].Semantics do append ItemCollection item
			for item in DAOMatIni.MatTypes[ddl_types.selection].SemanticsExt do append ItemCollection item
			
			local SemanticID = case rdo_lod.state of
			(
				1:(ItemCollection[ddl_semantics.selection].LOD1Semantic)
				2:(ItemCollection[ddl_semantics.selection].LOD2Semantic)
				3:(ItemCollection[ddl_semantics.selection].LOD3Semantic)
			)
			animate off
			(
				if SemanticID != undefined then MatSemanticID = SemanticID
				MatTypeID = DAOMatIni.MatTypes[ddl_types.selection].Type
			)
		)
		
		fn UpdateProperties = (
			local ItemCollection = #()
			animate off
			(
				for item in DAOMatIni.MatTypes[ddl_types.selection].Semantics do append ItemCollection item
				for item in DAOMatIni.MatTypes[ddl_types.selection].SemanticsExt do append ItemCollection item
				MatMapsRollout.CurrentType = MatPropertyRollout.CurrentType = MatTypeID
				MatMapsRollout.CurrentSem = MatPropertyRollout.CurrentSem = if ddl_semantics.selection > 0 then ItemCollection[ddl_semantics.selection].ID else 1
				MatMapsRollout.CurrentLOD = MatPropertyRollout.CurrentLOD = rdo_lod.state
				MatPropertyRollout.UpdateDialog Reset:(if TypeHasChanged then true else false)
				MatMapsRollout.UpdateDialog()
			)
		)
		
		fn InitDialog = (
			local ItemCollection
			
			animate off
			(
				--*******************************************************************************
				--Rollout initialization, remove and add to ensure intended rollout display order
				--Material Type -> Properties -> Maps
				--*******************************************************************************
				removeRollout MatPropertyRollout
				removeRollout MatMapsRollout
				
				addRollout MatPropertyRollout rolledup:(not DAOMatIni.RollUpStates[2])
				addRollout MatMapsRollout rolledup:(not DAOMatIni.RollUpStates[3])
				
				--****************************
				--Initializing Material basic
				--****************************
				if ParamPropsInit == true then this.name = "DAO_newmat_" + ((random 1 9999) as string)	--set default name, filename friendly
				if MatTypeID == 0 then MatTypeID = 14	--set prop again, just to be sure
				
				--***********************************
				--Init Types List with Material Types
				--***********************************
				ItemCollection = for item in DAOMatIni.MatTypes collect item.TypeDisplay
				ddl_types.items = ItemCollection
				for i = 1 to DAOMatIni.MatTypes.count do if DAOMatIni.MatTypes[i].Type == MatTypeID do ddl_types.selection = CurrTypeSelection = i
				
				--*********************************************
				--Init Semantics List, based on selected Type
				--*********************************************
				ItemCollection = #()
				for item in DAOMatIni.MatTypes[ddl_types.selection].Semantics do append ItemCollection item
				for item in DAOMatIni.MatTypes[ddl_types.selection].SemanticsExt do append ItemCollection item
				
				local SelectThisSemantic
				local SelectThisLOD
				if MatSemanticID != 0 then for i = 1 to ItemCollection.count where SelectThisSemantic == undefined do
				(
					if ItemCollection[i].LOD1Semantic == MatSemanticID then (SelectThisSemantic = i; SelectThisLOD = 1)
					if ItemCollection[i].LOD2Semantic == MatSemanticID then (SelectThisSemantic = i; SelectThisLOD = 2)
					if ItemCollection[i].LOD3Semantic == MatSemanticID then (SelectThisSemantic = i; SelectThisLOD = 3)
				)
				
				if MatSemanticID == 0 then (SelectThisSemantic = DAOMatIni.MatTypes[ddl_types.selection].DefaultSem; SelectThisLOD = 1)
				if SelectThisSemantic == undefined then	(SelectThisSemantic = DAOMatIni.MatTypes[ddl_types.selection].DefaultSem; SelectThisLOD = 1)
			
				FillSemanticList Ext:(SelectThisSemantic > DAOMatIni.MatTypes[ddl_types.selection].Semantics.count) Select:SelectThisSemantic
				chk_debug.state = (SelectThisSemantic > DAOMatIni.MatTypes[ddl_types.selection].Semantics.count)
				rdo_lod.state = SelectThisLOD
				
				
				--**********************************************
				--Push Update forward
				--**********************************************
				UpdateLODButtons KeepSelect:Keep
				UpdateDebugBox()
				WriteParams()
				UpdateProperties()
			)
		)
		
		fn UpdateAppearance = (
			if MatSemanticID == 111 then delegate.wire = true else delegate.wire = false
			if MatTypeID == 4 then delegate.twoSided = true else delegate.twoSided = false
		)
		
		fn UpdateDialog Keep:false = (
			UpdateDebugBox()
			FillSemanticList Ext:(chk_debug.checked) KeepSelect:Keep
			UpdateLODButtons KeepSelect:Keep
			WriteParams()
			UpdateProperties()
			UpdateAppearance()
			TypeHasChanged = false	--reset this one, updates are over
		)
		
		on ddl_types selected item do
		(
			if item != CurrTypeSelection then
			(
				TypeHasChanged = true	--forcing resets thru update chain
				UpdateDialog()
			)
			CurrTypeSelection = item
		)
		on ddl_semantics selected item do (UpdateDialog Keep:true)
		on chk_debug changed state do (UpdateDialog Keep:true)
		on rdo_lod changed item do (UpdateDialog Keep:true)
		
		on MatTypeRollout open do with animate off ( InitDialog() )
		on MatTypeRollout reload do with animate off ( InitDialog() )
		--on MatTypeRollout rolledup state do DAOMatIni.RollUpStates[1] = state
	)
	
	on create do with animate off
	(
		MatTypeID = 14		--setting default prop mat, on fresh material
	)
)