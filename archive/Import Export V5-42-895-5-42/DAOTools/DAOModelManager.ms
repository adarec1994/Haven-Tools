/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edited:
13 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Model Manager Script
************************************************
*/

struct DAOModelManagerStruct
(
	--MenuSettings
	--************
	UI_Pos,
	UI_Height
)
try (DestroyDialog DAOModelManagerRollout)catch()
global DAOModelManagerRollout
global DAOModelMan
(

	include "DAOTools\\DAORollouts.ms"
	include "DAOTools\\DAOcommon.ms"
	
	fn Initialize = (
		DAOModelMan = DAOModelManagerStruct UI_Height:437
		if ((maxversion())[1] >= 7000) then DAOModelMan.UI_Pos = getMAXWindowPos() + (getMAXWindowSize()/4) else DAOModelMan.UI_Pos = [100,100]
		try(DAOModelMan.UI_Pos.x = (DAOTools.ReadIni "ModelManager" "UI_Pos_X") as integer)catch()
		try(DAOModelMan.UI_Pos.y = (DAOTools.ReadIni "ModelManager" "UI_Pos_Y") as integer)catch()
		try(DAOModelMan.UI_Height = (DAOTools.ReadIni "ModelManager" "UI_Height") as integer)catch()
	)
	
	fn RemParams Objs = (
		--***********************************************************
		--Removing any of the Bioware Parameters from a set of Objects
		--**********************************************************
		for Obj in Objs do
		(
			while (GetBoneParams Obj) > 0 do custAttributes.delete Obj (GetBoneParams Obj)
			while (GetCollParams Obj) > 0 do custAttributes.delete Obj (GetCollParams Obj)
			while (GetMeshParams Obj) > 0 do custAttributes.delete Obj (GetMeshParams Obj)
		)
	)
	
	fn AddParams Objs Params = (
		--**********************************************
		--Adding Parameters to a set of Objects.
		--Updating, or switching if already present
		--Params: 1 = "Node", 2 = "Collision", 3 = "Mesh"
		--**********************************************
		local InvalidObj = false
		
		for Obj in Objs do
		(
			InvalidObj = false
			case Params of
			(
				1:(
					if (GetBoneParams Obj) > 0 then UpdateParams Obj else
					(
						InvalidObj = (classof Obj) == DAO_Weapontrail or \
									 (classof Obj) == DAO_GameObject or \
									 (classof Obj) == DAO_Crust or \
									 (classof Obj) == DAO_UserPoint or \
									 (classof Obj) == DAO_SnapPoint or \
									 (GetModifier Obj DAOCloth) != 0
						if InvalidObj then (MessageBox ("Object \""+Obj.name+"\" is of Type \""+(Classof Obj) as string+"\"\n\nSupported Node Objects are anything except any special DAO object class.\n\nAborting...") title:"Parameter Setup")
						if not InvalidObj then
						(
							RemParams #(Obj)
							custAttributes.add Obj customBoneParams
						)
					)
				)
				2:(
					if (GetCollParams Obj) > 0 then UpdateParams Obj else
					(
						InvalidObj =((classof Obj) != Box and \
					   				 (classof Obj) != Sphere and \
					   				 (classof Obj) != Capsule and \
					   				 (classof Obj) != Cylinder and \
									 (superclassof Obj) != GeometryClass) or \
									 (classof Obj) == DAO_Weapontrail or \
									 (classof Obj) == DAO_GameObject or \
									 (classof Obj) == DAO_Crust or \
									 (classof Obj) == DAO_UserPoint or \
									 (classof Obj) == DAO_SnapPoint or \
									 (GetModifier Obj DAOCloth) != 0
						if InvalidObj then (MessageBox ("Object \""+Obj.name+"\" is of Type \""+(Classof Obj) as string+"\"\n\nSupported Collision Objects are:\nBox,Sphere,Capsule,Cylinder, Editable Poly,Editable Mesh.\n\nAborting...") title:"Parameter Setup")
						if not InvalidObj then
						(
							RemParams #(Obj)
							custAttributes.add Obj customCollisionParams
						)
					)
				)
				3:(
					if (GetMeshParams Obj) > 0 then UpdateParams Obj else
					(
						InvalidObj = (superclassof Obj) != GeometryClass or \
									 (classof Obj) == DAO_Weapontrail or \
									 (classof Obj) == DAO_GameObject or \
									 (classof Obj) == DAO_Crust or \
									 (classof Obj) == DAO_UserPoint or \
									 (classof Obj) == DAO_SnapPoint or \
									 (GetModifier Obj DAOCloth) != 0
						if InvalidObj then (MessageBox ("Object \""+Obj.name+"\" is of Type \""+(Classof Obj) as string+"\"\n\nSupported Mesh Objects are:\nEditable Poly,Editable Mesh.\n\nAborting...") title:"Parameter Setup")
						if not InvalidObj then
						(
							RemParams #(Obj)
							custAttributes.add Obj customMeshParams
							--Obj.custMeshParams.NodeName = Obj.name	--obsolete with new gameobject class
							Obj.custMeshParams.NodeID = RandomNodeIDString()
						)
					)
				)
			)
		)
	)
	
	fn UpdateAllParams RootObj = (
		--************************************************
		--Batch Updating all Objects. GOB is object sent
		--***********************************************
		local Objs = GetHierarchy #(RootObj)
		local IsOldNodes = false
		local OldNodes = #()
		local DoUpdate = true
		local SkipOldNodes = false
		
		--Testing for old Crust/Snap/Userpoints, that will now loose their properties due updates. These params dont exist anymore
		for Obj in Objs do
		(
			if (GetBoneParams Obj) > 0 then try
			(
				if Obj.CrustParamAct == true or Obj.UsepParamAct == true or Obj.SnapParamAct == true then
				(
					IsOldNodes = true
					append OldNodes Obj
				)
			)
			catch()
		)
		
		if IsOldNodes == true then
		(
			local Message = ""
			Message += "Warning!! Non updateable Parameters found\n\n"
			Message += "Due Updates after v5.30, Crust,Snappoints and Usepoints do not require Parameters.\n"
			Message += "If updating them, the Object will be exchanged instead and resetted (looses Animation etc).\n\n"
			Message += "Press <yes> to update all. (recommended but SAVE YOUR WORK!)\n"
			Message += "Press <no> to update all except those 3. (Exporter is kept backwards compatible for now)\n"
			Message += "Press <cancel> to abort."
			
			local Answer = yesNoCancelBox (Message) title:"Model Manager"
			
			if Answer == #cancel then DoUpdate = false
			if Answer == #no then SkipOldNodes = true
			if Answer == #yes then
			(
				if DAO_Crust == undefined or DAO_UserPoint == undefined or DAO_SnapPoint == undefined then
				(
					MessageBox ("New Crust/Snap/Userpoint Helpers not installed. Check Installation!\n\nAbort Update..") title:"Model Manager"
					DoUpdate = false
				)
				else
				(
					for Obj in Objs do with animate off
					(
						local SwitchThis = undefined
						local NewObj
						
						if (GetBoneParams Obj) > 0 then try
						(
							if Obj.CrustParamAct == true then SwitchThis = #Crust
							if Obj.UsepParamAct == true then SwitchThis = #Usep
							if Obj.SnapParamAct == true then SwitchThis = #Snap
						)catch()
						
						if SwitchThis == #Crust then
						(
							NewObj = DAO_Crust position:[0,0,0] Name:Obj.Name
							NewObj.Size = 0.1 * DAOTools.WorldScale
							NewObj.HookID = Obj.CrustParamHookID
						)
						if SwitchThis == #Usep then
						(
							NewObj = DAO_UserPoint position:[0,0,0] Name:Obj.Name
							NewObj.Size = 0.3 * DAOTools.WorldScale
						)
						if SwitchThis == #Snap then
						(
							NewObj = DAO_SnapPoint position:[0,0,0] Name:Obj.Name
							NewObj.Size = 0.3 * DAOTools.WorldScale
						)
						if SwitchThis == #Crust or SwitchThis == #Usep or SwitchThis == #Snap then
						(
							NewObj.transform = Obj.transform
							NewObj.parent = Obj.parent
							for Child in Obj.children do Child.parent = NewObj
							try(delete obj)catch()
						)
					)
				)
			)
		)
		
		--Plain Param Update here
		if DoUpdate == true then
		(
			Objs = GetHierarchy #(RootObj)	--must recollect, because objects may have changed.
			for Obj in Objs do
			(
				if SkipOldNodes == true then
				(
					if (finditem OldNodes Obj) == 0 then UpdateParams Obj
				)
				else
				(
					UpdateParams Obj
				)
			)
			MessageBox ("Done updating Parameters!") title:"Model Manager"
		)
	)
	
	rollout ModelInfoDialog "Model Statistics" width:471 height:536
	(
		GroupBox grp_gobs "Available Game Objects:" pos:[6,1] width:199 height:45
		dropdownList ddl_gobs "" pos:[16,19] width:182 height:21 --items:#("GOB_Secondary01")
		label lbl28 "Nothing here yet!" pos:[7,54] width:279 height:19
	)
	
	rollout ParamSetupDialog "Parameter Setup" width:471 height:536
	(
		GroupBox grp_gobs "Available Model Root GOBs:" pos:[6,1] width:199 height:45
		dropdownList ddl_gobs "" pos:[16,19] width:182 height:21 --items:#("GOB_Secondary01")
		
		groupBox grp3 "Edit Parameters" pos:[209,1] width:256 height:86
		dropDownList ddl_params "" pos:[215,19] width:111 height:21 items:#("Node", "Collision", "Mesh")
		button btn_addparams "Add / Replace" pos:[221,52] width:100 height:25
		button btn_remparams "Remove" pos:[349,52] width:100 height:25
		button btn_updparams "Update All" pos:[349,18] width:100 height:25
		
		groupBox grp5 "Filter" pos:[6,49] width:195 height:38
		checkbox chk_FilterNode "Node" pos:[15,65] width:47 height:18 checked:true
		checkbox chk_FilterMesh "Mesh" pos:[74,65] width:53 height:15 checked:true
		checkbox chk_FilterColl "Collision" pos:[136,65] width:62 height:14 checked:true
		
		multilistbox lbx_hierarchy "" pos:[5,95] width:460 height:16
		
		local GOBChilds = #()	--Should contain Nodes, correspond 1:1 to current displayed hierarchy
		
		fn CollectGOBHierarchy Children Depth = (
			local item

			If Children.count > 0 then for Obj in Children do
			(
				item = (Depth+Obj.name)
				for i = 1 to ((270-(Depth.count*3)-(GetTextExtent Obj.name).x)/3) do item += " "
				if GetBoneParams Obj >0 then
				(
					item += "\"Node\" (Parameters)"
					if not (IsBoneParamsUptoDate Obj) then item += "  (Outdated)"
					if chk_FilterNode.checked then
					(
						append lbx_hierarchy.items item
						append GOBChilds Obj
					)
				)
				if GetCollParams Obj >0 then
				(
					item += "\"Collision\" (Parameters)"
					if not (IsCollParamsUptoDate Obj) then item += "  (Outdated)"
					if chk_FilterColl.checked then
					(
						append lbx_hierarchy.items item
						append GOBChilds Obj
					)
				)
				if GetMeshParams Obj >0 then
				(
					item += "\"Mesh\" (Parameters)"
					if not (IsMeshParamsUptoDate Obj) then item += "  (Outdated)"
					if chk_FilterMesh.checked then
					(
						append lbx_hierarchy.items item
						append GOBChilds Obj
					)
				)
				if (Classof Obj) == DAO_Weapontrail then
				(
					item += "Weapontrail Object"
					append lbx_hierarchy.items item
					append GOBChilds Obj
				)
				if (Classof Obj) == DAO_GameObject then
				(
					item += "Model Base"
					append lbx_hierarchy.items item
					append GOBChilds Obj
				)
				if (Classof Obj) == DAO_Crust then
				(
					item += "Crust Object"
					append lbx_hierarchy.items item
					append GOBChilds Obj
				)
				if (Classof Obj) == DAO_UserPoint then
				(
					item += "UserPoint Object"
					append lbx_hierarchy.items item
					append GOBChilds Obj
				)
				if (Classof Obj) == DAO_SnapPoint then
				(
					item += "SnapPoint Object"
					append lbx_hierarchy.items item
					append GOBChilds Obj
				)
				if (GetModifier Obj DAOCloth) > 0 then
				(
					item += "DAO Cloth Object"
					append lbx_hierarchy.items item
					append GOBChilds Obj
				)
				if (GetBoneParams Obj == 0) and (GetCollParams Obj == 0) and (GetMeshParams Obj == 0) and ((Classof Obj) != DAO_Weapontrail) and ((Classof Obj) != DAO_GameObject) and ((Classof Obj) != DAO_Crust) and ((Classof Obj) != DAO_UserPoint) and ((Classof Obj) != DAO_SnapPoint) and ((GetModifier Obj DAOCloth) == 0) then
				(
					append lbx_hierarchy.items (item+"No Type (undefined)")
					append GOBChilds Obj
				)
				CollectGOBHierarchy Obj.children (Depth+"  ")
			)
		)
		
		fn GetGOBSelection = (
			local GOBs = DAOModelManagerRollout.GOBs
			local GOB
			if GOBs.count > 0 and ddl_gobs.selection > 0 then
			(
				GOB = (getNodeByName (GOBs[ddl_gobs.selection].ObjName + GOBs[ddl_gobs.selection].ObjExtension) exact:true ignoreCase:true all:false)
			)
			return GOB
		)

		fn UpdateHierarchy = (
			lbx_hierarchy.items = #()
			GOBChilds = #()
			
			local GOB = GetGOBSelection()
			if GOB != undefined then
			(
				CollectGOBHierarchy #(GOB) ""
				lbx_hierarchy.items = lbx_hierarchy.items
			)
			
			local SetSel = #{}
			for i = 1 to selection.count do
			(
				append SetSel (finditem GOBChilds Selection[i])
			)
			lbx_hierarchy.selection = SetSel
		)
		
		fn GetObjSelection = (
			local Sel
			local Objs = #()
			if lbx_hierarchy.selection != 0 then
			(
				Sel = lbx_hierarchy.selection as array
				for i = 1 to Sel.count do
				(
					append Objs GOBChilds[Sel[i]]
				)
			)
			return Objs
		)
		
		fn SetObjSelection = (
			DAOModelManagerRollout.SuspendUpdates = true
			try(DAOImportToolsRollout.SuspendUpdates = true)catch()
			clearSelection()
			local items = lbx_hierarchy.selection as array
			for item in items do
			(
				selectmore GOBChilds[item]
			)
			DAOModelManagerRollout.SuspendUpdates = false
			try(DAOImportToolsRollout.SuspendUpdates = false)catch()
			try(DAOImportToolsRollout.UpdateDialog())catch()
		)
		
		--********************************************************
		--********* Dialog Initiating ****************************
		--********************************************************
		
		on ParamSetupDialog open do ()

		
		on ddl_gobs selected item do UpdateHierarchy()
		on chk_FilterNode changed state do UpdateHierarchy()
		on chk_FilterMesh changed state do UpdateHierarchy()
		on chk_FilterColl changed state do UpdateHierarchy()
		
		on lbx_hierarchy selectionend do SetObjSelection()
		
		on btn_addparams pressed do
		(
			if (GetObjSelection()).count > 0 then
			(
				if (QueryBox ("Are you sure you want to add Parameters to selected Objects?\n\nThis will overwrite existing Parameters and any custom setting will be lost!!\n(including settings from imported Models)\nObjects without Parameters will NOT be able to be exported.") title:"Parameter Setup") then
				(
					with undo "Add Dragon Age Parameters" on AddParams (GetObjSelection()) ddl_params.selection
					UpdateHierarchy()
				)
			)
		)
		on btn_remparams pressed do
		(
			if (GetObjSelection()).count > 0 then
			(
				if (QueryBox ("Are you sure you want to delete Parameters from selected Objects?\n\nAny custom setting will be lost.(including settings from imported Models)\nObjects without Parameters will NOT be able to be exported.") title:"Parameter Setup") then
				(
					with undo "Remove Dragon Age Parameters" on RemParams (GetObjSelection())
					UpdateHierarchy()
				)
			)
		)
		on btn_updparams pressed do
		(
			if GetGOBSelection() != undefined then
			(
				if (QueryBox ("Are you sure you want to update Parameters on ALL Objects?\n\nObjects with outdated Parameters may NOT be able to be exported.") title:"Parameter Setup") then
				(
					DAOModelManagerRollout.SuspendUpdates = true
					with undo "Update Dragon Age Parameters" on UpdateAllParams (GetGOBSelection())
					DAOModelManagerRollout.SuspendUpdates = false
					UpdateHierarchy()
				)
			)
		)
	)
	
	rollout DAOModelManagerRollout "Dragon Age Model Manager" width:484 height:527
	(
		GroupBox grp1 "" pos:[142,-1] width:198 height:45
		label lbl1 "Model Manager" pos:[200,9] width:127 height:16
		label lbl2 "Dragon Age: Origins" pos:[191,26] width:95 height:16
		edittext txt_maxversion "" pos:[0,507] width:68 height:16 enabled:false
		subRollout rollouts "" pos:[0,49] width:484 height:454
		label lbl10 "Script by Eshme" pos:[350,507] width:85 height:16 enabled:false
		label lbl11 "" pos:[440,507] width:40 height:16 enabled:false

		local GOBs = #()
		local SuspendUpdates = false

		fn UpdateDialog = (
			if not SuspendUpdates then
			(
				GOBs = GetGobs()
				local GOBHold = #()
				for GOB in GOBs where GOB.isGOB == true do append GOBHold (GOB.ObjName+GOB.ObjExtension)
				ParamSetupDialog.ddl_gobs.items = GOBHold
				if selection[1] != undefined then ParamSetupDialog.ddl_gobs.selection = finditem GOBHold ((GetGOB selection[1]).name)
				ParamSetupDialog.UpdateHierarchy()
			)
		)
		
		on DAOModelManagerRollout open do
		(
			callbacks.addScript #selectionSetChanged "DAOModelManagerRollout.UpdateDialog()" id:#DAModManUpdateChange		--6 events sending a dialog update across
			callbacks.addScript #nodePostDelete "DAOModelManagerRollout.UpdateDialog()" id:#DAModManUpdateDelete
			callbacks.addScript #systemPostReset "DAOModelManagerRollout.UpdateDialog()" id:#DAModManUpdateReset
			callbacks.addScript #systemPostNew "DAOModelManagerRollout.UpdateDialog()" id:#DAModManUpdateNew
			callbacks.addScript #filePostOpen "DAOModelManagerRollout.UpdateDialog()" id:#DAModManUpdateFile
			callbacks.addScript #postImport "DAOModelManagerRollout.UpdateDialog()" id:#DAModManUpdateImport		

			lbl11.text = DAOTools.Version
			local maxver = maxVersion()		
			txt_maxversion.text = "MAX  v"+((maxver[1] / 1000) as string)
			if maxver[1] <= 4200 then txt_maxversion.text = " GMAX"
			Rollouts.height = DAOModelMan.UI_Height - 70
			lbl10.pos = [lbl10.pos.x,DAOModelMan.UI_Height - 17]
			lbl11.pos = [lbl11.pos.x,DAOModelMan.UI_Height - 17]
			txt_maxversion.pos = [txt_maxversion.pos.x,DAOModelMan.UI_Height - 20]
		)
		on DAOModelManagerRollout close do
		(
			callbacks.removeScripts id:#DAModManUpdateChange
			callbacks.removeScripts id:#DAModManUpdateDelete
			callbacks.removeScripts id:#DAModManUpdateReset
			callbacks.removeScripts id:#DAModManUpdateNew
			callbacks.removeScripts id:#DAModManUpdateFile
			callbacks.removeScripts id:#DAModManUpdateImport
			
			DAOTools.WriteIni "ModelManager" "UI_Pos_X" DAOModelMan.UI_Pos.x
			DAOTools.WriteIni "ModelManager" "UI_Pos_Y" DAOModelMan.UI_Pos.y
			DAOTools.WriteIni "ModelManager" "UI_Height" DAOModelMan.UI_Height
		)
		on DAOModelManagerRollout resized Size do
		(
			if DAOModelManagerRollout.open then
			(
				DAOModelMan.UI_Height = Size.y
				Rollouts.height = DAOModelMan.UI_Height - 70
				lbl10.pos = [lbl10.pos.x,DAOModelMan.UI_Height - 17]
				lbl11.pos = [lbl11.pos.x,DAOModelMan.UI_Height - 17]
				txt_maxversion.pos = [txt_maxversion.pos.x,DAOModelMan.UI_Height - 20]
			)
		)
		on DAOModelManagerRollout moved pos do DAOModelMan.UI_Pos = pos
	)
	
	on execute do with undo off
	(
		if DAOModelMan == undefined do Initialize()
		--Initialize()
		
		CreateDialog DAOModelManagerRollout style:#(#style_sysmenu,#style_titlebar,#style_minimizebox,#style_resizing) height:DAOModelMan.UI_Height lockWidth:true pos:DAOModelMan.UI_Pos
		
		AddSubRollout DAOModelManagerRollout.Rollouts ParamSetupDialog rolledup:false
		AddSubRollout DAOModelManagerRollout.Rollouts ModelInfoDialog rolledup:true
		DAOModelManagerRollout.UpdateDialog()

	)
)