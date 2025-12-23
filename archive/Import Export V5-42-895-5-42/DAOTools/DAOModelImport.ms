/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edit:
15 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Include Script
DAOModelImport.ms
Main Functions Model Import
************************************************
*/

	struct VertexStreamStruct (
		Stream,
		Offset,
		DataType,
		Usage,
		UsageIndex,
		Method
	)
	
	struct MeshDataStruct (
		GroupName,
		Parent,
		Rotation,
		Position,
		
		BonesUsed,
		
		--Vertex Data
		VertexDeclarator,	--List of Values defining the Vertex Streams
		
		PrimitiveType,		--??
		BaseVertexIndex,	--??
		MinIndex,			--??
		ReferencedVerts,	--??
		AdditionalStreams,	--??
		
		VertexCount,	--As many Vertices are being Streamed
		VertexSize,		--Byte Size of a single Vertices Data. Usage is interleaved across the stream.
		VertexOffset,	--Offset to the Vertex Data within the Vertex Stream. Relevant if multiple Mesh Chunks present
		VertexData,		--Array of 1, this is an Offset to the Beginning of a Bytestream inside the File
		
		IndexCount,		--As many faces are indexed
		IndexFormat,	--Format the Indices are in. 8bit ,16 bit or so
		IndexOffset,	--Offset to the Index Data within the Index Stream. Relevant if multiple Mesh Chunks present
		IndexData		--Array of 1, this is an Offset to the Beginning of a Bytestream inside the File
	)
	
	fn GetVertexStreamFromDeclarator FileStream VertexDeclarator Usage = (
	--********************************************************************************************
	--Getting the Vertex Stream from the Declarator for a specific Usage (like Position or Normals Data)
	--For use in the Mesh Creation
	--Inputting the Declarator Reference List #(RefID,Offset) ...
	--Output, an Array of Streams that match the Usage. Or undefined if none
	--********************************************************************************************
		local VertexStream
		local out = #()
		for i = 1 to VertexDeclarator.Count do
		(
			local DeclStructRef		= VertexDeclarator[i][1]
			local DeclStructOffset	= VertexDeclarator[i][2]
			if (ReadDataByID FileStream GFFFile.Structs[DeclStructRef] 8029 DeclStructOffset) == Usage then
			(
				VertexStream			= VertexStreamStruct()
				VertexStream.Stream		= ReadDataByID FileStream GFFFile.Structs[DeclStructRef] 8026 DeclStructOffset
				VertexStream.Offset		= ReadDataByID FileStream GFFFile.Structs[DeclStructRef] 8027 DeclStructOffset
				VertexStream.DataType	= ReadDataByID FileStream GFFFile.Structs[DeclStructRef] 8028 DeclStructOffset
				VertexStream.Usage		= ReadDataByID FileStream GFFFile.Structs[DeclStructRef] 8029 DeclStructOffset
				VertexStream.UsageIndex	= ReadDataByID FileStream GFFFile.Structs[DeclStructRef] 8030 DeclStructOffset
				VertexStream.Method		= ReadDataByID FileStream GFFFile.Structs[DeclStructRef] 8031 DeclStructOffset
				append out VertexStream
			)
		)
		if out.count == 0 then out = undefined
		return out
	)
	
	fn CreateDAOMesh Stream MeshData = (
		--********************************************************************
		--Mesh Creation function. Reads in multiple Streams of Vertex Data.
		--The Vertex Declarator is defining the Streams.
		--
		--Vertex Position Stream + (Index Stream for faces)
		--Vertex Normals Stream
		--Vertex TexCoord Streams, multiple
		--Vertex Color Stream (Color and Alpha)
		--Vertex Blend Weights and Indices, for Skinmod
		--
		--
		local MeshObj
		local VertexStream
		local CurrentVertOffset
		
		try
		(
			try
			(
				MeshObj = editable_mesh name:(MeshData.GroupName + DAOImportVars.Extension)
				convertTo MeshObj (Editable_Poly)
				MeshObj.parent = MeshData.Parent
				in coordsys parent MeshObj.Rotation = MeshData.Rotation
				in coordsys parent MeshObj.Position = MeshData.Position * DAOTools.WorldScale
			)
			catch
			(
				MessageBox ("Error creating Mesh Object. Mesh: "+(MeshData.GroupName + DAOImportVars.Extension) as string+"\n\nRuntime Error:\n\n"+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error"
				if MeshObj != undefined do delete MeshObj
				throw()
			)
			if (VertexStream = GetVertexStreamFromDeclarator Stream MeshData.VertexDeclarator VertexType.DECLUSAGE_POSITION) != undefined then
			(
				--Getting Position. Only one Stream expected, thus using VertexStream[1] throughout
				local FaceCount = 0
				local FailTracker = #(0,0)
				local FaceTwoP
				try
				(
					CurrentVertOffset = GFFFile.Header.DataOffset + MeshData.VertexData[1] + 4 + MeshData.VertexOffset + VertexStream[1].Offset
					--Datablock + VertexDataBlock + 4(skipping count) + VertexOffset of the Chunk + Startindex of the UsageStream
					for j = 1 to MeshData.VertexCount do
					(
						Data = ReadDeclType Stream CurrentVertOffset VertexStream[1].DataType
						at time 0 in coordsys local polyop.createvert MeshObj [Data[1]*DAOTools.WorldScale,Data[2]*DAOTools.WorldScale,Data[3]*DAOTools.WorldScale] node:MeshObj
						CurrentVertOffset += MeshData.VertexSize
						ProgressDialog.BarSmall.value = j * 100 / MeshData.VertexCount
					)
					--***Create the Polygons too***---
					case MeshData.IndexFormat of	--Seeking Datablock+IndexDataBlock+4(skipping count)+Startindex*bytescount
					(
						0:		(fseek Stream (GFFFile.Header.DataOffset + MeshData.IndexData[1] + 4 + (MeshData.IndexOffset * 2)) #seek_set)
						1:		(fseek Stream (GFFFile.Header.DataOffset + MeshData.IndexData[1] + 4 + (MeshData.IndexOffset * 4)) #seek_set)
						Default:(fseek Stream (GFFFile.Header.DataOffset + MeshData.IndexData[1] + 4 + (MeshData.IndexOffset * 2)) #seek_set)
					)
					for j = 1 to (MeshData.IndexCount / 3) do
					(
						case MeshData.IndexFormat of
						(
							0:		(FaceTwoP = #((readshort Stream #unsigned)+1,(readshort Stream #unsigned)+1,(readshort Stream #unsigned)+1))
							1:		(FaceTwoP = #((readlong Stream #unsigned)+1,(readlong Stream #unsigned)+1,(readlong Stream #unsigned)+1))
							Default:(FaceTwoP = #((readlong Stream #unsigned)+1,(readlong Stream #unsigned)+1,(readlong Stream #unsigned)+1))
						)
						if FaceTwoP[1] != FaceTwoP[2] and FaceTwoP[2] != FaceTwoP[3] and FaceTwoP[1] != FaceTwoP[3] then
						(
							if polyop.createpolygon MeshObj FaceTwoP != undefined then
							(
								FaceCount += 1
								polyOp.SetFaceSmoothGroup MeshObj FaceCount 1
							)
							else FailTracker[2] += 1	--when creation failed (ie duplicates)
						)
						else FailTracker[1] += 1	--(2point poly fails) Seen it happen, on a mesh exported with the exporter
						ProgressDialog.BarSmall.value = j * 100 / (MeshData.IndexCount / 3)
					)
					if FailTracker[1] >0 then MessageBox ("Mesh "+(MeshData.GroupName + DAOImportVars.Extension) as string+" has "+FailTracker[1] as string+" Polygons with only one edge.\nThose have been removed") title:"MSH Import"
					if FailTracker[2] >0 then MessageBox ("Mesh "+(MeshData.GroupName + DAOImportVars.Extension) as string+" has "+FailTracker[2] as string+" Polygons whose creation failed.\nLikely been duplicates") title:"MSH Import"
				)
				catch
				(
					MessageBox ("Error creating Mesh Object. Mesh: "+(MeshData.GroupName + DAOImportVars.Extension) as string+"\nAt Face: "+FaceCount as string+"\n\nRuntime Error:\n\n"+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error"
					update MeshObj
					throw()
				)
			)
			ProgressDialog.BarBig.value += 90/FileStates.MeshCount/4
			
			if (VertexStream = GetVertexStreamFromDeclarator Stream MeshData.VertexDeclarator VertexType.DECLUSAGE_NORMAL) != undefined and (maxversion())[1] > 4200 then
			(
				try
				(
					addModifier MeshObj (Edit_Normals())
					max modify mode
					select MeshObj
					
					CurrentVertOffset = GFFFile.Header.DataOffset + MeshData.VertexData[1] + 4 + MeshData.VertexOffset + VertexStream[1].Offset
					for j = 1 to MeshData.VertexCount do
					(
						local a = #{}		--Thats a bitarray, hard to notice
						MeshObj.modifiers[#Edit_normals].ConvertVertexSelection #{j} a
						Data = ReadDeclType Stream CurrentVertOffset VertexStream[1].DataType
						for k = 1 to (a as array).count do
						(
							at time 0 MeshObj.modifiers[#Edit_normals].SetNormal (a as array)[k] [Data[1],Data[2],Data[3]] node:MeshObj
							at time 0 MeshObj.modifiers[#Edit_normals].SetNormalExplicit (a as array)[k] node:MeshObj
						)
						CurrentVertOffset += MeshData.VertexSize
						ProgressDialog.BarSmall.value = j * 100 / MeshData.VertexCount
					)
				)
				catch
				(
					MessageBox ("Error adding Mesh Normals. Mesh: "+(MeshData.GroupName + DAOImportVars.Extension) as string+"\n\nRuntime Error:\n\n"+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error"
					if MeshObj.modifiers[#Edit_normals] != undefined do deleteModifier MeshObj MeshObj.modifiers[#Edit_normals]
				)
				update MeshObj					
			)
			ProgressDialog.BarBig.value += 90 / FileStates.MeshCount / 4
			
			if (VertexStream = GetVertexStreamFromDeclarator Stream MeshData.VertexDeclarator VertexType.DECLUSAGE_TEXCOORD) != undefined then
			(
				try
				(
					for i = 1 to VertexStream.count do		--Multiple coords indexed
					(
						CurrentVertOffset = GFFFile.Header.DataOffset + MeshData.VertexData[1] + 4 + MeshData.VertexOffset + VertexStream[i].Offset
						if (maxversion())[1] > 4200 then
						(
							polyOp.applyUVWMap MeshObj #planar channel:i 
							for j = 1 to MeshData.VertexCount do
							(
								Data = ReadDeclType Stream CurrentVertOffset VertexStream[i].DataType
								polyOp.setMapVert MeshObj i j [Data[1],1-Data[2],1]
								CurrentVertOffset += MeshData.VertexSize
								ProgressDialog.BarSmall.value = j * 100 / MeshData.VertexCount
							)
							addModifier MeshObj (Unwrap_UVW ())
							if (maxversion())[1] >= 11000 and i > 1 then
							(	--Due changes in max 2009 and above: Do not switch the Channel from 1, because it will maintain Channel 1 data until the user forces Reset
								MeshObj.modifiers[1].name="Unwrap UVW Ch."+(i as string)+"!! Channel 1 still active"
							)
							else
							(
								MeshObj.modifiers[1].unwrap.setMapChannel i
								MeshObj.modifiers[1].name="Unwrap UVW Ch."+(i as string)	
							)
						)
						else	--gmax version
						(
							--above v5.30, UV's stored in the mesh via SetVertColor (works in gmax, not sure about 3dsmax)
							--setMapVert doesnt work at all
							polyOp.applyUVWMap MeshObj #planar channel:i
							for j = 1 to MeshData.VertexCount do
							(
								Data = ReadDeclType Stream CurrentVertOffset VertexStream[i].DataType
								polyOp.setVertColor MeshObj i j (color (Data[1] * 255) (255 - (Data[2] * 255)) 255)
								CurrentVertOffset += MeshData.VertexSize
								ProgressDialog.BarSmall.value = j * 100 / MeshData.VertexCount
							)
							addModifier MeshObj (Unwrap_UVW ())
							MeshObj.modifiers[1].unwrap.setMapChannel i		--doesnt seem to work, modifier stays at Channel 1.
							MeshObj.modifiers[1].name="Unwrap UVW Ch."+(i as string)
							if i > 1 then MeshObj.modifiers[1].name="Unwrap UVW Ch."+(i as string)+"!! Channel 1 still active"
						)
					)
				)
				catch
				(
					MessageBox ("Error adding UV Map. Object: "+(MeshData.GroupName + DAOImportVars.Extension) as string+ "\n\nRuntime Error:\n\n"+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error"
					if MeshObj.modifiers[#Unwrap_UVW] != undefined do deleteModifier MeshObj MeshObj.modifiers[#Unwrap_UVW]
				)
			)
			ProgressDialog.BarBig.value += 90 / FileStates.MeshCount / 4
			
			if (VertexStream = GetVertexStreamFromDeclarator Stream MeshData.VertexDeclarator VertexType.DECLUSAGE_COLOR) != undefined then
			(
				--**************************************************************************
				--if there are vertex colors (R G B Alpha)
				--Used: Hair,  (unknown purpose)
				--Lava, Vertex color = Lava Tint, overlay, Alpha unknown. Default 0 0 0 0
				--Water, Vertex color = Water color, Alpha = transparency
				--***************************************************************************
				try
				(
					if VertexStream.count > 1 then MessageBox ("Multiple Vertex Color Channels not supported. "+VertexStream.count as string+"\nNotify Eshme") title:"MSH Import"
					
					polyOp.applyUVWMap MeshObj #planar channel:0		--Color channel
					polyOp.applyUVWMap MeshObj #planar channel:-2		--Alpha channel
					
					CurrentVertOffset = GFFFile.Header.DataOffset + MeshData.VertexData[1] + 4 + MeshData.VertexOffset + VertexStream[1].Offset
					for i = 1 to MeshData.VertexCount do
					(
						Data = ReadDeclType Stream CurrentVertOffset VertexStream[1].DataType
						polyOp.setVertColor MeshObj 0 i (color (abs(Data[1]*255)) (abs(Data[2]*255)) (abs(Data[3]*255)))
						--polyOp.setMapVert MeshObj 0 i [abs(Data[1]),abs(Data[2]),abs(Data[3])]	--doesnt work in gmax
						if Data[4] != undefined then
						(
							polyOp.setVertColor MeshObj -2 i (color (abs(Data[4]*255)) (abs(Data[4]*255)) (abs(Data[4]*255)))
							--polyOp.setMapVert MeshObj -2 i [abs(Data[4]),abs(Data[4]),abs(Data[4])]	--doesnt work in gmax
						)
						CurrentVertOffset += MeshData.VertexSize
						ProgressDialog.BarSmall.value = i * 100 / MeshData.VertexCount
					)
				)
				catch
				(
					MessageBox ("Error adding Vertex Colors. Object: "+(MeshData.GroupName + DAOImportVars.Extension) as string+"\n\nRuntime Error:\n\n"+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error"
				)
			)
			update MeshObj
			
			if	(GetVertexStreamFromDeclarator Stream MeshData.VertexDeclarator VertexType.DECLUSAGE_BLENDWEIGHT) != undefined and \
				(GetVertexStreamFromDeclarator Stream MeshData.VertexDeclarator VertexType.DECLUSAGE_BLENDINDICES) != undefined then
			(
				local BlendWeightStream = GetVertexStreamFromDeclarator Stream MeshData.VertexDeclarator VertexType.DECLUSAGE_BLENDWEIGHT
				local BlendIndexStream = GetVertexStreamFromDeclarator Stream MeshData.VertexDeclarator VertexType.DECLUSAGE_BLENDINDICES
				local UsedBone
				local Weighttable
				try
				(
					SkinMod = skin()
					if (maxversion())[1] >= 5000 then SkinMod.bone_limit = 4
					addModifier MeshObj SkinMod
					max modify mode
					select MeshObj					
					for i = 1 to MeshData.BonesUsed.count do
					(
						try		--first add the bones 
						(
							UsedBone = getNodeByName (DAOImportVars.BoneIndexArray[MeshData.BonesUsed[i][1]+1] + DAOImportVars.Extension) exact:true ignoreCase:false all:false
							if UsedBone != undefined do skinops.addbone SkinMod UsedBone 1	--should be there thou
						)
						catch	--??
						(
							MessageBox ("Error in MMH File Bone Index. Index: "+(MeshData.BonesUsed[i][1]-1) as string+"\nCannot apply Skin Modifier"+"\n\nRuntime Error:\n\n"+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error"
							throw ()
						)
					)
					begone = MeshObj.numverts	--Need to do this so weighting works.
					
					local CurrentVertOffsetWeights = GFFFile.Header.DataOffset + MeshData.VertexData[1] + 4 + MeshData.VertexOffset + BlendWeightStream[1].Offset
					local CurrentVertOffsetIndices = GFFFile.Header.DataOffset + MeshData.VertexData[1] + 4 + MeshData.VertexOffset + BlendIndexStream[1].Offset
					for i = 1 to MeshData.VertexCount do
					(
						try
						(
							Weighttable = #(#(),#(),#())	--Indices,Weights,WeightsPriorZeroRemove
							
							Weighttable[3] = ReadDeclType Stream CurrentVertOffsetWeights BlendWeightStream[1].DataType
							if (maxversion())[1] < 5000 then Weighttable[3][(finditem Weighttable[3] (amin Weighttable[3]))] = 0		--Removing the least weighted bone ,for gmax can only have 3 bone affecting.
							if Weighttable[3][1] > 0 then Weighttable[2][1] = Weighttable[3][1]		--Adding only non zero weights
							if Weighttable[3][2] > 0 then Weighttable[2][2] = Weighttable[3][2]
							if Weighttable[3][3] > 0 then Weighttable[2][3] = Weighttable[3][3]
							if Weighttable[3][4] > 0 then Weighttable[2][4] = Weighttable[3][4]
							
							Data = ReadDeclType Stream CurrentVertOffsetIndices BlendIndexStream[1].DataType
							if Weighttable[2][1] != undefined then Weighttable[1][1] = (RoundMe Data[1])+1	--Reading bone indices, non zero weights
							if Weighttable[2][2] != undefined then Weighttable[1][2] = (RoundMe Data[2])+1
							if Weighttable[2][3] != undefined then Weighttable[1][3] = (RoundMe Data[3])+1
							if Weighttable[2][4] != undefined then Weighttable[1][4] = (RoundMe Data[4])+1				
							at time 0 skinOps.ReplaceVertexWeights SkinMod i Weighttable[1] Weighttable[2]
							
							CurrentVertOffsetWeights += MeshData.VertexSize
							CurrentVertOffsetIndices += MeshData.VertexSize
						)
						catch
						(
							MessageBox ("Error in MSH File Bone Weights. Object: "+(MeshData.GroupName + DAOImportVars.Extension) as string+"\nCannot apply Skin Modifier"+"\n\nRuntime Error:\n\n"+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error"
							throw ()
						)
						ProgressDialog.BarSmall.value = i * 100 / MeshData.VertexCount
					)
				)
				catch
				(
					if MeshObj.modifiers[#skin] != undefined do deleteModifier MeshObj MeshObj.modifiers[#skin]
					MessageBox ("Error adding Skin Modifier. Object: "+(MeshData.GroupName + DAOImportVars.Extension) as string+"\n\nRuntime Error:\n\n"+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error"
				)
			)
		)
		catch
		(
			MessageBox ("Error creating Mesh Object. Mesh: "+(MeshData.GroupName + DAOImportVars.Extension) as string+"\nAccident ,or error in Model File\n\nRuntime Error:\n\n"+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error"
		)
		if MeshObj != undefined and not isDeleted MeshObj then
		(
			update MeshObj
		)
		return MeshObj
	)

	fn ImportMSH MMHStream ChildFields Offset Parent = ( --Imports the current "mshh" type. Is one mesh.
		
		local MSHStream = 0
		FileStates.MSHloaded = false
		
  		local MeshObj
		
		local MeshData = MeshDataStruct()	--made to send to CreatMesh function. Should refine this one
		MeshData.Parent = Parent
		
		--Continueing on reading the MMH...mshh type .. This data belongs to only that single mesh (named Groupname) thus this funtion only imports a single mesh
		--*******************************************************************
		local MSHFile	 			= ReadDataByID MMHStream GFFFile.Structs[1] 6005 0
		local Name					= ReadDataByID MMHStream ChildFields 6000 Offset
		local MaterialObj			= ReadDataByID MMHStream ChildFields 6001 Offset
		local MaterialLib			= ReadDataByID MMHStream ChildFields 6002 Offset
		local MeshID				= ReadDataByID MMHStream ChildFields 6004 Offset
		MeshData.GroupName			= ReadDataByID MMHStream ChildFields 6006 Offset
		local RuntimeShadowCasting	= ReadDataByID MMHStream ChildFields 6176 Offset
		local CastBakedShadow		= ReadDataByID MMHStream ChildFields 6177 Offset
		local CutAway				= ReadDataByID MMHStream ChildFields 6193 Offset
		local PunchThrough			= ReadDataByID MMHStream ChildFields 6194 Offset
		MeshData.BonesUsed			= ReadDataByID MMHStream ChildFields 6255 Offset
		local ReceiveBakedShadow	= ReadDataByID MMHStream ChildFields 6301 Offset
		local ReceiveRuntimeShadow	= ReadDataByID MMHStream ChildFields 6304 Offset
		local NodeMeshName			= ReadDataByID MMHStream ChildFields 6307 Offset
		local IsVFXMesh				= ReadDataByID MMHStream ChildFields 6334 Offset
		local MaterialColor			= ReadDataByID MMHStream ChildFields 6335 Offset
		local UseVariationTint		= ReadDataByID MMHStream ChildFields 6340 Offset
		local DataStructs 			= ReadDataByID MMHStream ChildFields 6999 Offset
		MeshData.Position			= ReadDataByIDInStructs MMHStream DataStructs 6047
		MeshData.Rotation			= ReadDataByIDInStructs MMHStream DataStructs 6048

		--Now Getting the MSH File
		--*******************************************************
		MeshObj = getNodeByName (MeshData.GroupName + DAOImportVars.Extension) exact:true ignoreCase:true all:false
		if MeshObj == undefined then	--if it doesnt already exist
		(
			if NodeMeshName != "Null Reference" and NodeMeshName != undefined then MSHFile = NodeMeshName		--If meshchunk is expected in a specific file different from default
			
			if FileStates.MSHFileName == undefined or (getfilenamefile FileStates.MSHFileName) != MSHFile then \	--when msh file hasnt been found before
					 MSHFile = SearchFile DAOTools.ImportPaths (getFilenamePath FileStates.MMHFileName) MSHFile "MSH"	\	--Search in setup paths, or MMH path
				else MSHFile = FileStates.MSHFileName
		
			if MSHFile != undefined then
			(
				MSHStream = fopen MSHFile "rb"
				if (LoadGFF MSHStream) and IsMSH() then
				(
					FileStates.MSHloaded = true
					FileStates.MSHFileName = MSHFile		--remembering name for next time
				)
				else MessageBox "File is not a MSH. Skipping.." title:"MSH Import"
			)
			else MessageBox "MSH File not found, Skipping.." title:"MSH Import"
		)
		
		--MSH ready to read
		--****************************
		if FileStates.MSHloaded then
		(
			MeshData.VertexData		= ReadDataByID MSHStream GFFFile.Structs[1] 8022 0		--Returns only an Offset
			MeshData.IndexData		= ReadDataByID MSHStream GFFFile.Structs[1] 8023 0		--Returns only an Offset
			MeshChunks				= ReadDataByID MSHStream GFFFile.Structs[1] 8021 0		--List of all Mesh Chunks, (Struct List ?)
			
			if MeshChunks == "Null Reference" then	--not found, dont know if relevant
			(
				MeshObj = Teapot Name:(MeshData.GroupName + DAOImportVars.Extension) Pos:MeshData.Position Rotation:MeshData.Rotation Radius:(0.02 * DAOTools.WorldScale)	--lol ,wonder whats that for
				MeshObj.parent = MeshData.Parent
			)
			if MeshChunks != "Null Reference" then for i = 1 to MeshChunks.count do		--Browsing the Chunks until finding the mesh were looking for
			(
				ChunkOffset		= MeshChunks[i][2]
				ChunkStructID	= MeshChunks[i][1]
				ChunkFields		= GFFFile.Structs[ChunkStructID]	
				ChunkName		= ReadDataByID MSHStream ChunkFields 2 ChunkOffset
							
				if ChunkName == MeshData.GroupName then	--names from MSH and MMH match, create it
				(
					MeshData.VertexSize			= ReadDataByID MSHStream ChunkFields 8000 ChunkOffset
					MeshData.VertexCount		= ReadDataByID MSHStream ChunkFields 8001 ChunkOffset
					MeshData.IndexCount			= ReadDataByID MSHStream ChunkFields 8002 ChunkOffset
					MeshData.PrimitiveType		= ReadDataByID MSHStream ChunkFields 8003 ChunkOffset
					MeshData.IndexFormat		= ReadDataByID MSHStream ChunkFields 8004 ChunkOffset
					MeshData.BaseVertexIndex	= ReadDataByID MSHStream ChunkFields 8005 ChunkOffset
					MeshData.VertexOffset		= ReadDataByID MSHStream ChunkFields 8006 ChunkOffset
					MeshData.MinIndex			= ReadDataByID MSHStream ChunkFields 8007 ChunkOffset
					MeshData.ReferencedVerts	= ReadDataByID MSHStream ChunkFields 8008 ChunkOffset
					MeshData.IndexOffset		= ReadDataByID MSHStream ChunkFields 8009 ChunkOffset
					MeshData.AdditionalStreams	= ReadDataByID MSHStream ChunkFields 8011 ChunkOffset
					--Bounds = ReadDataByID MSHStream ChunkFields 2 ChunkOffset
					MeshData.VertexDeclarator	= ReadDataByID MSHStream ChunkFields 8025 ChunkOffset
					disableSceneRedraw()
					
					MeshObj = CreateDAOMesh MSHStream MeshData
					ProgressDialog.BarBig.value += 90 / FileStates.MeshCount / 4
					
					if MeshObj != undefined and not isDeleted MeshObj then
					(
						custAttributes.add MeshObj customMeshParams
						if MeshID != "Null Reference "	then MeshObj.custMeshParams.NodeID = MeshID else MeshObj.custMeshParams.NodeID = RandomNodeIDString()
						if RuntimeShadowCasting == 1	then MeshObj.custMeshParams.CastRuntimeShadow = true
						if CastBakedShadow == 1			then MeshObj.custMeshParams.CastBakedShadow = true
						if ReceiveRuntimeShadow == 1	then MeshObj.custMeshParams.ReceiveRuntimeShadow = true
						if ReceiveBakedShadow == 1		then MeshObj.custMeshParams.ReceiveBakedShadow = true
						if CutAway == 1					then MeshObj.custMeshParams.CutAway = true
						if PunchThrough == 1			then MeshObj.custMeshParams.PunchThrough = true
						
						ImportMaterial MeshObj MaterialObj
					)
					gc()
					enableSceneRedraw()
				)
			)
		)
		fclose MSHStream
		LoadGFF MMHStream	--Going back at the MMH
	)
	
	fn ImportMMHChildren MMHStream ChildStructs Limit Parent Filetype = (		--Input #(#(StructRef,Flags,Offset),#(...),#(...))
																						--recursive function
		local Offset
		local ChildStructID
		local ChildFields
		local DataStructs
		local DataStructID
		local DataFields
		local pos
		local rot
		local Name
		local Index
		local Obj
		local CollObj
		local ParentNode
		
		if Parent != undefined then ParentNode = (getNodeByName (Parent + DAOImportVars.Extension) exact:true ignoreCase:true all:false)
		
		for i = 1 to ChildStructs.count do
		(
			ProgressDialog.BarSmall.value = 0
			Offset = ChildStructs[i][3]
			ChildStructID = ChildStructs[i][1]
			ChildFields = GFFFile.Structs[ChildStructID]
			
			if (ReadDataByID MMHStream ChildFields 6999 Offset) != undefined then 	--When there are Children
			(
				DataStructs = ReadDataByID MMHStream ChildFields 6999 Offset	--Collect the Children
				
				if GFFFile.Structs[ChildStructID].StructType == "node" and Limit == "Node" and Filetype == "MMH" then		--Is a node
				(
					Name			= ReadDataByID MMHStream ChildFields 6000 Offset
					Index			= ReadDataByID MMHStream ChildFields 6254 Offset
					pos				= ReadDataByIDInStructs MMHStream DataStructs 6047
					rot				= ReadDataByIDInStructs MMHStream DataStructs 6048
					MakeBoneArray Name Parent Pos Rot Index
					if Index >= 0 then DAOImportVars.BoneIndexArray[Index+1] = Name		--IndexArray for Bonesused stuff
				)
				if GFFFile.Structs[ChildStructID].StructType == "mshh" and Limit == "Node" and Filetype == "MMH" then		--If is a mesh
				(
					FileStates.MeshCount += 1	--Counting meshes for progressbar purpose
					append DAOImportVars.MeshNodeNames (ReadDataByID MMHStream ChildFields 6000 Offset)
					append DAOImportVars.MeshNames (ReadDataByID MMHStream ChildFields 6006 Offset)
				)
				if GFFFile.Structs[ChildStructID].StructType == "node" and Limit == "All" and Filetype == "MMH" then		--Is a node (has already been created -> parent it if not already)
				(
					Name			= ReadDataByID MMHStream ChildFields 6000 Offset
					pos				= ReadDataByIDInStructs MMHStream DataStructs 6047
					rot				= ReadDataByIDInStructs MMHStream DataStructs 6048
					Obj				= getNodeByName (Name + DAOImportVars.Extension) exact:true ignoreCase:true all:false
					if ParentNode == undefined and (classof Obj) == DAO_GameObject then
					(
						local SoundMat = (ReadDataByID MMHStream ChildFields 6330 Offset)
						if SoundMat != undefined then Obj.SoundMatType = SoundMat + 1
					)
					if Obj.parent == undefined and ParentNode != undefined then
					(
						Obj.parent = ParentNode
						in coordsys parent Obj.rotation = rot
						in coordsys parent Obj.position = pos * DAOTools.WorldScale
					)
				)
				if GFFFile.Structs[ChildStructID].StructType == "crst" and Limit == "All" and Filetype == "MMH" then		--Is Crust
				(
					Name			= ReadDataByID MMHStream ChildFields 6000 Offset
					Index			= ReadDataByID MMHStream ChildFields 6235 Offset
					pos				= ReadDataByIDInStructs MMHStream DataStructs 6047
					rot				= ReadDataByIDInStructs MMHStream DataStructs 6048
					if pos == undefined then pos = [0,0,0]		--temporary fix
					if rot == undefined then rot = (quat 0 0 0 1)--temporary fix
					Obj			= getNodeByName (Name + DAOImportVars.Extension) exact:true ignoreCase:true all:false
					if Obj == undefined then
					(
						Obj			= DAO_Crust position:pos Name:(Name + DAOImportVars.Extension)
						Obj.Size	= 0.1 * DAOTools.WorldScale
						Obj.HookID	= Index
						Obj.parent	= ParentNode
						in coordsys parent Obj.rotation = rot
						in coordsys parent Obj.position = pos * DAOTools.WorldScale
					)
				)
				if GFFFile.Structs[ChildStructID].StructType == "usrp" and Limit == "All" and Filetype == "MMH" then		--UserPoint
				(
					Name			= ReadDataByID MMHStream ChildFields 6000 Offset
					pos				= ReadDataByIDInStructs MMHStream DataStructs 6047
					rot				= ReadDataByIDInStructs MMHStream DataStructs 6048
					Obj				= getNodeByName (Name + DAOImportVars.Extension) exact:true ignoreCase:true all:false
					if Obj == undefined then
					(
						Obj 		= DAO_UserPoint position:pos Name:(Name + DAOImportVars.Extension)
						Obj.Size	= 0.3 * DAOTools.WorldScale
						Obj.parent	= ParentNode
						in coordsys parent Obj.rotation = rot
						in coordsys parent Obj.position = pos * DAOTools.WorldScale
					)				
				)
				if GFFFile.Structs[ChildStructID].StructType == "snap" and Limit == "All" and Filetype == "MMH" then		--SnaptoPoint
				(
					Name			= ReadDataByID MMHStream ChildFields 6000 Offset
					pos				= ReadDataByIDInStructs MMHStream DataStructs 6047
					rot				= ReadDataByIDInStructs MMHStream DataStructs 6048
					Obj				= getNodeByName (Name + DAOImportVars.Extension) exact:true ignoreCase:true all:false
					if Obj == undefined then
					(
						Obj			= DAO_SnapPoint position:pos Name:(Name + DAOImportVars.Extension)
						Obj.Size	= 0.3 * DAOTools.WorldScale
						Obj.parent	= ParentNode
						in coordsys parent Obj.rotation = rot
						in coordsys parent Obj.position = pos * DAOTools.WorldScale
					)
				)
				if GFFFile.Structs[ChildStructID].StructType == "wtrl" and Limit == "All" and Filetype == "MMH" then		--WeaponTrail
				(
					Name				= ReadDataByID MMHStream ChildFields 6000 Offset
					MaterialObj			= ReadDataByID MMHStream ChildFields 6001 Offset
					WtrlSegmentLenght	= ReadDataByID MMHStream ChildFields 6292 Offset
					WtrlDuration		= ReadDataByID MMHStream ChildFields 6293 Offset
					UseVariationTint	= ReadDataByID MMHStream ChildFields 6340 Offset
					pos					= ReadDataByIDInStructs MMHStream DataStructs 6047
					rot					= ReadDataByIDInStructs MMHStream DataStructs 6048
					Obj					= getNodeByName (Name + DAOImportVars.Extension) exact:true ignoreCase:true all:false
					if Obj == undefined then
					(
						Obj = DAO_Weapontrail Name:(Name + DAOImportVars.Extension) SegmentLenght:(WtrlSegmentLenght*DAOTools.WorldScale) Duration:(WtrlDuration)
						if (EqualsCaseless MaterialObj "weapontrail_streaks") or (EqualsCaseLess MaterialObj "weapontrail_arrow") then
						(
							Obj.MatObject = MaterialObj	--these 2 presets are build in Dragon Age, dont import MAO
						)
						else	--import custom mat from MAO
						(
							Obj.MatObject = ""
							Obj.MatObjectMat = ImportMaterial Obj MaterialObj ApplyMat:false
						)
						Obj.parent		= ParentNode
						in coordsys parent Obj.rotation = rot
						in coordsys parent Obj.position = pos * DAOTools.WorldScale
					)
				)
				if GFFFile.Structs[ChildStructID].StructType == "nemt" and Limit == "All" and Filetype == "MMH" then		--NodeEmitter
				(
				)
				if GFFFile.Structs[ChildStructID].StructType == "amap" and Limit == "All" and Filetype == "MMH" then		--AgeMap
				(
				)
				if GFFFile.Structs[ChildStructID].StructType == "amel" and Limit == "All" and Filetype == "MMH" then		--AgeMap Element
				(
				)
				if GFFFile.Structs[ChildStructID].StructType == "mshh" and Limit == "All" and Filetype == "MMH" then		--Is a Mesh
				(
					if DAOImport.ImportMesh == true then ImportMSH MMHStream ChildFields Offset ParentNode
				)
				if GFFFile.Structs[ChildStructID].StructType == "nplt" and Limit == "All" and Filetype == "MMH" then		--PointLight
				(
					Name 				= ReadDataByID MMHStream ChildFields 6000 Offset
					Index 				= ReadDataByID MMHStream ChildFields 6004 Offset
					PLTColor 			= ReadDataByID MMHStream ChildFields 6007 Offset
					PLTRadius 			= ReadDataByID MMHStream ChildFields 6008 Offset
					PLTStatic 			= ReadDataByID MMHStream ChildFields 6009 Offset
					PLTIntVar 			= ReadDataByID MMHStream ChildFields 6249 Offset
					PLTIntPer 			= ReadDataByID MMHStream ChildFields 6250 Offset
					PLTIntPerD 			= ReadDataByID MMHStream ChildFields 6251 Offset
					LightAffectDomain 	= ReadDataByID MMHStream ChildFields 6296 Offset
					CanBeOccluded 		= ReadDataByID MMHStream ChildFields 6339 Offset
					UseVariationTint 	= ReadDataByID MMHStream ChildFields 6340 Offset
					LightVersion 		= ReadDataByID MMHStream ChildFields 6345 Offset
					pos					= ReadDataByIDInStructs MMHStream DataStructs 6047
					rot					= ReadDataByIDInStructs MMHStream DataStructs 6048
					Obj					= getNodeByName (Name + DAOImportVars.Extension) exact:true ignoreCase:true all:false
					if Obj == undefined then
					(
						if (maxversion())[1] > 4200 then \
							Obj = Free_Point position:pos else \
							Obj = Omnilight position:pos
						Obj.rgb		= (color (PLTColor[1]*255) (PLTColor[2]*255) (PLTColor[3]*255))
						Obj.name	= Name + DAOImportVars.Extension
						Obj.parent	= ParentNode
						in coordsys parent Obj.rotation = rot
						in coordsys parent Obj.position = pos * DAOTools.WorldScale
					)
				)
				
				if GFFFile.Structs[ChildStructID].StructType == "nclt" and Limit == "All" and Filetype == "MMH" then		--Cloth Object
				(
					local MeshData = MeshDataStruct()
					MeshData.Parent				= ParentNode
					MeshData.GroupName			= ReadDataByID MMHStream ChildFields 6000 Offset
					local MaterialObj			= ReadDataByID MMHStream ChildFields 6001 Offset
					local MaterialLib			= ReadDataByID MMHStream ChildFields 6002 Offset
					local MeshID				= ReadDataByID MMHStream ChildFields 6004 Offset
					
					local ClothAttachmentType
					local ClothTwoWayAttachment
					local ClothTearableAttachment
					
					for j = 1 to DataStructs.count do	--search into Children Structs
					(
						local DataOffset = DataStructs[j][3]
						local DataStructID = DataStructs[j][1]
						local DataFields = GFFFile.Structs[DataStructID]
						
						if GFFFile.Structs[DataStructID].StructType == "trsl" then
						(
							MeshData.Position			= ReadDataByID MMHStream DataFields 6047 DataOffset
						)
						if GFFFile.Structs[DataStructID].StructType == "rota" then
						(
							MeshData.Rotation			= ReadDataByID MMHStream DataFields 6048 DataOffset
						)
						if GFFFile.Structs[DataStructID].StructType == "msgr" then
						(
							MeshData.VertexSize			= ReadDataByID MMHStream DataFields 8000 DataOffset
							MeshData.VertexCount		= ReadDataByID MMHStream DataFields 8001 DataOffset
							MeshData.IndexCount			= ReadDataByID MMHStream DataFields 8002 DataOffset
							MeshData.PrimitiveType		= ReadDataByID MMHStream DataFields 8003 DataOffset
							MeshData.IndexFormat		= ReadDataByID MMHStream DataFields 8004 DataOffset
							MeshData.VertexData			= ReadDataByID MMHStream DataFields 8022 DataOffset
							MeshData.IndexData			= ReadDataByID MMHStream DataFields 8023 DataOffset
							MeshData.VertexDeclarator	= ReadDataByID MMHStream DataFields 8025 DataOffset
							MeshData.VertexOffset		= 0	--only one chunk ever ,thus assume 0 ?
							MeshData.IndexOffset		= 0
						)
						if GFFFile.Structs[DataStructID].StructType ==  "catl" then
						(
							local ChildStructs2		= ReadDataByID MMHStream DataFields 6999 DataOffset
							ClothAttachmentType		= ReadDataByIDInStructs MMHStream ChildStructs2 6225
							ClothTwoWayAttachment	= ReadDataByIDInStructs MMHStream ChildStructs2 6227
							ClothTearableAttachment	= ReadDataByIDInStructs MMHStream ChildStructs2 6228
						)
					)
					local MeshObj = CreateDAOMesh MMHStream MeshData
					
					if MeshObj != undefined and not isDeleted MeshObj then
					(
						if (GetModifier MeshObj Skin) == 1 then addModifier MeshObj (DAO_Mod_Cloth ()) before:1 else addModifier MeshObj (DAO_Mod_Cloth ())
						
						local ClothMod = MeshObj.Modifiers[(GetModifier MeshObj DAO_Mod_Cloth)]
						
						if MeshID != "Null Reference"		then ClothMod.ClothNodeID = MeshID else ClothMod.ClothNodeID = RandomNodeIDString()
						
						ClothMod.ClothCastRuntimeShadow		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6176 Offset) false
						ClothMod.ClothCastBakedShadow		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6177 Offset) false
						ClothMod.ClothReceiveRuntimeShadow	= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6304 Offset) false
						ClothMod.ClothReceiveBakedShadow	= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6301 Offset) false
						ClothMod.ClothCutAway				= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6193 Offset) false
						ClothMod.ClothPunchThrough			= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6194 Offset) false
						
						ClothMod.ClothShapeCollMaskItems		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6065 Offset) false
						ClothMod.ClothShapeCollMaskCreatures	= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6066 Offset) false
						ClothMod.ClothShapeCollMaskPlaceables	= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6067 Offset) false
						ClothMod.ClothShapeCollMaskTriggers		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6068 Offset) false
						ClothMod.ClothShapeCollMaskStatic		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6069 Offset) false
						ClothMod.ClothShapeCollMaskEffects		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6178 Offset) false
						ClothMod.ClothShapeCollMaskWaypoints	= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6179 Offset) false
						ClothMod.ClothShapeCollMaskWater		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6277 Offset) false
						ClothMod.ClothShapeCollMaskTerrainWall	= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6295 Offset) false
						ClothMod.ClothShapeCollMaskWalkable		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6305 Offset) false
						ClothMod.ClothShapeCollMaskNonWalkable	= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6070 Offset) false
						
						ClothMod.ClothThickness						= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6195 Offset) (ClothMod.ClothThickness)
						ClothMod.ClothDensity						= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6196 Offset) (ClothMod.ClothDensity)
						ClothMod.ClothBendingStiffness				= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6197 Offset) (ClothMod.ClothBendingStiffness)
						ClothMod.ClothStretchingStiffness			= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6198 Offset) (ClothMod.ClothStretchingStiffness)
						ClothMod.ClothDampingCoefficient			= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6199 Offset) (ClothMod.ClothDampingCoefficient)
						ClothMod.ClothFriction						= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6200 Offset) (ClothMod.ClothFriction)
						ClothMod.ClothPressure						= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6201 Offset) (ClothMod.ClothPressure)
						ClothMod.ClothTearFactor					= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6202 Offset) (ClothMod.ClothTearFactor)
						ClothMod.ClothCollisionResponseCoefficient	= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6203 Offset) (ClothMod.ClothCollisionResponseCoefficient)
						ClothMod.ClothAttachmentResponseCoefficient	= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6204 Offset) (ClothMod.ClothAttachmentResponseCoefficient)
						ClothMod.ClothAttachmentTearFactor			= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6205 Offset) (ClothMod.ClothAttachmentTearFactor)
						ClothMod.ClothSolverIterations				= PushValue.AsInteger (ReadDataByID MMHStream ChildFields 6206 Offset) (ClothMod.ClothSolverIterations)
						ClothMod.ClothExternalAcceleration			= PushValue.AsPoint3 (ReadDataByID MMHStream ChildFields 6207 Offset) (ClothMod.ClothExternalAcceleration)
						ClothMod.ClothWakeUpCounter					= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6208 Offset) (ClothMod.ClothWakeUpCounter)
						ClothMod.ClothSleepLinearVelocity			= PushValue.AsFloat (ReadDataByID MMHStream ChildFields 6209 Offset) (ClothMod.ClothSleepLinearVelocity)
						
						ClothMod.ClothFlagPressure				= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6211 Offset) (ClothMod.ClothFlagPressure)
						ClothMod.ClothFlagStatic				= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6212 Offset) (ClothMod.ClothFlagStatic)
						ClothMod.ClothFlagDisableCollision		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6213 Offset) (ClothMod.ClothFlagDisableCollision)
						ClothMod.ClothFlagSelfCollision			= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6214 Offset) (ClothMod.ClothFlagSelfCollision)
						ClothMod.ClothFlagVisualization			= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6215 Offset) (ClothMod.ClothFlagVisualization)
						ClothMod.ClothFlagGravity				= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6216 Offset) (ClothMod.ClothFlagGravity)
						ClothMod.ClothFlagBending				= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6217 Offset) (ClothMod.ClothFlagBending)
						ClothMod.ClothFlagBendingOrtho			= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6218 Offset) (ClothMod.ClothFlagBendingOrtho)
						ClothMod.ClothFlagDamping				= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6219 Offset) (ClothMod.ClothFlagDamping)
						ClothMod.ClothFlagCollisionTwoWay		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6220 Offset) (ClothMod.ClothFlagCollisionTwoWay)
						ClothMod.ClothFlagTriangleCollision		= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6221 Offset) (ClothMod.ClothFlagTriangleCollision)
						ClothMod.ClothFlagTearable				= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6222 Offset) (ClothMod.ClothFlagTearable)
						ClothMod.ClothFlagHardware				= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6223 Offset) (ClothMod.ClothFlagHardware)
						ClothMod.ClothFlagCOMDamping			= PushValue.AsBool (ReadDataByID MMHStream ChildFields 6224 Offset) (ClothMod.ClothFlagCOMDamping)
						
						ClothMod.ClothAttachmentType			= (PushValue.AsInteger ((ClothAttachmentType)) (ClothMod.ClothAttachmentType - 1) ) + 1	--fix 1 basedness of Parameter
						ClothMod.ClothFlagTwoWayAttachment		= PushValue.AsBool (ClothTwoWayAttachment) (ClothMod.ClothFlagTwoWayAttachment)
						ClothMod.ClothFlagTearableAttachment	= PushValue.AsBool (ClothTearableAttachment) (ClothMod.ClothFlagTearableAttachment)
						
						ImportMaterial MeshObj MaterialObj
						deselect MeshObj	--must deselect, because the Cloth UI isnt up to date. Will update if the object is selected again
					)
				)
				if GFFFile.Structs[ChildStructID].StructType == "cobj" and Limit == "All" and Filetype == "PHY" then		--Collision Object
				(
					--CollObjDensity	= ReadDataByID MMHStream ChildFields 6056 Offset
					CollObjType		= ReadDataByID MMHStream ChildFields 6057 Offset
					--CollObjGroup	= ReadDataByID MMHStream ChildFields 6245 Offset
					for j = 1 to DataStructs.count do	--go thru single children, which are the shapes (can be multiple)
					(
						DataOffset = DataStructs[j][3]
						DataStructID = DataStructs[j][1]
						DataFields = GFFFile.Structs[DataStructID]
			
						if GFFFile.Structs[DataStructID].StructType == "shap" then
						(
							ShapeType				= ReadDataByID MMHStream DataFields 6058 DataOffset
							ShapeRotation			= ReadDataByID MMHStream DataFields 6060 DataOffset
							ShapePosition			= ReadDataByID MMHStream DataFields 6061 DataOffset
							ShapeCollMaskAll		= ReadDataByID MMHStream DataFields 6063 DataOffset
							ShapeCollMaskNone		= ReadDataByID MMHStream DataFields 6064 DataOffset
							ShapeCollMaskItems		= ReadDataByID MMHStream DataFields 6065 DataOffset
							ShapeCollMaskCreatures	= ReadDataByID MMHStream DataFields 6066 DataOffset
							ShapeCollMaskPlaceables	= ReadDataByID MMHStream DataFields 6067 DataOffset
							ShapeCollMaskTriggers	= ReadDataByID MMHStream DataFields 6068 DataOffset
							ShapeCollMaskStatic		= ReadDataByID MMHStream DataFields 6069 DataOffset
							ShapeCollMaskNonWalkable= ReadDataByID MMHStream DataFields 6070 DataOffset
							ShapeCollMaskEffects	= ReadDataByID MMHStream DataFields 6178 DataOffset
							ShapeCollMaskWaypoints	= ReadDataByID MMHStream DataFields 6179 DataOffset
							ShapeCollMaskWater		= ReadDataByID MMHStream DataFields 6277 DataOffset
							ShapeCollMaskTerrainWall= ReadDataByID MMHStream DataFields 6295 DataOffset
							ShapeCollMaskWalkable	= ReadDataByID MMHStream DataFields 6305 DataOffset
							PhysicsShapeVolume		= ReadDataByID MMHStream DataFields 6240 DataOffset
							ShapeName				= ReadDataByID MMHStream DataFields 6241 DataOffset
							ShapeAllowEmitter		= ReadDataByID MMHStream DataFields 6244 DataOffset
							ShapeCollGroup			= ReadDataByID MMHStream DataFields 6245 DataOffset
							ShapeStruct				= ReadDataByID MMHStream DataFields 6998 DataOffset
							
							CollObj					= getNodeByName (ShapeName + DAOImportVars.Extension) exact:true ignoreCase:true all:false
							if CollObj == undefined then
							(							
								if GFFFile.Structs[ShapeStruct[1][1]].StructType == "boxs" then
								(
									ShapeBoxDim = ReadDataByID MMHStream GFFFile.Structs[ShapeStruct[1][1]] 6071 ShapeStruct[1][3]
									CollObj = Box position:ShapePosition width:(ShapeBoxDim.x * 2 * DAOTools.WorldScale) length:(ShapeBoxDim.y * 2 * DAOTools.WorldScale) height:(ShapeBoxDim.z * 2 * DAOTools.WorldScale)
									if (maxversion())[1] > 4200 then CenterPivot CollObj else CollObj.pivot = CollObj.Center
									CollObj.name = ShapeName + DAOImportVars.Extension
									CollObj.parent = ParentNode
									in coordsys parent CollObj.rotation = ShapeRotation
									in coordsys parent CollObj.position = ShapePosition * DAOTools.WorldScale						
								)
								if GFFFile.Structs[ShapeStruct[1][1]].StructType == "caps" then
								(
									ShapeRadius = ReadDataByID MMHStream GFFFile.Structs[ShapeStruct[1][1]] 6072 ShapeStruct[1][3]
									ShapeHeight = ReadDataByID MMHStream GFFFile.Structs[ShapeStruct[1][1]] 6073 ShapeStruct[1][3]
									if (maxversion())[1] > 4200 then
									(
										CollObj = Capsule radius:(ShapeRadius * DAOTools.WorldScale) height:(ShapeHeight * DAOTools.WorldScale) heighttype:1
									)
									else	--For gmax doing a funky cylinder.
									(
										if ShapeHeight < (ShapeRadius * 2) then ShapeHeight = (ShapeRadius * 2)
										CollObj = Cylinder radius:(ShapeRadius * DAOTools.WorldScale) height:(ShapeHeight * DAOTools.WorldScale)
									)
									CollObj.name = ShapeName + DAOImportVars.Extension
									CollObj.parent = ParentNode
									in coordsys parent CollObj.rotation = ShapeRotation
									in coordsys parent CollObj.position = ShapePosition * DAOTools.WorldScale
								)
								if GFFFile.Structs[ShapeStruct[1][1]].StructType == "sphs" then
								(
									ShapeRadius = ReadDataByID MMHStream GFFFile.Structs[ShapeStruct[1][1]] 6072 ShapeStruct[1][3]
									CollObj = Sphere radius:(ShapeRadius * DAOTools.WorldScale)
									if (maxversion())[1] > 4200 then CenterPivot CollObj else CollObj.pivot = CollObj.Center
									CollObj.name = ShapeName + DAOImportVars.Extension
									CollObj.parent = ParentNode
									in coordsys parent CollObj.rotation = ShapeRotation
									in coordsys parent CollObj.position = ShapePosition * DAOTools.WorldScale	
								)
								if GFFFile.Structs[ShapeStruct[1][1]].StructType == "mshs" then
								(
									MeshShapeFlags			= ReadDataByID MMHStream GFFFile.Structs[ShapeStruct[1][1]] 6074 ShapeStruct[1][3]
									MeshHeightFieldAxis		= ReadDataByID MMHStream GFFFile.Structs[ShapeStruct[1][1]] 6075 ShapeStruct[1][3]
									MeshHeightFieldExtend	= ReadDataByID MMHStream GFFFile.Structs[ShapeStruct[1][1]] 6076 ShapeStruct[1][3]
									BinaryCookedData		= ReadDataByID MMHStream GFFFile.Structs[ShapeStruct[1][1]] 6077 ShapeStruct[1][3] --Array of UINT8, but only looking for the [1] here which i added the offset in the file to read it once more 
									if BinaryCookedData != "Null Reference" then
									(
										fseek MMHStream (GFFFile.Header.DataOffset + BinaryCookedData[1] + 4) #seek_set	--Thats the [1] ,only thing needed in the list. +4 skipping the listcount
										--fseek MMHStream (GFFFile.Header.DataOffset + BinaryCookedData[1]) #seek_set	--Thats the [1] ,only thing needed in the list. +4 skipping the listcount

										CollObj = editable_mesh name:(ShapeName + DAOImportVars.Extension)
										convertTo CollObj (Editable_Poly)
										
										--listcount = readlong MMHStream
										--format (listcount as string+"\n")
										
										NXSHeader = ""
										for k = 1 to 8 do NXSHeader += (bit.intaschar (readbyte MMHStream))
										NXSUnknown1 = readlong MMHStream
										NXSUnknown2 = readlong MMHStream
										NXSUnknown3 = readlong MMHStream
										NXSUnknown4 = readlong MMHStream
										NXSUnknown5 = readlong MMHStream
										NXSVertCount = readlong MMHStream
										NXSFaceCount = readlong MMHStream
										for k = 1 to NXSVertCount do at time 0 polyop.createvert CollObj [(readfloat MMHStream) * DAOTools.WorldScale,(readfloat MMHStream) * DAOTools.WorldScale,(readfloat MMHStream) * DAOTools.WorldScale] node:CollObj
										for k = 1 to NXSFaceCount do at time 0 polyop.createpolygon CollObj #((readbyte MMHStream #unsigned)+1,(readbyte MMHStream #unsigned)+1,(readbyte MMHStream #unsigned)+1)
										CollObj.parent = ParentNode
										in coordsys parent CollObj.rotation = ShapeRotation
										in coordsys parent CollObj.position = ShapePosition * DAOTools.WorldScale
										update CollObj
										--Lot of unknown stuff not read
									)
									else
									(
										MessageBox ("Error importing Collision Mesh Shape \""+ShapeName as string+"\"\nNo Mesh Data found.") title:"Model Import"
										CollObj = Box position:ShapePosition width:(1 * DAOTools.WorldScale) length:(1 * DAOTools.WorldScale) height:(1 * DAOTools.WorldScale)
										if (maxversion())[1] > 4200 then CenterPivot CollObj else CollObj.pivot = CollObj.Center
										CollObj.name = ShapeName + DAOImportVars.Extension
										CollObj.parent = ParentNode
										in coordsys parent CollObj.rotation = ShapeRotation
										in coordsys parent CollObj.position = ShapePosition * DAOTools.WorldScale
									)
								)
							)
						)
						--***************Adding Collision Parameters**********************
						custAttributes.add CollObj customCollisionParams
						
						if CollObjType				== 1 then CollObj.custCollParams.Type = 1
						if CollObjType				== 2 then CollObj.custCollParams.Type = 2
						if ShapeCollMaskWalkable	== 1 then CollObj.custCollParams.Walkable = 1
						if ShapeCollMaskNonWalkable	== 1 then CollObj.custCollParams.Walkable = 2
						--items:#("None","Item","Creature","Placeable","Static Geometry","Trigger","Terrain Wall","Water","Effects","WayPoint")
						if ShapeCollMaskWaypoints	== 1 then CollObj.custCollParams.Mask = 10
						if ShapeCollMaskEffects		== 1 then CollObj.custCollParams.Mask = 9
						if ShapeCollMaskWater		== 1 then CollObj.custCollParams.Mask = 8
						if ShapeCollMaskTerrainWall	== 1 then CollObj.custCollParams.Mask = 7
						if ShapeCollMaskTriggers	== 1 then CollObj.custCollParams.Mask = 6
						if ShapeCollMaskStatic		== 1 then CollObj.custCollParams.Mask = 5
						if ShapeCollMaskPlaceables 	== 1 then CollObj.custCollParams.Mask = 4
						if ShapeCollMaskCreatures 	== 1 then CollObj.custCollParams.Mask = 3
						if ShapeCollMaskItems 		== 1 then CollObj.custCollParams.Mask = 2
						
						--Catching a message if multiple Masks set
						if ShapeCollMaskWaypoints + ShapeCollMaskEffects + ShapeCollMaskWater + ShapeCollMaskTerrainWall + ShapeCollMaskTriggers + \
						   ShapeCollMaskStatic + ShapeCollMaskPlaceables + ShapeCollMaskCreatures + ShapeCollMaskItems > 1 then
						(
							MessageBox "OOps ,an unforseeable situation is happened. (Multiple Collision Masks)\nThe Model is fine dont worry.\nCan you tell Me or Chewy about it? (with model name)" title:"Error"
						)
					)--All Shapes Closed
				)--Collision Closed
				if DataStructs != "Null Reference" then
				(
					Name = (ReadDataByID MMHStream ChildFields 6000 Offset)
					ImportMMHChildren MMHStream DataStructs Limit Name Filetype		--recursive
				)
			)
			ProgressDialog.BarSmall.value = 100
		)
	)
	
	fn ImportMMH File = (			--Root function for everything about models
	
		local MMHStream = 0
		local PHYStream = 0
		local mmhName
		FileStates.MMHloaded = false
		FileStates.MeshCount = 0
		
		--Opening the MMH.
		--****************************************************************************************
		if File != undefined and DAOTools.ExistFile File then
		(
			MMHStream = fopen File "rb"
			if (LoadGFF MMHStream) and IsMMH() then
			(
				FileStates.MMHloaded = true
				FileStates.MMHFileName = File		--Giving guesses as to where the other files may be
				FileStates.MAOFileName = File		--These handle themselves if multiple files are loaded the guesses are refined
				FileStates.DDSFileName = File
			)
			else MessageBox "File is not a MMH. Aborting.." title:"Error"
		)
		else MessageBox "Wrong or no File was specified. Aborting.." title:"MMH Import"
		
		--Run one. Only Nodes, so the Skeleton can be created. Skeleton cant be created recursive.
		--****************************************************************************************
		if FileStates.MMHloaded then
		(
			createdialog ProgressDialog
			mmhName = ReadDataByID MMHStream GFFFile.Structs[1] 6000 0
			ChildStruct = (ReadDataByID MMHStream GFFFile.Structs[1] 6999 0)		--Getting first field, first childstruct, zero offset
			DAOImportVars.BoneArray = #()
			DAOImportVars.BoneIndexArray = #()
			DAOImportVars.MeshNodeNames = #()
			DAOImportVars.MeshNames = #()
			DAOImportVars.FXActorName = (ReadDataByID MMHStream GFFFile.Structs[1] 6248 0); if DAOImportVars.FXActorName == "Null Reference" then DAOImportVars.FXActorName = ""
			
			if ChildStruct != "Null Reference" then ImportMMHChildren MMHStream ChildStruct "Node" undefined "MMH"
			CreateSystem()
			ProgressDialog.BarBig.value = 5
		)
		
		--Run two. So the Skeleton has been done, create everything else.
		--****************************************************************************************
		if FileStates.MMHloaded then
		(
			LoadGFF MMHStream
			ChildStruct = (ReadDataByID MMHStream GFFFile.Structs[1] 6999 0)		--Getting first field ,first childstruct, zero offset
			if ChildStruct != "Null Reference" then ImportMMHChildren MMHStream ChildStruct "All" undefined "MMH"
		)
		
		--Lastly load the PHY File and create the collision stuff. Pretty similar to MMH , so runs same function with a limiter
		--****************************************************************************************		
		if FileStates.MMHloaded and DAOImport.ImportColl then		--Assuming that MMH has been loaded
		(
			PHYFile = SearchFile DAOTools.ImportPaths (getFilenamePath FileStates.MMHFileName) ((getFilenameFile FileStates.MMHFileName)+".phy") "PHY"
			if PHYFile != undefined then
			(
				PHYStream = fopen PHYFile "rb"
				if (LoadGFF PHYStream) and IsPHY() then 
				(
					if MMHName == (ReadDataByID PHYStream GFFFile.Structs[1] 6000 0) then
					(
						ChildStruct = (ReadDataByID PHYStream GFFFile.Structs[1] 6999 0)
						if ChildStruct != "Null Reference" then ImportMMHChildren PHYStream ChildStruct "All" undefined "PHY"
						ProgressDialog.BarBig.value = 100
					)
					else MessageBox "MMH and PHY Names dont match, Skipping.." title:"MMH Import"
				)
				else MessageBox "PHY File is invalid, Skipping.." title:"MMH Import"
			)
			else MessageBox "PHY File not found, Skipping.." title:"MMH Import"
		)
		
		--Finish up
		if FileStates.MMHloaded then 
		(
			MessageBox "Finished." title:"MMH Import"
			try(Destroydialog ProgressDialog)catch()
		)
		fclose MMHStream
		fclose PHYStream
	)