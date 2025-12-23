/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edit:
8 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Model / Animation Import Main Script
************************************************
*/

struct DAOImportStruct
(
	--MenuSettings
	--************
	UI_Pos,
	UI_Height,
	
	--Various, storing per session
	LastMMHPath = "",
	LastANIPath = "",
	PresetPath = "C:\\",
	AniStartFrame = 1,	AniFrameRate = 30,
	AniFilter = true,
	AniFilter2 = false,
	ImportGAD = true,
	ImportMesh = true,
	ImportColl = true
)

try (DestroyDialog DAOImportToolsRollout)catch()
global DAOImportToolsRollout
global DAOImport
(
	struct DAOVarsImportStruct (
		DefaultExtension = "_Secondary",	--Extension used by multiple Models ,so similar names do not collide
		TemplateExtension = "Template",		--For the Template skeleton, temporarily build while creating the Skeletons
		Extension,							--Current Extension in use while import is in progress
		
		BoneArray = #(),		--Stores the skeleton
		MeshNodeNames = #(),	--Stores mesh MMH node names to be used when creating skeleton and set up GOB
		MeshNames = #(),		--Stores meshnames to be used when creating skeleton and set up GOB
		BoneIndexArray = #(),	--Stores the boneindex while reading Bones, to be used when adding the bones to skinmod on a mesh
		FXActorName = ""		--Stores the FX ActorName of MMH ,to be written to GameObject
	)
	
	struct FileStatesStruct (
		MMHFileName,
		MMHloaded,
		MSHFileName,
		MSHloaded,
		MAOFileName,
		DDSFileName,
		ANIloaded,
		GADloaded,
		MeshCount
	)
	
	local DAOImportVars = DAOVarsImportStruct()
	local FileStates = FileStatesStruct ()
	
	struct DAONodeStruct (	--Definition for Nodes ,to create a Skeleton from
		Type,
		Name,
		Parent,
		Pos,
		Rot,
		Lenght,
		BoneParamAct,
		BoneIndex,
		HookID,
		BoneParamPos,
		BoneParamRot,
		BoneParamAdd
	)
	
	include "DAOTools\\DAORollouts.ms"
	include "DAOTools\\DAOcommon.ms"
	include "DAOTools\\DAOImportHelper.ms"
	include "DAOTools\\DAOGFFMath.ms"
	include "DAOTools\\DAOGFFFormat.ms"
	include "DAOTools\\DAOPresetSkeletons.ms"
	include "DAOTools\\DAOMaterialImport.ms"
	include "DAOTools\\DAOModelImport.ms"
	include "DAOTools\\DAOAnimImport.ms"
	
	fn Initialize = (
		DAOImport = DAOImportStruct()
		try(DAOImport.UI_Pos.x = (DAOTools.ReadIni "Import" "UI_Pos_X") as integer)catch()
		try(DAOImport.UI_Pos.y = (DAOTools.ReadIni "Import" "UI_Pos_Y") as integer)catch()
		try(DAOImport.UI_Height = (DAOTools.ReadIni "Import" "UI_Height") as integer)catch()
		try(DAOImport.LastMMHPath = (DAOTools.ReadIni "Paths" "MMHPath"))catch()
		try(DAOImport.LastANIPath = (DAOTools.ReadIni "Paths" "ANIPath"))catch()
		try(DAOImport.AniFrameRate = (DAOTools.ReadIni "Import" "AniFrameRate") as integer)catch()
		if DAOImport.UI_Pos == undefined then
		(
			if ((maxversion())[1] >= 7000) then DAOImport.UI_Pos = getMAXWindowPos() + (getMAXWindowSize()/4) else DAOImport.UI_Pos = [100,100]
		)		
		if DAOImport.UI_Height == undefined then DAOImport.UI_Height = 480
		if DAOImport.LastMMHPath == undefined then DAOImport.LastMMHPath = ""
		if DAOImport.LastANIPath == undefined then DAOImport.LastANIPath = ""
		if DAOImport.AniFrameRate == undefined then DAOImport.AniFrameRate = 30
	)
	
	rollout DAOImportDialog "Model / Animation Import" width:505 height:372
	(
		GroupBox grp12 "Game Objects (GOB) in Scene" pos:[4,1] width:318 height:102
		listbox lbx_gobjects "" pos:[10,16] width:304 height:6
		GroupBox grp5 "Model Import" pos:[4,105] width:495 height:127
		GroupBox grp44 "Options" pos:[16,124] width:144 height:98
		checkbox chk_merge "Merge with selected" pos:[28,142] width:114 height:14
		
		label lbl17 "File:" pos:[185,124] width:26 height:16
		edittext edt_mmhfile "" pos:[212,122] width:240 height:17
		button btn_mmhfile "..." pos:[460,122] width:25 height:17
		
		GroupBox grp3 "Import" pos:[332,143] width:147 height:84
		button btn_importmmh "Import" pos:[371,199] width:66 height:24 enabled:false
		GroupBox grp45 "Skeleton Presets" pos:[184,143] width:141 height:84
		dropdownList drp_type "" pos:[207,166] width:92 height:21 items:#("Human (Male)", "Elf (Female)")
		button btn_createpreset "Create Preset" pos:[210,199] width:86 height:24
		checkbox chk_importmesh "Import Meshes" pos:[351,162] width:104 height:15 checked:true
		checkbox chk_importcoll "Import Collision" pos:[351,180] width:106 height:15 checked:true
		
		GroupBox grp9 "Animation Import" pos:[4,233] width:495 height:133
		GroupBox grp19 "Options" pos:[16,250] width:144 height:108
		spinner spn_startframe "" pos:[30,268] width:51 height:16 range:[1,1e+006,1] type:#integer
		spinner spn_framerate "" pos:[30,290] width:51 height:16 range:[1,1e+006,1] type:#integer
		label lbl26 "Start Frame" pos:[90,270] width:65 height:15
		label lbl27 "Frame Rate" pos:[90,290] width:59 height:17
		checkbox chk_anifilter "Noise Filter (light)" pos:[32,310] width:110 height:16
		checkbox chk_anifilter2 "Noise Filter (strong)" pos:[32,329] width:113 height:16
		
		GroupBox grp4 "Import" pos:[332,270] width:147 height:91
		label lbl40 "File:" pos:[185,250] width:28 height:15
		edittext edt_anifile "" pos:[212,249] width:240 height:17
		button btn_anifile "..." pos:[457,249] width:25 height:16
		button btn_importani "Import" pos:[371,329] width:66 height:24 enabled:false
		checkbox chk_importgad "Import GAD" pos:[351,288] width:86 height:15 checked:true
		GroupBox grp11 "File Details" pos:[184,270] width:141 height:91
		label lbl61 "Has GAD:" pos:[196,339] width:57 height:13
		label lbl42 "Lenght:" pos:[195,292] width:71 height:14
		label lbl43 "Is Additive:" pos:[196,307] width:73 height:15
		label lbl44 "Is Override:" pos:[196,323] width:76 height:17
		
		label lbl_WS "WorldScale from Setup:" pos:[326,18] width:116 height:16
		label lbl_Scale "1" pos:[450,18] width:39 height:15
		
		
		fn FindValidExtension = (			--Finds extension from currently selected GOB
			DAOImportVars.Extension = undefined
			if lbx_gobjects.selected != undefined and matchpattern lbx_gobjects.selected pattern:"GOB*" then
			(
				DAOImportVars.Extension = DAOImportToolsRollout.GOBs[lbx_gobjects.selection].ObjExtension
				if (getNodeByName ("GOB"+DAOImportVars.Extension) exact:true ignoreCase:true all:false) == undefined then DAOImportVars.Extension = undefined
			)
			if DAOImportVars.Extension != undefined then true else false
		)
		
		fn CreateValidExtension = (			--Creates a unique object name extension to use for the Model, to allow for multiple Models in a scene.
			DAOImportVars.Extension = undefined
			if chk_merge.checked == true then	--When merging, find current selected gob extension
			(
				if not FindValidExtension() then Messagebox "Cannot merge with unknown selection" title:"Error"
			)
			else								--When not merging, create a new one.
			(
				if (getNodeByName "GOB" exact:true ignoreCase:true all:false) == undefined then DAOImportVars.Extension = "" else DAOImportVars.Extension = substring (Uniquename ("GOB"+DAOImportVars.DefaultExtension)) 4 20
			)
			if DAOImportVars.Extension != undefined then true else false	--Returns true if a valid extension was given.
		)
		
		fn LoadMMH File = (		--This function only loads and checks the file. (enables import button)
			local MMHStream		--could be used to preview content..
			btn_importmmh.enabled = false
			if File != undefined and DAOTools.ExistFile File then
			(
				edt_mmhfile.text = File
				MMHStream = fopen File "rb"
				DAOImport.LastMMHPath = getfilenamepath File	--Remembering this path for future loads
				if (LoadGFF MMHStream) and IsMMH() then btn_importmmh.enabled = true else MessageBox "File is not a MMH." title:"Error"
				fclose MMHStream
			)
		)
		
		fn LoadANI File = (		--This function only loads and checks the file. (enables import button)
			local ANIStream
			btn_importani.enabled = false
			if File != undefined and DAOTools.ExistFile File then
			(
				edt_anifile.text = File
				ANIStream = fopen File "rb"
				DAOImport.LastANIPath = getfilenamepath File
				if (LoadGFF ANIStream) and IsANI() then btn_importani.enabled = true else MessageBox "File is not a ANI." title:"Error"
				fclose ANIStream
			)
		)
		
		on DAOImportDialog open do
		(
			lbl_Scale.text = (DAOTools.WorldScale as string)
			spn_startframe.value = DAOImport.AniStartFrame	--4 presistent
			spn_framerate.value = DAOImport.AniFrameRate
			chk_anifilter.checked = DAOImport.AniFilter
			chk_anifilter2.checked = DAOImport.AniFilter2
			DAOImport.ImportGAD = chk_importgad.checked		--3 not persistent
			DAOImport.ImportMesh = chk_importmesh.checked
			DAOImport.ImportColl = chk_importcoll.checked
		)
		
		on spn_startframe changed value do DAOImport.AniStartFrame = value
		on spn_framerate changed value do DAOImport.AniFrameRate = value
		on chk_anifilter changed state do DAOImport.AniFilter = state
		on chk_anifilter2 changed state do DAOImport.AniFilter2 = state
		on chk_importgad changed state do DAOImport.ImportGAD = state
		on chk_importmesh changed state do DAOImport.ImportMesh = state
		on chk_importcoll changed state do DAOImport.ImportColl = state
		
		on lbx_gobjects selected item do
		(
			if (getNodeByName (lbx_gobjects.items[item]) exact:true ignoreCase:true all:false) != undefined then
			(
				select (getNodeByName (lbx_gobjects.items[item]) exact:true ignoreCase:true all:false)
			)
			else deselect (getcurrentselection())
		)
		
		on btn_createpreset pressed do
		(
			DAOImportToolsRollout.SuspendUpdates = true
			if CreateValidExtension () then
			(
				local WarningText = ""
				if chk_merge.checked == true	then WarningText += "Preset will be merged with current Selection\n\"GOB"+(DAOImportVars.Extension as string)+"\"\nMake sure it fits (for example multipart models).\n\n" else if not DAOImportVars.Extension == "" then WarningText += "Creating secondary Model due one already existing.\n\n"
				WarningText += "Scale setting:  x"+(DAOTools.WorldScale as string)+"\n\n"
				local warning = QueryBox ("Ready to create preset.\n\n"+WarningText+"Continue?") title:"Import"			
				if warning then
				(
					GetSkeletonPreset drp_type.selection
					if CreateSystem () and chk_merge.checked == false then	--Createsystem returned true, which means duplicates have been found.
					(
						MessageBox "Warning!! \nAt least one Node already existed and wasnt adjusted. \nResults may be unexpected. \n\nMake sure you delete any of the Nodes and try again." title:"Model Import"
					)
					if chk_merge.checked == true then MessageBox ("Skeleton has been merged with current Selection.\n\"GOB"+(DAOImportVars.Extension as string)+"\"\n\nMake sure that both skeletons were equal otherwise results are wrong.") title:"Done"
				)
			)
			DAOImportToolsRollout.SuspendUpdates = false
			DAOImportToolsRollout.UpdateDialog()
		)
		on btn_mmhfile pressed do
		(
			File = getOpenFileName caption:"Select an MMH File" filename:(getfilenamepath DAOImport.LastMMHPath) \
					  types:"MMH files (*.mmh)|*.mmh|All Files (*.*)|*.*|"
			LoadMMH File
		)
		on edt_mmhfile changed File do btn_importmmh.enabled = false
		on edt_mmhfile entered File do LoadMMH File
		
		on btn_importmmh pressed do with animate off
		(
			DAOImportToolsRollout.SuspendUpdates = true
			try(DAOModelManagerRollout.SuspendUpdates = true)catch()
			if CreateValidExtension () then
			(
				local WarningText = ""
				WarningText += "\""+(filenameFromPath edt_mmhfile.text) + "\"\n\n"
				if chk_merge.checked == true	then WarningText += "Model will be merged with current Selection\n\"GOB"+(DAOImportVars.Extension as string)+"\"\nMake sure it fits (for example multipart models).\n\n" else if not DAOImportVars.Extension == "" then WarningText += "Creating secondary Model due one already existing.\n\n"
				WarningText += "Scale setting:  x"+(DAOTools.WorldScale as string)+"\n\n"
				local warning = QueryBox ("Ready to Import.\n"+WarningText+"Continue?") title:"Import"
				if warning then
				(
					ImportMMH edt_mmhfile.text
				)
			)
			DAOImportToolsRollout.SuspendUpdates = false
			try(DAOModelManagerRollout.SuspendUpdates = false)catch()
			DAOImportToolsRollout.UpdateDialog()
			try(DAOModelManagerRollout.UpdateDialog())catch()
		)
		
		on btn_anifile pressed do
		(
			File = getOpenFileName caption:"Select an ANI File" filename:(getfilenamepath DAOImport.LastANIPath) \
			   types:"ANI files (*.ani)|*.ani|All Files (*.*)|*.*|"
			LoadANI File
		)
		on edt_anifile changed File do btn_importani.enabled = false
		on edt_anifile entered File do LoadANI File
		
		on btn_importani pressed do
		(
			DAOImportToolsRollout.SuspendUpdates = true
			if FindValidExtension() then ImportAni edt_anifile.text else Messagebox "Cannot add Animation to unknown Game Object selection\nSelect one in the list above" title:"Error"
			DAOImportToolsRollout.SuspendUpdates = false
			select (GetCurrentSelection())	--forcing dialog update
		)
	)
	
	rollout DAOImportToolsRollout "Dragon Age Importer" width:518 height:527
	(
		GroupBox grp1 "" pos:[162,1] width:198 height:45
		label lbl1 "Model / Animation Importer" pos:[200,10] width:127 height:16
		label lbl2 "Dragon Age: Origins" pos:[220,27] width:95 height:16
		edittext txt_maxversion "" pos:[0,507] width:68 height:16 enabled:false
		subRollout rollouts "" pos:[0,49] width:515 height:454
		label lbl10 "Script by Eshme" pos:[360,507] width:85 height:16 enabled:false
		label lbl11 "" pos:[470,507] width:40 height:16 enabled:false

		local GOBs = #()
		local SuspendUpdates = false

		fn UpdateDialog = (
			if not SuspendUpdates then
			(
				GOBs = GetGobs()
				local GOBHold = #()
				local Unknown = 0
				for GOB in GOBs where GOB.isGOB == true do append GOBHold (GOB.ObjName+GOB.ObjExtension)
				for GOB in GOBs where GOB.isGOB == false do Unknown += 1
				if Unknown > 0 then append GOBHold (Unknown as string+" unknown Objects")
				DAOImportDialog.lbx_gobjects.items = GOBHold
				if selection[1] != undefined then DAOImportDialog.lbx_gobjects.selection = finditem GOBHold ((GetGOB selection[1]).name) else DAOImportDialog.lbx_gobjects.selection = 0
			)
		)
		
		on DAOImportToolsRollout open do
		(
			callbacks.addScript #selectionSetChanged "DAOImportToolsRollout.UpdateDialog()" id:#DAOImporterUpdateChange		--6 events sending a dialog update across
			callbacks.addScript #nodePostDelete "DAOImportToolsRollout.UpdateDialog()" id:#DAOImporterUpdateDelete
			callbacks.addScript #systemPostReset "DAOImportToolsRollout.UpdateDialog()" id:#DAOImporterUpdateReset
			callbacks.addScript #systemPostNew "DAOImportToolsRollout.UpdateDialog()" id:#DAOImporterUpdateNew
			callbacks.addScript #filePostOpen "DAOImportToolsRollout.UpdateDialog()" id:#DAOImporterUpdateFile
			callbacks.addScript #postImport "DAOImportToolsRollout.UpdateDialog()" id:#DAOImporterUpdateImport		
		
			lbl11.text = DAOTools.Version
			local maxver = maxVersion()
			txt_maxversion.text = "MAX  v"+((maxver[1] / 1000) as string)
			if maxver[1] <= 4200 then txt_maxversion.text = " GMAX"
			Rollouts.height = DAOImport.UI_Height - 70
			lbl10.pos = [lbl10.pos.x,DAOImport.UI_Height - 17]
			lbl11.pos = [lbl11.pos.x,DAOImport.UI_Height - 17]
			txt_maxversion.pos = [txt_maxversion.pos.x,DAOImport.UI_Height - 20]
		)
		on DAOImportToolsRollout close do 
		(
			callbacks.removeScripts id:#DAOImporterUpdateChange
			callbacks.removeScripts id:#DAOImporterUpdateDelete
			callbacks.removeScripts id:#DAOImporterUpdateReset
			callbacks.removeScripts id:#DAOImporterUpdateNew
			callbacks.removeScripts id:#DAOImporterUpdateFile
			callbacks.removeScripts id:#DAOImporterUpdateImport		
		
			DAOTools.WriteIni "Import" "UI_Pos_X" DAOImport.UI_Pos.x
			DAOTools.WriteIni "Import" "UI_Pos_Y" DAOImport.UI_Pos.y
			DAOTools.WriteIni "Import" "UI_Height" DAOImport.UI_Height
			DAOTools.WriteIni "Paths" "MMHPath" DAOImport.LastMMHPath
			DAOTools.WriteIni "Paths" "ANIPath" DAOImport.LastANIPath
			DAOTools.WriteIni "Import" "AniFrameRate" DAOImport.AniFrameRate
		)
		on DAOImportToolsRollout resized Size do
		(
			if DAOImportToolsRollout.open then
			(
				DAOImport.UI_Height = Size.y
				Rollouts.height = DAOImport.UI_Height - 70
				lbl10.pos = [lbl10.pos.x,DAOImport.UI_Height - 17]
				lbl11.pos = [lbl11.pos.x,DAOImport.UI_Height - 17]
				txt_maxversion.pos = [txt_maxversion.pos.x,DAOImport.UI_Height - 20]
			)
		)
		on DAOImportToolsRollout moved pos do DAOImport.UI_Pos = pos
	)
	
	on execute do with undo off
	(
		if DAOImport == undefined do Initialize()
		--Initialize()
		
		CreateDialog DAOImportToolsRollout style:#(#style_sysmenu,#style_titlebar,#style_minimizebox,#style_resizing) height:DAOImport.UI_Height lockWidth:true pos:DAOImport.UI_Pos
		AddSubRollout DAOImportToolsRollout.Rollouts DAOImportDialog rolledup:false
		DAOImportToolsRollout.UpdateDialog()
	)
)