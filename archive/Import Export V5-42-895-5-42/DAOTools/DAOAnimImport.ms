/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edit:
6 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Include Script
DAOAnimImport.ms
Main Functions Animation Import
************************************************
*/

	struct DAOAnimImportSeqStruct (
		Name,
		Pivot,
		Start,
		End,
		HQ,
		Addv,
		Override,
		GAD,
		GADEnt,
		GADFH,
		APRScale
	)
	DAOAnimImportSeq = DAOAnimImportSeqStruct()
	
	struct DAOAnimImportTracksStruct (
		Nodes,
		SettingPos,
		SettingRot
	)
	DAOAnimImportTracks = DAOAnimImportTracksStruct()
	
	fn StoreAnimSequence GOBNode = (
		--*********************************************************
		--Storing the collected Sequence settings into Game object
		--*************************************************************
		if (classof GOBNode) == DAO_GameObject then
		(
			--unify count. Due to updates on gameobject, added properties may need to be forced in line
			GOBNode.SequencePivot.count = GOBNode.SequenceStart.count = GOBNode.SequenceEnd.count = GOBNode.SequenceAPRScale.count = GOBNode.SequenceHQ.count = GOBNode.SequenceAddv.count = GOBNode.SequenceOverride.count = GOBNode.SequenceGAD.count = GOBNode.SequenceGADEnt.count = GOBNode.SequenceGADFH.count = GOBNode.SequenceName.count
			
			if DAOAnimImportSeq.Addv or DAOAnimImportSeq.Override then
			(
				DAOAnimImportSeq.GAD = false
				DAOAnimImportSeq.GADEnt = false
				DAOAnimImportSeq.GADFH = false
			)
			
			append GOBNode.SequenceName (DAOAnimImportSeq.Name as string)
			append GOBNode.SequencePivot (DAOAnimImportSeq.Pivot as integer)
			append GOBNode.SequenceStart (DAOAnimImportSeq.Start as integer)
			append GOBNode.SequenceEnd (DAOAnimImportSeq.End as integer)
			append GOBNode.SequenceHQ (DAOAnimImportSeq.HQ as booleanclass)
			append GOBNode.SequenceAddv (DAOAnimImportSeq.Addv as booleanclass)
			append GOBNode.SequenceOverride (DAOAnimImportSeq.Override as booleanclass)
			append GOBNode.SequenceGAD (DAOAnimImportSeq.GAD as booleanclass)
			append GOBNode.SequenceGADEnt (DAOAnimImportSeq.GADEnt as booleanclass)
			append GOBNode.SequenceGADFH (DAOAnimImportSeq.GADFH as booleanclass)
			append GOBNode.SequenceAPRScale (DAOAnimImportSeq.APRScale as point3)
			
			local NewSeqIndex = GOBNode.SequenceName.count
			local NewSetting
			for i = 1 to DAOAnimImportTracks.Nodes.count do
			(
				NewSetting = UnpackTrackSetting 0	--get the default setting
				NewSetting.Pos = DAOAnimImportTracks.SettingPos[i]
				NewSetting.Rot = DAOAnimImportTracks.SettingRot[i]
				SetTrackSetting GOBNode DAOAnimImportTracks.Nodes[i] NewSeqIndex NewSetting
			)
		)
	)
	
	fn ImportGAD AniFile AniName AniEndFrame = (
		--***********************************************
		--GAD Import, adding on top and merging with "Root"
		--Anifile is for automaticly finding GAD
		--AniName is to compare names
		--**********************************************
		FileStates.GADloaded = false
		
		local GADStream
		local GADAniName
		local GADAniLenght
		local HasGOBAnim
		local Warning = false
		local Elements
		local Index
		
		local RootObj
		local GOBObj = getNodeByName ("GOB"+DAOImportVars.Extension) exact:true ignoreCase:true all:false
		local objs = GetHierarchy #(GOBObj)
		for object in objs do if (EqualsCaseLess (cutstring object.name DAOImportVars.Extension) "Root") then RootObj = object
		
		
		if RootObj != undefined and GOBObj != undefined then
		(
			GADFile = SearchFile DAOTools.ImportPaths (getFilenamePath AniFile) ((getFilenameFile AniFile)+".gad") "GAD"
			if GADFile != undefined then
			(
				GADStream = fopen GADFile "rb"
				if (LoadGFF GADStream) and isGAD() then
				(
					FileStates.GADloaded = true
				)
				else MessageBox "GAD File is invalid. Skipping.." title:"Ani Import"
			)
			else MessageBox "No GAD Animation File was found. Skipping.." title:"Ani Import"
		)
		else MessageBox "Root and/or GOB wasnt found in the Model. Cannot apply GAD. Skipping.." title:"Ani Import"
		
		if FileStates.GADloaded then
		(
			GADAniName		= ReadDataByID GadStream GFFFile.Structs[1] 4007 0
			GADAniLenght	= ReadDataByID GadStream GFFFile.Structs[1] 4009 0
			HasGOBAnim		= ReadDataByID GadStream GFFFile.Structs[1] 4008 0
			--Elements		= ReadDataByID GadStream GFFFile.Structs[1] 4003 0	--not needed?
			
			if not matchpattern AniName pattern:("*"+GADAniName+"*") then Warning = true
			if Warning == true then if QueryBox "Anim Names dont seem to match. Likely this GAD will not work \nContinue anyway?" title:"Warning" then Warning = false
		
			if Warning == false then
			(
				if HasGOBAnim == 1 then
				(
					DAOAnimImportSeq.GAD = true
					
					NodeList 	= ReadDataByID GadStream GFFFile.Structs[1] 4005 0	--Is Structs: Format:#(#(StructRef,Offset),#(StructRef,Offset),...)
					
					for runs = 1 to 2 do	--doing 2 runs to ensure: rotation first, translation last
					(
						if NodeList != "Null Reference" then for i = 1 to NodeList.Count do		--For number of Animation Nodes
						(
							StructRef	= NodeList[i][1]
							Offset		= NodeList[i][2]
							
							NodeName 	= ReadDataByID GadStream GFFFile.Structs[StructRef]	4000 Offset	--Name
							Elements	= ReadDataByID GadStream GFFFile.Structs[StructRef]	4003 Offset --Think that is a number count
							NodeData	= ReadDataByID GadStream GFFFile.Structs[StructRef]	4004 Offset	--Array of manymany numbers
							
							HasRotation 	= matchpattern NodeName pattern:"*rotation*"
							HasTranslation 	= matchpattern NodeName pattern:"*translation*"	
							
							local StillAnim = true	--detect movement, to determine a tracksetting
							
							animate on
							(
								--Setting T-Pose Frames at 0, cause on max 2011+ the autokey can be turned off
								addnewkey RootObj.position.controller 0		
								addnewkey RootObj.rotation.controller 0
								addnewkey GOBObj.position.controller 0
								addnewkey GOBObj.rotation.controller 0
								
								--Locking current animation of Root, by keyframing all the transforms.
								if not DAOTools.isGmax then 
								(
									for j = DAOImport.AniStartFrame to AniEndFrame do at time j RootObj.transform = RootObj.transform
								)
								else
								(
									for j = DAOImport.AniStartFrame to AniEndFrame do
									(	--Gmax behaves weird here
										at time j RootObj.position = RootObj.position
										at time j RootObj.rotation = RootObj.rotation
									)
								)
								--And now can work on the GAD
								
								for j = 1 to (NodeData.count/(Elements+1)) do
								(
									Index = (j * (Elements+1))-Elements		--Need to index plain list
									KeyTime 	= NodeData[Index][1]
									KeyData0 	= NodeData[Index+1][1]
									KeyData1 	= NodeData[Index+2][1]
									KeyData2 	= NodeData[Index+3][1]
									if Elements >= 4 then KeyData3 	= NodeData[Index+4][1]	--Not anymore Elements needed anyway so not counting
								
									TimeStep = RoundMe ((KeyTime * DAOImport.AniFrameRate)+DAOImport.AniStartFrame)
									if HasRotation and runs == 1 then	--Run 1, do rotation first
									(
										if ((abs KeyData0) > 0.0001 or (abs KeyData1) > 0.0001 or (abs KeyData2) > 0.0001) then
										(
											StillAnim = false
											if j == 1 then DAOAnimImportSeq.GADEnt = true
										)
										
										local GADrot = inverse (quat KeyData0 KeyData1 KeyData2 KeyData3) as matrix3
										at time TimeStep RootObj.transform = RootObj.transform * inverse GOBObj.transform * GADrot * GOBObj.transform
									)
									if HasTranslation and runs == 2 then	--gad is crazy Run2, translation last
									(
										if ((abs KeyData0) > 0.0001 or (abs KeyData1) > 0.00001 or (abs KeyData2) > 0.00001) then
										(
											StillAnim = false
											if j == 1 then DAOAnimImportSeq.GADEnt = true
										)
										if (abs KeyData2) > 0.5 then DAOAnimImportSeq.GADFH = true
										
										local GADpos = transmatrix [KeyData0 * DAOTools.WorldScale,KeyData1 * DAOTools.WorldScale,KeyData2 * DAOTools.WorldScale]
										at time TimeStep RootObj.transform = RootObj.transform * inverse GOBObj.transform * GADpos * GOBObj.transform
									)
								)
							)	--animate on close
							
							--Adding special case to Tracksetting, for storing later
							if StillAnim == true then
							(
								local ObjIndex = finditem DAOAnimImportTracks.Nodes GOBObj
								if ObjIndex == 0 then
								(
									append DAOAnimImportTracks.Nodes GOBObj
									if HasTranslation and runs == 2 then append DAOAnimImportTracks.SettingPos #Min else append DAOAnimImportTracks.SettingPos #Auto
									if HasRotation and runs == 1	then append DAOAnimImportTracks.SettingRot #Min else append DAOAnimImportTracks.SettingRot #Auto
								)
								else
								(
									if HasTranslation and runs == 2 then DAOAnimImportTracks.SettingPos[ObjIndex] = #Min
									if HasRotation and runs == 1	then DAOAnimImportTracks.SettingRot[ObjIndex] = #Min
								)
							)
						)
					)
				)
				else MessageBox "No GAD available. Skipping.." title:"Ani Import"
			)
			else MessageBox "Name of GAD and Animation files dont match. Skipped by user.." title:"Ani Import"
		)
		try(fclose GadStream)catch()
	)

	fn ImportAni File = (
		
		local AniStream
		local AniName
		local AniLenght
		local IsAdditive
		local isOverride
		local AniEndframe
		local NodeName
		
		local GOBObj = getNodeByName ("GOB"+DAOImportVars.Extension) exact:true ignoreCase:true all:false
		local ObjsAll = GetHierarchy #(GOBObj)
		
		if File != undefined and DAOTools.ExistFile File then
		(
			AniStream = fopen File "rb"			
			if (LoadGFF AniStream) and IsAni() then
			(
				AniName		= ReadDataByID AniStream GFFFile.Structs[1] 4006 0
				AniLenght	= ReadDataByID AniStream GFFFile.Structs[1] 4009 0
				IsAdditive	= ReadDataByID AniStream GFFFile.Structs[1] 4011 0
				IsOverride	= ReadDataByID AniStream GFFFile.Structs[1] 4012 0
				
				AniEndFrame = RoundMe ((DAOImport.AniFrameRate * AniLenght) + DAOImport.AniStartFrame)
				
				DAOAnimImportSeq.Name = (cutstring AniName ".ani") as string
				DAOAnimImportSeq.Pivot = 2
				DAOAnimImportSeq.Start = DAOImport.AniStartFrame as integer
				DAOAnimImportSeq.End = AniEndFrame as integer
				DAOAnimImportSeq.HQ = true
				DAOAnimImportSeq.Addv = (if IsAdditive == 0 then false else true)
				DAOAnimImportSeq.Override = (if IsOverride == 0 then false else true)
				DAOAnimImportSeq.GAD = false
				DAOAnimImportSeq.GADEnt = false
				DAOAnimImportSeq.GADFH = false
				DAOAnimImportSeq.APRScale = [1,1,1] as point3
				
				DAOAnimImportTracks.Nodes = #()
				DAOAnimImportTracks.SettingPos = #()
				DAOAnimImportTracks.SettingRot = #()
				
				if querybox ("Importing...\n\nAnimationname:    "+AniName as string+ \
							 "\n\nAdding to Game Object:\"GOB"+DAOImportVars.Extension+"\""+ \
							 "\n\nAnimationlenght:   "+AniLenght as string+\
							 " sek\nIs Additive:           "+(if IsAdditive == 1 then "Yes" else "No")+\
							 "\nIs Override:          "+(if IsOverride == 1 then "Yes" else "No")+\
							 "\nImporting GAD:     "+(if DAOImport.ImportGAD == true then "Yes" else "No")+\
							 "\n\nStarting Frame:    "+DAOImport.AniStartFrame as string+\
							 "\nEnd Frame:          "+AniEndFrame as string+\
							 "  (projected)\nFramerate:          "+DAOImport.AniFrameRate as string+\
							 "\n\nWorldscale setting:     x"+DAOTools.WorldScale as string+\
							 (if framerate != DAOImport.AniFrameRate then "\n\nSwitching to new Framerate.." else "\n\n")+\
							 "\nContinue with these settings?") title:"Ani Import" then
				(
					framerate = DAOImport.AniFrameRate
					animationrange = interval DAOImport.AniStartFrame AniEndFrame
					disableSceneRedraw()
					createdialog ProgressDialog

					NodeList 	= ReadDataByID AniStream GFFFile.Structs[1] 4005 0
					
					if NodeList != "Null Reference" then for i = 1 to NodeList.Count do try		--For number of Animation Nodes
					(
						ProgressDialog.BarBig.value = i * 100 / NodeList.Count
						StructRef	= NodeList[i][1]
						Offset		= NodeList[i][2]
						NodeName 	= ReadDataByID AniStream GFFFile.Structs[StructRef] 			4000 Offset				--Is the name of the Node
						Target 		= ReadDataByID AniStream GFFFile.Structs[StructRef] 			4001 Offset				--Target is number of values or something
						NodeData1 	= ReadDataByID AniStream GFFFile.Structs[StructRef] 			4004 Offset				--Is List of one Ref: Format: #(#(StructRef, FieldFlags ,FieldOffset))
						NodeData2 	= ReadDataByID AniStream GFFFile.Structs[NodeData1[1][1]] 		4004 NodeData1[1][3]	--Is List of Structs: Format:#(#(StructRef,Offset),#(StructRef,Offset),..) 
							
						HasRotation 	= matchpattern NodeName pattern:"*rotation*"
						HasTranslation 	= matchpattern NodeName pattern:"*translation*"
						
						Namebuild=""
						for k = 1 to ((filterstring NodeName "_").count-1) do
						(
							if k > 1 then Namebuild += "_"
							Namebuild += (filterstring NodeName "_")[k]
						)
						local Obj = undefined
						for object in ObjsAll do if (EqualsCaseLess (cutstring object.name DAOImportVars.Extension) Namebuild) then Obj = object
				
						if Obj != undefined and (HasRotation or HasTranslation) then
						(
							local ObjTransform0		= at time 0 					  (Obj.transform * inverse(Obj.parent.transform))	--reference for all animations
							local ObjTransformStart = at time DAOImport.AniStartFrame (Obj.transform * inverse(Obj.parent.transform))	--additional reference for additive animations
							
							local StillAnim = true	--detect movement, to determine a tracksetting
							
							animate on
							(
								addnewkey Obj.position.controller 0		--Setting T-Pose Frames at 0, cause on max 2011 the autokey can be turned off (or to frame 1)
								addnewkey Obj.rotation.controller 0
								for j = 1 to NodeData2.Count do			--For Number of DataEntries (Keyframes) per Node
								(
									ProgressDialog.BarSmall.value = j * 100 / NodeData2.count
									NodeStructRef 	= NodeData2[j][1]
									NodeOffset		= NodeData2[j][2]
									
									KeyTime		= ReadDataByID AniStream GFFFile.Structs[NodeStructRef] 4035 NodeOffset
									KeyData0 	= ReadDataByID AniStream GFFFile.Structs[NodeStructRef] 4036 NodeOffset
									KeyData1 	= ReadDataByID AniStream GFFFile.Structs[NodeStructRef] 4037 NodeOffset
									KeyData2 	= ReadDataByID AniStream GFFFile.Structs[NodeStructRef] 4038 NodeOffset
									KeyData3 	= ReadDataByID AniStream GFFFile.Structs[NodeStructRef] 4039 NodeOffset
									
									TimeStep 	= RoundMe ((KeyTime * AniLenght / 65535 * DAOImport.AniFrameRate) + DAOImport.AniStartFrame)
									
									if HasRotation then
									(
										local ObjRotationPivot
										local ObjRotationDiff	--for rotation detection, to interpret a tracksetting
										
										if Target == 2 then DAOAnimImportSeq.HQ = false		--16 bit quats, store that in sequence as well
										if Target == 4 then ObjRotation = (DecompressQuat KeyData0[2] KeyData0[1] 0 Target)
										if Target == 2 then ObjRotation = (DecompressQuat KeyData0 0 0 Target)
										if Target == 3 then ObjRotation = (DecompressQuat KeyData0 KeyData1 KeyData2 Target)
										
										/*	doesnt work for some reason
										if IsAdditive == 1 then		--Relative rotation on additive animations
										(
											at time TimeStep ObjRotationPivot = obj.parent.transform.rotationpart * ObjRotation * inverse ObjTransformStart.rotationpart
										)
										else	--absolute rotation
										(
											at time TimeStep ObjRotationPivot = obj.parent.transform.rotationpart * ObjRotation
										)
										ObjRotationDiff = ObjRotationOld * inverse ObjRotationPivot
										*/
										at time TimeStep ObjRotationPivot = translate(inverse(ObjRotation as matrix3)*(obj.parent.transform.rotationpart) as matrix3) obj.transform.translationpart	--Applying the rotation, weird method.. rotate around its pivot only..while keeping translation
										if IsAdditive == 1 then ObjRotationPivot = ObjTransformStart * ObjRotationPivot	--adding the additive reference transformation. This isnt as featureful as the game is so very rough
										
										--Detect Animation
										--ObjRotationDiff = ObjRotationOld * inverse ObjRotationPivot.rotationpart
										ObjRotationDiff = ObjRotation * ObjTransform0.rotationpart
										
										--format (Obj.Name+"  Frame: "+TimeStep as string+ "  rot: "+ObjRotationDiff as string+"\n")
										if abs ObjRotationDiff.x >= 0.0001 or \
										   abs ObjRotationDiff.y >= 0.0001 or \
										   abs ObjRotationDiff.z >= 0.0001 then StillAnim = false
										
										
										--at time TimeStep obj.rotation = ObjRotationPivot
										at time TimeStep obj.transform = ObjRotationPivot
										if DAOTools.isGmax and keytime == 0 then
										(	--This line is for convinience only, adds a helpful keyframe to even beziercurve (Frame0 often has abrupt transforms)
											at time (TimeStep+1) obj.transform = ObjRotationPivot
											selectkeys obj.position.controller (Timestep+1)
											deletekeys obj.position.controller #selection
										)
										
										--************************************************
										--Deleting Position key, added by applying rotation
										--It interferes when no real position exists at that frame
										--This assumes Rotation is read first from file
										--*************************************************
										selectkeys obj.position.controller Timestep
										deletekeys obj.position.controller #selection
									)
									if HasTranslation and Target == 6 then
									(
										local ObjPositionDiff
										local ObjPosition = [KeyData0 * DAOTools.WorldScale,KeyData1 * DAOTools.WorldScale,KeyData2 * DAOTools.WorldScale]
										if DAOImport.AniFilter == true and obj.parent.boneenable == true and (abs KeyData0) < 0.02 and (abs KeyData1) < 0.02 and (abs KeyData2) < 0.02 then
										(	--filtered, doing nothing		(Filtering: Bones shall not move out of their dependency, removing inbetween 2cm of movement as that very likely is not nessesary. Anything slipping thru will add to trouble i am certain.)
											--format ("Filtered out Light:   "+obj.name as string+"  "+KeyData0 as string+"  "+Keydata1 as string+"  "+Keydata2 as string+"\n")
										)
										else if DAOImport.AniFilter2 == true and obj.parent.boneenable == true then
										(	--Filtered any translation between bones. This causes jittering, but could be a wrong bone enabled.
											--format ("Filtered out Strong:   "+obj.name as string+"  "+KeyData0 as string+"  "+Keydata1 as string+"  "+Keydata2 as string+"\n")
										)
										else
										(
											at time TimeStep obj.position = ((ObjTransform0)*(transmatrix ObjPosition)*(obj.parent.transform)).translationpart
											if DAOTools.isGmax and keytime == 0 then
											(	--This line is for convinience only, adds a helpful keyframe to even beziercurve (Frame0 often has abrupt transforms)
												at time (TimeStep+1) obj.position = ((ObjTransform0)*(transmatrix ObjPosition)*(obj.parent.transform)).translationpart
											)
											
											--Detect Animation
											--format (Obj.Name+"  Frame: "+TimeStep as string+ "  pos: "+ObjPosition as string+"\n")
											ObjPositionDiff = ObjTransform0.translationpart - ObjPosition
											if abs ObjPositionDiff.x >= 0.0001 or abs ObjPositionDiff.y >= 0.0001 or abs ObjPositionDiff.z >= 0.0001 then StillAnim = false
										)
									)
								) --frame loop end
							) --animate on close
							
							--Adding special case to Tracksetting, for storing later
							if StillAnim == true then
							(
								local ObjIndex = finditem DAOAnimImportTracks.Nodes Obj
								if ObjIndex == 0 then
								(
									append DAOAnimImportTracks.Nodes Obj
									if HasTranslation	then append DAOAnimImportTracks.SettingPos #Min else append DAOAnimImportTracks.SettingPos #Auto
									if HasRotation		then append DAOAnimImportTracks.SettingRot #Min else append DAOAnimImportTracks.SettingRot #Auto
								)
								else
								(
									if HasTranslation	then DAOAnimImportTracks.SettingPos[ObjIndex] = #Min
									if HasRotation		then DAOAnimImportTracks.SettingRot[ObjIndex] = #Min
								)
							)
						)
					) --node loop end
					catch
					(
						MessageBox ("Error creating Animation while adding to Node: "+(NodeName) as string+"\nAccident ,or error in Animation File\n\nRuntime Error:\n\n"+(if (maxversion())[1] >= 7000 then GetCurrentException() else "unavailable")) title:"Error"
					)
					if DAOImport.ImportGAD == true then ImportGAD File AniName AniEndFrame
					StoreAnimSequence GOBObj
					try(Destroydialog ProgressDialog)catch()
					enableSceneRedraw()
					MessageBox "Finished." title:"Ani Import"
				)
				else MessageBox "User Abort.." title:"Ani Import"		
			)
			else MessageBox "File is not an ANI. Aborting.." title:"Ani Import"
			fclose AniStream			
		)
		else MessageBox "Wrong or no File was specified. Aborting.." title:"Ani Import"	
	)