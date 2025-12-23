/**********************************************************************************************************
Dragon Age Origins Tools
Author: Eshme  http://social.bioware.com/project/2336/

latest edit:
15 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Include Script
DAOcommon.ms
Common Functions
************************************************************************************************************
*/
	struct PushValue (
		--******************************************************************
		--Value conversion tools, which supports fallback default input
		--Useful when reading in GFF files
		--******************************************************************
		fn AsBool Value Default = (
			if (Classof Value) == BooleanClass then return Value
			if (Classof Value) == Integer then return (Value >= 1)
			if (Classof Value) == Float then return (Value >= 1.0)
			if (Classof Value) == String then return (Value != "0")
			return Default
		),
		fn AsInteger Value Default = (
			if (Classof Value) == Integer then return Value
			if (Classof Value) == Float then return (Value as Integer)
			if (Classof Value) == BooleanClass then return (if Value == true then 1 else 0)
			if (Classof Value) == String then
			(
				if (Classof (Value as Integer) == Integer) then return (Value as Integer)
			)
			return Default
		),
		fn AsPoint3 Value Default = (
			if (Classof Value) == Point3 then return Value
			return Default
		),
		fn AsFloat Value Default = (
			if (Classof Value) == Float then return Value
			if (Classof Value) == Integer then return (Value as Float)
			if (Classof Value) == BooleanClass then return (if Value == true then 1.0 else 0.0)
			if (Classof Value) == String then
			(
				if (Classof (Value as Float) == Float) then return (Value as Float)
			)
		)
	)
	
	fn CutString InString Cut = (
		local out = copy InString
		if (classof Cut) == String and Cut.count > 0 then
		(
			local Position = Findstring InString Cut
			if Position != undefined then out = Replace InString Position Cut.count ""
		)
		out
	)
	
	fn EqualsCaseless String1 String2 = (
		if String1.count != String2.count then return false
		for i = 1 to String1.count do
		(
			local Comp = abs((bit.charasint String1[i]) - (bit.charasint String2[i]))
			if (Comp != 0 and Comp != 32) then return false
		)
		return true
	)
	
	fn RandomNodeIDString = (
		local Rand = "QWERTZUIOPASDFGHJKLYXCVBNM123456789poiuztrwqlkjhgfdsmnbvcxya"
		local out = "_"
		for i = 1 to 3 do
		(
			out += Rand[(random 1 (Rand.count))]
		)
		out
	)
	
	fn RoundMe Number = (if Number - (floor Number) < .5 then (floor Number) as integer else (ceil Number) as integer)
	
	fn GetHierarchy theobj Limit:#None = (	--Gets all children including the one which was sent
		Objarray = #()
		--if (classof theobj) != Array and (classof theobj) != NodeChildrenArray then theobj = #(theobj)
		for obj in theobj do
		(
			if Limit == #None then append Objarray obj
			if Limit == #Mesh then if (GetMeshParams Obj > 0) and (superclassof Obj == GeometryClass) then append Objarray obj
			if Limit == #Coll then if (GetCollParams Obj > 0) then append Objarray obj
			if Limit == #Node then if (GetBoneParams Obj > 0) then append Objarray obj
			join ObjArray (GetHierarchy obj.children Limit:Limit)
		)
		Objarray
	)
	
	fn GetGOB FromObj = (	--Gets the superparent of any node ("GOB" in Bioware models)
		local obj = FromObj
		while obj.parent != undefined do obj = obj.parent
		obj
	)
	
	struct GOBStruct (
		ObjName,
		ObjExtension,
		IsGOB
	)
	
	fn GetGOBs = (	--Gets all the GOBs in the scene. Used to make them show up in the menu's
		local GOBs= #()
		
		for obj in rootnode.children do if (EqualsCaseless obj.name "GOB") then
		(
			append GOBs (GobStruct ObjName:obj.name ObjExtension:"" IsGOB:true)
		)
		for obj in rootnode.children do if not(EqualsCaseless obj.name "GOB") and matchpattern obj.name pattern:"GOB*" then
		(
			append GOBs (GobStruct ObjName:(substring obj.name 1 3) ObjExtension:(substring obj.name 4 20) IsGOB:true)
		)
		for obj in rootnode.children do if not matchpattern obj.name pattern:"GOB*" then
		(
			append GOBs (GobStruct ObjName:obj.name ObjExtension:"" IsGOB:false)
		)
		--for obj in rootnode.children do if classof obj == DAO_GameObject then		--requires lot updates in export functions
		--(
		--	append GOBs (GobStruct ObjName:obj.name ObjExtension:"" IsGOB:true)
		--)
		GOBs
	)
	
	fn GetNodeByNameInHierarchy HierarchyNode SearchNode = (
		--*******************************************************************************************
		--Searching for a Node by a String, in a fixed Hierarchy supplied by a Node in that Hierarchy
		--Case INsensitive, exact Name
		--Expecting no double
		--*********************************************************************************************
		local OutNode = undefined
		local Objs = GetHierarchy (GetGOB #(HierarchyNode))
		for Obj in Objs do
		(
			if (EqualsCaseLess (Obj.Name) (SearchNode)) == true then OutNode = Obj
		)
		OutNode
	)
	
	fn GetModifier Obj ModName = (
		--**************************************************
		--Returns modifier index, 0 if none
		--**************************************************
		local out = 0
		for i = 1 to Obj.modifiers.count where out == 0 do
		(
			if (Classof Obj.modifiers[i]) == ModName then out = i		--search by class name.
			--if Obj.modifiers[i].Name == ModName then out = i
		)
		out
	)
	
	fn AllocateMemory Size = (
		--*******************************************
		--Ensures the Heap is at least of size "Size"
		--********************************************
		local MemInc = (Size - HeapSize)
		if HeapSize + MemInc > 50000000 then MemInc = 50000000 - HeapSize	--Limit heap to 50meg
		MemInc = ((MemInc / 100000) as integer) * 100000
		if MemInc > 0 then HeapSize += MemInc
	)
	
	fn FixOutputName Input TruncateArray = (
		--**************************************************************
		--Cuts the output name of any invalid or unnessecary Characters
		--The filenames you enter in the Export output fields
		--and autogenerated Material MAO filenames
		--**************************************************************
		local Output = copy Input
		local InvalidChars = " /\:*?<>|\""
		for CutMe in TruncateArray do Output = Cutstring Output CutMe	--cutting extensions to avoid mistakes
		for i = Output.count to 1 by -1 do if Output[i] == "\\" then Output = replace Output 1 i ""	--cutting all but the filename
		for i = Output.count to 1 by -1 do if (findString InvalidChars Output[i]) != undefined then Output = replace Output i 1 "_" --replace invalid chars
		Output
	)
	
	fn GetObjectExportType Obj = (
		--********************************************************
		--Returning the export type of the object. Model type
		--Common function to unify detection and avoid errors
		--Used in export functions (ie GetModelStats, ExportMMHChilds, ExportAnimChilds..)
		--*******************************************************
		local 	out = undefined
		if 		(classof Obj) == DAO_GameObject and obj.parent == undefined		then out = #GameObject
		else if (classof Obj) == DAO_Weapontrail								then out = #Weapontrail
		else if (classof Obj) == DAO_Crust										then out = #Crust
		else if (classof Obj) == DAO_UserPoint									then out = #UserPoint
		else if (classof Obj) == DAO_SnapPoint									then out = #SnapPoint
		else if (GetModifier Obj DAO_Mod_Cloth) > 0								then out = #Cloth
		else if (GetBoneParams Obj > 0)											then out = #Node
		else if (GetMeshParams Obj > 0) and (superclassof Obj == GeometryClass) then out = #Mesh
		else if (GetCollParams Obj > 0) and (superclassof Obj == GeometryClass) then out = #Collision
		else 	out = undefined
		out
	)
	
	fn GetObjectAnimExportType Obj = (
		--********************************************************
		--Returning the export type of the object. Animation type
		--Used in export functions (ExportMMHChilds, ExportAnimChilds..)
		--Used by Animation Track Editor
		--*******************************************************
		local 	out = undefined
		if 		(classof Obj) == DAO_GameObject and obj.parent == undefined		then out = #GameObject
		else if (classof Obj) == DAO_UserPoint									then out = #UserPoint
		else if (classof Obj) == DAO_SnapPoint									then out = #SnapPoint
		else if (GetBoneParams Obj > 0)											then out = #Node
		else if (GetMeshParams Obj > 0) and (superclassof Obj == GeometryClass) and (GetModifier Obj Skin) == 0 then out = #Mesh
		else 	out = undefined
		out
	)
	
	struct DAOTrackSettingStruct
	(
		Pos,
		Rot
	)
	
	fn UnpackTrackSetting Setting = (
		--***************************************************
		--Animation Track Editor
		--Packing and Unpacking the Setting stored as Integer
		--Simple flags or whatever
		--***************************************************
		local out = DAOTrackSettingStruct ()
		local Pos = bit.and Setting 7			--bit 1&2	3 is reserved
		local Rot = (bit.and Setting 56) / 8	--bit 4&5	6 is reserved
		
		out.Pos = case Pos of
		(
			0: #Auto
			1: #Nil
			2: #Min
			3: #Full
		)
		out.Rot = case Rot of
		(
			0: #Auto
			1: #Nil
			2: #Min
			3: #Full
		)
		out
	)
	
	fn PackTrackSetting Setting = (
		--************************************************
		--Animation Track Editor
		--***********************************************
		local out = 0
		
		case Setting.Pos of
		(
			#Nil: out+=1	--Bit 1
			#Min: out+=2	--Bit 2
			#Full: out+=3	--Bit 1&2	3 is reserved
		)
		case Setting.Rot of
		(
			#Nil: out+=8	--bit 4
			#Min: out+=16	--Bit 5
			#Full: out+=24	--Bit 4&5	6 is reserved
		)
		out
	)
	
	fn GetTrackArrayIndex Self Node Sequence = (
		--****************************************************
		--Animation Track Editor
		--Finding the array index for a tracksetting
		--By Node and by Sequence
		--Returns 0 if no Index to Array exists
		--******************************************************
		local out = 0
		if Self != Node then
		(
			for i = 1 to Self.AnimationNode.count do
			(
				if Self.AnimationNode[i] == Node then
				(
					if Self.AnimationNodeSeqLink[i] == Sequence then
					(
						out = i
					)
				)
			)
		)
		if Self == Node then
		(
			for i = 1 to Self.AnimationGOBSeqLink.count do
			(
				if Self.AnimationGOBSeqLink[i] == Sequence then
				(
					out = i
				)
			)
		)
		out
	)
	
	fn GetTrackSetting Self Node Sequence = (
		--***************************************
		--Animation Track Editor
		--Return the Track Setting
		--For specific node and sequence
		--***************************************
		local out = UnpackTrackSetting 0		--default setting for any node
		local TrackIndex = (GetTrackArrayIndex Self Node Sequence)
		if TrackIndex > 0 then
		(
			if Self != Node then out = UnpackTrackSetting Self.AnimationNodeSetting[TrackIndex]
			if Self == Node then out = UnpackTrackSetting Self.AnimationGOBSetting[TrackIndex]
		)
		out
	)
	
	fn SetTrackSetting Self Node Sequence InSetting = (
		--*************************************
		--Animation Track Editor
		--Updating the Track setting for a node and sequence
		--0 setting equals (all flags off) default and is deleted from the array
		--Other settings are updated in the Array
		--Or appended if no setting existed
		--*************************************
		local Setting = PackTrackSetting InSetting
		local TrackIndex = (GetTrackArrayIndex Self Node Sequence)
		if Setting == 0 then
		(
			if TrackIndex > 0 then
			(
				if Self != Node then
				(
					deleteitem Self.AnimationNode TrackIndex
					deleteitem Self.AnimationNodeSeqLink TrackIndex
					deleteitem Self.AnimationNodeSetting TrackIndex
				)
				if Self == Node then
				(
					deleteitem Self.AnimationGOBSeqLink TrackIndex
					deleteitem Self.AnimationGOBSetting TrackIndex
				)
			)
		)
		else if TrackIndex > 0 then
		(
			if Self != Node then Self.AnimationNodeSetting[TrackIndex] = Setting
			if Self == Node then Self.AnimationGOBSetting[TrackIndex] = Setting
		)
		else
		(
			if Self != Node then
			(
				append Self.AnimationNode Node
				append Self.AnimationNodeSeqLink Sequence
				append Self.AnimationNodeSetting Setting
			)
			if Self == Node then
			(
				append Self.AnimationGOBSeqLink Sequence
				append Self.AnimationGOBSetting Setting
			)
		)
	)
	
	fn TrackGarbageCollect Self = (
		--**********************************************
		--Animation Track Editor
		--Doing some cleaning in the GOB array parameters
		--***********************************************
		Self.AnimationNodeSeqLink.count = Self.AnimationNodeSetting.count = Self.AnimationNode.count
		Self.AnimationGOBSeqLink.count = Self.AnimationGOBSetting.count
		
		local Objs = GetHierarchy #(Self)
		for i = Self.AnimationNodeSeqLink.count to 1 by -1 do
		(
			--********************************************
			--Removes deleted nodes from Trackfields
			--also Nodes which arent in hierarchy anymore
			--*********************************************
			if Self.AnimationNode[i] == undefined or (finditem Objs Self.AnimationNode[i]) == 0 then
			(
				deleteitem Self.AnimationNode i
				deleteitem Self.AnimationNodeSeqLink i
				deleteitem Self.AnimationNodeSetting i
			)
		)
	)
	
	fn TestParameters Obj = (
		--********************************************************************************************
		--Testing any Parameters are all readable and up to date.
		--This causes errors everywhere if not true. Used by Export Main functions prior exporting.
		--********************************************************************************************
		local TestOK = true
		
		if (GetBoneParams obj) > 0 then
		(
			if not (IsBoneParamsUptoDate Obj) then TestOK = false
		)
		if (GetCollParams obj) > 0 then
		(
			if not (IsCollParamsUptoDate Obj) then TestOK = false
		)
		if (GetMeshParams obj) > 0 then
		(
			if not (IsMeshParamsUptoDate Obj) then TestOK = false
		)
		TestOK
	)
	
	struct ModelStatsStruct (
		
		NodeCount = 0,
		UndefinedNodes = #(),
		UndefinedNodesChilds = #(),
		OutdatedNodes = #(),
		ScaledNodes = #(),
		NonExportableChilds = #()
	)
	
	fn GetModelStats RootNode = (
		--****************************************************************************************
		--Should be a function to collect many information on the model, of ModelStatsStruct type
		--perhaps makes it easier to find errors when there is some feedback
		--used prior model export main function
		--*****************************************************************************************
		local Stats = ModelStatsStruct()
		local Objs
		
		Objs = GetHierarchy #(RootNode)
		
		Stats.NodeCount = Objs.count
		for obj in objs do
		(
			--*****************************************************************************************
			--Collect Nodes that are Childs to Collision, no object will be exported beyond collision
			--*****************************************************************************************
			local UpChain = obj
			while UpChain.parent != undefined do
			(
				UpChain = UpChain.parent
				if (GetObjectExportType UpChain) == #Collision then		--when collision is found, above in the hierarchy
				(
					if (finditem stats.NonExportableChilds Obj) == 0 then append stats.NonExportableChilds Obj	--append if not already collected
				)
			)
			--**************************************
			--Find undefined Nodes
			--**************************************
			if (GetObjectExportType Obj) == undefined then
			(
				append stats.UndefinedNodes Obj
				local double = (finditem stats.UndefinedNodesChilds Obj)
				if double > 0 then deleteItem stats.UndefinedNodesChilds double
				
				local Childs = GetHierarchy #(Obj)
				for Child in Childs do
				(
					--********************************************************
					--This is a Child of one, and wont recursively be exported
					--********************************************************
					if (Child != Obj) and \
					   ((finditem stats.UndefinedNodes Child) == 0) and \
					   ((finditem stats.UndefinedNodesChilds Child) == 0) then
					(
						append stats.UndefinedNodesChilds Child
					)
				)
			)
			if (GetObjectExportType Obj) != undefined then
			(
				if not (TestParameters Obj) then
				(
					--**********************************
					--This Node has outdated Parameters
					--**********************************
					append stats.OutdatedNodes Obj
				)
				
				if abs(Obj.Scale.X - 1) > 0.001 or abs(Obj.Scale.Y - 1) > 0.001 or abs(Obj.Scale.Z - 1) > 0.001 then
				(
					if (GetObjectExportType Obj) != #Collision then
					(
						--****************************************************
						--The following Nodes need to be Reset Xform
						--... except Collision, which supports weird Xforms.
						--***************************************************
						append stats.ScaledNodes Obj
					)
				)
			)
		)
		return Stats
	)
	
	struct DAOMatIDStruct (
		ID,					--Holds ID<->String translation table
		IDString
	)
	
	fn GetMatTypeIDs = (	--translation table. Edit with Care!! Used by Material import/export
		local Out = #()
		append Out (DAOMatIDStruct ID:1 IDString:"Black.mat")
		append Out (DAOMatIDStruct ID:2 IDString:"Canopy.mat")
		append Out (DAOMatIDStruct ID:3 IDString:"Character.mat")
		append Out (DAOMatIDStruct ID:4 IDString:"Cloth.mat")
		append Out (DAOMatIDStruct ID:5 IDString:"Clouds.mat")
		append Out (DAOMatIDStruct ID:6 IDString:"DistortionMaskDiffuse.mat")
		append Out (DAOMatIDStruct ID:7 IDString:"DADistortionMask.mat")
		append Out (DAOMatIDStruct ID:8 IDString:"Eye.mat")
		append Out (DAOMatIDStruct ID:9 IDString:"Face.mat")
		append Out (DAOMatIDStruct ID:10 IDString:"GeneralGui.mat")
		append Out (DAOMatIDStruct ID:11 IDString:"HairAlpha.mat")
		append Out (DAOMatIDStruct ID:12 IDString:"InstancingMaterial.mat")
		append Out (DAOMatIDStruct ID:13 IDString:"Lava.mat")
		append Out (DAOMatIDStruct ID:14 IDString:"Prop.mat")
		append Out (DAOMatIDStruct ID:15 IDString:"Sky.mat")
		append Out (DAOMatIDStruct ID:16 IDString:"Static.mat")
		append Out (DAOMatIDStruct ID:17 IDString:"VFX.mat")
		append Out (DAOMatIDStruct ID:18 IDString:"Water.mat")
		append Out (DAOMatIDStruct ID:19 IDString:"Weapon.mat")
		append Out (DAOMatIDStruct ID:20 IDString:"Weapontrail.mat")
		Out
	)
	
	fn GetMatSemanticIDs = (	--translation table. Edit with Care!! Used by Material import/export
		local Out = #()
		append Out (DAOMatIDStruct ID:1 IDString:"Default")
		append Out (DAOMatIDStruct ID:2 IDString:"Punch")
		append Out (DAOMatIDStruct ID:3 IDString:"Blend")
		append Out (DAOMatIDStruct ID:4 IDString:"AddvPunchthroughMultiplyTint2S")
		append Out (DAOMatIDStruct ID:5 IDString:"AddvPunchthroughMultiplyTint2SL2")
		--append Out (DAOMatIDStruct ID:6 IDString:"MultiplyTintL3")	--double
		append Out (DAOMatIDStruct ID:7 IDString:"LerpedTint")
		append Out (DAOMatIDStruct ID:8 IDString:"LerpedTintL2")
		append Out (DAOMatIDStruct ID:9 IDString:"LerpedTintL3")
		append Out (DAOMatIDStruct ID:10 IDString:"AlphaLerpedTint")
		append Out (DAOMatIDStruct ID:11 IDString:"PunchthroughLerpedTint")
		append Out (DAOMatIDStruct ID:12 IDString:"PunchthroughLerpedTintL2")
		append Out (DAOMatIDStruct ID:13 IDString:"EmissiveLerpedTint")
		append Out (DAOMatIDStruct ID:14 IDString:"EmissiveLerpedTintL2")
		append Out (DAOMatIDStruct ID:15 IDString:"AlphaEmissiveLerpedTint")
		append Out (DAOMatIDStruct ID:16 IDString:"PunchthroughEmissiveLerpedTint")
		append Out (DAOMatIDStruct ID:17 IDString:"PunchthroughEmissiveLerpedTintL2")
		append Out (DAOMatIDStruct ID:18 IDString:"MultiplyTint")
		append Out (DAOMatIDStruct ID:19 IDString:"MultiplyTintL2")
		append Out (DAOMatIDStruct ID:20 IDString:"MultiplyTintL3")
		append Out (DAOMatIDStruct ID:21 IDString:"AlphaMultiplyTint")
		append Out (DAOMatIDStruct ID:22 IDString:"PunchthroughMultiplyTint")
		append Out (DAOMatIDStruct ID:23 IDString:"PunchthroughMultiplyTintL2")
		append Out (DAOMatIDStruct ID:24 IDString:"EmissiveMultiplyTint")
		append Out (DAOMatIDStruct ID:25 IDString:"EmissiveMultiplyTintL2")
		append Out (DAOMatIDStruct ID:26 IDString:"AlphaEmissiveMultiplyTint")
		append Out (DAOMatIDStruct ID:27 IDString:"PunchthroughEmissiveMultiplyTint")
		append Out (DAOMatIDStruct ID:28 IDString:"PunchthroughEmissiveMultiplyTintL2")
		append Out (DAOMatIDStruct ID:29 IDString:"ArmourSkinTint")
		append Out (DAOMatIDStruct ID:30 IDString:"ArmourSkinTintL2")
		append Out (DAOMatIDStruct ID:31 IDString:"ArmourSkinTintL3")
		append Out (DAOMatIDStruct ID:32 IDString:"AlphaArmourSkinTint")
		append Out (DAOMatIDStruct ID:33 IDString:"PunchthroughArmourSkinTint")
		append Out (DAOMatIDStruct ID:34 IDString:"PunchthroughArmourSkinTintL2")
		append Out (DAOMatIDStruct ID:35 IDString:"EmissiveArmourSkinTint")
		append Out (DAOMatIDStruct ID:36 IDString:"EmissiveArmourSkinTintL2")
		append Out (DAOMatIDStruct ID:37 IDString:"AlphaEmissiveArmourSkinTint")
		append Out (DAOMatIDStruct ID:38 IDString:"PunchthroughEmissiveArmourSkinTint")
		append Out (DAOMatIDStruct ID:39 IDString:"PunchthroughEmissiveArmourSkinTintL2")
		append Out (DAOMatIDStruct ID:40 IDString:"NoTint")
		append Out (DAOMatIDStruct ID:41 IDString:"NoTintL2")
		append Out (DAOMatIDStruct ID:42 IDString:"NoTintL3")
		append Out (DAOMatIDStruct ID:43 IDString:"AlphaNoTint")
		append Out (DAOMatIDStruct ID:44 IDString:"PunchthroughNoTint")
		append Out (DAOMatIDStruct ID:45 IDString:"PunchthroughNoTintL2")
		append Out (DAOMatIDStruct ID:46 IDString:"EmissiveNoTint")
		append Out (DAOMatIDStruct ID:47 IDString:"EmissiveNoTintL2")
		append Out (DAOMatIDStruct ID:48 IDString:"EmissiveNoTintL3")
		append Out (DAOMatIDStruct ID:49 IDString:"AlphaEmissiveNoTint")
		append Out (DAOMatIDStruct ID:50 IDString:"PunchthroughEmissiveNoTint")
		append Out (DAOMatIDStruct ID:51 IDString:"PunchthroughEmissiveNoTintL2")
		append Out (DAOMatIDStruct ID:52 IDString:"AlphaNoTintBody")
		append Out (DAOMatIDStruct ID:53 IDString:"AlphaMulTintBody")
		append Out (DAOMatIDStruct ID:54 IDString:"Diffuse")
		append Out (DAOMatIDStruct ID:55 IDString:"Volume")
		--append Out (DAOMatIDStruct ID:54 IDString:"AddvPunchthroughMultiplyTint2S")	--doubles
		--append Out (DAOMatIDStruct ID:55 IDString:"AddvPunchthroughMultiplyTint2SL2")
		append Out (DAOMatIDStruct ID:56 IDString:"LOD2")
		append Out (DAOMatIDStruct ID:57 IDString:"LOD3")
		append Out (DAOMatIDStruct ID:58 IDString:"Clouds")
		append Out (DAOMatIDStruct ID:59 IDString:"PropDistortionDiffuse")
		append Out (DAOMatIDStruct ID:60 IDString:"StaticDistortionDiffuse")
		append Out (DAOMatIDStruct ID:61 IDString:"VFXAddvDistortionDiffuse")
		append Out (DAOMatIDStruct ID:62 IDString:"VFXBlendDistortionDiffuse")
		append Out (DAOMatIDStruct ID:63 IDString:"VFXAddvCSDistortionDiffuse")
		append Out (DAOMatIDStruct ID:64 IDString:"VFXBlendCSDistortionDiffuse")
		append Out (DAOMatIDStruct ID:65 IDString:"VFXChunkyDistortionDiffuse")
		append Out (DAOMatIDStruct ID:66 IDString:"Skinned")
		append Out (DAOMatIDStruct ID:67 IDString:"Particle")
		append Out (DAOMatIDStruct ID:68 IDString:"Particle_CS")
		append Out (DAOMatIDStruct ID:69 IDString:"Eyelash")
		append Out (DAOMatIDStruct ID:70 IDString:"ReticalTint")
		append Out (DAOMatIDStruct ID:71 IDString:"Opaque")
		append Out (DAOMatIDStruct ID:72 IDString:"OpaqueVolume")
		append Out (DAOMatIDStruct ID:73 IDString:"Sky")
		append Out (DAOMatIDStruct ID:74 IDString:"FadeSky")
		append Out (DAOMatIDStruct ID:75 IDString:"LOD1Relief")
		append Out (DAOMatIDStruct ID:76 IDString:"LOD1Blend")
		append Out (DAOMatIDStruct ID:77 IDString:"LOD1BlendFlat")
		append Out (DAOMatIDStruct ID:78 IDString:"LOD1Punch")
		append Out (DAOMatIDStruct ID:79 IDString:"LOD3Punch")
		append Out (DAOMatIDStruct ID:80 IDString:"Addv")
		append Out (DAOMatIDStruct ID:81 IDString:"Blend_Atm")
		append Out (DAOMatIDStruct ID:82 IDString:"ContactSheetAddv")
		append Out (DAOMatIDStruct ID:83 IDString:"ContactSheetBlend")
		append Out (DAOMatIDStruct ID:84 IDString:"VolTexAddv")
		append Out (DAOMatIDStruct ID:85 IDString:"VolTexBlend")
		append Out (DAOMatIDStruct ID:86 IDString:"AlternateDecalBlend")
		append Out (DAOMatIDStruct ID:87 IDString:"AlternateDecalAddv")
		append Out (DAOMatIDStruct ID:88 IDString:"Chunky")
		append Out (DAOMatIDStruct ID:89 IDString:"ChunkyBlend")
		append Out (DAOMatIDStruct ID:90 IDString:"ChunkyAddv")
		append Out (DAOMatIDStruct ID:91 IDString:"ChunkyScroll")
		append Out (DAOMatIDStruct ID:92 IDString:"ChunkyScrollBlend")
		append Out (DAOMatIDStruct ID:93 IDString:"ChunkyScrollAddv")
		append Out (DAOMatIDStruct ID:94 IDString:"ChunkyContactSheet")
		append Out (DAOMatIDStruct ID:95 IDString:"ChunkyContactSheetBlend")
		append Out (DAOMatIDStruct ID:96 IDString:"ChunkyContactSheetAddv")
		append Out (DAOMatIDStruct ID:97 IDString:"ChunkyVolTex")
		append Out (DAOMatIDStruct ID:98 IDString:"ChunkyVolTexBlend")
		append Out (DAOMatIDStruct ID:99 IDString:"ChunkyVolTexAddv")
		append Out (DAOMatIDStruct ID:100 IDString:"Blend_DBA")
		append Out (DAOMatIDStruct ID:101 IDString:"Blend_ContactSheet_DBA")
		append Out (DAOMatIDStruct ID:102 IDString:"Additive_DBA")
		append Out (DAOMatIDStruct ID:103 IDString:"Additive_ContactSheet_DBA")
		append Out (DAOMatIDStruct ID:104 IDString:"ContactSheetBlendSplat")
		append Out (DAOMatIDStruct ID:105 IDString:"ContactSheetAddvSplat")
		append Out (DAOMatIDStruct ID:106 IDString:"Sorted")
		append Out (DAOMatIDStruct ID:107 IDString:"NoSpec")
		append Out (DAOMatIDStruct ID:108 IDString:"FlowingWater")
		append Out (DAOMatIDStruct ID:109 IDString:"ToolsetProbeFlowingWater")
		append Out (DAOMatIDStruct ID:110 IDString:"ToolsetWater")
		append Out (DAOMatIDStruct ID:111 IDString:"Wireframe")
		append Out (DAOMatIDStruct ID:112 IDString:"PSSM")
		append Out (DAOMatIDStruct ID:113 IDString:"DepthWrite")
		append Out (DAOMatIDStruct ID:114 IDString:"DepthWriteDebug")
		append Out (DAOMatIDStruct ID:115 IDString:"DepthWriteLowLOD")
		append Out (DAOMatIDStruct ID:116 IDString:"OpaqueDepthWrite")
		append Out (DAOMatIDStruct ID:117 IDString:"Debug")
		append Out (DAOMatIDStruct ID:118 IDString:"Overdraw")
		Out
	)
	
	fn GetDAOMatIDByString IDTable InString = (
		local OutID = 1
		for Item in IDTable do
		(
			if (EqualsCaseless Item.IDString InString) == true then OutID = Item.ID
		)
		OutID
	)
	
	fn GetDAOMatStringByID IDTable InID = (
		local OutString = ""
		for Item in IDTable do
		(
			if Item.ID == InID then OutString = Item.IDString
		)
		OutString
	)
	
	fn TimeDiff Starttime Endtime = (
		local h1 = (substring Starttime 12 2) as integer
		local m1 = (substring Starttime 15 2) as integer
		local s1 = (substring Starttime 18 2) as integer
		local h2 = (substring Endtime 12 2) as integer
		local m2 = (substring Endtime 15 2) as integer
		local s2 = (substring Endtime 18 2) as integer
		local h3
		local m3
		local s3
		local out
		
		if s2 >= s1 then s3 = s2-s1 else (m2 -= 1; s3 = 60 + s2-s1)
		if m2 >= m1 then m3 = m2-m1 else (h2 -= 1; m3 = 60 + m2-m1)
		if h2 >= h1 then h3 = h2-h1
		
		out = h3 as string + "h " + m3 as string + "m " + s3 as string + "s"
		out
	)