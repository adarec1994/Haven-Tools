/**********************************************************************************************************
Dragon Age Origins Tools
Author: Eshme  http://social.bioware.com/project/2336/

last edit:
13 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Include Script
DAOModelExportHelper.ms
Helper Functions for Model Export
************************************************************************************************************
*/

	struct MetaDataPartsListStruct (
		MeshName,
		Value,
		ID,
		BBox
	)
	
	struct MetaDataPartsLightmappableStruct (
		MeshName,
		Value,
		LightmapSize,
		LightmapUVWChannel,
		LightmapUVWChannelIndex
	)
	
	struct MetaDataPartsGeometryStruct (
		MeshName,
		Value
	)
	
	struct MetaDataPartsRenderableStruct (
		MeshName,
		Value
	)
	
	struct MetaDataStruct (
		--ModificationTrackingInformation
		lastExportedOn,
		lastExportedBy,
		lastExportedFrom,
		--DependencyInformation
		ModelMeshData = #(),
		PhysicsMesh = #(),
		MaterialObject = #(),
		--ResourceInformation
		GameResource,
		Type,
		Title,
		Description,
		LevelType,		--these 5 for levelprops?
		LevelStyle1,
		LevelStyle2,
		LevelModularSystem,
		LevelLODLevels,
		CharacterType,	--these 3 for creatures?
		CharacterStyle1,
		CharacterStyle2,
		--Parts
		PartsList = #(),
		PartsLightmappable = #(),
		PartsGeometry = #(),
		PartsRenderable = #()
	)
	
	struct DAOInputMeshStruct (
	
		Obj,
		MeshObj,
		MeshName,
		InvMeshTransform,
		InWorldSpace,
		VertexCount,
		FaceCount,
		
		StreamPosition = false,
		StreamNormal = false,
		StreamTexCoord0 = false,
		StreamTexCoord1 = false,
		StreamBinormal = false,
		StreamTangent = false,
		StreamColor0 = false,
		StreamBlendWeights = false,
		StreamBlendIndices = false,
		
		HasMap0 = false,
		HasMap1 = false,
		HasMapAlpha = false,
		HasMapColor = false,
		
		UseMapColor = false,
		UseMapAlpha = false,
		OverrideColorValue = (color 0 0 0),
		OverrideAlphaValue = 0.0,
		
		HasSkinMod = false
	)
	
	struct BWMeshStruct (
		Verts = #(),		--Final Vertices (indices)
		--VertsIndex,			--Current Indexing
		TVertsAlpha = #(),
		TVertsColor = #(),
		TVerts1 = #(),		--Final TVerts (indices)
		TVerts2 = #(),		--Final TVerts (indices)
		Norms = #(),		--Final Normals
		PlusVerts = #(),	--Fraction added by splitting verts
		PlusTVertsAlpha = #(),
		PlusTVertsColor = #(),
		PlusTVerts1 = #(),
		PlusTVerts2 = #(),
		PlusNorms = #(),
		Faces = #(),		--Final Face Data
		FacesPerVert = #(),	--Faces connected to each Vert (indices), for tangent generation
		PlusFacesPerVert = #(),	--These for splitted verts
		VertCounter = 0,
		PlusCounter = 0,
		VertMapping = #()		--mapping original vertex index to the optimized final index
	)
	
	struct FaceDataStruct (
		Face,
		FaceList = #(),
		TVertChAlpha,
		TVertChColor,
		TVertCh1,
		TVertCh2,
		Normal
	)
	
	struct BlendWeightStruct (
		Weight = #(),
		Index = #()
	)
	
	fn IsValidSkinMod SkinObj = (
		--Random test function for tests
		try
		(
			max modify mode
			select SkinObj
			local testme = skinops.GetNumberBones SkinObj.modifiers[1]
			return true
		)
		catch
		(
			return false
		)
	)
	
	fn GetInputMeshData TheMesh TheObj = (
		local Out = DAOInputMeshStruct()
		local Mat
		
		Out.Obj = TheObj
		Out.MeshName = TheObj.Name
		--Out.MeshObj = TheMesh
		Out.InvMeshTransform = inverse TheObj.transform		--compensator, which can switch mesh data from Object to World Space and vice versa if needed.
		Out.VertexCount = TheMesh.numverts
		Out.FaceCount = TheMesh.numfaces
		
		Out.HasMapAlpha = meshop.GetMapSupport TheMesh -2
		Out.HasMapColor = meshop.GetMapSupport TheMesh 0
		Out.HasMap0 = meshop.GetMapSupport TheMesh 1
		Out.HasMap1 = meshop.GetMapSupport TheMesh 2
		Out.HasSkinMod = (GetModifier TheObj Skin) == 1 -- > 0
		
		Mat = TheObj.material
		if ClassOf Mat == Multimaterial then Mat = Mat.MaterialList[(GetFaceMatID TheMesh 1)]
		
		if ClassOf Mat == DAOMaterial then
		(
			append DAOModelExVars.MaterialObjects #(TheObj,Mat)
			case Mat.MatTypeID of
			(
				1:(Out.StreamPosition = true)
				2:(Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTangent = Out.StreamBinormal = true)
				3:(Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTangent = Out.StreamBinormal = true)
				4:(Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTangent = Out.StreamBinormal = true)
				5:(Out.StreamPosition = Out.StreamTexCoord0 = true)
				6:(
					Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTangent = Out.StreamBinormal = true
					case Mat.MatSemanticID of
					(
						60:(Out.StreamTexCoord1 = true)
						61:(Out.StreamColor0 = true)
						62:(Out.StreamColor0 = true)
						63:(Out.StreamColor0 = true)
						64:(Out.StreamColor0 = true)
					)
				)
				7:(
					Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTangent = Out.StreamBinormal = true
					case Mat.MatSemanticID of
					(
						67:(Out.StreamColor0 = true)
						68:(Out.StreamColor0 = true)
					)
				)
				8:(Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTangent = Out.StreamBinormal = true)
				9:(Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTangent = Out.StreamBinormal = true)
				10:(Out.StreamPosition = Out.StreamTexCoord0 = true)
				11:(Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamColor0 = Out.StreamTangent = Out.StreamBinormal = true)
				12:(Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTexCoord1 = true)
				13:(Out.StreamPosition = Out.StreamNormal = Out.StreamTangent = Out.StreamBinormal = Out.StreamTexCoord0 = Out.StreamColor0 = true)
				14:(Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTangent = Out.StreamBinormal = true)
				15:(Out.StreamPosition = Out.StreamTexCoord0 = true)
				16:(Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTexCoord1 = Out.StreamTangent = Out.StreamBinormal = true)
				17:(Out.StreamPosition = Out.StreamTexCoord0 = Out.StreamColor0 = Out.StreamNormal = true)
				18:(
					Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamColor0 = Out.StreamTangent = Out.StreamBinormal = true
					Out.UseMapColor = true	--Mat.Mat_UseColorChannel
					Out.UseMapAlpha = true	--Mat.Mat_UseAlphaChannel
					Out.OverrideColorValue = (color 127 127 127)	--Mat.Mat_VertexColor
					Out.OverrideAlphaValue = 0.5	--Mat.Mat_VertexAlpha
				)
				19:(Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTangent = Out.StreamBinormal = true)
				20:(Out.StreamPosition = Out.StreamTexCoord0 = Out.StreamColor0 = true)
			)
		)
		else	--if not a DAO Material. Make it basic
		(
			Out.StreamPosition = Out.StreamNormal = Out.StreamTexCoord0 = Out.StreamTangent = Out.StreamBinormal = true
			Out.StreamColor0 = QueryBox ("No Dragon Age Material was assigned to \""+TheObj.name+"\".\n\nDoes the targetted Material require Vertex Color? Yes/No") title:"Model Export"
			Out.StreamTexCoord1 = QueryBox ("No Dragon Age Material was assigned to \""+TheObj.name+"\".\n\nDoes the targetted Material require UV Channel 2 (Lightmap etc)? Yes/No") title:"Model Export"
			if DAOModelEx.ExportMAO == true then
			(
				MessageBox ("MAO Material Export not possible for \""+TheObj.name+"\" because no Dragon Age Material was assigned.") title:"Model Export"
			)
			if ClassOf Mat == StandardMaterial then
			(
				append DAOModelExVars.MaterialObjects #(TheObj,Mat)
			)
			else
			(
				MessageBox ("Object: \""+TheObj.Name+"\" doesnt have a valid Material assigned to it.\nValid are Standard Material and Dragon Age Material.\n\nPlease assign and try again.\n\nExporting Mesh without Material (black)..") title:"Model Export"
			)
		)
		
		Out.StreamBlendWeights = (Out.HasSkinMod and (IsValidSkinMod TheObj))
		Out.StreamBlendIndices = (Out.HasSkinMod and (IsValidSkinMod TheObj))

		--*****************************************************************
		--Skin Weighted meshes are relocated to 0,0,0 due the game requirements.
		--Other meshes should not due direct mesh animations
		--*****************************************************************
		Out.InWorldSpace = false
		if Out.StreamBlendWeights == true then
		(
			Out.InWorldSpace = true
			append DAOModelExVars.MeshesInWorldSpace TheObj
		)
		
		--********************************************************************
		--Checking if all the Streams are exportable
		--Managing workarounds
		--*******************************************************************
		
		if Out.StreamTexCoord1 == true then	--when exporting 2nd channel
		(
			if Out.HasMap1 == false then
			(
				MessageBox ("Warning.. Object \""+TheObj.Name+"\"s Material requires UV Channel 2 but that Channel does not exist.\nIt wont be exported!") title:"Model Export"
				Out.StreamTexCoord1 = false
			)
		)
		
		if Out.StreamTexCoord0 == true then
		(
			if HasMap0 == false then
			(
				Out.StreamTexCoord0 = false
				MessageBox ("Warning.. Objects \""+TheObj.Name+"\"s Material requires UV Channel 1 but that Channel does not exist.\nIt wont be exported!") title:"Model Export"
				if Out.StreamTangent == true or Out.StreamBinormal == true then
				(
					MessageBox ("Warning.. the Model may not work due UV Channel 1 missing on Object \""+TheObj.Name+"\".\nTangent generation failed.") title:"Model Export"
					Out.StreamTangent = false
					Out.StreamBinormal = false
				)
				if Out.StreamTexCoord1 == true then
				(
					MessageBox ("Warning.. Objects \""+TheObj.Name+"\"s Material requires UV Channel 2,\nbut UV Channel 2 will not be exported due Channel 1 missing!") title:"Model Export"
					Out.StreamTexCoord1 = false
				)
			)
		)
		if Out.StreamColor0 == true then
		(
			if Out.UseMapColor == true and Out.HasMapColor == false then
			(
				MessageBox ("Warning.. Objects \""+TheObj.Name+"\"s Material settings require Vertex Color Channel 0 ,but that Channel does not exist.\nSee the readme on Material requirements.") title:"Model Export"
				Out.UseMapColor = false
				--Out.OverrideColorValue = (color 0 0 0)
			)
			if Out.UseMapAlpha == true and Out.HasMapAlpha == false then
			(
				MessageBox ("Warning.. Objects \""+TheObj.Name+"\"s Material settings require Vertex Alpha Channel -2 but that Channel does not exist.\nSee the readme on Material requirements.") title:"Model Export"
				Out.UseMapAlpha = false
				--Out.OverrideAlphaValue = 1.0
			)
		)
		Out
	)
	
	fn GetLightmapSize Obj = (
		--************************************************
		--Getting the rough size to write to the Meta file
		--Should be good enough i guess.
		--Todo: get size depending on surface size, instead bbox size.
		--************************************************
		local LMSize
		local BBox = (Obj.max - Obj.min) / DAOTools.WorldScale
		local ObjSize = BBox.x + BBox.y + BBox.z
		
		if ObjSize < 2.0 then LMSize = 8
		if ObjSize >= 2.0 and ObjSize < 6.0 then LMSize = 16
		if ObjSize >= 6.0 and ObjSize < 12.0 then LMSize = 32
		if ObjSize >= 12.0 and ObjSize < 25.0 then LMSize = 64
		if ObjSize >= 25.0 and ObjSize < 50.0 then LMSize = 128
		if ObjSize >= 50.0 and ObjSize < 100.0 then LMSize = 256
		if ObjSize >= 100.0 then LMSize = 512
		
		LMSize
	)
	
	struct OutputTransformStruct (
		Pos,
		Rot
	)
	
	fn ConvertOutputTransform Obj ObjTransform = (
		--*********************************************************************************************************************************
		--Calculate the position and rotation for output ,individual for each kind of situation.
		--By default the ObjTransform is recalculated into parent space, but some events require something else
		--.................
		--"Obj" is the Object
		--"ObjTransform" is the location, in particular this is also the actual required game location of predefined objects 
		--(ie the center of a box) even if the Pivot is actually elsewhere.
		--.................
		--Notes:
		--This is accounting to Transform Values only. The Objects Data (vertices etc) are treated accordingly in other functions.
		--!! watch priority top bottom !!
		--**********************************************************************************************************************************
		local OutputTransform = OutputTransformStruct()
		
		if Obj.parent == undefined then
		(
			--Exporting GOB as zero, no exception
			OutputTransform.pos = [0,0,0]
			OutputTransform.rot = (quat 0 0 0 1)
		)
		else if (GetObjectExportType Obj) == #Mesh and (finditem DAOModelExVars.MeshesInWorldSpace Obj) > 0 then
		(
			--This in particular is skinned meshes, relocate to 0 0 0. (Mesh Export is countering this)
			OutputTransform.pos = [0,0,0]
			OutputTransform.rot = (quat 0 0 0 1)
		)
		else if (GetObjectExportType Obj.parent) == #Weapontrail then
		(
			--When Weapontrail is the Parent, the Parent Space is in its "objecttransform"
			OutputTransform.pos = (ObjTransform * inverse Obj.parent.objecttransform).translationpart / DAOTools.WorldScale
			OutputTransform.rot = inverse (ObjTransform * inverse Obj.parent.objecttransform).rotationpart
		)
		else if Obj.parent.parent == undefined and DAOModelEx.GOBOffset == 1 then
		(
			--This is direct childs of GOB , only at world space setting
			OutputTransform.pos = ObjTransform.translationpart / DAOTools.WorldScale
			OutputTransform.rot = inverse ObjTransform.rotationpart
		)
		else
		(
			--default for the rest. Parent space
			OutputTransform.pos = (ObjTransform * inverse Obj.parent.transform).translationpart / DAOTools.WorldScale
			OutputTransform.rot = inverse (ObjTransform * inverse Obj.parent.transform).rotationpart
		)
		OutputTransform
	)
	
	fn CheckModelExportReady RootNode = (
		--*******************************************
		--function query model stats and check errors
		--used prior model export main function
		--*******************************************
		
		local Stats = GetModelStats RootNode
		local IsReady = true
		local WarningText = ""
		local MaxLines = 20
		local CurrentLine = 1
		
		--*************************
		--GOB is alone--> Warning
		--************************
		if Stats.NodeCount == 1 then
		(
			WarningText += "No Object is a Child to GOB. Nothing will be exported!\n\n"
		)
		--***********************************
		--Some Object is undefined --> Warning
		--***********************************
		if Stats.UndefinedNodes.count > 0 then
		(
			WarningText += "Undefined Objects found in the Hierarchy will not get exported!\n(no Dragon Age Parameters ,invalid or unknown)\n-----------------\n"
			for Node in Stats.UndefinedNodes do
			(
				if CurrentLine < MaxLines then WarningText += (Node.Name+"\n")
				CurrentLine += 1
			)
			if CurrentLine >= MaxLines then WarningText += "...more (" + Stats.UndefinedNodes.count as string + " Nodes)\n"
			WarningText += "-----------------\n\n"
			
			if Stats.UndefinedNodesChilds.count > 0 then
			(
				WarningText += "Following Objects are Children of one or more Undefined Nodes, and wont get exported as well:\n\n-----------------\n"
				for Node in Stats.UndefinedNodesChilds do
				(
					if CurrentLine < MaxLines then WarningText += (Node.Name+"\n")
					CurrentLine += 1
				)
				if CurrentLine >= MaxLines then WarningText += "...more (" + Stats.UndefinedNodesChilds.count as string + " Nodes)\n"
				WarningText += "-----------------\n\n"
			)
		)
		--*****************************************
		--Object has outdated Parameters--> Warning
		--*****************************************
		if Stats.OutdatedNodes.count > 0 then
		(
			WarningText += "Following Object(s) found in the Hierarchy have outdated Dragon Age Parameters!\nCan cause crashes, make sure you update (via \"Model Manager\")\n-------------\n"
			for Node in Stats.OutdatedNodes do
			(
				if CurrentLine < MaxLines then WarningText += (Node.Name+"\n")
				CurrentLine += 1
			)
			if CurrentLine >= MaxLines then WarningText += "...more (" + Stats.OutdatedNodes.count as string + " Nodes)\n"
			WarningText += "-----------------\n\n"
		)
		--*******************************************
		--Unexportable Childs of Collision.
		--*******************************************
		if Stats.NonExportableChilds.count > 0 then
		(
			WarningText += "Following Object(s) found in the Hierarchy cannot be exported due being Child of Collision!\n-------------\n"
			for Node in Stats.NonExportableChilds do
			(
				if CurrentLine < MaxLines then WarningText += (Node.Name+"\n")
				CurrentLine += 1
			)
			if CurrentLine >= MaxLines then WarningText += "...more (" + Stats.NonExportableChilds.count as string + " Nodes)\n"
			WarningText += "-----------------\n\n"
		)
		
		--*****************************************
		--Object is improperly scaled. Needs Xform
		--*****************************************
		if Stats.ScaledNodes.count > 0 then
		(
			WarningText += "Following Object(s) found in the Hierarchy are improperly scaled, and their Xform should be resetted to avoid errors\n-------------\n"
			for Node in Stats.ScaledNodes do
			(
				if CurrentLine < MaxLines then WarningText += (Node.Name+"\n")
				CurrentLine += 1
			)
			if CurrentLine >= MaxLines then WarningText += "...more (" + Stats.ScaledNodes.count as string + " Nodes)\n"
			WarningText += "-----------------\n\n"
		)
		
		if WarningText != "" then
		(
			WarningText = ("Warning!! One or more problems have been found:\n\n" + WarningText + "Model may not work as expected. Continue Export anyway?..")
			if not (QueryBox (WarningText) title:"Warning") then isReady = false
		)
		return isReady
	)

	fn GetBBox Obj = (
		
		local BBox = false
		local MinBox
		local MaxBox
		local Objs = GetHierarchy #(Obj)
		
		for i = 1 to Objs.count do
		(
			if (GetObjectExportType Objs[i]) == #Mesh then
			(
				--Get Mesh min/max
				local MeshObj = snapshotasmesh Objs[i]
				local Vert
				MinBox = undefined
				MaxBox = undefined
				for v = 1 to MeshObj.numverts do
				(
					Vert = getVert MeshObj v
					if DAOModelEx.GOBOffset == 2 then Vert = Vert * inverse (GetGOB Objs[i]).transform
					if MinBox == undefined then
					(
						MinBox = copy Vert
						MaxBox = copy Vert
					)
					else
					(
						if Vert.x < MinBox.x then (MinBox.x = Vert.x)
						if Vert.y < MinBox.y then (MinBox.y = Vert.y)
						if Vert.z < MinBox.z then (MinBox.z = Vert.z)
						if Vert.x > MaxBox.x then (MaxBox.x = Vert.x)
						if Vert.y > MaxBox.y then (MaxBox.y = Vert.y)
						if Vert.z > MaxBox.z then (MaxBox.z = Vert.z)
					)
				)
				try(delete MeshObj)catch()
				MinBox = MinBox / DAOTools.WorldScale
				MaxBox = MaxBox / DAOTools.WorldScale
				--Get all meshes min/max
				if BBox == false then
				(
					BBox = #(MinBox,MaxBox)
				)
				else
				(
					if MinBox.x < BBox[1].x then BBox[1].x = MinBox.x
					if MinBox.y < BBox[1].y then BBox[1].y = MinBox.y
					if MinBox.z < BBox[1].z then BBox[1].z = MinBox.z
					if MaxBox.x > BBox[2].x then BBox[2].x = MaxBox.x
					if MaxBox.y > BBox[2].y then BBox[2].y = MaxBox.y
					if MaxBox.z > BBox[2].z then BBox[2].z = MaxBox.z
				)
			)
		)
		BBox
	)
	
	fn MetaDataJoin MetaData JoinMetaData = (
		--************************************************
		--Appending 2 sets of Metadata
		--used to collect Metadata from various functions.
		--*************************************************
		
		if JoinMetaData.lastExportedOn != undefined then MetaData.lastExportedOn = JoinMetaData.lastExportedOn
		if JoinMetaData.lastExportedBy != undefined then MetaData.lastExportedBy = JoinMetaData.lastExportedBy
		if JoinMetaData.lastExportedFrom != undefined then MetaData.lastExportedFrom = JoinMetaData.lastExportedFrom
		
		if JoinMetaData.GameResource != undefined then MetaData.GameResource = JoinMetaData.GameResource
		if JoinMetaData.Type != undefined then MetaData.Type = JoinMetaData.Type
		if JoinMetaData.Title != undefined then MetaData.Title = JoinMetaData.Title
		if JoinMetaData.Description != undefined then MetaData.Description = JoinMetaData.Description
		if JoinMetaData.LevelType != undefined then MetaData.LevelType = JoinMetaData.LevelType
		if JoinMetaData.LevelStyle1 != undefined then MetaData.LevelStyle1 = JoinMetaData.LevelStyle1
		if JoinMetaData.LevelStyle2 != undefined then MetaData.LevelStyle2 = JoinMetaData.LevelStyle2
		if JoinMetaData.LevelModularSystem != undefined then MetaData.LevelModularSystem = JoinMetaData.LevelModularSystem
		if JoinMetaData.LevelLODLevels != undefined then MetaData.LevelLODLevels = JoinMetaData.LevelLODLevels
		if JoinMetaData.CharacterType != undefined then MetaData.CharacterType = JoinMetaData.CharacterType
		if JoinMetaData.CharacterStyle1 != undefined then MetaData.CharacterStyle1 = JoinMetaData.CharacterStyle1
		if JoinMetaData.CharacterStyle2 != undefined then MetaData.CharacterStyle2 = JoinMetaData.CharacterStyle2
		
		join MetaData.ModelMeshData JoinMetaData.ModelMeshData
		join MetaData.PhysicsMesh JoinMetaData.PhysicsMesh
		join MetaData.MaterialObject JoinMetaData.MaterialObject
		
		join MetaData.PartsList JoinMetaData.PartsList
		join MetaData.PartsLightmappable JoinMetaData.PartsLightmappable
		join MetaData.PartsGeometry JoinMetaData.PartsGeometry
		join MetaData.PartsRenderable JoinMetaData.PartsRenderable
		
		return MetaData
	)
	
	fn GetNormals MeshObj CurrentVertex FacesArray = (
		--******************************************************************
		--Calculating Normals based on a Vertex ,by multiple appending Faces.
		--This will return one Normal for the number of Faces.
		--******************************************************************
		local Normals = #()
		local CurrentFaceIndex
		
		local NormID
		local NormVertID
		local AverageNormal
		local CurrentSmoothingGroup
		local NearFaceSmoothingGroup
		
		local AllVertNormals
		local done = false
		
		local UseEditNormals = (if DAOModelEx.UsingEditNormals == true then true else false)
		
		--**************************************************************************************************
		--Edit Normals, this one allows explicit Normals, and will inherit any smoothing group automatically
		--Gmax not allowed here
		--**************************************************************************************************
		if UseEditNormals == true then try
		(
			AllVertNormals = #{}
			MeshObj.modifiers[#Edit_normals].ConvertVertexSelection (#(CurrentVertex) as bitarray) AllVertNormals
			
			/*	--maybe this a alternative	(though convertfaceselection may not work on editmesh)
			for f = 1 to FacesArray.count do
			(
				AllFaceNormals = #{}
				MeshObj.modifiers[#Edit_normals].ConvertFaceSelection (#(FacesArray[f]) as bitarray) AllFaceNormals
				CurrentNormID = ((AllVertNormals * AllFaceNormals) as array)[1]		--AND filter operation, retrieves one normal ID
				Normals[f] = normalize (MeshObj.modifiers[#Edit_normals].GetNormal CurrentNormID)
			)
			*/
			for f = 1 to FacesArray.count where Normals[FacesArray.count] == undefined do	--loop, while array is not filled yet
			(
				CurrentFaceIndex = FacesArray[f]
				for Corner = 1 to (MeshObj.modifiers[#Edit_normals].GetFaceDegree CurrentFaceIndex) do
				(
					NormVertID = MeshObj.modifiers[#Edit_normals].GetVertexID CurrentFaceIndex Corner
					if NormVertID == CurrentVertex then
					(
						NormID = MeshObj.modifiers[#Edit_normals].GetNormalID CurrentFaceIndex Corner
						Normals[f] = normalize (MeshObj.modifiers[#Edit_normals].GetNormal NormID)
						
						--Shortcut, if a Vertex only shares a single Normal. like 99% the case
						if (AllVertNormals as array).count == 1 then
						(
							for r = 2 to FacesArray.count do Normals[r] = Normals[1]
						)
						--FaceData[f].Normal = FaceData[f].Normal * InvMeshTransform.rotationpart
					)
				)
			)
		)
		catch
		(
			UseEditNormals = false
		)
		
		--****************************************************
		--Smoothing Group calculation, this is just basic atm
		--****************************************************
		if UseEditNormals == false then
		(
			for f = 1 to FacesArray.count do
			(
				CurrentFaceIndex = FacesArray[f]
				AverageNormal = [0,0,0]
				CurrentSmoothingGroup = getFaceSmoothGroup MeshObj CurrentFaceIndex
				
				if CurrentSmoothingGroup == 0 then
				(
					--************************************
					--No Smoothing Group ,this is facetted
					--**************************************
					Normals[f] = normalize (getFaceNormal MeshObj CurrentFaceIndex)
				)
				else
				(
					--*************************************************************************
					--Get the Uber smoothing group, by iterating thru all possible combinations
					--**************************************************************************
					for SmoothingIterations = 1 to FacesArray.count do
					(
						for NearFaceIndex in FacesArray do
						(
							NearFaceSmoothingGroup = getFaceSmoothGroup MeshObj NearFaceIndex
							if (bit.and CurrentSmoothingGroup NearFaceSmoothingGroup) > 0 then
							(
								CurrentSmoothingGroup = (bit.or CurrentSmoothingGroup NearFaceSmoothingGroup)
							)
						)
					)
					--********************************************************
					--Then Combine participating Faces Normals of the Smoothing group
					--********************************************************
					for NearFaceIndex in FacesArray do
					(
						NearFaceSmoothingGroup = getFaceSmoothGroup MeshObj NearFaceIndex
						if (bit.and CurrentSmoothingGroup NearFaceSmoothingGroup) > 0 then
						(
							AverageNormal += GetFaceNormal MeshObj NearFaceIndex
						)
					)
					Normals[f] = (normalize AverageNormal)
					--Normals[f] = Normals[f] * InvMeshTransform.rotationpart
				)
			)
		)
		return Normals
	)
	
	fn GetTangent MeshObj FaceList Normal = (
		--*********************************************************************
		--Calculate the Average Tangent and BiTangent for any number of Faces
		--FaceList contains List of Faces, sorted by Vertex.
		--Only participating Faces of Vertex are sent
		--Will get aligned to Normal
		--**********************************************************************
		local Tangent
		local BiTangent
		local AverageTangent = [0,0,0]
		local AverageBiTangent = [0,0,0]
		
		local Face
		local MapFace
		
		local out = #()
		
		for FaceIndex in FaceList do
		(
			Face = GetFace MeshObj FaceIndex
			local Vert1 = getVert MeshObj Face.x
			local Vert2 = getVert MeshObj Face.y
			local Vert3 = getVert MeshObj Face.z
			
			MapFace = meshOp.getMapFace MeshObj 1 FaceIndex
			local TVert1 = meshOp.GetMapVert MeshObj 1 MapFace.x
			local TVert2 = meshOp.GetMapVert MeshObj 1 MapFace.y
			local TVert3 = meshOp.GetMapVert MeshObj 1 MapFace.z
			
			local Edge1 = Vert2 - Vert1
			local Edge2 = Vert3 - Vert1
			
			local TEdge1 = TVert2 - TVert1
			local TEdge2 = TVert3 - TVert1
			
			Tangent = Edge1 * TEdge2.y - Edge2 * TEdge1.y
			BiTangent = Edge1 * TEdge2.x - Edge2 * TEdge1.x
			--BiTangent = - Edge1 * TEdge2.x + Edge2 * TEdge1.x
			
			local UVFlip = if (TEdge1.x * TEdge2.y - TEdge2.x * TEdge1.y) > 0 then 1 else -1
			
			Tangent = Tangent * UVFlip
			BiTangent = BiTangent * UVFlip
			
			--Orthogonalaltlalization, to the normal
			Tangent = (Tangent - Normal * (dot Normal Tangent))
			BiTangent = (BiTangent - Normal * (dot Normal BiTangent))
			
			AverageTangent += Tangent
			AverageBiTangent += BiTangent
		)
		AverageTangent = normalize AverageTangent
		AverageBiTangent = normalize AverageBiTangent
		
		out = #(AverageTangent,AverageBiTangent)
	)
	
	
	fn GetBonesUsed SkinObj = (		--Returns integer array of used bones.. undefined if serious error occured finding indices
		local Objs
		local BonesUsedCount
		local NamesUsed = #()
		local BonesUsed = #()
		local Warning = false
		
		try
		(
			max modify mode
			select SkinObj
			
			BonesUsedCount = skinops.GetNumberBones SkinObj.modifiers[1]
			if BonesUsedCount > 67 then
			(
				MessageBox ("Warning..\n\nMesh object \""+SkinObj.name+"\" has too many Bones added to the Skinmod (over 67).\nMore Bones are not allowed and will be ignored. Split Mesh into smaller parts to avoid the problem.\nModel may not work as desired!") title:"Warning"
			)
		)
		catch
		(
			--Cannot Access Skin Mod here
			--thus...
			MessageBox ("Error reading from Skin modifier\n\nMesh object \""+SkinObj.name+"\"\n\nCritical error, cannot read Skin Data. Dont know what happend. Make sure Skin mod is on top of the stack.\n\nRuntime Error: "+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error" 
			BonesUsed = undefined
			return BonesUsed
		)
		
		--*************************************************
		--First get the list of Bone Names in the Skinmod
		--***********************************************
		for i = 1 to BonesUsedCount do
		(
			local Name = skinops.GetBoneName SkinObj.modifiers[1] i 1
			append NamesUsed Name
		)
		
		--*********************************************************************
		--Translate the Names into the BoneIndex given by the Bones parameters
		--********************************************************************
		Objs = GetHierarchy #(DAOModelEx.GOBNode)
		
		for Obj in Objs do
		(
			local ArrayIndex = findItem NamesUsed Obj.name
			
			if ArrayIndex > 0 then --When this Obj is listed in the Skin Mod do:
			(
				if (GetBoneParams Obj > 0) then
				(
					try
					(
						BonesUsed[ArrayIndex] = Obj.BoneParamIndex
						if Obj.BoneParamIndex == -1 then
						(
							MessageBox ("Warning..\n\nMesh object \""+SkinObj.name+"\" with Skin modifier has invalid Bone entry.\n\n\"Bone Index out of Range (is inactive -1): "+Obj.name+"\"\nCheck Parameters. Model may not work as desired!") title:"Warning"
						)
					)
					catch
					(
						BonesUsed[ArrayIndex] = -1
						MessageBox ("Warning..\n\nMesh object \""+SkinObj.name+"\" with Skin modifier has invalid Bone entry.\n\n\"Could not read Bone Index of Parameters: "+Obj.name+"\"\nMay be outdated Parameters?") title:"Warning"
					)
				)
				else
				(
					BonesUsed[ArrayIndex] = -1
					MessageBox ("Warning..\n\nMesh object \""+SkinObj.name+"\" with Skin modifier has invalid Bone entry.\n\n\"No Dragon Age Node: "+Obj.name+"\"\nParameters missing..") title:"Warning"
				)
			)
		)
		
		--*****************
		--Checks woop
		--***************** Buggy need fix..
		if BonesUsed.count != NamesUsed.count then
		(
			local MissingBones = #()
			for i = (BonesUsed.count + 1) to NamesUsed.count do
			(
				append MissingBones NamesUsed[i]
			)
			if MissingBones.count > 0 then
			(
				MessageBox ("Error..\n\nMesh object \""+SkinObj.name+"\" with Skin modifier has invalid Bone entry.\n\n\"No Bones exist for: \n"+(for i = 1 to MissingBones.count do (MissingBones[i]+"\n"))+"\"\nParameters missing..") title:"Error"
				Warning = true
			)
			else
			(
				Warning = true
				MessageBox ("Error..\n\nMesh object \""+SkinObj.name+"\" with Skin modifier has invalid Bone entry.\n\n\"BonesUsed.count != NamesUsed.count \n"+BonesUsed+"\n"+NamesUsed+"\n"+"\"") title:"Error"
			)
		)
		
		For Bones = 1 to BonesUsed.count do
		(
			if BonesUsed[Bones] == undefined then
			(
				Warning = true
				local Victim = NamesUsed[Bones]
				MessageBox ("Error..\n\nMesh object \""+SkinObj.name+"\" with Skin modifier has invalid Bone entry.\n\n\"Missing Bone: "+Victim+"\"\nParameters missing..") title:"Error"
			)
		)
		if Warning == true then BonesUsed = undefined
		return BonesUsed
	)
	
	fn GetBlendValues SkinObj VertsArray = (
		--***********************************************************************
		--Function to retrieve Vert Values of skinmod for all Verts being written
		--***********************************************************************
		local CurrentVert
		local WeightCount
		local Weights = #()
		local Indices = #()
		local MinWeight
		local Excess
		local Normal
		local OutValues = BlendWeightStruct()
		
		OutValues.Weight[VertsArray.count] = 0
		OutValues.Index[VertsArray.count] = 0
		
		for i = 1 to VertsArray.count do
		(
			CurrentVert = VertsArray[i]
			WeightCount = skinops.GetVertexWeightCount SkinObj.modifiers[1] CurrentVert
			
			Weights = #()
			Indices = #()
			--***********************************
			--Get all Weights that are available
			--**********************************
			for j = 1 to WeightCount do
			(
				Weights[j] = skinops.GetVertexWeight SkinObj.modifiers[1] CurrentVert j
				if not (Weights[j] >= 0) then Weights[j] = 0	--avoid buggy skinmod (indeterminant values). Must test "not greater" instead "lower"
				
				Indices[j] = skinops.GetVertexWeightBoneID SkinObj.modifiers[1] CurrentVert j
				Indices[j] -= 1	--Reduce by 1, cause BW index 0-..
				Indices[j] = Indices[j] as integer
			)
			
			--**********************************
			--Filling missing Weights.. up to 4
			--***********************************
			for j = (WeightCount + 1) to 4 do
			(
				Weights[j] = 0.0
				Indices[j] = 0 as integer
			)
			
			--********************************************
			--When more then 4 have been read, reduce to 4
			--********************************************
			while Weights.count > 4 do
			(
				MinWeight = amin Weights
				Excess = findItem Weights MinWeight
				Weights = deleteItem Weights Excess
				Indices = deleteItem Indices Excess
			)
			
			--*************************************************************
			--Normalize 4 Weights to 1, albeit the game doesnt seem to care
			--***************************************************************
			Normal = 0
			for j = 1 to Weights.count do Normal += Weights[j]
			if Normal > 0 then Normal = 1 / Normal
			for j = 1 to Weights.count do Weights[j] *= Normal
			
			--**********
			--Result
			--*********
			OutValues.Weight[i] = Weights
			OutValues.Index[i] = Indices
		)
		
		return OutValues
	)
	
	fn IsPrimitiveShape Obj = (
		--*****************************************************************************
		--Checks if Primitive object is within bounds of still being a primitive shape,
		--detected via scale vector ortogonal crosschecks, lenght uniformity.
		--Used by export function, collision primitives.
		--*****************************************************************************
		local Vector1 = Obj.objecttransform.row1
		local Vector2 = Obj.objecttransform.row2
		local Vector3 = Obj.objecttransform.row3
		local Threshold = 10	--angleskew
		
		local LengthThreshold = (length Vector2) * 0.12	--12% essentially
		local IsPrimitive = true
		
		if (classof Obj) == Capsule or (classof Obj) == Cylinder then Threshold = 15
		if (classof Obj) == Sphere then Threshold = 15
		
		--Axis skew..
		if abs ((acos (dot (normalize Vector1) (normalize Vector2))) - 90) > Threshold then (IsPrimitive = false)
		if abs ((acos (dot (normalize Vector2) (normalize Vector3))) - 90) > Threshold then (IsPrimitive = false)
		if abs ((acos (dot (normalize Vector1) (normalize Vector3))) - 90) > Threshold then (IsPrimitive = false)
		
		--Uniformity..
		if (classof Obj) == Sphere then	--all axis crosscheck
		(
			if (abs ((length Vector1) - (length Vector2))) > LengthThreshold then (IsPrimitive = false)
			if (abs ((length Vector2) - (length Vector3))) > LengthThreshold then (IsPrimitive = false)
			if (abs ((length Vector1) - (length Vector3))) > LengthThreshold then (IsPrimitive = false)
		)
		if (classof Obj) == Capsule or (classof Obj) == Cylinder then --xy axis crosscheck
		(
			if (abs ((length Vector1) - (length Vector2))) > LengthThreshold then (IsPrimitive = false)
		)
		if (classof Obj) == Capsule then	--z height, caps uniformity
		(
			if (abs ((length Vector3) - (length (normalize Vector3)))) > (LengthThreshold * 3.5) then (IsPrimitive = false)
		)
		if IsPrimitive == false then
		(
			MessageBox ("Warning!\nPrimitive Object \""+Obj.Name as string+"\" is not in primitive shape, due improper scaling.\n\nExporting as freeform mesh instead..") title:"Model Export"
		)
		IsPrimitive
	)
	
	fn MakeMeshSnapShot TheObj = (
		--************************************************************************************
		--Creating the Mesh Snapshot, in 2 different ways whether Edit Normals is used or not
		--Used in Mesh Export Function prior Mesh access
		--**********************************************************************************
		
		local MeshObj
		
		if DAOModelEx.UsingEditNormals == true then try
		(
			MeshObj = Snapshot TheObj
			addModifier MeshObj (Edit_Normals())
			max modify mode
			select MeshObj
		)
		catch
		(
			try(delete MeshObj)catch()
			max create mode
			DAOModelEx.UsingEditNormals = false
		)
		
		if DAOModelEx.UsingEditNormals == false then
		(
			MeshObj = SnapshotAsMesh TheObj
		)
		MeshObj
	)
	
	fn ConvertInputMesh MeshObj DAOInputMeshData = (
		--*************************************************************
		--Takes the Input Mesh in Max format, and some relevant Data
		--Converts it to streamable format, readable by the game
		--*************************************************************
		
		AllocateMemory (DAOInputMeshData.VertexCount * 1050)	--Oh yea, 1050byte per Vertex needed.. 8)
		--Variables for output Mesh
		local BWMesh = BWMeshStruct()
		local FaceData = #()
		local FacesArray
		local CurrentFace
		local CurrentFaceIndex
		local CurrentMapFaceChAlpha
		local CurrentMapFaceChColor
		local CurrentMapFaceCh1
		local CurrentMapFaceCh2
		local TVertsArrayChAlpha
		local TVertsArrayChColor
		local TVertsArrayCh1
		local TVertsArrayCh2
		
		if DAOModelEx.UsingEditNormals == true then	(max modify mode; select MeshObj) else (max create mode)
				--******************************
				--Prepare by getting Vertex info
				--TVerts and Faces per Vertex
				--******************************
				ProgressDialog.Label.text = ("Prepare Mesh Vertices :"+DAOInputMeshData.MeshName as string+"\"")
				local TiedTVertsChAlpha = for i = 1 to DAOInputMeshData.VertexCount collect #{}
				local TiedTVertsChColor = for i = 1 to DAOInputMeshData.VertexCount collect #{}
				local TiedTVertsCh1 = for i = 1 to DAOInputMeshData.VertexCount collect #{}
				local TiedTVertsCh2 = for i = 1 to DAOInputMeshData.VertexCount collect #{}
				local TiedFaces = for i = 1 to DAOInputMeshData.VertexCount collect #{}
				
				local Face
				local TFace
				for i = 1 to DAOInputMeshData.FaceCount do
				(
					Face = getFace MeshObj i
					if DAOInputMeshData.StreamColor0 == true then
					(
						if DAOInputMeshData.UseMapColor == true then
						(
							TFace = meshop.getMapFace MeshObj 0 i
							append TiedTVertsChColor[Face.x] TFace.x
							append TiedTVertsChColor[Face.y] TFace.y
							append TiedTVertsChColor[Face.z] TFace.z
						)
						if DAOInputMeshData.UseMapAlpha == true then
						(
							TFace = meshop.getMapFace MeshObj -2 i
							append TiedTVertsChAlpha[Face.x] TFace.x
							append TiedTVertsChAlpha[Face.y] TFace.y
							append TiedTVertsChAlpha[Face.z] TFace.z
						)
					)
					if DAOInputMeshData.StreamTexCoord0 == true then
					(
						TFace = meshop.getMapFace MeshObj 1 i
						append TiedTVertsCh1[Face.x] TFace.x
						append TiedTVertsCh1[Face.y] TFace.y
						append TiedTVertsCh1[Face.z] TFace.z
					)
					if DAOInputMeshData.StreamTexCoord1 == true then
					(
						TFace = meshop.getMapFace MeshObj 2 i
						append TiedTVertsCh2[Face.x] TFace.x
						append TiedTVertsCh2[Face.y] TFace.y
						append TiedTVertsCh2[Face.z] TFace.z
					)
					append TiedFaces[Face.x] i
					append TiedFaces[Face.y] i
					append TiedFaces[Face.z] i
				)
				--****************************************************
				--Pre Initialize Output Arrays
				--****************************************************
				--BWMesh.Verts[VertexCount] = undefined
				--BWMesh.VertsIndex = 0
				
				--******************************************************
				--Adding Faces to Output. These will need to be remapped
				--*******************************************************
				BWMesh.Faces[DAOInputMeshData.FaceCount] = undefined
				for i = 1 to DAOInputMeshData.FaceCount do
				(
					BWMesh.Faces[i] = (getFace MeshObj i)
				)
				--*********************************
				--Process Vertices
				--**********************************
				for v = 1 to DAOInputMeshData.VertexCount do
				(
					ProgressDialog.BarSmall.value = v * 50 / DAOInputMeshData.VertexCount
					
					FacesArray = TiedFaces[v] as array
					TVertsArrayChAlpha = TiedTVertsChAlpha[v] as array
					TVertsArrayChColor = TiedTVertsChColor[v] as array
					TVertsArrayCh1 = TiedTVertsCh1[v] as array
					TVertsArrayCh2 = TiedTVertsCh2[v] as array
					
					--*******************************************
					--Cutting Isolated Vertices, or cant process
					--******************************************
					local ValidVertex = true
					if FacesArray.count == 0 then ValidVertex = false	--no faces connected to the vertex
					if DAOInputMeshData.StreamColor0 == true then
					(
						if DAOInputMeshData.UseMapColor == true and TVertsArrayChColor.count == 0 then ValidVertex = false
						if DAOInputMeshData.UseMapAlpha == true and TVertsArrayChAlpha.count == 0 then ValidVertex = false
					)
					if DAOInputMeshData.StreamTexCoord0 == true and TVertsArrayCh1.count == 0 then ValidVertex = false
					if DAOInputMeshData.StreamTexCoord1 == true and TVertsArrayCh2.count == 0 then ValidVertex = false
					if not ValidVertex then
					(
						BWMesh.VertMapping[v] = 0
					)
					--***********************
					--Process
					--***********************
					if ValidVertex then
					(
						BWMesh.VertCounter += 1
						BWMesh.VertMapping[v] = BWMesh.VertCounter
						FaceData = for i = 1 to FacesArray.count collect FaceDataStruct()
						
						--******************************************
						--Get T Vertex Definition per Vertex
						--*******************************************
						ProgressDialog.Label.text = ("Get T Vertex Definition :"+DAOInputMeshData.MeshName as string+"\"")
						for f = 1 to FacesArray.count do	--For any face connected do:
						(
							CurrentFaceIndex = FacesArray[f]
							FaceData[f].Face = CurrentFaceIndex
							
							CurrentFace = getface MeshObj CurrentFaceIndex
							
							if DAOInputMeshData.StreamColor0 == true then
							(
								if DAOInputMeshData.UseMapColor == true then CurrentMapFaceChColor = meshOp.getMapFace MeshObj 0 CurrentFaceIndex
								if DAOInputMeshData.UseMapAlpha == true then CurrentMapFaceChAlpha = meshOp.getMapFace MeshObj -2 CurrentFaceIndex
							)
							if DAOInputMeshData.StreamTexCoord0 == true then CurrentMapFaceCh1 = meshOp.getMapFace MeshObj 1 CurrentFaceIndex
							if DAOInputMeshData.StreamTexCoord1 == true then CurrentMapFaceCh2 = meshOp.getMapFace MeshObj 2 CurrentFaceIndex
							
							
							for t = 1 to TVertsArrayChAlpha.count do
							(
								if CurrentFace.x == v and CurrentMapFaceChAlpha.x == TVertsArrayChAlpha[t] then
								(
									FaceData[f].TVertChAlpha = TVertsArrayChAlpha[t]
								)
								if CurrentFace.y == v and CurrentMapFaceChAlpha.y == TVertsArrayChAlpha[t] then
								(
									FaceData[f].TVertChAlpha = TVertsArrayChAlpha[t]
								)
								if CurrentFace.z == v and CurrentMapFaceChAlpha.z == TVertsArrayChAlpha[t] then
								(
									FaceData[f].TVertChAlpha = TVertsArrayChAlpha[t]
								)
							)
							for t = 1 to TVertsArrayChColor.count do
							(
								if CurrentFace.x == v and CurrentMapFaceChColor.x == TVertsArrayChColor[t] then
								(
									FaceData[f].TVertChColor = TVertsArrayChColor[t]
								)
								if CurrentFace.y == v and CurrentMapFaceChColor.y == TVertsArrayChColor[t] then
								(
									FaceData[f].TVertChColor = TVertsArrayChColor[t]
								)
								if CurrentFace.z == v and CurrentMapFaceChColor.z == TVertsArrayChColor[t] then
								(
									FaceData[f].TVertChColor = TVertsArrayChColor[t]
								)
							)
							for t = 1 to TVertsArrayCh1.count do
							(
								if CurrentFace.x == v and CurrentMapFaceCh1.x == TVertsArrayCh1[t] then
								(
									FaceData[f].TVertCh1 = TVertsArrayCh1[t]
								)
								if CurrentFace.y == v and CurrentMapFaceCh1.y == TVertsArrayCh1[t] then
								(
									FaceData[f].TVertCh1 = TVertsArrayCh1[t]
								)
								if CurrentFace.z == v and CurrentMapFaceCh1.z == TVertsArrayCh1[t] then
								(
									FaceData[f].TVertCh1 = TVertsArrayCh1[t]
								)
							)
							for t = 1 to TVertsArrayCh2.count do
							(
								if CurrentFace.x == v and CurrentMapFaceCh2.x == TVertsArrayCh2[t] then
								(
									FaceData[f].TVertCh2 = TVertsArrayCh2[t]
								)
								if CurrentFace.y == v and CurrentMapFaceCh2.y == TVertsArrayCh2[t] then
								(
									FaceData[f].TVertCh2 = TVertsArrayCh2[t]
								)
								if CurrentFace.z == v and CurrentMapFaceCh2.z == TVertsArrayCh2[t] then
								(
									FaceData[f].TVertCh2 = TVertsArrayCh2[t]
								)
							)
						)
						--*************************************************
						--Get Normals per Surrounding Face, for this Vertex
						--*************************************************
						ProgressDialog.Label.text = ("Calculate Normals :"+DAOInputMeshData.MeshName as string+"\"")
						
						local TempNormals = GetNormals MeshObj v FacesArray
						for f = 1 to FacesArray.count do
						(
							FaceData[f].Normal = TempNormals[f]
						)
						
						--**********************************************************************************
						--Optimize Vertex Data.
						--This means throwing out similar Face Data, and keep different for Vertex splitting
						--from Normals and their Smoothinggroups attached or split UV verts
						--**********************************************************************************
						ProgressDialog.Label.text = ("Optimize :"+DAOInputMeshData.MeshName as string+"\"")
						for f = 1 to FaceData.count do
						(
							FaceData[f].FaceList = #(FaceData[f].Face)
						)
						for DataStructs = 1 to FaceData.count do
						(
							for CompareDataStructs = FaceData.count to (DataStructs + 1) by -1 do
							(
								local equalTVAlpha = false
								local equalTVColor = false
								local equalTV1 = false
								local equalTV2 = false
								local equalNorm = false
								
								if DAOInputMeshData.StreamColor0 == true then
								(
									if (DAOInputMeshData.UseMapColor and FaceData[DataStructs].TVertChColor == FaceData[CompareDataStructs].TVertChColor) or not DAOInputMeshData.UseMapColor then equalTVColor = true
									if (DAOInputMeshData.UseMapAlpha and FaceData[DataStructs].TVertChAlpha == FaceData[CompareDataStructs].TVertChAlpha) or not DAOInputMeshData.UseMapAlpha then equalTVAlpha = true
								)
								else equalTVAlpha = equalTVColor = true	--irrelevant ,when not exporting color
								
								if (DAOInputMeshData.StreamTexCoord0 and FaceData[DataStructs].TVertCh1 == FaceData[CompareDataStructs].TVertCh1) or not DAOInputMeshData.StreamTexCoord0 then equalTV1 = true
								if (DAOInputMeshData.StreamTexCoord1 and FaceData[DataStructs].TVertCh2 == FaceData[CompareDataStructs].TVertCh2) or not DAOInputMeshData.StreamTexCoord1 then equalTV2 = true
								if FaceData[DataStructs].Normal == FaceData[CompareDataStructs].Normal then equalNorm = true
								
								if equalTVAlpha and equalTVColor and equalTV1 and equalTV2 and equalNorm then
								(
									append FaceData[DataStructs].FaceList FaceData[CompareDataStructs].Face
									deleteItem FaceData CompareDataStructs
								)
							)
						)
						
						--*****************************
						--Add Vertex to Output BWMesh
						--****************************
						for DataStructs = 1 to FaceData.count do
						(
							if DataStructs == 1 then
							(
								--BWMesh.VertsIndex += 1
								--BWMesh.Verts[BWMesh.VertsIndex] = v
								append BWMesh.Verts v
								append BWMesh.TVertsAlpha FaceData[DataStructs].TVertChAlpha
								append BWMesh.TVertsColor FaceData[DataStructs].TVertChColor
								append BWMesh.TVerts1 FaceData[DataStructs].TVertCh1
								append BWMesh.TVerts2 FaceData[DataStructs].TVertCh2
								append BWMesh.Norms FaceData[DataStructs].Normal
								append BWMesh.FacesPerVert FaceData[DataStructs].FaceList
							)
							else
							(
								BWMesh.PlusCounter += 1
								append BWMesh.PlusVerts v
								append BWMesh.PlusTVertsAlpha FaceData[DataStructs].TVertChAlpha
								append BWMesh.PlusTVertsColor FaceData[DataStructs].TVertChColor
								append BWMesh.PlusTVerts1 FaceData[DataStructs].TVertCh1
								append BWMesh.PlusTVerts2 FaceData[DataStructs].TVertCh2
								append BWMesh.PlusNorms FaceData[DataStructs].Normal
								append BWMesh.PlusFacesPerVert FaceData[DataStructs].FaceList
								
								for CurrentFaceIndex in FaceData[DataStructs].FaceList do
								(
									CurrentFace = getface MeshObj CurrentFaceIndex
									
									if DAOInputMeshData.StreamColor0 == true then
									(
										if DAOInputMeshData.UseMapColor then CurrentMapFaceChColor = meshOp.getMapFace MeshObj 0 CurrentFaceIndex
										if DAOInputMeshData.UseMapAlpha then CurrentMapFaceChAlpha = meshOp.getMapFace MeshObj -2 CurrentFaceIndex
									)
									if DAOInputMeshData.StreamTexCoord0 then CurrentMapFaceCh1 = meshOp.getMapFace MeshObj 1 CurrentFaceIndex
									if DAOInputMeshData.StreamTexCoord1 then CurrentMapFaceCh2 = meshOp.getMapFace MeshObj 2 CurrentFaceIndex
									
									if CurrentFace.x == v then
									(
										BWMesh.Faces[CurrentFaceIndex].x = BWMesh.PlusCounter + DAOInputMeshData.VertexCount
									)
									if CurrentFace.y == v then
									(
										BWMesh.Faces[CurrentFaceIndex].y = BWMesh.PlusCounter + DAOInputMeshData.VertexCount
									)
									if CurrentFace.z == v then
									(
										BWMesh.Faces[CurrentFaceIndex].z = BWMesh.PlusCounter + DAOInputMeshData.VertexCount
									)
								)
							)
						)			
					)
				)	--Vertex Cycle End
				
		--*************************
		--Collect Results
		--**************************
		ProgressDialog.Label.text = ("Collecting Results :"+DAOInputMeshData.MeshName as string+"\"")
		for i = 1 to BWMesh.PlusVerts.count do
		(
			BWMesh.VertCounter += 1
			append BWMesh.VertMapping BWMesh.VertCounter
			append BWMesh.Verts BWMesh.PlusVerts[i]
			--BWMesh.Verts[BWMesh.VertsIndex+BWMesh.PlusVerts.Count] = undefined
			--BWMesh.VertsIndex += 1
			--BWMesh.Verts[BWMesh.VertsIndex] = BWMesh.PlusVerts[i]
			
			if DAOInputMeshData.StreamColor0 == true then
			(
				if DAOInputMeshData.UseMapColor then append BWMesh.TVertsColor BWMesh.PlusTVertsColor[i]
				if DAOInputMeshData.UseMapAlpha then append BWMesh.TVertsAlpha BWMesh.PlusTVertsAlpha[i]
			)
			if DAOInputMeshData.StreamTexCoord0 then append BWMesh.TVerts1 BWMesh.PlusTVerts1[i]
			if DAOInputMeshData.StreamTexCoord1 then append BWMesh.TVerts2 BWMesh.PlusTVerts2[i]
			
			append BWMesh.Norms BWMesh.PlusNorms[i]
			append BWMesh.FacesPerVert BWMesh.PlusFacesPerVert[i]
		)
		BWMesh
	)
	
	fn StreamOutputMesh Stream MeshObj DAOInputMeshData BWMesh = (
		--***********************
		--Export Vertex Positions
		--***********************
		if DAOInputMeshData.StreamPosition then
		(
			local Vert
			format "<Data ElementCount=\"%\" Semantic=\"POSITION\" Type=\"Float4\">\n" BWMesh.Verts.count to:Stream
			format "<![CDATA[\n" to:Stream
			
			for j = 1 to BWMesh.Verts.count do
			(
				if DAOInputMeshData.InWorldSpace == true then	--Skinned meshes ,take WORLD aligned positions
				(
					Vert = (getvert MeshObj BWMesh.Verts[j])
				)
				else	--Parent aligned positions for non skinned meshes
				(
					Vert = ((getvert MeshObj BWMesh.Verts[j]) * DAOInputMeshData.InvMeshTransform)
				)
				if DAOModelEx.GOBOffset == 2 then Vert = Vert * inverse (GetGOB DAOInputMeshData.Obj).transform		--All meshes, compensate GOB transform at GOB pivot setting
				Vert = Vert / DAOTools.WorldScale
				format "% % % 1.0\n" (Vert.x) (Vert.y) (Vert.z) to:Stream
			)
			
			format "]]>\n" to:Stream
			format "</Data>\n" to:Stream
			ProgressDialog.BarSmall.value = 55
		)
		--*********************
		--Export Texture Coords
		--*********************
				if DAOInputMeshData.StreamTexCoord0 then
				(
					local TVert
					format "<Data ElementCount=\"%\" Semantic=\"TEXCOORD\" Type=\"Float2\">\n" BWMesh.TVerts1.count to:Stream
					format "<![CDATA[\n" to:Stream
				
					for j = 1 to BWMesh.TVerts1.count do
					(
						TVert = (meshOp.GetMapVert MeshObj 1 BWMesh.TVerts1[j])
						format "% %\n" (TVert.x) (1-TVert.y) to:Stream
					)				
					
					format "]]>\n" to:Stream
					format "</Data>\n" to:Stream
				)
				ProgressDialog.BarSmall.value = 60
				
				if DAOInputMeshData.StreamTexCoord1 then
				(
					local TVert
					format "<Data ElementCount=\"%\" Semantic=\"TEXCOORD\" Type=\"Float2\">\n" BWMesh.TVerts2.count to:Stream
					format "<![CDATA[\n" to:Stream
				
					for j = 1 to BWMesh.TVerts2.count do
					(
						TVert = (meshOp.GetMapVert MeshObj 2 BWMesh.TVerts2[j])
						format "% %\n" (TVert.x) (1-TVert.y) to:Stream
					)				
					
					format "]]>\n" to:Stream
					format "</Data>\n" to:Stream
				)
				ProgressDialog.BarSmall.value = 65
				
				BWMesh.TVerts1 = #()
				BWMesh.TVerts2 = #()
				gc()
				--*******************
				--Export Tangents
				--*******************
				if DAOInputMeshData.StreamTangent then
				(
					format "<Data ElementCount=\"%\" Semantic=\"TANGENT\" Type=\"Float4\">\n" BWMesh.Norms.count to:Stream
					format "<![CDATA[\n" to:Stream
					local TempNormal
					local Vert
					
					for j = 1 to BWMesh.Norms.count do
					(
						
						if DAOModelEx.UsingEditNormals == false then
						(
							TempNormal = (BWMesh.Norms[j])
						)
						else	--Getting Normal into Worldspace, since Using Edit Normal uses SnapShot instead of SnapShotasMesh
						(
							TempNormal = (BWMesh.Norms[j]) * inverse DAOInputMeshData.InvMeshTransform.rotationpart
						)
						Vert = GetTangent MeshObj (BWMesh.FacesPerVert[j]) TempNormal
						
						if DAOInputMeshData.InWorldSpace == false then Vert[1] = Vert[1] * DAOInputMeshData.InvMeshTransform.rotationpart
						
						format "% % % 1.0\n" (Vert[1].x) (Vert[1].y) (Vert[1].z) to:Stream
					)
					
					format "]]>\n" to:Stream
					format "</Data>\n" to:Stream				
					ProgressDialog.BarSmall.value = 70
				)
				
				--*******************
				--Export Binormals
				--*******************
				if DAOInputMeshData.StreamBinormal then
				(
					format "<Data ElementCount=\"%\" Semantic=\"BINORMAL\" Type=\"Float4\">\n" BWMesh.Norms.count to:Stream
					format "<![CDATA[\n" to:Stream
					local TempNormal
					local Vert
					
					for j = 1 to BWMesh.Norms.count do
					(
						
						if DAOModelEx.UsingEditNormals == false then
						(
							TempNormal = (BWMesh.Norms[j])
						)
						else	--Getting Normal into Worldspace, since Using Edit Normal uses SnapShot instead of SnapShotas Mesh
						(		--To apply Tangents which are in Worldspace !?
							TempNormal = (BWMesh.Norms[j]) * inverse DAOInputMeshData.InvMeshTransform.rotationpart	--double inverse woo
						)
						Vert = GetTangent MeshObj (BWMesh.FacesPerVert[j]) TempNormal
	
						if DAOInputMeshData.InWorldSpace == false then Vert[2] = Vert[2] * DAOInputMeshData.InvMeshTransform.rotationpart
						format "% % % 1.0\n" (Vert[2].x) (Vert[2].y) (Vert[2].z) to:Stream
					)
				
					format "]]>\n" to:Stream
					format "</Data>\n" to:Stream
					ProgressDialog.BarSmall.value = 75
				)
				
				--*******************
				--Export Normals
				--*******************
				if DAOInputMeshData.StreamNormal then
				(
					format "<Data ElementCount=\"%\" Semantic=\"NORMAL\" Type=\"Float4\">\n" BWMesh.Norms.count to:Stream
					format "<![CDATA[\n" to:Stream
					local VertNormal
					
					for j = 1 to BWMesh.Norms.count do
					(
						VertNormal = (BWMesh.Norms[j])
						if DAOModelEx.UsingEditNormals == false and DAOInputMeshData.InWorldSpace == false then
						(
							VertNormal = VertNormal * DAOInputMeshData.InvMeshTransform.rotationpart
						)
						format "% % % 1.0\n" (VertNormal.x) (VertNormal.y) (VertNormal.z) to:Stream
						--format "% % %\n" (VertNormal.x) (VertNormal.y) (VertNormal.z) to:Stream
					)
					
					format "]]>\n" to:Stream
					format "</Data>\n" to:Stream
					ProgressDialog.BarSmall.value = 80
					
					BWMesh.Norms = #()
					gc()
				)
				
				if DAOInputMeshData.StreamBlendWeights or DAOInputMeshData.StreamBlendIndices then
				(
					max modify mode
					select DAOInputMeshData.Obj
					local Blend = GetBlendValues DAOInputMeshData.Obj BWMesh.Verts
					if Blend != undefined then
					(
						--*******************
						--Export Blendweights
						--*******************
						format "<Data ElementCount=\"%\" Semantic=\"BLENDWEIGHT\" Type=\"Float4\">\n" (BWMesh.Verts.count) to:Stream
						format "<![CDATA[\n" to:Stream
						
						for j = 1 to BWMesh.Verts.count do
						(
							format "% % % %\n" (Blend.Weight[j][1]) (Blend.Weight[j][2]) (Blend.Weight[j][3]) (Blend.Weight[j][4]) to:Stream
						)
						
						format "]]>\n" to:Stream
						format "</Data>\n" to:Stream
						ProgressDialog.BarSmall.value = 85
						
						--*******************
						--Export Blendindices
						--*******************
						format "<Data ElementCount=\"%\" Semantic=\"BLENDINDICES\" Type=\"Short4\">\n" (BWMesh.Verts.count) to:Stream
						format "<![CDATA[\n" to:Stream
						
						for j = 1 to BWMesh.Verts.count do
						(
							format "% % % %\n" (Blend.Index[j][1]) (Blend.Index[j][2]) (Blend.Index[j][3]) (Blend.Index[j][4]) to:Stream
						)
						
						format "]]>\n" to:Stream
						format "</Data>\n" to:Stream
						ProgressDialog.BarSmall.value = 90
					)
				)
				
				--******************************************************************************
				--Export Color Channel
				--Hair mat ->unknown effect
				--Lava mat ->affecting actual vertex color in "toolset" as sort of tint override. [0,0,0] is base. Alpha unknown
				--Water mat -> affecting Water color (RGB), and transparency (A)
				--*******************************************************************************
				if DAOInputMeshData.StreamColor0 then
				(
					local VertColor
					local VertAlpha
					format "<Data ElementCount=\"%\" Semantic=\"COLOR\" Type=\"Float4\">\n" (BWMesh.Verts.count) to:Stream
					format "<![CDATA[\n" to:Stream
					
					for j = 1 to BWMesh.TVertsColor.count do	--Vertex count is equal anyway , for output mesh
					(
						if DAOInputMeshData.UseMapColor == true then
						(
							VertColor = (meshOp.GetMapVert MeshObj 0 BWMesh.TVertsColor[j])
						)
						else
						(
							VertColor = (DAOInputMeshData.OverrideColorValue as point3 ) / 255
						)
						
						if DAOInputMeshData.UseMapAlpha == true then
						(
							VertAlpha = (meshOp.GetMapVert MeshObj -2 BWMesh.TVertsAlpha[j])
							VertAlpha = ((VertAlpha.x + VertAlpha.y + VertAlpha.z) / 3) as float	--average
						)
						else
						(
							VertAlpha = DAOInputMeshData.OverrideAlphaValue
						)
						format "% % % %\n" VertColor.x VertColor.y VertColor.z VertAlpha to:Stream
						
						--format (VertColor.x as string + " " + VertColor.y as string + " " + VertColor.z as string + " " + VertAlpha as string + "\n")
					)
					
					format "]]>\n" to:Stream
					format "</Data>\n" to:Stream
					ProgressDialog.BarSmall.value = 95
				)
				
				--*******************
				--Export Face indices
				--*******************
				format "<Data IndexCount=\"%\" IndexType=\"Index32\" Semantic=\"Indices\">\n" ((BWMesh.Faces.count)*3) to:Stream
				format "<![CDATA[\n" to:Stream
				local Face
				local FaceX
				local FaceY
				local FaceZ
				
				for j = 1 to BWMesh.Faces.count do
				(
					Face = BWMesh.Faces[j]
					Face.x = BWMesh.VertMapping[Face.x]		--remapping
					Face.y = BWMesh.VertMapping[Face.y]
					Face.z = BWMesh.VertMapping[Face.z]
					FaceX = ((Face.x-1) as integer)			--Max indices based 1, output based 0
					FaceY = ((Face.y-1) as integer)
					FaceZ = ((Face.z-1) as integer)
					format ("% % %\n") FaceX FaceY FaceZ to:Stream
				)
				
				format "]]>\n" to:Stream
				format "</Data>\n" to:Stream
				ProgressDialog.BarSmall.value = 100
	)
	
	fn ExportMetaData Stream MetaData = (
		format "<?xml version=\"1.0\" ?>\n" to:Stream
		format "<!-- Created with Eshme's DAO Model and Animation Import Export Tool Version: % -->\n" DAOTools.Version to:Stream
		format "<!-- Visit my Page at http://social.bioware.com/project/2336/  -->\n" to:Stream
		format "<!-- On : % -->\n\n" (localtime) to:Stream
		
		format "<MetaData MetaDataVersion=\"1\">\n" to:Stream
			
		format "     <ModificationTrackingInformation>\n" to:Stream
				
		format "          <lastExportedOn>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" (MetaData.lastExportedOn) to:Stream
		format "               </Value>\n" to:Stream
		format "          </lastExportedOn>\n" to:Stream
				
		format "          <lastExportedBy>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" (MetaData.lastExportedBy) to:Stream
		format "               </Value>\n" to:Stream
		format "          </lastExportedBy>\n" to:Stream
				
		format "          <lastExportedFrom>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" (MetaData.lastExportedFrom) to:Stream
		format "               </Value>\n" to:Stream
		format "          </lastExportedFrom>\n" to:Stream
			
		format "     </ModificationTrackingInformation>\n" to:Stream
		
		format "     <DependencyInformation>\n" to:Stream
			
		format "          <%>\n" (MetaData.ModelMeshData[1]) to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[ModelMeshData]]>\n" to:Stream
		format "               </Value>\n" to:Stream
		format "          </%>\n" (MetaData.ModelMeshData[1]) to:Stream
				
		format "          <%>\n" (MetaData.PhysicsMesh[1]) to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[PhysicsMesh]]>\n" to:Stream
		format "               </Value>\n" to:Stream
		format "          </%>\n" (MetaData.PhysicsMesh[1]) to:Stream
				
		for i = 1 to MetaData.MaterialObject.count do
		(
			format "          <%>\n" (MetaData.MaterialObject[i]) to:Stream
			format "               <Value>\n" to:Stream
			format "                    <![CDATA[MaterialObject]]>\n" to:Stream
			format "               </Value>\n" to:Stream
			format "          </%>\n" (MetaData.MaterialObject[i]) to:Stream
		)
			
		format "     </DependencyInformation>\n" to:Stream
			
		format "     <ResourceInformation>\n" to:Stream
			
		format "          <GameResource>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" (0) to:Stream
		format "               </Value>\n" to:Stream
		format "          </GameResource>\n" to:Stream
			
		format "          <Type>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" ("Level") to:Stream
		format "               </Value>\n" to:Stream
		format "          </Type>\n" to:Stream
			
		format "          <Title>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" ("") to:Stream
		format "               </Value>\n" to:Stream
		format "          </Title>\n" to:Stream
			
		format "          <Description>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" ("") to:Stream
		format "               </Value>\n" to:Stream
		format "          </Description>\n" to:Stream
			
		format "          <LevelType>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" ("") to:Stream
		format "               </Value>\n" to:Stream
		format "          </LevelType>\n" to:Stream
				
		format "          <LevelStyle1>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" ("") to:Stream
		format "               </Value>\n" to:Stream
		format "          </LevelStyle1>\n" to:Stream
			
		format "          <LevelStyle2>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" ("") to:Stream
		format "               </Value>\n" to:Stream
		format "          </LevelStyle2>\n" to:Stream
			
		format "          <LevelModularSystem>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" (0) to:Stream
		format "               </Value>\n" to:Stream
		format "          </LevelModularSystem>\n" to:Stream
			
		format "          <LevelLODLevels>\n" to:Stream
		format "               <Value>\n" to:Stream
		format "                    <![CDATA[%]]>\n" (0) to:Stream
		format "               </Value>\n" to:Stream
		format "          </LevelLODLevels>\n" to:Stream
			
		format "     </ResourceInformation>\n" to:Stream
			
		format "     <Parts>\n" to:Stream
				
		format "          <List>\n" to:Stream
					
		for i = 1 to MetaData.PartsList.count do
		(
			format "               <%>\n" (MetaData.PartsList[i].MeshName) to:Stream
			format "                    <Value>\n" to:Stream
			format "                         <![CDATA[%]]>\n" ("") to:Stream
			format "                    </Value>\n" to:Stream
			format "                    <ID>\n" to:Stream
			format "                         <Value>\n" to:Stream
			format "                              <![CDATA[%]]>\n" (MetaData.PartsList[i].ID) to:Stream
			format "                         </Value>\n" to:Stream
			format "                    </ID>\n" to:Stream
			format "                    <BBoxSize>\n" to:Stream
			format "                         <Value>\n" to:Stream
			format "                              <![CDATA[% % %]]>\n" (MetaData.PartsList[i].BBox.x) (MetaData.PartsList[i].BBox.z) (MetaData.PartsList[i].BBox.z) to:Stream
			format "                         </Value>\n" to:Stream
			format "                    </BBoxSize>\n" to:Stream
			format "               </%>\n" (MetaData.PartsList[i].MeshName) to:Stream					
		)
				
		format "          </List>\n" to:Stream
		
		format "          <Lightmappable>\n" to:Stream
		
		format "               <List>\n" to:Stream
				
		for i = 1 to MetaData.PartsLightmappable.count do
		(
			format "                    <%>\n" (MetaData.PartsLightmappable[i].MeshName) to:Stream
						
			format "                         <Value>\n" to:Stream
			format "                              <![CDATA[%]]>\n" ("") to:Stream
			format "                         </Value>\n" to:Stream
								
			format "                         <LightmapSize>\n" to:Stream
			format "                              <Value>\n" to:Stream
			format "                                   <![CDATA[%]]>\n" (MetaData.PartsLightmappable[i].LightmapSize) to:Stream
			format "                              </Value>\n" to:Stream
			format "                         </LightmapSize>\n" to:Stream
								
			format "                         <LightmapUVWChannel>\n" to:Stream
			format "                              <Value>\n" to:Stream
			format "                                   <![CDATA[%]]>\n" (MetaData.PartsLightmappable[i].LightmapUVWChannel) to:Stream
			format "                              </Value>\n" to:Stream
			format "                         </LightmapUVWChannel>\n" to:Stream
						
			format "                         <LightmapUVWChannelIndex>\n" to:Stream
			format "                              <Value>\n" to:Stream
			format "                                   <![CDATA[%]]>\n" (MetaData.PartsLightmappable[i].LightmapUVWChannelIndex) to:Stream
			format "                              </Value>\n" to:Stream
			format "                         </LightmapUVWChannelIndex>\n" to:Stream
							
			format "                    </%>\n" (MetaData.PartsLightmappable[i].MeshName) to:Stream
		)
						
		format "               </List>\n" to:Stream
		
		format "          </Lightmappable>\n" to:Stream
				
		format "          <Geometry>\n" to:Stream
				
		format "               <List>\n" to:Stream
			
		for i = 1 to MetaData.PartsGeometry.count do
		(
			format "                    <%>\n" (MetaData.PartsGeometry[i].MeshName) to:Stream
			format "                         <Value>\n" to:Stream
			format "                              <![CDATA[%]]>\n" ("") to:Stream
			format "                         </Value>\n" to:Stream
			format "                    </%>\n" (MetaData.PartsGeometry[i].MeshName) to:Stream
		)
						
		format "               </List>\n" to:Stream
					
		format "          </Geometry>\n" to:Stream

		format "          <Renderable>\n" to:Stream
					
		format "               <List>\n" to:Stream
					
		for i = 1 to MetaData.PartsRenderable.count do
		(
			format "                    <%>\n" (MetaData.PartsRenderable[i].MeshName) to:Stream
			format "                         <Value>\n" to:Stream
			format "                              <![CDATA[%]]>\n" ("") to:Stream
			format "                         </Value>\n" to:Stream
			format "                    </%>\n" (MetaData.PartsRenderable[i].MeshName) to:Stream
		)
						
		format "               </List>\n" to:Stream
					
		format "          </Renderable>\n" to:Stream
				
		format "     </Parts>\n" to:Stream
		
		format "</MetaData>\n" to:Stream
	)
