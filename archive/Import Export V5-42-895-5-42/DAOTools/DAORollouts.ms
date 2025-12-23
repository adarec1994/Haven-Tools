/**********************************************************************************************************
Dragon Age Origins Animation Export
Author: Eshme  http://social.bioware.com/project/2336/

last edited
9 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Include Script

************************************************************************************************************
*/

	customBoneParams = attributes custBoneParams
	(
		parameters main rollout:params
		(
			--****These parameters should stay to allow access by the Exporter****
			--BoneParamAct type:#boolean ui:BoneParamAct default:false
			BoneParamIndex type:#integer ui:spn_BoneParamIndex animatable:false default:-1

			--******These are custom Parameters to add internal funtionality, should not need to be accessed by external*****---
			genuine type:#boolean animatable:false default:false	--Custom accessed by Model Importer, to yield a warning when being edited
			OldBoneIndex type:#integer animatable:false default:-1	--Remembers index state, for when submit was canceled
		)
		rollout params "Dragon Age Node" width:165 height:87
		(
			GroupBox grp1 "Bone Setup:" pos:[4,3] width:157 height:79
			spinner spn_BoneParamIndex "Index:" pos:[22,26] width:100 height:16 range:[-1,10000,-1] type:#integer
			
			label lbl4 "(-1 = default, 0... = skinnable)" pos:[13,49] width:139 height:13
			label lbl2 "First unused:" pos:[12,64] width:83 height:13
			label lbl_free "" pos:[100,64] width:48 height:13
			
			fn NodeP_GetChildren theobj = (
				Objarray = #()
				for obj in theobj do
				(
					append Objarray obj
					join ObjArray (NodeP_GetChildren obj.children)
				)
				Objarray
			)
			
			fn NodeP_GetHierarchy obj = (
				local rootobj = obj
				while rootobj.parent != undefined do rootobj = rootobj.parent
				local Childs = NodeP_GetChildren #(rootobj)
				Childs
			)
			
			fn NodeP_GetCatDef obj = (
				--Returns an array, telling which params are on an object. Custom build for internal Node Parameter functionality.
				local CAT_Defs = CustAttributes.getdefs obj
				local OutDef = #()
				if CAT_Defs != undefined then
				(
					for def in CAT_Defs do
					(
						if def.name == #custBoneParams then append OutDef #Node
						if def.name == #custCollParams then append OutDef #Collision
						if def.name == #custMeshParams then append OutDef #Mesh
					)
				)
				OutDef
			)
			
			fn NodeP_GetFreeIndex = (
				local objs = NodeP_GetHierarchy selection[1]
				local Indices = #{}
				for i = 1 to objs.count do
				(
					local CAT_Defs = NodeP_GetCatDef objs[i]
					if (finditem CAT_Defs #Node) > 0 and objs[i].custBoneParams.BoneParamIndex >= 0 then
					(
						append Indices (objs[i].custBoneParams.BoneParamIndex + 1)	--add 1, index is 0 based but bitarray is 1 based
					)
				)
				Indices.count += 1	--extend bitarray by one zero value, to serve as the highest possible free index
				local FreeIndex = ((-Indices) as array)[1] - 1	--remove 1 again. Index is 0 based, bitarray 1 based.
				FreeIndex
			)
			
			fn NodeP_ResolveIndexConflict NewIndex = (
				local objs = NodeP_GetHierarchy selection[1]
				local Warning = ""
				
				for i = 1 to objs.count do
				(
					local CAT_Defs = NodeP_GetCatDef objs[i]
					
					if (finditem CAT_Defs #Node) > 0 and objs[i] != selection[1] and NewIndex != -1 then	--For each object which has the params (instead self) do this
					(
						if objs[i].custBoneParams.BoneParamIndex == NewIndex and NewIndex == OldBoneIndex then
						(
							--This case happens, when both bones share an Index to begin with. When cloning a bone for example. Switching the offender!
							Warning += "Warning!\n\nBoneindex is conflicting between multiple Bones, where Indices must be unique. (Clone?)\n"
							Warning += "Index: \""+(NewIndex as string)+"\"\nConflict with Bone: \""+Objs[i].name+"\"\n\n"
							Warning += "Automatically picking a new Index now. Click OK"
							MessageBox (Warning) title:"Node Parameters"
							BoneParamIndex = OldBoneIndex = NodeP_GetFreeIndex()
							genuine = false
						)
						else if objs[i].custBoneParams.BoneParamIndex == NewIndex then
						(
							--This case happens, when both bones share an Index, but the Index of the selected one was just changed. Swapping!
							Warning += "Index \""+NewIndex as string+"\" was already used. Boneindices must be unique per model.\n"
							Warning += "Swapping indices to avoid conflicts..\n\n"
							Warning += "\""+selection[1].name+"\": "+NewIndex as string+"\n"
							if OldBoneIndex == -1 then Warning += "\""+Objs[i].name+"\": "+OldBoneIndex as string+" (inactive)\n\n"
							if OldBoneIndex > -1 then Warning += "\""+Objs[i].name+"\": "+OldBoneIndex as string+"\n\n"
							Warning += "Continue?"
							
							if (QueryBox (Warning) title:"Node Parameters") then
							(
								objs[i].custBoneParams.BoneParamIndex = objs[i].custBoneParams.OldBoneIndex = OldBoneIndex
								objs[i].custBoneParams.genuine = false
								OldBoneIndex = BoneParamIndex.value
							)
							else
							(
								BoneParamIndex = OldBoneIndex
							)
						)
					)
				)
			)
			
			fn NodeP_UnlockParam = (
				local Warning = ""
				Warning += "Warning!! \n\nThe Model Importer has locked these fields from editting.\n\n"
				Warning += "The settings were applied automatically from the models data,\n"
				Warning += "and should never be changed when using this Skeleton for the same Creaturetype,\n"
				Warning += "otherwise bugs in models are imminent. \n\nParameters were set with Model Importer.\n\n"
				Warning += "Do you really want to continue?"
				
				if QueryBox (Warning) Title:"Node Parameters" then genuine = false
			)
			
			fn NodeP_UpdateDialog = (
				--Check if Params is unique on an object and warn. Just a precaution ,may not happen in all max versions..
				local CAT_Defs = NodeP_GetCatDef selection[1]
				if CAT_Defs.count > 1 then messageBox ("OOOps. Somehow 2 sets of Dragon Age Parameters made its way onto this Object!\nThis must not happen.\nUndo what you just done, or use the Model Manager to delete the excess Parameters.\n\nQuick tip: Undo; Avoid collapsing Skinmod with bones in it")
				--Show a free and unused index to the user
				lbl_free.caption = (NodeP_GetFreeIndex()) as string
				--Initiating Old Index save. Helps during switching and conflict solving.
				OldBoneIndex = BoneParamIndex
				--checking uniqueness, where no value could have been changed. This happens when cloning or something like that.
				NodeP_ResolveIndexConflict BoneParamIndex
			)
			
			on params open do NodeP_UpdateDialog()
			
			on spn_BoneParamIndex changed value do	--Bone Index change handler
			(
				if genuine == true then NodeP_UnlockParam()
				if genuine == true then BoneParamIndex = OldBoneIndex		--still true, was user abort
				if genuine == false then NodeP_ResolveIndexConflict value	--apply changes thru conflictsolver
				NodeP_UpdateDialog()
			)
		)
	)
	
	customCollisionParams = attributes custCollParams -- custom parameters for collision objects
	(
		parameters main rollout:params
		(
			Type type:#integer ui:Type animatable:false default:1
			Walkable type:#integer ui:Walkable animatable:false default:1
			Mask type:#integer ui:Mask animatable:false default:1
		)
		rollout params "Dragon Age Collision"
		(
			dropdownlist Type "Collision Object Type:" items:#("Kinematic","Static")
			dropdownlist Walkable "Walkable State:" items:#("Walkable","NonWalkable")
			dropdownlist Mask "Collision Mask:" items:#("None","Item","Creature","Placeable","Static Geometry","Trigger","Terrain Wall","Water","Effects","WayPoint")
			
			on Type selected NewType do		--Setting all brethren to equal type
			(
				local objs
				if selection[1].parent != undefined then objs = selection[1].parent.children else objs = #(selection[1])
				for obj in objs do
				(
					CAT_Defs = CustAttributes.getdefs obj
					local res = false
					if CAT_Defs != undefined then 
					(	
						for def in CAT_Defs where not res do res = (def.name == #custCollParams)
					)
					if res == true then obj.custCollParams.Type = NewType
				)
			)
		)
	)
	
	customMeshParams = attributes custMeshParams
	(
		parameters main rollout:params
		(
			NodeID type:#string default:""
			CastRuntimeShadow type:#boolean ui:CastRuntimeShadow animatable:false default:false
			CastBakedShadow type:#boolean ui:CastBakedShadow animatable:false default:false
			ReceiveRuntimeShadow type:#boolean ui:ReceiveRuntimeShadow animatable:false default:false
			ReceiveBakedShadow type:#boolean ui:ReceiveBakedShadow animatable:false default:false
			CutAway type:#boolean ui:CutAway animatable:false default:false
			PunchThrough type:#boolean ui:PunchThrough animatable:false default:false
		)
		
		rollout params "Dragon Age Mesh Object" width:165 height:131
		(
			checkbox CastRuntimeShadow "Cast Runtime Shadow" pos:[13,8] width:129 height:15 type:#boolean
			checkbox CastBakedShadow "Cast Baked Shadow" pos:[13,28] width:121 height:15 type:#boolean
			checkbox ReceiveRuntimeShadow "Receive Runtime Shadow" pos:[13,48] width:148 height:15 type:#boolean
			checkbox ReceiveBakedShadow "Receive Baked Shadow" pos:[13,68] width:140 height:15 type:#boolean
			checkbox CutAway "Cut Away" pos:[13,88] width:69 height:15 type:#boolean
			checkbox PunchThrough "Punch Through" pos:[13,108] width:98 height:15 type:#boolean
		)
	)

	rollout ProgressDialog "Progress" width:440 height:52
	(
		progressBar BarBig "" pos:[10,5] width:420 height:14 color:(color 250 50 90)
		progressBar BarSmall "" pos:[10,23] width:420 height:7 color:(color 250 50 90)
		label Label "" pos:[10,35] width:419 height:13
	)
	
	fn GetBoneParams obj = (		--Checks for Params, returns index. 0 if no params
		CAT_Defs = CustAttributes.getdefs obj
		local res = false
		local CATindex = 0
		if CAT_Defs != undefined then
		(
			for def in CAT_Defs where not res do (
				res = (def.name == #custBoneParams)
				CATindex = CATindex + 1
			)
		)
		if res == true then CATIndex else 0
	)
	
	fn GetCollParams obj = (		--Checks for Params, returns index. 0 if no params
		CAT_Defs = CustAttributes.getdefs obj
		local res = false
		local CATindex = 0
		if CAT_Defs != undefined then
		(
			for def in CAT_Defs where not res do (
				res = (def.name == #custCollParams)
				CATindex = CATindex + 1
			)
		)
		if res == true then CATIndex else 0
	)
	
	fn GetMeshParams obj = (		--Checks for Params, returns index. 0 if no params
		CAT_Defs = CustAttributes.getdefs obj
		local res = false
		local CATindex = 0
		if CAT_Defs != undefined then
		(
			for def in CAT_Defs where not res do (
				res = (def.name == #custMeshParams)
				CATindex = CATindex + 1
			)
		)
		if res == true then CATIndex else 0
	)
	
	fn UpdateParams Obj = (
		local Recover = #()
		if (GetBoneParams Obj > 0) then
		(
			try(Recover[1] = Obj.custBoneParams.BoneParamIndex)catch(Recover[1]=undefined)
			try(Recover[2] = Obj.custBoneParams.genuine)catch(Recover[2]=undefined)
			while (GetBoneParams Obj) > 0 do custAttributes.delete Obj (GetBoneParams Obj)
			custAttributes.add Obj customBoneParams
			if Recover[1] != undefined and (Classof Recover[1] == integer) then Obj.custBoneParams.BoneParamIndex = Recover[1]
			if Recover[2] != undefined and (Classof Recover[2] == BooleanClass) then Obj.custBoneParams.genuine = Recover[2]
		)
		if (GetCollParams Obj > 0) then
		(
			try(Recover[1] = Obj.custCollParams.Type)catch(Recover[1]=undefined)
			try(Recover[2] = Obj.custCollParams.Walkable)catch(Recover[2]=undefined)
			try(Recover[3] = Obj.custCollParams.Mask)catch(Recover[3]=undefined)
			while (GetCollParams Obj) > 0 do custAttributes.delete Obj (GetCollParams Obj)
			custAttributes.add Obj customCollisionParams
			if Recover[1] != undefined and (Classof Recover[1] == integer) then Obj.custCollParams.Type = Recover[1]
			if Recover[2] != undefined and (Classof Recover[2] == integer) then Obj.custCollParams.Walkable = Recover[2]
			if Recover[3] != undefined and (Classof Recover[3] == integer) then Obj.custCollParams.Mask = Recover[3]
		)
		if (GetMeshParams Obj > 0) then
		(
			try(Recover[2] = Obj.custMeshParams.NodeID)catch(Recover[2] = undefined)
			try(Recover[3] = Obj.custMeshParams.CastRuntimeShadow)catch(Recover[3] = undefined)
			try(Recover[4] = Obj.custMeshParams.CastBakedShadow)catch(Recover[4] = undefined)
			try(Recover[5] = Obj.custMeshParams.ReceiveRuntimeShadow)catch(Recover[5] = undefined)
			try(Recover[6] = Obj.custMeshParams.ReceiveBakedShadow)catch(Recover[6] = undefined)
			try(Recover[7] = Obj.custMeshParams.CutAway)catch(Recover[7] = undefined)
			try(Recover[8] = Obj.custMeshParams.PunchThrough)catch(Recover[8] = undefined)
			while (GetMeshParams Obj) > 0 do custAttributes.delete Obj (GetMeshParams Obj)
			custAttributes.add Obj customMeshParams
			if Recover[2] != undefined and (Classof Recover[2] == String) then Obj.custMeshParams.NodeID = Recover[2] else Obj.custMeshParams.NodeID = RandomNodeIDString()
			if Recover[3] != undefined and (Classof Recover[3] == BooleanClass) then Obj.custMeshParams.CastRuntimeShadow = Recover[3]
			if Recover[4] != undefined and (Classof Recover[4] == BooleanClass) then Obj.custMeshParams.CastBakedShadow = Recover[4]
			if Recover[5] != undefined and (Classof Recover[5] == BooleanClass) then Obj.custMeshParams.ReceiveRuntimeShadow = Recover[5]
			if Recover[6] != undefined and (Classof Recover[6] == BooleanClass) then Obj.custMeshParams.ReceiveBakedShadow = Recover[6]
			if Recover[7] != undefined and (Classof Recover[7] == BooleanClass) then Obj.custMeshParams.CutAway = Recover[7]
			if Recover[8] != undefined and (Classof Recover[8] == BooleanClass) then Obj.custMeshParams.PunchThrough = Recover[8]
		)
	)	
	
	fn IsBoneParamsUptoDate Obj = (
		--Testing if all Parameters are readable. Function requires to be held up to date as well
		--I assume this reduces all the doubts when it comes to parameters, when the peeps cannot see anything
		local TestOK = true
		local T
		try	--these must be present
		(
			T = Obj.BoneParamIndex
		)
		catch
		(
			TestOK = false	--failed
		)
		try	--these mustnt be present
		(
			T = Obj.CrustParamHookID
			TestOK = false
		)
		catch()
		TestOK
	)
	fn IsMeshParamsUptoDate Obj = (
		--Testing Mesh Parameters if they are readable
		local TestOK = true
		local T
		try
		(
			T = Obj.NodeID
			T = Obj.CastRuntimeShadow
			T = Obj.CastBakedShadow
			T = Obj.ReceiveRuntimeShadow
			T = Obj.ReceiveBakedShadow
			T = Obj.CutAway
			T = Obj.PunchThrough
		)
		catch
		(
			TestOK = false	--failed
		)
		TestOK
	)
	fn IsCollParamsUptoDate Obj = (
		--Testing Collision Parameters if they are readable
		local TestOK = true
		local T
		try
		(
			T = Obj.Type
			T = Obj.Walkable
			T = Obj.Mask
		)
		catch
		(
			TestOK = false	--failed
		)
		TestOK
	)