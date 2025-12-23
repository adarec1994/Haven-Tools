/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edit:
14 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Model Export Script
************************************************
*/

struct DAOModelExportStruct
(
	--MenuSettings
	--************
	UI_Pos,
	UI_Height,
	
	--Intermediate Variables
	--**********************
	Extension,
	GOBNode,
	GOBOffset = 1,
	UsingEditNormals,
	ExportMAO
)
try (DestroyDialog DAOModelToolsRollout)catch()
global DAOModelToolsRollout
global DAOModelEx
(
	struct DAOModelExVarsStruct (
		--Intermediate Vars for export stuff
		--**********************************
		MaterialObjects = #(),		--Collection of materials #(node,material) from MSH export , to supply to material exporter and MMH export
		MeshesInWorldSpace = #(),	--Used to collect all skinned meshes, to export them at 0,0,0 
		NodeIDCollection = #(),		--Used to determine if any NodeID is unique, by comparing them to the collection so far
		MeshIncrement
	)
	DAOModelExVars = DAOModelExVarsStruct()
	
	include "DAOTools\\DAORollouts.ms"
	include "DAOTools\\DAOcommon.ms"
	include "DAOTools\\DAOMaterialExport.ms"
	include "DAOTools\\DAOModelExportHelper.ms"
	
	fn Initialize = (
		DAOModelEx = DAOModelExportStruct UI_Height:415
		if ((maxversion())[1] >= 7000) then DAOModelEx.UI_Pos = getMAXWindowPos() + (getMAXWindowSize()/4) else DAOModelEx.UI_Pos = [100,100]
		try(DAOModelEx.UI_Pos.x = (DAOTools.ReadIni "ModelExport" "UI_Pos_X") as integer)catch()
		try(DAOModelEx.UI_Pos.y = (DAOTools.ReadIni "ModelExport" "UI_Pos_Y") as integer)catch()
		try(DAOModelEx.UI_Height = (DAOTools.ReadIni "ModelExport" "UI_Height") as integer)catch()
	)
	
	fn ExportMMHChilds Stream objs Progress = (
		
		local objTransform
		local OutTransform
		
		local isGOB		--Evaluating each node if it is GOB.
		local NodeName
		local CollShapes = #()
		local HasSkinMod
		local BonesUsed
		
		local MetaData = MetaDataStruct()
		local ChildMetaData	--temporary holder for recursed metadata
		
		for i = 1 to Objs.count do
		(
			ProgressDialog.BarSmall.value = Progress[1]*100/Progress[2]
			ProgressDialog.Label.text = ("Writing MMH Model Data \""+Objs[i].name+"\"")
			Progress[1] += 1
			NodeName = (cutstring Objs[i].name DAOModelEx.Extension)
			isGOB = (EqualsCaseless NodeName "GOB")
			
			--******************************************
			--GOB export from custom GOB object here
			--******************************************
			if (GetObjectExportType Objs[i]) == #GameObject then
			(
				--format "<Node Name=\"%\">\n" (NodeName) to:Stream
				format "<Node Name=\"%\" SoundMaterialType=\"%\">\n" (NodeName) ((Objs[i].SoundMatType - 1) as string) to:Stream
				format "<Export TagName=\"GobTranslation\" ExportName=\"%_GobTranslation\" ControllerType=\"Gob_Vector3\" />\n" (NodeName) to:Stream
				format "<Export TagName=\"GobRotation\" ExportName=\"%_GobRotation\" ControllerType=\"Gob_Quaternion\" />\n" (NodeName) to:Stream
				--***********************************
				--Attach Bounding Box of Child Meshes
				--***********************************
				local BBox = GetBBox Objs[i]
				if BBox != false then
				(
					format "<BoundingBox>\n" to:Stream
					format "<![CDATA[% % % 1.0 % % % 1.0]]>\n" BBox[1].x BBox[1].y BBox[1].z BBox[2].x BBox[2].y BBox[2].z to:Stream
					format "</BoundingBox>\n" to:Stream
				)
				
				--************************
				--Translation and Rotation
				--**************************
				OutTransform = ConvertOutputTransform Objs[i] Objs[i].transform
				format "<Translation>\n" to:Stream
				format "<![CDATA[% % % 1.0]]>\n" (OutTransform.pos.x) (OutTransform.pos.y) (OutTransform.pos.z) to:Stream
				format "</Translation>\n" to:Stream
				format "<Rotation>\n" to:Stream
				format "<![CDATA[% % % %]]>\n" (OutTransform.rot.x) (OutTransform.rot.y) (OutTransform.rot.z) (OutTransform.rot.w) to:Stream
				format "</Rotation>\n" to:Stream
				
				--****************
				--Recurse Children
				--****************
				ChildMetaData = ExportMMHChilds Stream (Objs[i].Children) Progress
				MetaData = MetaDataJoin MetaData ChildMetaData
				
				format "</Node>\n" to:Stream
			)
			if (GetObjectExportType Objs[i]) == #Crust then
			(
				format "<NodeCrustHierarchy Name=\"%\" HookID=\"%\">\n" (NodeName) (Objs[i].HookID as string) to:Stream
				format "<Export TagName=\"Translation\" ExportName=\"%_Translation\" ControllerType=\"Vector3\" />\n" (NodeName) to:Stream
				format "<Export TagName=\"Rotation\" ExportName=\"%_Rotation\" ControllerType=\"Quaternion\" />\n" (NodeName) to:Stream
				
				OutTransform = ConvertOutputTransform Objs[i] Objs[i].Transform
				format "<Translation>\n" to:Stream
				format "<![CDATA[% % % 1.0]]>\n" (OutTransform.pos.x) (OutTransform.pos.y) (OutTransform.pos.z) to:Stream
				format "</Translation>\n" to:Stream
				format "<Rotation>\n" to:Stream
				format "<![CDATA[% % % %]]>\n" (OutTransform.rot.x) (OutTransform.rot.y) (OutTransform.rot.z) (OutTransform.rot.w) to:Stream
				format "</Rotation>\n" to:Stream
				
				--****************
				--Recurse Children
				--****************
				ChildMetaData = ExportMMHChilds Stream (Objs[i].Children) Progress
				MetaData = MetaDataJoin MetaData ChildMetaData
				
				format "</NodeCrustHierarchy>\n" to:Stream
			)
			if (GetObjectExportType Objs[i]) == #UserPoint then
			(
				format "<NodeUsePoint Name=\"%\" Type=\"Front\">\n" (NodeName) to:Stream
				format "<Export TagName=\"Translation\" ExportName=\"%_Translation\" ControllerType=\"Vector3\" />\n" (NodeName) to:Stream
				format "<Export TagName=\"Rotation\" ExportName=\"%_Rotation\" ControllerType=\"Quaternion\" />\n" (NodeName) to:Stream
				
				OutTransform = ConvertOutputTransform Objs[i] Objs[i].Transform
				format "<Translation>\n" to:Stream
				format "<![CDATA[% % % 1.0]]>\n" (OutTransform.pos.x) (OutTransform.pos.y) (OutTransform.pos.z) to:Stream
				format "</Translation>\n" to:Stream
				format "<Rotation>\n" to:Stream
				format "<![CDATA[% % % %]]>\n" (OutTransform.rot.x) (OutTransform.rot.y) (OutTransform.rot.z) (OutTransform.rot.w) to:Stream
				format "</Rotation>\n" to:Stream
				
				--****************
				--Recurse Children
				--****************
				ChildMetaData = ExportMMHChilds Stream (Objs[i].Children) Progress
				MetaData = MetaDataJoin MetaData ChildMetaData
				
				format "</NodeUsePoint>\n" to:Stream
			)
			if (GetObjectExportType Objs[i]) == #SnapPoint then
			(
				format "<NodeSnaptoPoint Name=\"%\">\n" (NodeName) to:Stream
				format "<Export TagName=\"Translation\" ExportName=\"%_Translation\" ControllerType=\"Vector3\" />\n" (NodeName) to:Stream
				format "<Export TagName=\"Rotation\" ExportName=\"%_Rotation\" ControllerType=\"Quaternion\" />\n" (NodeName) to:Stream
				
				OutTransform = ConvertOutputTransform Objs[i] Objs[i].Transform
				format "<Translation>\n" to:Stream
				format "<![CDATA[% % % 1.0]]>\n" (OutTransform.pos.x) (OutTransform.pos.y) (OutTransform.pos.z) to:Stream
				format "</Translation>\n" to:Stream
				format "<Rotation>\n" to:Stream
				format "<![CDATA[% % % %]]>\n" (OutTransform.rot.x) (OutTransform.rot.y) (OutTransform.rot.z) (OutTransform.rot.w) to:Stream
				format "</Rotation>\n" to:Stream
				
				--****************
				--Recurse Children
				--****************
				ChildMetaData = ExportMMHChilds Stream (Objs[i].Children) Progress
				MetaData = MetaDataJoin MetaData ChildMetaData
				
				format "</NodeSnaptoPoint>\n" to:Stream
			)
			
			--*******************************
			--Common Nodes come here
			--*******************************
			if (GetObjectExportType Objs[i]) == #Node then
			(
				--*******************************
				--Mark the beginning of the Node
				--*******************************
				if IsBoneParamsUptoDate Objs[i] then	--thats new node parameters
				(
					if Objs[i].BoneParamIndex == -1 then
					(
						format "<Node Name=\"%\">\n" (NodeName) to:Stream
					)
					else
					(
						format "<Node Name=\"%\" BoneIndex=\"%\">\n" (NodeName) (Objs[i].BoneParamIndex as string) to:Stream
					)
				)
				else try --legacy.. reading outdated parameters (trying)	
				(
					if Objs[i].CrustParamAct == true then
					(
						if Objs[i].CrustParamHookID < 0 then MessageBox ("Error in Crust Node \""+Objs[i].name+"\"\n\nInvalid HookID (-1)") title:"Model Export"
						format "<NodeCrustHierarchy Name=\"%\" HookID=\"%\">\n" (NodeName) (Objs[i].CrustParamHookID as string) to:Stream
					)
					else if Objs[i].UsepParamAct == true then
					(
						format "<NodeUsePoint Name=\"%\" Type=\"Front\">\n" (NodeName) to:Stream
					)
					else if Objs[i].SnapParamAct == true then
					(
						format "<NodeSnaptoPoint Name=\"%\">\n" (NodeName) to:Stream
					)
					else
					(
						if Objs[i].BoneParamAct == true then
						(
							if Objs[i].BoneParamIndex < 0 then MessageBox ("Error in Node \""+Objs[i].name+"\"\n\nInvalid Bone Index (-1) of active Bone") title:"Model Export"
							format "<Node Name=\"%\" BoneIndex=\"%\">\n" (NodeName) (Objs[i].BoneParamIndex as string) to:Stream
						)
						else
						(
							format "<Node Name=\"%\">\n" (NodeName) to:Stream
						)
					)
				)
				catch	--all failed
				(
					MessageBox ("Fatal Error!\nFailed to read Node Parameters, Object \""+Objs[i].name+"\".\nCheck for Updates in the DAO Model Manager\n\nAborting..") title:"Model Export"
					EnableSceneRedraw()
					throw()	--quitting hard, for the lack of a better method
				)
				
				--*******************************
				--Animation Export Tag
				--*******************************
				if (GetObjectAnimExportType Objs[i]) != undefined then
				(
					if isGOB then	--legacy, replaced by Gameobject helper
					(
						format "<Export TagName=\"GobTranslation\" ExportName=\"%_GobTranslation\" ControllerType=\"Gob_Vector3\" />\n" (NodeName) to:Stream
						format "<Export TagName=\"GobRotation\" ExportName=\"%_GobRotation\" ControllerType=\"Gob_Quaternion\" />\n" (NodeName) to:Stream					
					)
					else
					(
						format "<Export TagName=\"Translation\" ExportName=\"%_Translation\" ControllerType=\"Vector3\" />\n" (NodeName) to:Stream
						format "<Export TagName=\"Rotation\" ExportName=\"%_Rotation\" ControllerType=\"Quaternion\" />\n" (NodeName) to:Stream
					)
				)
				--**************************************************************************************************************
				--Attach Bounding Box of Child Meshes	-legacy (was supposed to apply to GOB before "modelroot" was introduced)
				--**************************************************************************************************************
				if Objs[i].parent == undefined then
				(
					local BBox = GetBBox Objs[i]
					if BBox != false then
					(
						format "<BoundingBox>\n" to:Stream
						format "<![CDATA[% % % 1.0 % % % 1.0]]>\n" BBox[1].x BBox[1].y BBox[1].z BBox[2].x BBox[2].y BBox[2].z to:Stream
						format "</BoundingBox>\n" to:Stream
					)
				)
				--************************
				--Translation and Rotation
				--**************************
				OutTransform = ConvertOutputTransform Objs[i] Objs[i].Transform
				format "<Translation>\n" to:Stream
				format "<![CDATA[% % % 1.0]]>\n" (OutTransform.pos.x) (OutTransform.pos.y) (OutTransform.pos.z) to:Stream
				format "</Translation>\n" to:Stream
				format "<Rotation>\n" to:Stream
				format "<![CDATA[% % % %]]>\n" (OutTransform.rot.x) (OutTransform.rot.y) (OutTransform.rot.z) (OutTransform.rot.w) to:Stream
				format "</Rotation>\n" to:Stream
				
				--****************
				--Recurse Children
				--****************
				ChildMetaData = ExportMMHChilds Stream (Objs[i].Children) Progress
				MetaData = MetaDataJoin MetaData ChildMetaData
				
				--***************************
				--Marking the End of the Node
				--***************************
				if IsBoneParamsUptoDate Objs[i] then
				(
					format "</Node>\n" to:Stream
				)
				else try		--legacy, old parameters read
				(
					if Objs[i].CrustParamAct == true then
					(
						format "</NodeCrustHierarchy>\n" to:Stream
					)
					else if Objs[i].UsepParamAct == true then
					(
						format "</NodeUsePoint>\n" to:Stream
					)
					else if Objs[i].SnapParamAct == true then
					(
						format "</NodeSnaptoPoint>\n" to:Stream
					)
					else
					(
						format "</Node>\n" to:Stream
					)
				)
				catch	--all failed
				(
					MessageBox ("Fatal Error!\nFailed to read Node Parameters, Object \""+Objs[i].name+"\".\nCheck for Updates in the DAO Model Manager\n\nAborting..") title:"Model Export"
					EnableSceneRedraw()
					throw()	--quitting hard, for the lack of a better method
				)
			)
			
			--*************************************************************************************
			--Mesh Object Export starts here, not the actual Mesh, just the link to it (MMH export)
			--*************************************************************************************
			if (GetObjectExportType Objs[i]) == #Mesh then
			(
				--********************************************************************************************************************
				--Getting Node ID, 
				--should really be using a table of free ID's, because the lightmapper requires unique ID's across all models
				--but for now its a random one, unique across the single model.
				--********************************************************************************************************************
				local MeshUniqueID = Objs[i].NodeID
				while (finditem DAOModelExVars.NodeIDCollection MeshUniqueID) > 0 do Objs[i].NodeID = MeshUniqueID = RandomNodeIDString()
				append DAOModelExVars.NodeIDCollection MeshUniqueID
				
				--*******************
				--Get Output Info
				--********************
				local MatName
				
				for Mat in DAOModelExVars.MaterialObjects do	--browsing the collection of Mats collected by MSH export
				(
					if Mat[1] == Objs[i] then MatName = (FixOutputName Mat[2].Name #())		--This is the mat. +Removing the spaces from the name. Making filefriendly
				)
				if MatName == undefined then MatName = "black"
				
				HasSkinMod = (GetModifier Objs[i] Skin) == 1
				if HasSkinMod then BonesUsed = GetBonesUsed Objs[i]
				
				--***********************
				--MetaData block
				--***********************
				local MetaDataPartsList = MetaDataPartsListStruct()
				MetaDataPartsList.MeshName = (cutstring Objs[i].name DAOModelEx.Extension)
				MetaDataPartsList.Value = ""
				MetaDataPartsList.ID = MeshUniqueID
				MetaDataPartsList.BBox = (Objs[i].max - Objs[i].min) / DAOTools.WorldScale
				append MetaData.PartsList MetaDataPartsList
				append MetaData.MaterialObject (MatName + ".MAO")
				
				--*************************************
				--Get NodeName of GOB object setting
				--*************************************
				local NodeMeshName = (cutstring Objs[i].name DAOModelEx.Extension)	--default, if GOB isnt of type GameObject
				DAOModelExVars.MeshIncrement += 1
				if (classof DAOModelEx.GOBNode) == DAO_GameObject then
				(
					if DAOModelEx.GOBNode.modeltype == 2 then NodeMeshName = (cutstring Objs[i].name DAOModelEx.Extension)
					if DAOModelEx.GOBNode.modeltype == 1 then
					(
						if DAOModelEx.GOBNode.meshtype == 1 then NodeMeshName = "Mesh" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 2 then NodeMeshName = "ArmorM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 3 then NodeMeshName = "BootsM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 4 then NodeMeshName = "GlovesM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 5 then NodeMeshName = "HelmetM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 6 then NodeMeshName = "ClothesM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 7 then NodeMeshName = "RobeM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 8 then NodeMeshName = "NeckM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 9 then NodeMeshName = "FaceM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 10 then NodeMeshName = "EyesM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 11 then NodeMeshName = "HairM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 12 then NodeMeshName = "LashesM" + DAOModelExVars.MeshIncrement as string
						if DAOModelEx.GOBNode.meshtype == 13 then NodeMeshName = "BeardM" + DAOModelExVars.MeshIncrement as string
					)
				)
				
				--**************
				--Output
				--***************
				format "<NodeMesh Name=\"%\" " (NodeMeshName) to:Stream
				if HasSkinMod and (BonesUsed != undefined) then
				(
					format "BonesUsed=\"" to:Stream
					for Bones = 1 to BonesUsed.count do
					(
						format "%" (BonesUsed[Bones]) to:Stream
						if Bones != BonesUsed.count then
						(
							format " " to:Stream
						)
					)
					format "\" " to:Stream
				)
				format "ID=\"%\" " (MeshUniqueID) to:Stream
				format "MeshGroupName=\"%\" " (cutstring Objs[i].name DAOModelEx.Extension) to:Stream	--the actual link to the mesh in the MSH
				format "MaterialLibrary=\"%\" " (MatName) to:Stream
				format "MaterialObject=\"%\" " (MatName) to:Stream
				format "CastRuntimeShadow=\"%\" " (if Objs[i].CastRuntimeShadow == true then "1" else "0") to:Stream
				format "ReceiveRuntimeShadow=\"%\" " (if Objs[i].ReceiveRuntimeShadow == true then "1" else "0") to:Stream
				format "CastBakedShadow=\"%\" " (if Objs[i].CastBakedShadow == true then "1" else "0") to:Stream
				format "ReceiveBakedShadow=\"%\" " (if Objs[i].ReceiveBakedShadow == true then "1" else "0") to:Stream
				format "CutAway=\"%\" " (if Objs[i].CutAway == true then "1" else "0") to:Stream
				format "PunchThrough=\"%\" >\n" (if Objs[i].PunchThrough == true then "1" else "0") to:Stream
				
				format "<Attribute AttributeName=\"BaseLight\" SourceName=\"BaseLight\" />\n" to:Stream
				
				--**********************************
				--Export Tag ,Animatable Meshes only
				--***********************************
				if (GetObjectAnimExportType Objs[i]) != undefined then
				(
					format "<Export TagName=\"Translation\" ExportName=\"%_Translation\" ControllerType=\"Vector3\" />\n" (cutstring Objs[i].name DAOModelEx.Extension) to:Stream
					format "<Export TagName=\"Rotation\" ExportName=\"%_Rotation\" ControllerType=\"Quaternion\" />\n" (cutstring Objs[i].name DAOModelEx.Extension) to:Stream
				)
				
				--************************
				--Translation and Rotation
				--**************************
				OutTransform = ConvertOutputTransform Objs[i] Objs[i].Transform
				format "<Translation>\n" to:Stream
				format "<![CDATA[% % % 1.0]]>\n" (OutTransform.pos.x) (OutTransform.pos.y) (OutTransform.pos.z) to:Stream
				format "</Translation>\n" to:Stream
				format "<Rotation>\n" to:Stream
				format "<![CDATA[% % % %]]>\n" (OutTransform.rot.x) (OutTransform.rot.y) (OutTransform.rot.z) (OutTransform.rot.w) to:Stream
				format "</Rotation>\n" to:Stream
				
				--****************
				--Recurse Children
				--****************
				ChildMetaData = ExportMMHChilds Stream (Objs[i].Children) Progress
				MetaData = MetaDataJoin MetaData ChildMetaData
				
				format "</NodeMesh>\n" to:Stream
			)
			--******************
			--Weapontrails
			--******************
			if (GetObjectExportType Objs[i]) == #Weapontrail then
			(
				local WtrlMat
				local WtrlMatName = Objs[i].MatObject
				
				if WtrlMatName == "" then	--When MatObject is "", then MatObjectMat has custom Material -> retrieve name + append for MAO export
				(
					WtrlMat = Objs[i].MatObjectMat
					try(WtrlMatName = Objs[i].MatObjectMat.Name)catch
					(		--error occured that should have been catched by the user earlier. Setting default
						MessageBox ("Weapontrail \""+Objs[i].name+"\" has no Material set.\nExpected custom Material (Standard or Dragon Age Material) where none was picked.\n\nApplying \"weapontrail_streaks\" instead") title:"Model Export"
						WtrlMatName = "weapontrail_streaks"
					)
				
					if ClassOf WtrlMat == DAOMaterial then
					(
						append DAOModelExVars.MaterialObjects #(Objs[i],WtrlMat)	--append it to list of to-do MAO export
					)
					else
					(
						if DAOModelEx.ExportMAO == true then
						(
							Messagebox ("Weapontrail \""+Objs[i].name+"\" doesnt have a valid Material assigned for performing MAO export.\nMust only be \"Dragon Age Material\"\nWithout a MAO, no Weapontrail will be displayed\n\nSkipping..") title:"Model Export"
						)
					)
				)
				
				format "<NodeWeapontrail Name=\"%\" " Objs[i].name to:Stream
				format "SegmentLength=\"%\" Duration=\"%\" " (Objs[i].segmentlenght * Objs[i].objecttransform.scale.Z / DAOTools.WorldScale) Objs[i].duration to:Stream
				format "MaterialObject=\"%\">\n" (WtrlMatName) to:Stream
				
				--************************
				--Translation and Rotation
				--**************************
				OutTransform = ConvertOutputTransform Objs[i] Objs[i].objecttransform
				format "<Translation>\n" to:Stream
				format "<![CDATA[% % % 1.0]]>\n" (OutTransform.pos.x) (OutTransform.pos.y) (OutTransform.pos.z) to:Stream
				format "</Translation>\n" to:Stream
				format "<Rotation>\n" to:Stream
				format "<![CDATA[% % % %]]>\n" (OutTransform.rot.x) (OutTransform.rot.y) (OutTransform.rot.z) (OutTransform.rot.w) to:Stream
				format "</Rotation>\n" to:Stream
				
				--****************
				--Recurse Children
				--****************
				ChildMetaData = ExportMMHChilds Stream (Objs[i].Children) Progress
				MetaData = MetaDataJoin MetaData ChildMetaData
				
				format "</NodeWeapontrail>\n" to:Stream
			)
			if (GetObjectExportType Objs[i]) == #Cloth then
			(
				local ClothMod = Objs[i].Modifiers[(GetModifier Objs[i] DAO_Mod_Cloth)]
				
				--********************************************************************************************************************
				--Getting Node ID, 
				--should really be using a table of free ID's, because the lightmapper requires unique ID's across all models
				--but for now its a random one, unique across the single model.
				--********************************************************************************************************************
				local MeshUniqueID = ClothMod.ClothNodeID
				while (finditem DAOModelExVars.NodeIDCollection MeshUniqueID) > 0 do ClothMod.ClothNodeID = MeshUniqueID = RandomNodeIDString()
				append DAOModelExVars.NodeIDCollection MeshUniqueID
				
				--*******************
				--Get Output Info
				--********************
				local MeshObj = MakeMeshSnapShot Objs[i]
				local DAOInputMeshData = GetInputMeshData MeshObj Objs[i]
				local BWMesh = ConvertInputMesh MeshObj DAOInputMeshData
				
				local MatName
				
				for Mat in DAOModelExVars.MaterialObjects do	--browsing the collection of Mats collected by MSH export
				(
					if Mat[1] == Objs[i] then MatName = (FixOutputName Mat[2].Name #())		--This is the mat. +Removing the spaces from the name. Making filefriendly
				)
				if MatName == undefined then MatName = "black"
				
				HasSkinMod = (GetModifier Objs[i] Skin) == 1
				if HasSkinMod then BonesUsed = GetBonesUsed Objs[i]
				
				--***********************
				--MetaData block
				--***********************
				local MetaDataPartsList = MetaDataPartsListStruct()
				MetaDataPartsList.MeshName = (cutstring Objs[i].name DAOModelEx.Extension)
				MetaDataPartsList.Value = ""
				MetaDataPartsList.ID = MeshUniqueID
				MetaDataPartsList.BBox = (Objs[i].max - Objs[i].min) / DAOTools.WorldScale
				append MetaData.PartsList MetaDataPartsList
				append MetaData.MaterialObject (MatName + ".MAO")
				
				format "<NodeCloth "							to:Stream
				format "Name=\"%\" "							(NodeName) to:Stream
				format "ID=\"%\" "								(ClothMod.ClothNodeID) to:Stream
				format "MeshGroupName=\"%\" "					(NodeName) to:Stream
				format "MaterialLibrary=\"%\" "					(MatName) to:Stream
				format "MaterialObject=\"%\" "					(MatName) to:Stream
				format "CastRuntimeShadow=\"%\" "				(if ClothMod.ClothCastRuntimeShadow == true then "1" else "0") to:Stream
				format "ReceiveRuntimeShadow=\"%\" "			(if ClothMod.ClothReceiveRuntimeShadow == true then "1" else "0") to:Stream
				format "CastBakedShadow=\"%\" "					(if ClothMod.ClothCastBakedShadow == true then "1" else "0") to:Stream
				format "ReceiveBakedShadow=\"%\" "				(if ClothMod.ClothReceiveBakedShadow == true then "1" else "0") to:Stream
				format "CutAway=\"%\" "							(if ClothMod.ClothCutAway == true then "1" else "0") to:Stream
				format "PunchThrough=\"%\" "					(if ClothMod.ClothPunchThrough == true then "1" else "0") to:Stream
				
				format "Thickness=\"%\" "						(ClothMod.ClothThickness) to:Stream
				format "Density=\"%\" "							(ClothMod.ClothDensity) to:Stream
				format "BendingStiffness=\"%\" "				(ClothMod.ClothBendingStiffness) to:Stream
				format "StretchingStiffness=\"%\" "				(ClothMod.ClothStretchingStiffness) to:Stream
				format "DampingCoefficient=\"%\" "				(ClothMod.ClothDampingCoefficient) to:Stream
				format "Friction=\"%\" "						(ClothMod.ClothFriction) to:Stream
				format "Pressure=\"%\" "						(ClothMod.ClothPressure) to:Stream
				format "TearFactor=\"%\" "						(ClothMod.ClothTearFactor) to:Stream
				format "CollisionResponseCoefficient=\"%\" "	(ClothMod.ClothCollisionResponseCoefficient) to:Stream
				format "AttachmentResponseCoefficient=\"%\" "	(ClothMod.ClothAttachmentResponseCoefficient) to:Stream
				format "AttachmentTearFactor=\"%\" "			(ClothMod.ClothAttachmentTearFactor) to:Stream
				format "SolverIterations=\"%\" "				(ClothMod.ClothSolverIterations) to:Stream
				format "ExternalAcceleration=\"% % %\" "		(ClothMod.ClothExternalAcceleration.X) (ClothMod.ClothExternalAcceleration.Y) (ClothMod.ClothExternalAcceleration.Z) to:Stream
				format "WakeUpCounter=\"%\" "					(ClothMod.ClothWakeUpCounter) to:Stream
				format "SleepLinearVelocity=\"%\" "				(ClothMod.ClothSleepLinearVelocity) to:Stream
				format "Fadeable=\"%\" "						(ClothMod.ClothFadeable) to:Stream
				format "GROUP_MASK_ITEMS=\"%\" "				(ClothMod.ClothShapeCollMaskItems) to:Stream
				format "GROUP_MASK_CREATURES=\"%\" "			(ClothMod.ClothShapeCollMaskCreatures) to:Stream
				format "GROUP_MASK_PLACEABLES=\"%\" "			(ClothMod.ClothShapeCollMaskPlaceables) to:Stream
				format "GROUP_MASK_TRIGGERS=\"%\" "				(ClothMod.ClothShapeCollMaskTriggers) to:Stream
				format "GROUP_MASK_STATICGEOMETRY=\"%\" "		(ClothMod.ClothShapeCollMaskStatic) to:Stream
				format "GROUP_MASK_EFFECTS=\"%\" "				(ClothMod.ClothShapeCollMaskEffects) to:Stream
				format "GROUP_MASK_WAYPOINTS=\"%\" "			(ClothMod.ClothShapeCollMaskWaypoints) to:Stream
				format "GROUP_MASK_WATER=\"%\" "				(ClothMod.ClothShapeCollMaskWater) to:Stream
				format "GROUP_MASK_TERRAIN_WALL=\"%\" "			(ClothMod.ClothShapeCollMaskTerrainWall) to:Stream
				format "GROUP_MASK_WALKABLE=\"%\" "				(ClothMod.ClothShapeCollMaskWalkable) to:Stream
				format "GROUP_MASK_NONWALKABLE=\"%\" "			(ClothMod.ClothShapeCollMaskNonWalkable) to:Stream
				format "CF_PRESSURE=\"%\" "						(if ClothMod.ClothFlagPressure == true then "1" else "0") to:Stream
				format "CF_STATIC=\"%\" "						(if ClothMod.ClothFlagStatic == true then "1" else "0") to:Stream
				format "CF_DISABLE_COLLISION=\"%\" "			(if ClothMod.ClothFlagDisableCollision == true then "1" else "0") to:Stream
				format "CF_SELFCOLLISION=\"%\" "				(if ClothMod.ClothFlagSelfCollision == true then "1" else "0") to:Stream
				format "CF_VISUALIZATION=\"%\" "				(if ClothMod.ClothFlagVisualization == true then "1" else "0") to:Stream
				format "CF_GRAVITY=\"%\" "						(if ClothMod.ClothFlagGravity == true then "1" else "0") to:Stream
				format "CF_BENDING=\"%\" "						(if ClothMod.ClothFlagBending == true then "1" else "0") to:Stream
				format "CF_BENDING_ORTHO=\"%\" "				(if ClothMod.ClothFlagBendingOrtho == true then "1" else "0") to:Stream
				format "CF_DAMPING=\"%\" "						(if ClothMod.ClothFlagDamping == true then "1" else "0") to:Stream
				format "CF_COLLISION_TWOWAY=\"%\" "				(if ClothMod.ClothFlagCollisionTwoWay == true then "1" else "0") to:Stream
				format "CF_TRIANGLE_COLLISION=\"%\" "			(if ClothMod.ClothFlagTriangleCollision == true then "1" else "0") to:Stream
				format "CF_TEARABLE=\"%\" "						(if ClothMod.ClothFlagTearable == true then "1" else "0") to:Stream
				format "CF_HARDWARE=\"%\" "						(if ClothMod.ClothFlagHardware == true then "1" else "0") to:Stream
				format "CF_COMDAMPING=\"%\" "					(if ClothMod.ClothFlagCOMDamping == true then "1" else "0") to:Stream
				--format "WindEnabled=\"%\" " ("true") to:Stream
				--format "WindSpace=\"%\" " ("world") to:Stream
				--format "WindDirection=\"%\" " ("1.0 0.0 0.0") to:Stream
				--format "WindResponse=\"%\" " ("30.0") to:Stream
				--format "WindResponseLimit=\"%\" " ("30.0") to:Stream
				--format "WindStrength=\"%\" " ("2.0") to:Stream
				--format "GustingMinStrength=\"%\" " ("10.0") to:Stream
				--format "GustingMaxStrength=\"%\" " ("25.0") to:Stream
				--format "GustingMinDuration=\"%\" " ("0.1") to:Stream
				--format "GustingMaxDuration=\"%\" " ("2.0") to:Stream
				--format "GustingMinInterval=\"%\" " ("0.0") to:Stream
				--format "GustingMaxInterval=\"%\" " ("1.5") to:Stream
				--format "GustingDirectionChange=\"%\" " ("0.4") to:Stream
				--format "GustingDirectionAxisRatio=\"%\" " ("1.0 1.0 0.2") to:Stream
				--format "SpeedTreeWindUpdateTime=\"%\" " ("10000.0") to:Stream
				--format "SpeedTreeWindStrength=\"%\" " ("true") to:Stream
				--format "SpeedTreeWindDirection=\"%\" " ("true") to:Stream
				--format "SpeedTreeWindParams=\"%\" " ("true") to:Stream
				format ">\n" to:Stream
				
				format "<Attribute AttributeName=\"BaseLight\" SourceName=\"BaseLight\" />\n" to:Stream
				
				--************************
				--Translation and Rotation
				--**************************
				OutTransform = ConvertOutputTransform Objs[i] Objs[i].Transform
				format "<Translation>\n" to:Stream
				format "<![CDATA[% % % 1.0]]>\n" (OutTransform.pos.x) (OutTransform.pos.y) (OutTransform.pos.z) to:Stream
				format "</Translation>\n" to:Stream
				format "<Rotation>\n" to:Stream
				format "<![CDATA[% % % %]]>\n" (OutTransform.rot.x) (OutTransform.rot.y) (OutTransform.rot.z) (OutTransform.rot.w) to:Stream
				format "</Rotation>\n" to:Stream
				
				local AttachType = "Nattfodd is such a Nerd!"
				case ClothMod.ClothAttachmentType of
				(
					2: AttachType = "CollidingShapes"
					3: AttachType = "Shape"
					4: AttachType = "Vertex"
				)
				
				format "<ClothAttachments>\n"				to:Stream
				format "<ClothAttachment Type=\"%\" "		(AttachType) to:Stream
				format "TwoWayAttachment=\"%\" "			(if ClothMod.ClothFlagTwoWayAttachment == true then "1" else "0") to:Stream
				format "TearableAttachment=\"%\" />\n"		(if ClothMod.ClothFlagTearableAttachment == true then "1" else "0") to:Stream
				format "</ClothAttachments>\n" to:Stream
				
				AllocateMemory (BWMesh.Verts.count * 1000)
				
				format "<MeshGroup Name=\"%\">\n" (cutstring Objs[i].name DAOModelEx.Extension) to:Stream
				StreamOutputMesh Stream MeshObj DAOInputMeshData BWMesh
				format "</MeshGroup>\n" to:Stream
				
				try (delete MeshObj)catch()
				
				--****************
				--Recurse Children
				--****************
				ChildMetaData = ExportMMHChilds Stream (Objs[i].Children) Progress
				MetaData = MetaDataJoin MetaData ChildMetaData
				
				format "</NodeCloth>\n" to:Stream
			)
		)
		
		--*************************************************************
		--Collision Objects come here	..do not recurse after Collision
		--***************************************************************
		for obj in objs do if (GetObjectExportType obj) == #Collision then append CollShapes obj
		
		if CollShapes.count > 0 and Objs[1].parent != undefined then
		(
			local isShapesValid = true
			local IsPrimitive
			local DimX
			local DimY
			local DimZ
			
			for i = 1 to CollShapes.count do
			(
				if (superclassof CollShapes[i] != GeometryClass) then isShapesValid = false
			)

			if not isShapesValid then
			(
				local CollMSG = ""
				for Shp in CollShapes do CollMSG += Shp as string+"\n"
				MessageBox ("Error in Collision Object:\n"+CollMSG as string+"\n\nInvalid Object Type.\nAllowed is Box,Sphere,Capsule,Cylinder (primitives), and Editable Poly,Editable Mesh\n\nSkipping..") title:"Model Export"
			)
			
			if isShapesValid then
			(
				if CollShapes[1].type == 1 then	--type is equal among all brethren collision, reading one of them only
				(
					format "<CollisionObject Kinematic=\"true\">\n" to:Stream
				)
				if CollShapes[1].type == 2 then
				(
					format "<CollisionObject Static=\"true\">\n" to:Stream
				)
				for i = 1 to CollShapes.count do
				(
					ProgressDialog.BarSmall.value = Progress[1]*100/Progress[2]
					ProgressDialog.Label.text = ("Writing MMH Model Data \""+CollShapes[i].name+"\"")
					Progress[1] += 1
					
					--******************************************
					--Getting Collision Transform in Worldcoords
					--Mark the Beginning of the Shape
					--******************************************
					if (classof CollShapes[i]) == Box and (IsPrimitiveShape CollShapes[i]) then
					(
						IsPrimitive = true
						objTransform = (translate CollShapes[i].objecttransform (CollShapes[i].center - CollShapes[i].objecttransform.translationpart))
						format "<Shape Name=\"%\" Type=\"Box\" AllowEmitterSpawn=\"1\" Fadeable=\"false\" " (CollShapes[i].name) to:Stream
						DimX = CollShapes[i].width  * CollShapes[i].objecttransform.Scale.X / 2 / DAOTools.WorldScale
						DimY = CollShapes[i].length * CollShapes[i].objecttransform.Scale.Y / 2 / DAOTools.WorldScale
						DimZ = CollShapes[i].height * CollShapes[i].objecttransform.Scale.Z / 2 / DAOTools.WorldScale
						format "DimX=\"%\" DimY=\"%\" DimZ=\"%\" " (DimX) (DimY) (DimZ) to:Stream
					)
					else if (classof CollShapes[i]) == Sphere and (IsPrimitiveShape CollShapes[i]) then
					(
						IsPrimitive = true
						objTransform = (CollShapes[i].objecttransform)
						if CollShapes[i].recenter == true then objTransform = pretranslate objTransform [0,0,CollShapes[i].Radius]		--when Sphere set to "base to pivot"
						format "<Shape Name=\"%\" Type=\"Sphere\" AllowEmitterSpawn=\"1\" Fadeable=\"false\" " (CollShapes[i].Name) to:Stream
						DimX = CollShapes[i].Radius * CollShapes[i].Scale.X / DAOTools.WorldScale
						format "Radius=\"%\" " (DimX) to:Stream
					)
					else if ((classof CollShapes[i]) == Capsule or (classof CollShapes[i]) == Cylinder) and (IsPrimitiveShape CollShapes[i]) then
					(
						IsPrimitive = true
						objTransform = (CollShapes[i].objecttransform)
						format "<Shape Name=\"%\" Type=\"Capsule\" AllowEmitterSpawn=\"1\" Fadeable=\"false\" " (CollShapes[i].Name) to:Stream
						DimX = CollShapes[i].Radius * CollShapes[i].Scale.X / DAOTools.WorldScale
						DimZ = CollShapes[i].Height * CollShapes[i].Scale.Z / DAOTools.WorldScale
						if (classof CollShapes[i]) == Capsule and CollShapes[i].heighttype == 0 then DimZ = DimZ - (DimX * 2)	--substract both caps from height, when height set to overall
						format "Radius=\"%\" Height=\"%\" " (DimX) (DimZ) to:Stream
					)
					else if (superclassof CollShapes[i] == GeometryClass) then
					(
						IsPrimitive = false
						objTransform = CollShapes[i].transform
						format "<Shape Name=\"%\" Type=\"Mesh\" AllowEmitterSpawn=\"1\" Fadeable=\"false\" " (CollShapes[i].Name) to:Stream
					)
					
					--*******************************
					--Output common stuff
					--******************************
					format "GROUP_MASK_WALKABLE=\"%\" " (if CollShapes[i].Walkable == 1 then "true" else "false") to:Stream
					format "GROUP_MASK_NONWALKABLE=\"%\" " (if CollShapes[i].Walkable == 2 then "true" else "false") to:Stream
					format "GROUP_MASK_ITEMS=\"%\" " (if CollShapes[i].Mask == 2 then "true" else "false") to:Stream
					format "GROUP_MASK_CREATURES=\"%\" " (if CollShapes[i].Mask == 3 then "true" else "false") to:Stream
					format "GROUP_MASK_PLACEABLES=\"%\" " (if CollShapes[i].Mask == 4 then "true" else "false") to:Stream
					format "GROUP_MASK_STATICGEOMETRY=\"%\" " (if CollShapes[i].Mask == 5 then "true" else "false") to:Stream
					format "GROUP_MASK_TRIGGERS=\"%\" " (if CollShapes[i].Mask == 6 then "true" else "false") to:Stream
					format "GROUP_MASK_TERRAIN_WALL=\"%\" " (if CollShapes[i].Mask == 7 then "true" else "false") to:Stream
					
					OutTransform = ConvertOutputTransform CollShapes[i] objTransform
					--!!! Rotation must come first in collision.. requirement by MMH converter PHY creation
					format "Rotation=\"% % % %\" " (OutTransform.rot.x) (OutTransform.rot.y) (OutTransform.rot.z) (OutTransform.rot.w) to:Stream
					format "Position=\"% % % 1.0\" >\n" (OutTransform.pos.x) (OutTransform.pos.y) (OutTransform.pos.z) to:Stream
					
					if IsPrimitive == false then
					(
						local CollMesh = SnapshotasMesh CollShapes[i]
						--local InvCollTransform = inverse CollShapes[i].transform
						local InvCollTransform = pretranslate (inverse CollShapes[i].transform.rotationpart as matrix3) -(CollShapes[i].transform.translationpart)	--cutting the scale
						local CollVert
						local CollFace
						local CollFaceX
						local CollFaceY
						local CollFaceZ
						
						if (getnumVerts CollMesh) > 300 then
						(
							MessageBox ("Warning:\n\nCollision Object \""+CollShapes[i].Name+"\" has a huge amount of Vertices (over 300).\nToo many can slow down the game.") title:"Model Export"
						)
						
						format "<VertexData length=\"%\">\n" (getnumVerts CollMesh) to:Stream
						for j = 1 to (getnumVerts CollMesh) do
						(
							CollVert = ((getVert CollMesh j) * InvCollTransform) / DAOTools.WorldScale
							format "% % %\n" (CollVert.x) (CollVert.y) (CollVert.z) to:Stream
						)
						format "</VertexData>\n" to:Stream
						
						format "<IndexData>\n" to:Stream
						for j = 1 to (getNumFaces CollMesh) do
						(
							CollFace = getFace CollMesh j
							CollFaceX = (((CollFace.x-1) as integer) as string)
							CollFaceY = (((CollFace.y-1) as integer) as string)
							CollFaceZ = (((CollFace.z-1) as integer) as string)
							format "% % %\n" (CollFaceX) (CollFaceY) (CollFaceZ) to:Stream
						)
						format "</IndexData>\n" to:Stream
						
						try (delete CollMesh)catch()
					)
					format "</Shape>\n" to:Stream
				)
				format "</CollisionObject>\n" to:Stream
			)
		)
		MetaData
	)
	
	fn ExportMSHChilds Stream objs Progress = (
	
		local MetaData = MetaDataStruct()
		local DAOInputMeshData
		local BWMesh
		local MeshObj
		
		for i = 1 to Objs.count do
		(
			if (GetMeshParams Objs[i] > 0) and (superclassof Objs[i] == GeometryClass) then
			(
				ProgressDialog.Label.text = ("Writing Mesh Chunk \""+Objs[i].name+"\"")
				
				--Create the Snapshot, with Edit Normal modifier if possible
				MeshObj = MakeMeshSnapShot Objs[i]
				
				--Collect all relevant Input Mesh Data, modifiers, channels, and Material Settings for Mesh Export
				DAOInputMeshData = GetInputMeshData MeshObj Objs[i]
				
				--Convert to streamable output format.
				BWMesh = ConvertInputMesh MeshObj DAOInputMeshData
				
				--Output
				AllocateMemory (BWMesh.Verts.count * 1000)
				ProgressDialog.BarBig.value = ((Progress[1]-0.5)*90/Progress[2])
				ProgressDialog.Label.text = ("Write Output: "+Objs[i].name+"\"")
				--format "<MeshGroup Name=\"%\" Optimize=\"None\">\n" (cutstring Objs[i].name DAOModelEx.Extension) to:Stream
				--format "<MeshGroup Name=\"%\" Optimize=\"Indices\">\n" (cutstring Objs[i].name DAOModelEx.Extension) to:Stream
				format "<MeshGroup Name=\"%\" Optimize=\"All\">\n" (cutstring Objs[i].name DAOModelEx.Extension) to:Stream
				
				StreamOutputMesh Stream MeshObj DAOInputMeshData BWMesh
				
				format "</MeshGroup>\n" to:Stream
				
				--***********************
				--Metadata block
				--***********************
				local MetaDataPartsLightmappable	= MetaDataPartsLightmappableStruct()
				local MetaDataPartsGeometry			= MetaDataPartsGeometryStruct()
				local MetaDataPartsRenderable		= MetaDataPartsRenderableStruct()

				MetaDataPartsGeometry.MeshName = (cutstring Objs[i].name DAOModelEx.Extension)
				MetaDataPartsGeometry.Value = ""
				append MetaData.PartsGeometry MetaDataPartsGeometry
				
				MetaDataPartsRenderable.MeshName = (cutstring Objs[i].name DAOModelEx.Extension)
				MetaDataPartsRenderable.Value = ""
				append MetaData.PartsRenderable MetaDataPartsRenderable
				
				if DAOInputMeshData.StreamTexCoord1 == true then
				(
					MetaDataPartsLightmappable.MeshName = (cutstring Objs[i].name DAOModelEx.Extension)
					MetaDataPartsLightmappable.Value = ""
					MetaDataPartsLightmappable.LightmapSize = GetLightmapSize Objs[i]
					MetaDataPartsLightmappable.LightmapUVWChannel = 3
					MetaDataPartsLightmappable.LightmapUVWChannelIndex = 1
					append MetaData.PartsLightmappable MetaDataPartsLightmappable
				)
				
				ProgressDialog.BarBig.value = (Progress[1]*90/Progress[2])
				Progress[1] += 1
				
				try (delete MeshObj)catch()
				DAOInputMeshData = undefined
				BWMesh = undefined
			)
			local ChildMetaData = ExportMSHChilds Stream Objs[i].Children Progress
			MetaData = MetaDataJoin MetaData ChildMetaData
		)
		MetaData
	)
	
	fn ExportModel GOB MMHFile MSHFile = (
		local Stream = undefined
		local CMD = ""
		local IsReady
		local MetaData = MetaDataStruct()
		local ChildMetaData
		
		--local Start = localtime
		
		DAOModelEx.GOBNode = undefined
		DAOModelEx.Extension = GOB.ObjExtension
		DAOModelEx.GOBNode = getNodeByName (GOB.ObjName+GOB.ObjExtension) exact:true ignoreCase:true all:false
		DAOModelEx.UsingEditNormals = ((maxversion())[1] >= 6000)
		--DAOModelEx.UsingEditNormals = false
		DAOModelExVars.MaterialObjects = #()
		DAOModelExVars.MeshesInWorldSpace = #()
		DAOModelExVars.NodeIDCollection = #()
		DAOModelExVars.MeshIncrement = 0
		
		IsReady = CheckModelExportReady DAOModelEx.GOBNode
		if not IsReady then
		(
			MessageBox ("Aborted") title:"Model Export"
			return false
		)
		
		if DAOTools.IsGmax == true then
		(
			MessageBox "GMax users, start YAGG in Snoop mode now before continueing." title:"Model Export"
			ClearListener()
		)
		
		--**************************
		--Meta Data block
		--****************************
		MetaData.lastExportedOn = localtime
		MetaData.lastExportedBy = "Author"
		MetaData.lastExportedFrom = "Eshme's Model and Animation Import Export"
		MetaData.ModelMeshData = #((MSHFile+".MSH"))
		MetaData.PhysicsMesh = #((MMHFile+".PHY"))
		
		--****************
		--MSH File opening
		--****************
		CreateDialog ProgressDialog
		ProgressDialog.BarBig.value = 0
		ProgressDialog.BarSmall.value = 0
		ProgressDialog.Label.text = ("Creating Output MSH File")
		
		Stream = DAOTools.FOPen (DAOTools.TempPath + MSHFile + ".msh.xml")
		if Stream == undefined then
		(
			MessageBox ("Error creating output file.\n"+(DAOTools.TempPath + MSHFile + ".msh.xml")) title:"Model Export"
			try(DestroyDialog ProgressDialog)catch()
			return false
		)
		
		--*******************
		--Outputting MSH File
		--*******************
		format "<?xml version=\"1.0\" ?>\n" to:Stream
		format "<!-- Created with Eshme's DAO Model and Animation Import Export Tool Version: % -->\n" DAOTools.Version to:Stream
		format "<!-- Visit my Page at http://social.bioware.com/project/2336/  -->\n" to:Stream
		format "<!-- On : % -->\n\n" (localtime) to:Stream
		
		format "<ModelMeshData Name=\"%\" Version=\"1\">\n" (MSHFile + ".MSH") to:Stream
		ProgressDialog.Label.text = "Writing Model Data"
		
		--disableSceneRedraw()
		ChildMetaData = ExportMSHChilds Stream #(DAOModelEx.GOBNode) #(1,(GetHierarchy #(DAOModelEx.GOBNode) Limit:#Mesh).count)
		MetaData = MetaDataJoin MetaData ChildMetaData
		--enableSceneRedraw()
		
		format "</ModelMeshData>\n" to:Stream
		
		--****************
		--Closing MSH File
		--****************
		if not(DAOTools.isgmax) then flush Stream
		DAOTools.Fclose Stream
		
		--****************
		--MMH File opening
		--****************
		ProgressDialog.Label.text = "Creating Output MMH File"
		Stream = DAOTools.FOPen (DAOTools.TempPath + MMHFile + ".mmh.xml")
		if Stream == undefined then
		(
			MessageBox ("Error creating output file.\n"+(DAOTools.TempPath + MMHFile + ".mmh.xml")) title:"Model Export"
			return false
		)
		
		--*******************
		--Outputting MMH File
		--*******************
		format "<?xml version=\"1.0\" ?>\n" to:Stream
		format "<!-- Created with Eshme's DAO Model and Animation Import Export Tool Version: % -->\n" DAOTools.Version to:Stream
		format "<!-- Visit my Page at http://social.bioware.com/project/2336/  -->\n" to:Stream
		format "<!-- On : % -->\n\n" (localtime) to:Stream
		
		format "<ModelHierarchy Name=\"%\" ModelDataName=\"%\"" (MMHFile + ".MMH") (MSHFile + ".MSH") to:Stream
		
		if (classof DAOModelEx.GOBNode) == DAO_Gameobject and DAOModelEx.GOBNode.FXActorName != "" then
		(
			format " FXActorName=\"%\"" (DAOModelEx.GOBNode.FXActorName) to:Stream
		)
		format ">\n" to:Stream
		
		ProgressDialog.Label.text = "Writing Model Data"
		
		ChildMetaData = ExportMMHChilds Stream #(DAOModelEx.GOBNode) #(1,(GetHierarchy #(DAOModelEx.GOBNode)).count)
		MetaData = MetaDataJoin MetaData ChildMetaData
		
		format "</ModelHierarchy>\n" to:Stream
		
		--****************
		--Closing MMH File
		--****************
		if not(DAOTools.isgmax) then flush Stream
		DAOTools.Fclose Stream
		ProgressDialog.BarBig.value = 92
		
		--********************
		--Creating MAO Files
		--********************
		if DAOModelEx.ExportMAO == true then
		(
			ProgressDialog.Label.text = "Creating Output MAO File(s)"
			ExportMAO()
		)
		ProgressDialog.BarBig.value = 94
		
		--****************
		--META File opening
		--****************
		ProgressDialog.Label.text = "Creating Output Meta File"
		Stream = DAOTools.FOPen (DAOTools.OutputPath + MMHFile + ".met")
		if Stream == undefined then
		(
			MessageBox ("Error creating output file.\n"+(DAOTools.OutputPath + MMHFile + ".met")) title:"Model Export"
			try(DestroyDialog ProgressDialog)catch()
			return false
		)
		--*******************
		--Outputting META File
		--*******************
		ExportMetaData Stream MetaData
		
		--****************
		--Closing META File
		--****************
		if not(DAOTools.isgmax) then flush Stream
		DAOTools.Fclose Stream
		ProgressDialog.BarBig.value = 96
		
		--***************************
		--Processing via bat file
		--***************************
		ProgressDialog.Label.text = "Processing XML Files.."
		Stream = DAOTools.FOPen (DAOTools.TempPath + "Donotrun.bat")
		
		format ((Substring DAOTools.OutputPath 1 2)+"\n") to:Stream
		format ("cd \"%\"\n") DAOTools.OutputPath to:Stream
		
		format ("\""+DAOTools.GamePath + "tools\\ResourceBuild\\Processors\\MMH\\GraphicsProcessorMMH.exe\" ") to:Stream
		format ((DAOTools.TempPath + MMHFile + ".mmh.xml")+" > " + DAOTools.TempPath + "MMHProcessorMSG.txt\n") to:Stream
		
		format ("\""+DAOTools.GamePath + "tools\\ResourceBuild\\Processors\\MSH\\GraphicsProcessorMSH.exe\" -platform pc mmdtogff ") to:Stream
		format ((DAOTools.TempPath + MSHFile + ".msh.xml")+" > " + DAOTools.TempPath + "MSHProcessorMSG.txt\n") to:Stream
		--format ("pause") to:stream
		
		DAOTools.Fclose Stream
		CMD = ("\""+DAOTools.TempPath + "Donotrun.bat\"")
		DAOTools.CMD (CMD)
		ProgressDialog.BarBig.value = 100
		
		--*********
		--Done here
		--*********
		DestroyDialog ProgressDialog
		--format ("Running: "+(TimeDiff Start localtime)+"\n")
		MessageBox ("Finished"+(if DAOTools.isgmax then "\n\nGmax Users:\nWait for Yagg to finish up (can take long for big models)" else "")) title:"Model Export"
	)
	
	rollout ModelExportDialog "Model Export" width:232 height:285
	(
		GroupBox grp_gobs "Available Model Root GOBs:" pos:[5,1] width:220 height:45
		dropdownList ddl_gobs "" pos:[15,19] width:182 height:21 --items:#("GOB_Secondary01")
		
		GroupBox grp17 "Game Pivot Control" pos:[5,48] width:220 height:53
		radiobuttons rdo_gobs "" pos:[17,64] width:153 height:32 labels:#("World Center is Game Pivot", "GOB is Game Pivot")
		
		GroupBox grp_seqs "File Names" pos:[5,103] width:220 height:83
		edittext edt_filenameMMH "" pos:[48,122] width:139 height:17
		label lbl_filenameMMH "Name:" pos:[18,124] width:33 height:15
		label lbl_extMMH ".MMH" pos:[190,124] width:30 height:15
		edittext edt_filenameMSH "" pos:[48,161] width:139 height:17 enabled:false
		label lbl_filenameMSH "Name:" pos:[18,163] width:33 height:15
		label lbl_extMSH ".MSH" pos:[190,163] width:30 height:15
		checkbox chk_changeMSHname "Change MSH Filename" pos:[52,143] width:152 height:16
		
		GroupBox grp_options "Export Options" pos:[5,188] width:220 height:55
		checkbox chk_createMAO "Create MAO Material File(s)" pos:[17,206] width:193 height:16
		label lbl_WS "WorldScale (from Setup):" pos:[16,226] width:131 height:13
		label lbl_Scale "1" pos:[163,227] width:53 height:13
		
		button btn_export "E X P O R T" pos:[4,246] width:223 height:34 --enabled:false
		
		local GOBs = #()
		
		--********************************************************
		--********* Dialog Initiating ****************************
		--********************************************************
		fn UpdateDialog = (
			GOBs = GetGobs()
			local GOBHold = #()
			for GOB in GOBs where GOB.IsGOB == true do append GOBHold (GOB.ObjName+GOB.ObjExtension)
			ddl_gobs.items = GOBHold
		)
		
		on ModelExportDialog open do
		(
			UpdateDialog()
			lbl_Scale.text = (DAOTools.WorldScale as string)
			
			DAOModelEx.ExportMAO = chk_createMAO.state = false
			rdo_gobs.state = DAOModelEx.GOBOffset
			if not(DAOTools.GamePathValid == true) or not(DAOTools.OutputPathValid == true) then
			(
				MessageBox "Output Paths are invalid or not set. Please run DAO Setup Tool" title:"Error"
				try (DestroyDialog DAOAnimToolsRollout)catch()
			)
		)
		
		--********************************************************
		--********* Settings  ************************************
		--********************************************************
		on chk_createMAO changed state do DAOModelEx.ExportMAO = state
		
		on chk_changeMSHname changed state do 
		(
			if state == false then edt_filenameMSH.text = edt_filenameMMH.text
			edt_filenameMSH.enabled = state
		)
		on edt_filenameMMH entered text do
		(
			edt_filenameMMH.text = FixOutputName text #(".mmh",".mmh.xml",".xml")
			if chk_changeMSHname.checked == false then edt_filenameMSH.text = edt_filenameMMH.text
		)
		on edt_filenameMSH entered text do
		(
			edt_filenameMSH.text = FixOutputName text #(".msh",".msh.xml",".xml")
		)		
		
		on rdo_gobs changed value do DAOModelEx.GOBOffset = Value
		
		--********************************************************
		--********* Export Button Pressed ************************
		--********************************************************
		on btn_export pressed do with undo off
		(
			if GOBs.count > 0 and ddl_gobs.selection > 0 and (getNodeByName (GOBs[ddl_gobs.selection].ObjName+GOBs[ddl_gobs.selection].ObjExtension) exact:true ignoreCase:true all:false) != undefined then
			(
				if (GOBs[ddl_gobs.selection].ObjExtension).count > 0 and (GOBs[ddl_gobs.selection].ObjExtension).count <= 4 then
				(
					MessageBox ("Warning!\n\nMake sure the GOB (\"" + (GOBs[ddl_gobs.selection].ObjExtension) as string + "\") name extension is distinct (usually long = better).\nThis is because of internal requirements, errors may occur.\n\nContinue..") title:"Model Export"
				)
				if edt_filenameMMH.text != "" and edt_filenameMSH.text != "" then
				(
					disablesceneredraw()
					--if ((maxversion())[1] >= 11000) and ((maxversion())[1] < 12000) then disablesceneredraw()	--due max2009 64bit making "unknown exception" during normals
					ExportModel GOBs[ddl_gobs.selection] edt_filenameMMH.text edt_filenameMSH.text
					try(DestroyDialog ProgressDialog)catch()
					--if ((maxversion())[1] >= 11000) and ((maxversion())[1] < 12000) then enablesceneredraw()
					enablesceneredraw()
				)
				else
				(
					Messagebox "Please enter a Filename for MMH and MSH first" title:"Model Export"
				)
			)
			else
			(
				MessageBox "No Game Object selected" title:"Model Export"
				UpdateDialog()
			)
		)
	)
	
	rollout DAOModelToolsRollout "Dragon Age Model Exporter" width:244 height:527
	(
		GroupBox grp1 "" pos:[22,-1] width:198 height:45
		label lbl1 "Model Exporter" pos:[75,9] width:127 height:16
		label lbl2 "Dragon Age: Origins" pos:[71,26] width:95 height:16
		edittext txt_maxversion "" pos:[0,507] width:68 height:16 enabled:false
		subRollout rollouts "" pos:[0,49] width:244 height:454
		label lbl10 "Script by Eshme" pos:[120,507] width:85 height:16 enabled:false
		label lbl11 "" pos:[210,507] width:40 height:16 enabled:false
		
		on DAOModelToolsRollout open do
		(

			lbl11.text = DAOTools.Version
			local maxver = maxVersion()		
			txt_maxversion.text = "MAX  v"+((maxver[1] / 1000) as string)
			if maxver[1] <= 4200 then txt_maxversion.text = " GMAX"
			Rollouts.height = DAOModelEx.UI_Height - 70
			lbl10.pos = [lbl10.pos.x,DAOModelEx.UI_Height - 17]
			lbl11.pos = [lbl11.pos.x,DAOModelEx.UI_Height - 17]
			txt_maxversion.pos = [txt_maxversion.pos.x,DAOModelEx.UI_Height - 20]
		)
		on DAOModelToolsRollout close do 
		(
			DAOTools.WriteIni "ModelExport" "UI_Pos_X" DAOModelEx.UI_Pos.x
			DAOTools.WriteIni "ModelExport" "UI_Pos_Y" DAOModelEx.UI_Pos.y
			DAOTools.WriteIni "ModelExport" "UI_Height" DAOModelEx.UI_Height
		)
		on DAOModelToolsRollout resized Size do
		(
			if DAOModelToolsRollout.open then
			(
				DAOModelEx.UI_Height = Size.y
				Rollouts.height = DAOModelEx.UI_Height - 70
				lbl10.pos = [lbl10.pos.x,DAOModelEx.UI_Height - 17]
				lbl11.pos = [lbl11.pos.x,DAOModelEx.UI_Height - 17]
				txt_maxversion.pos = [txt_maxversion.pos.x,DAOModelEx.UI_Height - 20]
			)
		)
		on DAOModelToolsRollout moved pos do DAOModelEx.UI_Pos = pos
	)
	
	on execute do with undo off
	(
		if DAOModelEx == undefined do Initialize()
		--Initialize()
		--try (DestroyDialog DAOModelToolsRollout)catch()
		
		CreateDialog DAOModelToolsRollout style:#(#style_sysmenu,#style_titlebar,#style_minimizebox,#style_resizing) height:DAOModelEx.UI_Height lockWidth:true pos:DAOModelEx.UI_Pos
		AddSubRollout DAOModelToolsRollout.Rollouts ModelExportDialog rolledup:false

	)
)