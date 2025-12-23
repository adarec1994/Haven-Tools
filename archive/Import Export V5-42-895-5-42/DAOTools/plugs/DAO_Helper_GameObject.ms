/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edited
6 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it.

Copyright © 2011 Eshme

************************************************************************************************************
DAO_Helper_GameObject.ms
Game Object Plugin

Adds a new helper object ,that simulates GOB, model root objects

Basically working like a common Node, only a slight difference in export detection.
Has 2 Sets of parameters, which influence export detection (both model and animation) for the whole model

-Model Settings: Straighforward import/export storage for Model information
-Animation Settings: has 2 sub parameters for Animation import/export
	-1 Sequences settings per sequence: "Master" parameters, also straightforward import/export
	-2 Track setting per node and sequence: 	"Slave", each tracksetting is linked to a sequence
												One can exist in the order of 0 to however many Nodes there are, per sequence
	Have to keep both synced. Changes to the order of the Master only done in this file (deletion of a sequence for instance)
************************************************************************************************************
*/
(
	struct DAO_GOBUtilStruct
	(
		RemoteGOB,
		RemoteSeq
	)
	global DAO_GOBUtil = DAO_GOBUtilStruct()
)
plugin Helper DAO_GameObject
name:"DAO Model Base"
classID:#(0x28c203f9, 0x2733132b)
category:"Dragon Age"
version:1
extends:dummy
replaceui:true
invisible:true
(
	parameters generalparams rollout:Generalrollout
	(
		size type:#float animatable:false ui:spn_size
		on size set val do delegate.boxsize = [size,size,size*0.5]
	)
	rollout GeneralRollout "General Properties" width:168 height:35
	(
		spinner spn_size "Gizmo Size" pos:[24,9] width:119 height:16 range:[0,1e+009,1]
	)
	parameters Modelparams rollout:ModelparamsRollout
	(
		modeltype type:#integer animatable:false ui:ddl_modeltype default:2
		meshtype type:#integer animatable:false ui:ddl_meshtype default:1
		FXActorName type:#string animatable:false default:""
		SoundMatType type:#integer animatable:false ui:ddl_SoundMat default:1	--1 based. Need to take care by import/export which is 0 based
	)
	rollout ModelparamsRollout "Model Settings" width:168 height:187 rolledup:true
	(
		dropdownList ddl_modeltype "Model Type" pos:[5,6] width:157 height:40 items:#("Character / Creature", "Other")
		dropdownList ddl_meshtype "Character Mesh Section" pos:[5,50] width:157 height:40
		dropdownList ddl_FXActorName "Facial Animation Blueprint" pos:[5,94] width:157 height:40 items:#("none","child.fxa", "dwarffemale.fxa","dwarfmale.fxa","elffemale.fxa","elfmale.fxa","golem.fxa","human.fxa","humanfemale.fxa","humanmale.fxa","lof.fxa","qunarimale.fxa","succubus.fxa","sylvan.fxa","werewolf.fxa")
		dropdownList ddl_SoundMat "Sound Material Type" pos:[5,138] width:157 height:40 items:#("Unknown", "Wood","Sand","Leaves","Grass","Stone","Dirt","Puddle","Water","Mud","Ice","Snow","Hollow Wood","Metal","Gravel","Carpet")
		
		fn GOBEqualCaseLess String1 String2 = (
			if String1.count != String2.count then return false
			for i = 1 to String1.count do
			(
				local Comp = abs((bit.charasint String1[i]) - (bit.charasint String2[i]))
				if (Comp != 0 and Comp != 32) then return false
			)
			return true
		)
		
		fn UpdateFXActorName = (
			FXActorName = case ddl_FXActorName.selection of
			(
				1: ""
				2: "child.fxa"
				3: "dwarffemale.fxa"
				4: "dwarfmale.fxa"
				5: "elffemale.fxa"
				6: "elfmale.fxa"
				7: "golem.fxa"
				8: "human.fxa"
				9: "humanfemale.fxa"
				10: "humanmale.fxa"
				11: "lof.fxa"
				12: "qunarimale.fxa"
				13: "succubus.fxa"
				14: "sylvan.fxa"
				15: "werewolf.fxa"
			)
		)
		
		fn updatedialogMP reset:false = (	--general dlg update
			case modeltype of
			(
				1:(
					ddl_meshtype.caption = "Character Mesh Section"
					ddl_meshtype.items = #("Full Character","Chest","Boots","Gloves","Helmet","Cloth","Robe","Neck","Face","Eyes","Hair","Eye Lashes","Beard")
					ddl_FXActorName.enabled = true
					ddl_SoundMat.enabled = false
					ddl_SoundMat.selection = 1
					if (GOBEqualCaseLess FXActorName "") then ddl_FXActorName.selection = 1
					else if (GOBEqualCaseLess FXActorName "child.fxa") then ddl_FXActorName.selection = 2
					else if (GOBEqualCaseLess FXActorName "dwarffemale.fxa") then ddl_FXActorName.selection = 3
					else if (GOBEqualCaseLess FXActorName "dwarfmale.fxa") then ddl_FXActorName.selection = 4
					else if (GOBEqualCaseLess FXActorName "elffemale.fxa") then ddl_FXActorName.selection = 5
					else if (GOBEqualCaseLess FXActorName "elfmale.fxa") then ddl_FXActorName.selection = 6
					else if (GOBEqualCaseLess FXActorName "golem.fxa") then ddl_FXActorName.selection = 7
					else if (GOBEqualCaseLess FXActorName "human.fxa") then ddl_FXActorName.selection = 8
					else if (GOBEqualCaseLess FXActorName "humanfemale.fxa") then ddl_FXActorName.selection = 9
					else if (GOBEqualCaseLess FXActorName "humanmale.fxa") then ddl_FXActorName.selection = 10
					else if (GOBEqualCaseLess FXActorName "lof.fxa") then ddl_FXActorName.selection = 11
					else if (GOBEqualCaseLess FXActorName "qunarimale.fxa") then ddl_FXActorName.selection = 12
					else if (GOBEqualCaseLess FXActorName "succubus.fxa") then ddl_FXActorName.selection = 13
					else if (GOBEqualCaseLess FXActorName "sylvan.fxa") then ddl_FXActorName.selection = 14
					else if (GOBEqualCaseLess FXActorName "werewolf.fxa") then ddl_FXActorName.selection = 15
					else ddl_FXActorName.selection = 1
				)
				2:(
					ddl_meshtype.caption = "Model Section"
					ddl_meshtype.items = #("generic")
					ddl_FXActorName.enabled = false
					ddl_FXActorName.selection = 1
					ddl_SoundMat.enabled = true
				)
			)
			if reset == true then ddl_meshtype.selection = ddl_FXActorName.selection = 1
			UpdateFXActorName()	--syncronize FXActorName Variable with UI setting
		)
		
		on ddl_FXActorName selected item do (UpdateFXActorName())
		on ddl_modeltype selected item do (updatedialogMP reset:true)
		on ModelparamsRollout open do (updatedialogMP())
	)
	parameters AnimSequences rollout:AnimSequencesRollout
	(
		--parallel Arrays with Rows of settings per sequence. Arrays Count is equal Sequence Count.
		SequenceName type:#stringtab tabSizeVariable:true animatable:false
		SequencePivot type:#inttab tabSizeVariable:true animatable:false
		SequenceStart type:#inttab tabSizeVariable:true animatable:false
		SequenceEnd type:#inttab tabSizeVariable:true animatable:false
		SequenceHQ type:#boolTab tabSizeVariable:true animatable:false
		SequenceAddv type:#boolTab tabSizeVariable:true animatable:false
		SequenceOverride type:#boolTab tabSizeVariable:true animatable:false
		SequenceGAD type:#boolTab tabSizeVariable:true animatable:false
		SequenceGADEnt type:#boolTab tabSizeVariable:true animatable:false
		SequenceGADFH type:#boolTab tabSizeVariable:true animatable:false
		SequenceAPRScale type:#point3Tab tabSizeVariable:true animatable:false
		
		--parallel Arrays with each Row tied to any Node, defining a track setting and which sequence it belongs to.
		--Any node can have multiple appearance for any much sequences.
		--Only nondefault settings are stored, expect this array not to be too big but no limits. random order.
		AnimationNode type:#nodetab tabSizeVariable:true animatable:false
		AnimationNodeSeqLink type:#inttab tabSizeVariable:true animatable:false	--2 arrays for sequence and its settings per sequence.
		AnimationNodeSetting type:#inttab tabSizeVariable:true animatable:false
		AnimationGOBSeqLink type:#inttab tabSizeVariable:true animatable:false	--2 array for setting and sequence only for GOB itself. GOB itself wont appear in the Node Array cause circular reference problem.
		AnimationGOBSetting type:#inttab tabSizeVariable:true animatable:false
	)
	rollout AnimSequencesRollout "Animation Sequences" width:168 height:448 rolledup:true
	(
		GroupBox grp3 "All Sequences" pos:[3,0] width:162 height:85
		dropdownList ddl_names "" pos:[8,20] width:154 height:21
		label lbl1 "Current:" pos:[11,47] width:45 height:12
		label lbl_stored "" pos:[74,47] width:82 height:13
		edittext edt_name "" pos:[5,63] width:152 height:16
		
		GroupBox grp5 "Pivot Control" pos:[3,85] width:162 height:68
		radiobuttons rdo_pivot "" pos:[7,101] width:153 height:48 labels:#("World Center is Game Pivot", "GOB is Game Pivot", "Current Location")
		
		GroupBox grp1 "Frame Range" pos:[3,153] width:162 height:74
		label lbl_framestart "Start" pos:[9,168] width:27 height:15
		label lbl_frameend "End" pos:[89,168] width:22 height:12
		spinner spn_start "" pos:[7,185] width:72 height:16 range:[1,1e+009,1] type:#integer
		spinner spn_end "" pos:[86,185] width:72 height:16 range:[1,1e+009,1] type:#integer
		button btn_start "Now" pos:[39,168] width:30 height:15
		button btn_end "Now" pos:[115,168] width:35 height:15
		button btn_makefit "Make Range Fit" pos:[44,206] width:84 height:17
		
		GroupBox grp10 "APR Scale Correction" pos:[4,227] width:162 height:43
		spinner spn_aprX "X" pos:[14,246] width:44 height:16 range:[0,2,1] type:#float
		spinner spn_aprY "Y" pos:[67,246] width:44 height:16 range:[0,2,1] type:#float
		spinner spn_aprZ "Z" pos:[121,246] width:44 height:16 range:[0,2,1] type:#float
		
		groupBox grp_animtracks "Animation Tracks" pos:[4,270] width:162 height:44
		button btn_animtracks "Setup Tracks" pos:[19,288] width:133 height:19
		
		GroupBox grp17 "Settings" pos:[4,314] width:162 height:130
		checkbox chk_highquality "High Quality" pos:[18,333] width:128 height:16
		checkbox chk_additive "Additive Animation" pos:[18,351] width:133 height:17
		checkbox chk_override "Override Animation" pos:[18,369] width:133 height:17
		checkbox chk_gad "GAD Animation" pos:[18,387] width:133 height:17
		checkbox chk_gaden "With Entrance GAD" pos:[18,405] width:134 height:15
		checkbox chk_gadheight "GAD follows height" pos:[18,423] width:134 height:17
		
		button btn_append "Append" pos:[18,450] width:60 height:19
		button btn_remove "Remove" pos:[91,450] width:60 height:19
		
		local CurrentSeqIndex = 0
		local AllSeqs = #()
		
		fn FixSequenceName InString = (
			local OutString = copy InString
			for i = 1 to OutString.count do if OutString[i] == " " then OutString = replace OutString i 1 "_"
			if OutString.count == 0 then OutString = ("unnamed_animation"+(random 1 99) as string)
			OutString
		)
		
		fn UpdateRollout = (
			CurrentSeqIndex = ddl_names.selection
			if CurrentSeqIndex <= SequenceName.count then	--read values
			(
				lbl_stored.text = ""
				btn_append.enabled = false; btn_remove.enabled = true
				rdo_pivot.enabled = spn_start.enabled = spn_end.enabled = btn_start.enabled = btn_end.enabled = btn_makefit.enabled = spn_aprX.enabled = spn_aprY.enabled = spn_aprZ.enabled = chk_highquality.enabled = chk_additive.enabled = chk_override.enabled = chk_gad.enabled = chk_gaden.enabled = chk_gadheight.enabled = btn_animtracks.enabled = true
				rdo_pivot.state = SequencePivot[CurrentSeqIndex]
				spn_start.value = SequenceStart[CurrentSeqIndex]
				spn_end.value = SequenceEnd[CurrentSeqIndex]
				spn_aprX.value = SequenceAPRScale[CurrentSeqIndex].X
				spn_aprY.value = SequenceAPRScale[CurrentSeqIndex].Y
				spn_aprZ.value = SequenceAPRScale[CurrentSeqIndex].Z
				chk_highquality.state = SequenceHQ[CurrentSeqIndex]
				chk_additive.state = SequenceAddv[CurrentSeqIndex]
				chk_override.state = SequenceOverride[CurrentSeqIndex]
				chk_gad.state = chk_gaden.enabled = chk_gadheight.enabled = SequenceGAD[CurrentSeqIndex]
				chk_gaden.state = SequenceGADEnt[CurrentSeqIndex]
				chk_gadheight.state = SequenceGADFH[CurrentSeqIndex]
			)
			else	--display new empty sequence
			(
				lbl_stored.text = "(not stored)"
				btn_append.enabled = true; btn_remove.enabled = false
				rdo_pivot.enabled = spn_start.enabled = spn_end.enabled = btn_start.enabled = btn_end.enabled = btn_makefit.enabled = spn_aprX.enabled = spn_aprY.enabled = spn_aprZ.enabled = chk_highquality.enabled = chk_additive.enabled = chk_override.enabled = chk_gad.enabled = chk_gaden.enabled = chk_gadheight.enabled = btn_animtracks.enabled = false
				rdo_pivot.state = 1
				spn_start.value = 1
				spn_end.value = 100
				spn_aprX.value = spn_aprY.value = spn_aprZ.value = 1.0
				chk_highquality.state = true
				chk_additive.state = chk_override.state = chk_gad.state = chk_gaden.state = chk_gadheight.state = false
			)
			edt_name.text = ddl_names.items[ddl_names.selection]
		)
		
		fn InitRollout = (
			AllSeqs = #()
			for item in SequenceName do append AllSeqs item
			append AllSeqs ("New_Seq"+(random 1 99) as string+"_Filename_here")
			ddl_names.items = AllSeqs
			ddl_names.selection = CurrentSeqIndex = AllSeqs.count	--Selecting last item, the unstored one
			btn_append.enabled = true; btn_remove.enabled = false
			edt_name.text = ddl_names.items[ddl_names.selection]
			
			--unify arrays, just to prevent crashes "in case".. Sequencename array is the ruler
			SequencePivot.count = SequenceStart.count = SequenceEnd.count = SequenceAPRScale.count = SequenceHQ.count = SequenceAddv.count = SequenceOverride.count = SequenceGAD.count = SequenceGADEnt.count = SequenceGADFH.count = SequenceName.count
			UpdateRollout()
		)
		
		fn StoreSequence add:true = (
			if add == true then		--true append
			(
				append SequenceName ddl_names.items[ddl_names.selection]
				append SequencePivot rdo_pivot.state
				append SequenceStart spn_start.value
				append SequenceEnd spn_end.value
				append SequenceAPRScale [spn_aprX.value,spn_aprY.value,spn_aprZ.value]
				append SequenceHQ chk_highquality.checked
				append SequenceAddv chk_additive.state
				append SequenceOverride chk_override.state
				append SequenceGAD chk_gad.state
				append SequenceGADEnt chk_gaden.state
				append SequenceGADFH chk_gadheight.state
			)
			if add == false and CurrentSeqIndex <= SequenceName.count then	--update on item change, if in array range
			(
				SequenceName[CurrentSeqIndex] = ddl_names.items[ddl_names.selection]
				SequencePivot[CurrentSeqIndex] = rdo_pivot.state
				SequenceStart[CurrentSeqIndex] = spn_start.value
				SequenceEnd[CurrentSeqIndex] = spn_end.value
				SequenceAPRScale[CurrentSeqIndex] = [spn_aprX.value,spn_aprY.value,spn_aprZ.value]
				SequenceHQ[CurrentSeqIndex] = chk_highquality.state
				SequenceAddv[CurrentSeqIndex] = chk_additive.state
				SequenceOverride[CurrentSeqIndex] = chk_override.state
				SequenceGAD[CurrentSeqIndex] = chk_gad.state
				SequenceGADEnt[CurrentSeqIndex] = chk_gaden.state
				SequenceGADFH[CurrentSeqIndex] = chk_gadheight.state
			)
		)
		fn RemoveSequence = (
			if CurrentSeqIndex <= SequenceName.count then	--remove if in array range
			(
				deleteItem SequenceName CurrentSeqIndex
				deleteItem SequencePivot CurrentSeqIndex
				deleteItem SequenceStart CurrentSeqIndex
				deleteItem SequenceEnd CurrentSeqIndex
				deleteItem SequenceAPRScale CurrentSeqIndex
				deleteItem SequenceHQ CurrentSeqIndex
				deleteItem SequenceAddv CurrentSeqIndex
				deleteItem SequenceOverride CurrentSeqIndex
				deleteItem SequenceGAD CurrentSeqIndex
				deleteItem SequenceGADEnt CurrentSeqIndex
				deleteItem SequenceGADFH CurrentSeqIndex
				
				--removes all the settings for that sequence
				--moves up settings linked beyond the old sequence (1 higher)
				for i = AnimationNodeSeqLink.count to 1 by -1 do
				(
					if AnimationNodeSeqLink[i] == CurrentSeqIndex then
					(
						deleteitem AnimationNode i
						deleteitem AnimationNodeSeqLink i
						deleteitem AnimationNodeSetting i
					)
					else if AnimationNodeSeqLink[i] > CurrentSeqIndex then AnimationNodeSeqLink[i] -= 1
				)
				for i = AnimationGOBSeqLink.count to 1 by -1 do
				(
					if AnimationGOBSeqLink[i] == CurrentSeqindex then
					(
						deleteitem AnimationGOBSeqLink i
						deleteitem AnimationGOBSetting i
					)
					else if AnimationGOBSeqLink[i] > CurrentSeqIndex then AnimationGOBSeqLink[i] -= 1
				)
			)
		)
		
		fn checkstart = (
			if spn_start.value < 1 then spn_start.value = 1
			if spn_start.value >= spn_end.value then (MessageBox ("Start must be less the End Frame"); spn_start.value = spn_end.value - 1)
		)
		fn checkend = (
			if spn_end.value < 1 then spn_end.value = 1
			if spn_start.value >= spn_end.value then (MessageBox ("End must be greater the Starting Frame"); spn_end.value = spn_start.value + 1)
		)
		
		on ddl_names selected sel do UpdateRollout()
		on edt_name changed text do
		(
			local ArrCopy = ddl_names.items; ArrCopy[ddl_names.selection] = (FixSequenceName text); ddl_names.items = ArrCopy
			StoreSequence add:false
		)
		
		on rdo_pivot changed state do StoreSequence add:false
		on spn_start changed val do (checkstart(); StoreSequence add:false)
		on spn_end changed val do (checkend(); StoreSequence add:false)
		on btn_start pressed do (spn_start.value = sliderTime.frame as integer; checkstart(); StoreSequence add:false)
		on btn_end pressed do (spn_end.value = sliderTime.frame as integer; checkend(); StoreSequence add:false)
		on btn_makefit pressed do (spn_start.value = animationrange.start.frame as integer; spn_end.value = animationrange.end.frame as integer; StoreSequence add:false)
		
		on spn_aprX changed val do StoreSequence add:false
		on spn_aprY changed val do StoreSequence add:false
		on spn_aprZ changed val do StoreSequence add:false
		
		on btn_animtracks pressed do
		(
			DAO_GOBUtil.RemoteGOB = Selection[1]
			DAO_GOBUtil.RemoteSeq = CurrentSeqIndex
			filein "DAOTools\\DAOTrackEdit.ms"
		)
		
		on chk_highquality changed state do StoreSequence add:false
		on chk_additive changed state do
		(
			if state == true then
			(
				chk_override.checked = chk_gad.checked = chk_gaden.checked = chk_gaden.enabled = chk_gadheight.checked = chk_gadheight.enabled = false
			)
			StoreSequence add:false
		)
		on chk_override changed state do
		(
			if state == true then
			(
				chk_additive.checked = chk_gad.checked = chk_gaden.checked = chk_gaden.enabled = chk_gadheight.checked = chk_gadheight.enabled = false
			)
			StoreSequence add:false
		)
		on chk_gad changed state do
		(
			if state == true then
			(
				chk_additive.checked = chk_override.checked = false
				chk_gaden.enabled = chk_gadheight.enabled = true
			)
			if state == false then
			(
				chk_gaden.checked = chk_gaden.enabled = chk_gadheight.checked = chk_gadheight.enabled = false
			)
			StoreSequence add:false
		)
		on chk_gaden changed state do StoreSequence add:false
		on chk_gadheight changed state do StoreSequence add:false
		
		on btn_append pressed do (StoreSequence add:true; InitRollout())
		on btn_remove pressed do (RemoveSequence();	InitRollout())
		on AnimSequencesRollout open do InitRollout()
	)
)
plugin Helper DAO_GameObject_Create
name:"Model Base"
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
						obj = DAO_GameObject pos:[0,0,0] --gridpoint
						select obj
						obj.name = (if (getnodebyname "GOB" exact:true ignorecase:false all:false) == undefined then "GOB" else (uniquename "GOB_Another"))
						--obj.pos = gridpoint
					)
				)
				2: (
					if obj.size < 0.3 then (delete obj; select oldselection)
					#stop
				)
			)
		)
		on mouseMove click do
		(
			case click of
			(
				2:(
					local size = (abs gridDist.z)
					obj.size = copy size
					obj.delegate.boxsize = [size,size,size*0.5]
				)
			)
		)
	)
)