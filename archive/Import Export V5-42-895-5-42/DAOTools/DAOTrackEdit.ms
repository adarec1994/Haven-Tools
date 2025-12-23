/**********************************************************************************************************
Dragon Age Origins Tools
Author: Eshme  http://social.bioware.com/project/2336/

last edited:
21 August 2010

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2010 Eshme

************************************************************************************************************
DAOTrackEdit.ms
Track Editor. Remote run by GOB Gameobject class
Operates on the GOB it was run from, accessing params
************************************************************************************************************
*/

struct DAOTrackEditStruct
(
	--MenuSettings
	--************
	UI_Pos
)
try (DestroyDialog DAOAnimTracksRollout)catch()
global DAOAnimTracksRollout
global DAOTrackEdit
(
	include "DAOTools\\DAORollouts.ms"
	include "DAOTools\\DAOcommon.ms"
	
	fn Initialize = (
		DAOTrackEdit = DAOTrackEditStruct()
		if ((maxversion())[1] >= 7000) then DAOTrackEdit.UI_Pos = getMAXWindowPos() + (getMAXWindowSize()/4) else DAOTrackEdit.UI_Pos = [100,100]
		try(DAOTrackEdit.UI_Pos.x = (DAOTools.ReadIni "TrackEdit" "UI_Pos_X") as integer)catch()
		try(DAOTrackEdit.UI_Pos.y = (DAOTools.ReadIni "TrackEdit" "UI_Pos_Y") as integer)catch()
	)
	
	struct DAOTrackEditVarsStruct
	(
		--Local variables to all functions
		CurrentGOB,
		CurrentSequence
	)
	DAOTrackEditVars = DAOTrackEditVarsStruct()
	
	rollout DAOAnimTracksRollout "Animation Tracks" width:629 height:465
	(
		GroupBox grp_sequences "Sequences" pos:[4,1] width:266 height:50
		dropdownList ddl_seqs "" pos:[12,19] width:249 height:21
		
		GroupBox grp_title "" pos:[277,1] width:338 height:50
		label lbl_title "Track Editor" pos:[286,12] width:68 height:16
		label lbl_current "Current Model:" pos:[286,28] width:77 height:16
		label lbl_model "2233" pos:[380,28] width:219 height:16
		
		GroupBox grp_tracks "All animatable Objects" pos:[4,51] width:357 height:338
		multilistbox lbx_tracks "" pos:[8,69] width:348 height:24
		
		GroupBox grp_options "Track Options" pos:[365,51] width:250 height:338
		label lbl_sel "Selected:" pos:[379,70] width:52 height:16
		label lbl_seltext "" pos:[446,70] width:115 height:16
		
		GroupBox grp_pos "Position Track" pos:[374,87] width:206 height:93
		checkbox chk_posoff "Export Off" pos:[391,101] width:110 height:15
		checkbox chk_posAuto "Autodetect (default)" pos:[391,118] width:167 height:17
		checkbox chk_posLeast "Force Export (with Autodetect)" pos:[391,136] width:169 height:18
		checkbox chk_posFull "Force Export (all frames)" pos:[391,152] width:146 height:14
		
		GroupBox grp_rot "Rotation Track" pos:[373,188] width:208 height:100
		checkbox chk_rotoff "Export Off" pos:[391,205] width:145 height:14
		checkbox chk_rotAuto "Autodetect (default)" pos:[391,224] width:123 height:15
		checkbox chk_rotLeast "Force Export (with Autodetect)" pos:[390,244] width:162 height:17
		checkbox chk_rotFull "Force Export (all frames)" pos:[389,263] width:152 height:16
		
		button btn_close "Close" pos:[222,400] width:143 height:27
		
		local ExportObjs = #()		--List of nodes drawn in the list
		local ExportObjsNames = #()	--List of display names for the nodes in the list
		
		fn UpdateSettings = (
			local Sel = lbx_tracks.selection as array
			if Sel.count != 1 then	lbl_seltext.caption = Sel.count as string+" Objects"
			else					lbl_seltext.caption = ExportObjs[Sel[1]].name
			
			if Sel.count > 0 then
			(
				local pos = 0
				local rot = 0
				
				for i = 1 to Sel.count do
				(
					local Setting = GetTrackSetting DAOTrackEditVars.CurrentGOB ExportObjs[Sel[i]] DAOTrackEditVars.CurrentSequence
					
					if Setting.Pos == #Nil then pos = bit.or pos 1
					if Setting.Pos == #Auto then pos = bit.or pos 2
					if Setting.Pos == #Min then pos = bit.or pos 4
					if Setting.Pos == #Full then pos = bit.or pos 8
					
					if Setting.Rot == #Nil then rot = bit.or rot 1
					if Setting.Rot == #Auto then rot = bit.or rot 2
					if Setting.Rot == #Min then rot = bit.or rot 4
					if Setting.Rot == #Full then rot = bit.or rot 8
				)
				if ((maxversion())[1] >= 5000) then		--tristate allowed since max5
				(
					chk_posOff.tristate = if (bit.and pos 1) == 0 then 0 else if pos == 1 then 1 else 2
					chk_posAuto.tristate = if (bit.and pos 2) == 0 then 0 else if pos == 2 then 1 else 2
					chk_posLeast.tristate = if (bit.and pos 4) == 0 then 0 else if pos == 4 then 1 else 2
					chk_posFull.tristate = if (bit.and pos 8) == 0 then 0 else if pos == 8 then 1 else 2
				
					chk_rotOff.tristate = if (bit.and rot 1) == 0 then 0 else if rot == 1 then 1 else 2
					chk_rotAuto.tristate = if (bit.and rot 2) == 0 then 0 else if rot == 2 then 1 else 2
					chk_rotLeast.tristate = if (bit.and rot 4) == 0 then 0 else if rot == 4 then 1 else 2
					chk_rotFull.tristate = if (bit.and rot 8) == 0 then 0 else if rot == 8 then 1 else 2	
				)
				else
				(
					chk_posOff.checked  = if (bit.and pos 1) == 0 then false else true
					chk_posAuto.checked  = if (bit.and pos 2) == 0 then false else true
					chk_posLeast.checked  = if (bit.and pos 4) == 0 then false else true
					chk_posFull.checked  = if (bit.and pos 8) == 0 then false else true
				
					chk_rotOff.checked  = if (bit.and rot 1) == 0 then false else true
					chk_rotAuto.checked  = if (bit.and rot 2) == 0 then false else true
					chk_rotLeast.checked  = if (bit.and rot 4) == 0 then false else true
					chk_rotFull.checked  = if (bit.and rot 8) == 0 then false else true
				)

			)
		)
		
		fn UpdateList Reset:true = (
			ExportObjsNames = #()
			for obj in ExportObjs do 
			(
				local item = obj.name
				local Setting = GetTrackSetting DAOTrackEditVars.CurrentGOB Obj DAOTrackEditVars.CurrentSequence
				for i = 1 to ((200-(GetTextExtent Obj.name).x)/3) do item += " "
				case Setting.Pos of
				(
					#Nil: item += ":pO "
					#Min: item += ":pM "
					#Full: item += ":pF "
				)
				case Setting.Rot of
				(
					#Nil: item += ":rO "
					#Min: item += ":rM "
					#Full: item += ":rF "
				)
				append ExportObjsNames item
			)
			lbx_tracks.items = ExportObjsNames
			if Reset == true then lbx_tracks.selection = 1
		)
		
		fn InitRollout = (
			--**************************************
			--Gets all the sequences, the hierarchy 
			--to prepare for that one model
			--************************************
			lbl_model.caption = DAOTrackEditVars.CurrentGOB.Name
			TrackGarbageCollect DAOTrackEditVars.CurrentGOB
			AllSeqs = #()
			for item in DAOTrackEditVars.CurrentGOB.SequenceName do append AllSeqs item
			ddl_seqs.items = AllSeqs
			ddl_seqs.selection = DAOTrackEditVars.CurrentSequence
			
			local objs = GetHierarchy #(DAOTrackEditVars.CurrentGOB)
			ExportObjs = #()
			for obj in objs do
			(
				if GetObjectAnimExportType obj != undefined then
				(
					append ExportObjs obj
				)
			)
			UpdateList()
			UpdateSettings()
		)
		
		fn StoreSettings Item:#none = (
			--emulate radiobuttons
			if Item != #none then
			(
				if Item.checked == false then Item.checked = true
				if Item.checked == true then
				(
					if item == chk_posoff or item == chk_posAuto or item == chk_posLeast or item == chk_posFull then
					(
						if item != chk_posOff then chk_posOff.checked = false
						if item != chk_posAuto then chk_posAuto.checked = false
						if item != chk_posLeast then chk_posLeast.checked = false
						if item != chk_posFull then chk_posFull.checked = false
					)
					if item == chk_rotoff or item == chk_rotAuto or item == chk_rotLeast or item == chk_rotFull then
					(
						if item != chk_rotOff then chk_rotOff.checked = false
						if item != chk_rotAuto then chk_rotAuto.checked = false
						if item != chk_rotLeast then chk_rotLeast.checked = false
						if item != chk_rotFull then chk_rotFull.checked = false
					)
				)
			)
			--store
			local Sel = lbx_tracks.selection as array
			if Sel.count > 0 then
			(
				for i = 1 to Sel.count do
				(
					local Setting = GetTrackSetting DAOTrackEditVars.CurrentGOB ExportObjs[Sel[i]] DAOTrackEditVars.CurrentSequence
					if item == chk_posoff or item == chk_posAuto or item == chk_posLeast or item == chk_posFull then
					(
						if chk_posOff.checked then Setting.Pos = #Nil
						if chk_posAuto.checked then Setting.Pos = #Auto
						if chk_posLeast.checked then Setting.Pos = #Min
						if chk_posFull.checked then Setting.Pos = #Full
					)
					if item == chk_rotoff or item == chk_rotAuto or item == chk_rotLeast or item == chk_rotFull then
					(
						if chk_rotOff.checked then Setting.Rot = #Nil
						if chk_rotAuto.checked then Setting.Rot = #Auto
						if chk_rotLeast.checked then Setting.Rot = #Min
						if chk_rotFull.checked then Setting.Rot = #Full
					)
					SetTrackSetting DAOTrackEditVars.CurrentGOB ExportObjs[Sel[i]] DAOTrackEditVars.CurrentSequence Setting
				)
			)
			UpdateList Reset:false
		)
		
		on chk_posOff changed state do (StoreSettings Item:chk_posOff)
		on chk_posAuto changed state do (StoreSettings Item:chk_posAuto)
		on chk_posLeast changed state do (StoreSettings Item:chk_posLeast)
		on chk_posFull changed state do (StoreSettings Item:chk_posFull)
		on chk_rotOff changed state do (StoreSettings Item:chk_rotOff)
		on chk_rotAuto changed state do (StoreSettings Item:chk_rotAuto)
		on chk_rotLeast changed state do (StoreSettings Item:chk_rotLeast)
		on chk_rotFull changed state do (StoreSettings Item:chk_rotFull)
		
		on ddl_seqs selected sel do
		(
			DAOTrackEditVars.CurrentSequence = sel
			UpdateList()
			UpdateSettings()
		)
		on lbx_tracks selectionEnd do UpdateSettings()
		on btn_close pressed do try(DestroyDialog DAOAnimTracksRollout)catch()
		
		on DAOAnimTracksRollout open do InitRollout()
		on DAOAnimTracksRollout close do
		(
			DAOTools.WriteIni "TrackEdit" "UI_Pos_X" DAOTrackEdit.UI_Pos.x
			DAOTools.WriteIni "TrackEdit" "UI_Pos_Y" DAOTrackEdit.UI_Pos.y
		)
		on DAOAnimTracksRollout moved pos do DAOTrackEdit.UI_Pos = pos
	)
	
	on execute do with undo off
	(
		if DAOTrackEdit == undefined do Initialize()
		DAOTrackEditVars.CurrentGOB = DAO_GOBUtil.RemoteGOB	--transfer from Gameobject to local
		DAOTrackEditVars.CurrentSequence = DAO_GOBUtil.RemoteSeq
		DAO_GOBUtil.RemoteGOB = undefined
		DAO_GOBUtil.RemoteSeq = undefined
		if DAOTrackEditVars.CurrentGOB == undefined or (isDeleted DAOTrackEditVars.CurrentGOB) or (classof DAOTrackEditVars.CurrentGOB) != DAO_GameObject then
		(
			MessageBox ("Must be run from within GOB, custom helper Object.\nIm not a standalone script, aborting..") title:"Track Editor"
		)
		else if DAOTrackEditVars.CurrentGOB.parent != undefined then
		(
			MessageBox ("Model Root object must also be Root of the Hierarchy.\nThis plan doesnt work out, aborting..") title:"Track Editor"
		)
		else if DAOTrackEditVars.CurrentGOB.SequenceName.count < DAOTrackEditVars.CurrentSequence then
		(
			MessageBox ("Current Sequence not found.\nCheck if a Sequence already exists before Editting Tracks.\naborting..") title:"Track Editor"
		)
		else
		(
			CreateDialog DAOAnimTracksRollout pos:DAOTrackEdit.UI_Pos
		)
	)
)