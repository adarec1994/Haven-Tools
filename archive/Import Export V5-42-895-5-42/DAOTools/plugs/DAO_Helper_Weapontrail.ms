/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edited
21 July 2010

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2010 Eshme

************************************************************************************************************
DAO_Helper_Weapontrail.ms
Weapontrail Plugin

Adds a new helper object ,that simulates Weapontrails
************************************************************************************************************
*/
plugin Helper DAO_Weapontrail
name:"DAO Weapontrail"
classID:#(0x28c203f8, 0x2733132a)
category:"Dragon Age"
version:1
extends:cylgizmo
replaceui:true
invisible:true
(
	parameters main rollout:params
	(
		segmentlenght type:#float animatable:false ui:spn_lenght
		duration type:#float animatable:false ui:spn_duration default:0.5
		MatObject type:#string animatable:false default:"weapontrail_streaks"
		MatObjectMat type:#material animatable:false
		
		on segmentlenght set len do (delegate.radius = len*0.05; delegate.height = len)
	)
	rollout params "Weapontrail Parameters" width:168 height:105
	(
		spinner spn_lenght "Lenght:" pos:[50,7] width:107 height:16 range:[0,1e+009,1]
		spinner spn_duration "Duration (sek):" pos:[17,28] width:140 height:16 range:[0,1e+009,1]
		dropdownList ddl_MatObject "Material Object" pos:[11,54] width:140 height:40 items:#("weapontrail_streaks.mao", "weapontrail_arrow.mao", "custom (pick below)")
		materialbutton mtl_btn1 "none" pos:[3,100] width:130 height:16
		label lbl_mao ".MAO" pos:[135,102]
		
		fn WtrlEqualCaseLess String1 String2 = (
			if String1.count != String2.count then return false
			for i = 1 to String1.count do
			(
				local Comp = abs((bit.charasint String1[i]) - (bit.charasint String2[i]))
				if (Comp != 0 and Comp != 32) then return false
			)
			return true
		)
		
		on mtl_btn1 picked mat do
		(
			if mat != undefined then
			(
				local IsUnique = true
				if classof Mat == DAOMaterial and Mat.ParamPropsInit == true then
				(
					Mat.name = "wtrl_newmat_" + ((random 1 999) as string)	--Name override, for user and filename friendliness
					IsUnique = medit.OkMtlForScene Mat	--but must check if unique
				)
				if IsUnique then
				(
					MatObjectMat = mat
					if classof MatObjectMat == DAOMaterial and MatObjectMat.ParamPropsInit == true then
					(	--apply weapontrail_streak settings for a start
						MatObjectMat.MatTypeID = 20
						MatObjectMat.MatSemanticID = 1
						MatObjectMat.Map_Diffuse = bitmapTexture filename:"weapontrail_streaks.dds"
						MatObjectMat.Map_DistortionModifier = bitmapTexture filename:"weapontrail_streaks.dds"
						MatObjectMat.Map_DistortionVolume = bitmapTexture filename:"weapontrail_normal.dds"
						MatObjectMat.mml_fdistortion_fade_distance = 25.0
						MatObjectMat.mml_fdistortion_fade_multiplier = 0.0
						MatObjectMat.mml_fdistortion_invert = 1.0
						MatObjectMat.mml_fdistortion_magnitude = 3.0
						MatObjectMat.ParamPropsInit = false		--Unset ini var to prevent resets
					)
					if (maxversion())[1] >= 6000 then MatEditor.Open()
					meditMaterials[medit.getactivemtlslot()] = MatObjectMat
					try(mtl_btn1.caption = MatObjectMat.name)catch(mtl_btn1.caption = "pick other"; MatObjectMat = undefined)
				)
			)
			else
			(
				mtl_btn1.caption = "!!!pick other!!!"
				MatObjectMat = undefined
			)
		)
		
		on ddl_MatObject selected item do
		(
			if ddl_MatObject.selection == 1 then (mtl_btn1.enabled = false; mtl_btn1.caption = "none"; MatObject = "weapontrail_streaks")
			if ddl_MatObject.selection == 2 then (mtl_btn1.enabled = false; mtl_btn1.caption = "none"; MatObject = "weapontrail_arrow")
			if ddl_MatObject.selection == 3 then
			(
				mtl_btn1.enabled = true
				if MatObjectMat != undefined then mtl_btn1.caption = MatObjectMat.name else mtl_btn1.caption = "!!!pick other!!!"
				MatObject = ""
			)
		)
		
		on params open do
		(
			if (WtrlEqualCaseLess MatObject "weapontrail_streaks") then
			(
				ddl_MatObject.selection = 1
				mtl_btn1.enabled = false
				mtl_btn1.caption = "none"
			)
			else if (WtrlEqualCaseLess MatObject "weapontrail_arrow") then
			(
				ddl_MatObject.selection = 2
				mtl_btn1.enabled = false
				mtl_btn1.caption = "none"
			)
			else if MatObject == "" then	--case when expecting custom mat in "matobjectmat"
			(
				ddl_MatObject.selection = 3
				if MatObjectMat != undefined then mtl_btn1.caption = MatObjectMat.name else mtl_btn1.caption = "!!!pick other!!!"
			)
			else	--default case if we fail (this shouldnt happen thou)
			(
				ddl_MatObject.selection = 1
				MatObject = "weapontrail_streaks"
				mtl_btn1.enabled = false
				mtl_btn1.caption = "none"
			)
		)
	)
)
plugin Helper DAO_Weapontrail_Create
name:"Weapontrail"
category:"Dragon Age"
version:1
(
	local obj
	local oldselection
	tool create
	(
		on mousePoint click do
		(
			case click of
			(
				1: (
					in coordsys grid
					(
						oldselection = getCurrentSelection()
						obj = DAO_Weapontrail pos:gridpoint
						select obj
						obj.name = uniquename "Weapontrail"
						obj.pos = gridpoint
					)
					--nodeTM.translation = gridPoint
				)
				2: (
					if obj.segmentlenght < 0.3 then (delete obj; select oldselection)
					--if obj.segmentlenght < 1 then (obj.segmentlenght = 1 ;obj.delegate.radius = 1*0.05; obj.delegate.height = 1)
					--select obj
					#stop
				)
			)
		)
		on mouseMove click do
		(
			case click of
			(
				2:(
					local len = (abs gridDist.z)
					obj.segmentlenght = copy len
					obj.delegate.radius = len*0.05
					obj.delegate.height = len
				)
			)
		)
		--on mouseabort click do
		--(
		--	if click != 1 then if obj != undefined then delete obj
		--)
	)
)