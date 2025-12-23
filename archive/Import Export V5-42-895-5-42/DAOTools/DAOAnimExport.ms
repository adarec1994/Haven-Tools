/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edited
8 September 2010

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2010 Eshme

************************************************************************************************************
Animation Export Script
************************************************
*/

struct DAOAnimStruct
(
	--MenuSettings
	--************
	UI_Pos,
	UI_Height,
	UI_HQ = true,
	UI_Addv = false,
	UI_Override = false,
	UI_GAD = false,
	UI_GADEnt = false,
	UI_GADHeight = false,
	UI_Pivot = 1,
	UI_Sequence = 3,
	UI_APRScale = [1.0,1.0,1.0]
)

try (DestroyDialog DAOAnimToolsRollout)catch()
global DAOAnimToolsRollout
global DAOAnim
(
	include "DAOTools\\DAORollouts.ms"
	include "DAOTools\\DAOcommon.ms"
	
	--******************************
	--Intermediate Variable storage
	--*******************************
	struct DAOAnimVarsStruct (
		CurrentExtension,
		GOBNode,
		RootNode
	)
	DAOAnimVars = DAOAnimVarsStruct()
	
	--***********************************************
	--Sequence settings stored during export here
	--**********************************************
	struct DAOAnimSequenceStruct (
		SequenceIndex,			--Index of Sequence, for getting its Track Data
		HasTrackSettings,		--Allows the Exporter to read Track Settings from GOB, only when exporting Sequences when GOB is Gameobject class
		Name,					--Animation sequence name
		FrameStart,				--Start Frame
		FrameEnd,				--End Frame
		GOBOffset,				--World/GOB/Current Location aligned export
		HighQuality,			--32 or 64 bit compression
		Additive,				--Animation Type additive yes/no
		Override,				--Animation Type override yes/no
		GADExport,				--Animation type GAD yes/no. Enable/disable XY animation for GOB
		GADExportEnt,			--Entrance GAD enabled/disabled
		GADExportHeight,		--Enable/Disable exporting Z animation for GOB
		APRScale				--Scale adjustments for the exporter
		
	)
	DAOAnimSeq = DAOAnimSequenceStruct()
	
	--***********************************************
	--Initialize Animation Export per Session
	--***********************************************
	fn Initialize = (
		DAOAnim = DAOAnimStruct UI_Height:597
		if ((maxversion())[1] >= 7000) then DAOAnim.UI_Pos = getMAXWindowPos() + (getMAXWindowSize()/4) else DAOAnim.UI_Pos = [100,100]
		try(DAOAnim.UI_Pos.x = (DAOTools.ReadIni "Anim" "UI_Pos_X") as integer)catch()
		try(DAOAnim.UI_Pos.y = (DAOTools.ReadIni "Anim" "UI_Pos_Y") as integer)catch()
		try(DAOAnim.UI_Height = (DAOTools.ReadIni "Anim" "UI_Height") as integer)catch()
	)
	
	--************************************************************
	--Main Animation Export calculations
	--Function is given one or more Child Objects to process
	--Recursive
	--Relies on sequence settings stored in higher level variables
	--*************************************************************
	fn ExportAnimChilds Stream Objs Progress = (
		
		local objTransform		--Current Transform in parent space
		local objTransform0		--Transform in parent space at reference pose
		local objOffsetPivot	--Calculating Offset Pivot when not exporting at World Center
		
		local objposition		--output fraction
		local objrotation
		local frameTime
		local posArray
		local rotArray
		
		local expposframe
		local exprotframe
		
		local isGOB		--Evaluating each node if it is GOB or Root.
		local isRoot	--This includes caseless match, as well a match regardless of the extension used.
	
		local GADNorm	--5 for GAD calculation
		local GADOffset
		local GADges
		local GADtrans
		local GADtransRoot
		
		local TrackSetting
		
		for i = 1 to Objs.count where (GetObjectAnimExportType Objs[i]) != undefined do
		(
			ProgressDialog.BarBig.value = Progress[1]*100/Progress[2]
			ProgressDialog.Label.text = (Progress[3]+"Writing Animation Data \""+Objs[i].name+"\"")
			Progress[1] += 1
			isGOB = (Objs[i] == DAOAnimVars.GOBNode)
			isRoot = (Objs[i] == DAOAnimVars.RootNode)
			
			posArray = #()
			rotArray = #()
			TrackSetting = undefined
			
			if DAOAnimSeq.HasTrackSettings == true then
			(
				TrackSetting = GetTrackSetting DAOAnimVars.GOBNode Objs[i] DAOAnimSeq.SequenceIndex
			)
			else	--make up a default setting ,when unable to read
			(
				TrackSetting = UnpackTrackSetting 0	--default autodetect setting
			)
			--Override some Settings on GAD Export by default.
			if DAOAnimSeq.GADExport == true and (isGOB or isRoot) then
			(
				TrackSetting.Pos = #Full
				TrackSetting.Rot = #Full
			)
			
			if TrackSetting.Pos != #Nil or TrackSetting.Rot != #Nil then
			(
				--********************************************
				--Getting Zero Transforms for Node (Reference)
				--********************************************
				if DAOAnimSeq.Additive == false then
				(
					if Objs[i].parent != undefined and (GetObjectExportType Objs[i].parent) == #Weapontrail then
					(
						at time 0 objTransform0 = (Objs[i].transform) * inverse(Objs[i].parent.objecttransform)
					)
					else if Objs[i].parent != undefined then
					(
						at time 0 objTransform0 = Objs[i].transform * inverse Objs[i].parent.transform
					)
					else at time 0 objTransform0 = Objs[i].transform
				)
				else if DAOAnimSeq.Additive == true then
				(
					if Objs[i].parent != undefined and (GetObjectExportType Objs[i].parent) == #Weapontrail then
					(
						at time DAOAnimSeq.FrameStart objTransform0 = Objs[i].transform * inverse Objs[i].parent.objecttransform
					)
					else if Objs[i].parent != undefined then
					(
						at time DAOAnimSeq.FrameStart objTransform0 = Objs[i].transform * inverse Objs[i].parent.transform
					)
					else at time DAOAnimSeq.FrameStart objTransform0 = Objs[i].transform
				)
				
				--**************************************************************************************
				--Get the Models Local Position, to start the Animation from, when it was selected
				--This is a Offset, just like GOB is when setting World Center, but with some limitations
				--*************************************************************************************
				if DAOAnimSeq.GOBOffset == 3 then
				(
					GADNorm = (at time 0 DAOAnimVars.RootNode.transform) * (at time 0 inverse DAOAnimVars.GOBNode.transform)
					GADOffset = (at time DAOAnimSeq.FrameStart DAOAnimVars.RootNode.transform) * (at time 0 inverse DAOAnimVars.RootNode.transform)
					GADges = inverse GADNorm * GADOffset * GADNorm
					at time DAOAnimSeq.FrameStart GADges = GADges * (DAOAnimVars.GOBNode.transform)
					objOffsetPivot = translate (rotatezmatrix (quattoeuler GADges.rotationpart).z) [GADges.translationpart.x,GADges.translationpart.y,GADges.translationpart.z]
					at time DAOAnimSeq.FrameStart objOffsetPivot = DAOAnimVars.GOBNode.transform * inverse objOffsetPivot
				)
				
				--************************************
				--Getting Transforms at all Timeframes
				--************************************
				for t = DAOAnimSeq.FrameStart to DAOAnimSeq.FrameEnd do
				(
					ProgressDialog.BarSmall.value = ((t-DAOAnimSeq.FrameStart) as integer)*50/((DAOAnimSeq.FrameEnd-DAOAnimSeq.FrameStart) as integer)
					
					--********************************************
					--Calculate Base Transform for all Nodes
					--********************************************
					if DAOAnimSeq.GOBOffset == 1 and (isRoot) then
					(
						at time t objTransform = (Objs[i].transform * DAOAnimVars.GOBNode.transform) * inverse(Objs[i].parent.transform)
					)
					else if DAOAnimSeq.GOBOffset == 2 and (isRoot) then
					(
						local GOBAnim = (at time t DAOAnimVars.GOBNode.transform) * (at time DAOAnimSeq.FrameStart (inverse DAOAnimVars.GOBNode.transform))
						at time t objTransform = (Objs[i].transform * inverse(Objs[i].parent.transform)) * GOBAnim
					)
					else if DAOAnimSeq.GOBOffset == 3 and (isRoot) then
					(
						local GOBAnim = (at time t DAOAnimVars.GOBNode.transform) * (at time DAOAnimSeq.FrameStart (inverse DAOAnimVars.GOBNode.transform))
						at time t objTransform = (Objs[i].transform * objOffsetPivot) * inverse(Objs[i].parent.transform) * GOBAnim
					)
					else if Objs[i].parent != undefined and (GetObjectExportType Objs[i].parent) == #Weapontrail then
					(
						at time t objTransform = (Objs[i].transform) * inverse(Objs[i].parent.objecttransform)
					)
					else if Objs[i].parent != undefined then
					(
						at time t objTransform = (Objs[i].transform) * inverse(Objs[i].parent.transform)
					)
					else	--this case shouldnt matter
					(
						at time t objTransform = Objs[i].transform
					)
					
					objposition = objTransform.translationpart - objTransform0.translationpart	--relative
					objrotation = (inverse(objTransform)).rotationpart	--absolute
					
					--***********************************
					--Additive Animations, small but fine
					--***********************************
					if DAOAnimSeq.Additive == true then
					(
						objRotation = (objRotation * (objTransform0.rotationpart))	--relative
					)
					if isGOB then	--GOB does not export animated, except for GAD further down
					(
						objposition = [0,0,0]
						objrotation = (quat 0 0 0 1)
					)
					
					--************************************************
					--Full Export at the base, unless "Off"
					--***********************************************
					if TrackSetting.Pos != #Nil then expposframe = true else expposframe = false
					if TrackSetting.Rot != #Nil then exprotframe = true else exprotframe = false
					
					--*********************************************************
					--Autodetect Keyframedata ,opt out single frames
					--*********************************************************
					if TrackSetting.Pos == #Min or TrackSetting.Pos == #Auto or \
					   TrackSetting.Rot == #Min or TrackSetting.Rot == #Auto then
					(
						local LastFrame
						--****************************************
						--Position Detecting (sorting out zero)
						--****************************************
						if TrackSetting.Pos == #Min or TrackSetting.Pos == #Auto then
						(
							if posArray.count == 0 then
							(
								LastFrame = objposition / DAOTools.WorldScale / DAOAnimSeq.APRScale
							)
							else
							(
								LastFrame = (PosArray[PosArray.count][2] - objposition) / DAOTools.WorldScale / DAOAnimSeq.APRScale
							)
							if abs LastFrame.x <= 0.0001 and abs LastFrame.y <= 0.0001 and abs LastFrame.z <= 0.0001 then expposframe = false
						)
						--****************************************
						--Rotation Detection (sorting out zero)
						--****************************************
						if TrackSetting.Rot == #Min or TrackSetting.Rot == #Auto then
						(
							if rotArray.count == 0 then
							(
								if DAOAnimSeq.Additive == true then
								(
									LastFrame = (quat 0 0 0 1)
								)
								else
								(
									LastFrame = inverse objTransform0.rotationpart
								)
							)
							else 
							(
								LastFrame = RotArray[RotArray.count][2]
							)
							
							local RotCompare = (objrotation * inverse LastFrame)
							if abs RotCompare.x <= 0.0001 and \
							   abs RotCompare.y <= 0.0001 and \
							   abs RotCompare.z <= 0.0001 then exprotframe = false	--near 0 0 0 1 = no rotation
						)
						
						--********************************************
						--Abit of Handwork on autodetect..
						--********************************************
						
						--Override first frame to always export, on this Tracksetting
						if t == DAOAnimSeq.FrameStart then
						(
							if TrackSetting.Pos == #Min then expposframe = true
							if TrackSetting.Rot == #Min then exprotframe = true
						)
						--Adding leading keyframes inbetween
						if t != DAOAnimSeq.FrameStart then
						(
							local LastFrameTime = ((t-DAOAnimSeq.FrameStart-1) as float)/4800
							
							if PosArray.count == 0 and expposframe == true then
							(
								--At Frame 0
								frameTime = 0 as float
								append posArray #(frameTime, [0,0,0])
								
								--Leading Keyframe before Animation kicks in
								if LastFrameTime > 0 then append posArray #(LastFrameTime, [0,0,0])
							)
							if RotArray.count == 0 and exprotframe == true then
							(
								--At Frame 0
								frameTime = 0 as float
								append rotArray #(frameTime, LastFrame)
								--Leading Keyframe before Animation kicks in
								if LastFrameTime > 0 then append rotArray #(LastFrameTime, LastFrame)
							)
							
							--Also a keyframe after some have been skipped ,so the game wont make a curve, assuming that the pause was flat
							if PosArray.count > 0 and PosArray[PosArray.count][1] != LastFrameTime and expposframe == true then
							(
								append posArray #(LastFrameTime, PosArray[PosArray.count][2])
							)
							if RotArray.count > 0 and RotArray[RotArray.count][1] != LastFrameTime and exprotframe == true then
							(
								append rotArray #(LastFrameTime, RotArray[RotArray.count][2])
							)
						)
						
						--Plus at the end a Key too as a leading frame for the next animation
						if t == DAOAnimSeq.FrameEnd then
						(
							if PosArray.count > 0 then expposframe = true
							if RotArray.count > 0 then exprotframe = true
						)
					)
					
					--******************************************
					--GAD Block, gosh this is crazy stuff (this one recalculates all its own stuff again)
					--***********************************
					if (isGOB or isRoot) and DAOAnimSeq.GADExport == true then
					(
					
						expposframe = true	--autodetect override, GAD always gets exported
						exprotframe = true
						
						--Getting teh targeted GOB Position. Inversely projecting from Root
						GADNorm = (at time 0 DAOAnimVars.RootNode.transform) * (at time 0 inverse DAOAnimVars.GOBNode.transform)
						GADOffset = (at time t DAOAnimVars.RootNode.transform) * (at time 0 inverse DAOAnimVars.RootNode.transform)
						GADges = inverse GADNorm * GADOffset * GADNorm
						
						--Adding GOB Offset, when World Zero is the Pivot (per sequence setting)
						if DAOAnimSeq.GOBOffset == 1 then
						(
							at time t GADges = GADges * (DAOAnimVars.GOBNode.transform) --(at time 0 DAOAnimVars.GOBNode.transform)
						)
						--Adding Current Location Offset, when CurrentLocation is the Pivot (per sequence setting)
						if DAOAnimSeq.GOBOffset == 3 then
						(
							at time t GADges = GADges * (objOffsetPivot)
						)
						
						--Extract the new planar GOB transform, either in Local, GOB's or Worldspace
						if DAOAnimSeq.GADExportHeight == false then	--If not following height
						(
							GADtrans = translate (rotatezmatrix (quattoeuler GADges.rotationpart).z) [GADges.translationpart.x,GADges.translationpart.y,0]
						)
						else	--Following height enabled. means include Z
						(
							GADtrans = translate (rotatezmatrix (quattoeuler GADges.rotationpart).z) [GADges.translationpart.x,GADges.translationpart.y,GADges.translationpart.z]
						)
						
						--*********************************
						--Evaluate Entrance GAD
						--*********************************
						if t == DAOAnimSeq.FrameStart and isGOB then
						(
							--Making a Warning
							if DAOAnimSeq.GADExportEnt == false then
							(
								objrotation = inverse GADtrans.rotationpart
								objposition = GADtrans.translationpart  / DAOTools.WorldScale / DAOAnimSeq.APRScale
								
								--That is about 70° rotation or so? And 50cm movement. too much i say!
								if abs objrotation.x > 0.6 or \
								   abs objrotation.y > 0.6 or \
								   abs objrotation.z > 0.6 or \
								   abs objposition.x > 0.5 or \
								   abs objposition.y > 0.5 or \
								   abs objposition.z > 0.5 then
								(
									local WarningText = "Entrance GAD evaluation detected high difference between the Actors Position or Rotation\n"
									WarningText += "and the desired Pivot at starting frame.\n"
									WarningText += "Cases like these shall inherit entrance GAD, as otherwise GAD will be skewed by a high amount.\n\n"
									WarningText += "You can counter this, by moving the Pivot (GOB or World Center) closer to the Model\n"
									WarningText += "to represent a correct gamelocation, if you still want no Entrance GAD.\n\n"
									WarningText += "The Skew would only be applied to the first second. (blending in)\n\n"
									WarningText += "Click yes to enabled it (yesyes!)\nClick no to remain off (enforce)"
									
									if QueryBox (WarningText) title:"Warning" then DAOAnimSeq.GADExportEnt = true
								)
							)
						)
						
						--*********************************************************************************
						--Adding Compensation if Entrance GAD is disabled. With Blending in, for Duration at most
						--************************************************************************************
						if DAOAnimSeq.GADExportEnt == false then
						(
							local BlendTime
							local BlendAmount	--Percent Blend Amount.
							local BlendRot
							local BlendPos
							local BlendTrans
							local BlendDuration = 1s
							
							if DAOAnimSeq.FrameEnd - DAOAnimSeq.FrameStart > BlendDuration then
							(
								BlendTime = DAOAnimSeq.FrameStart + BlendDuration
							)
							else
							(
								BlendTime = DAOAnimSeq.FrameEnd - 1f
								if BlendTime <= DAOAnimSeq.FrameStart then BlendTime = DAOAnimSeq.FrameStart + 1f	--Fix for 2 frame Animations
							)
							BlendAmount = ((t-DAOAnimSeq.FrameStart) as integer)*100/((BlendTime-DAOAnimSeq.FrameStart) as integer)
							
							if t == DAOAnimSeq.FrameStart then
							(
								GADtrans = GADtrans * inverse GADtrans	--This is exact compensation
							)
							else if BlendAmount <= 100 then		--the rest is blending
							(
								BlendAmount = ( BlendAmount as float / 100 )
								
								BlendRot = slerp GADtrans.rotationpart (quat 0 0 0 1) (BlendAmount)
								BlendPos = (GADtrans.translationpart * (1 - BlendAmount))
								BlendTrans = translate ((quat 0 0 0 1) as matrix3) BlendPos
								
								GADtrans = inverse(BlendRot as matrix3) * (GADtrans * inverse BlendTrans)
							)
						)
						
						--Output GOB Transforms
						if isGOB then
						(
							objrotation = inverse GADtrans.rotationpart
							objposition = GADtrans.translationpart
						)
						--Root is a different matter
						if isRoot then
						(
							--Getting the Roots Transform in Parent space, either in Local, GOB or Worldspace
							if DAOAnimSeq.GOBOffset == 1 then	--worldspace
							(
								at time t objTransform = (Objs[i].transform * DAOAnimVars.GOBNode.transform) * inverse(Objs[i].parent.transform)
							)
							if DAOAnimSeq.GOBOffset == 2 then	--gobspace
							(
								at time t objTransform = (Objs[i].transform) * inverse(Objs[i].parent.transform)
							)
							if DAOAnimSeq.GOBOffset == 3 then	--localspace
							(
								at time t objTransform = (Objs[i].transform * objOffsetPivot) * inverse(Objs[i].parent.transform)
							)
							--Transform the GAD into the same space Root is in, which is GOD
							at time t GADGes = xformmat (GADTrans) (DAOAnimVars.RootNode.parent.transform * inverse DAOAnimVars.GOBNode.transform)
							
							--Compensate Root for the new GAD, in effect extract the GAD transform from above
							GADtransRoot = (objTransform * inverse GADGes)
							
							--Output
							objrotation = inverse GADtransRoot.rotationpart
							objposition = GADtransRoot.translationpart - objTransform0.translationpart
						)
					)
					
					if expposframe then
					(
						frameTime = ((t-DAOAnimSeq.FrameStart) as float)/4800
						append posArray #(frameTime, objposition)
					)
					if exprotframe then
					(
						frameTime = ((t-DAOAnimSeq.FrameStart) as float)/4800
						append rotArray #(frameTime, objrotation)
					)
				)	--timeframe loop end
				
				--***************************************
				--Outputting the Animation Data per Track
				--***************************************
				--if TrackSetting.Pos != #Off and posArray.count > 0 then
				if posArray.count > 0 then
				(
					format "<AnimationNode ExportName=\"%_%Translation\" SourceType=\"KeyFrame\" Source=\"Vector\" Target=\"Vector\" Elements=\"3\">\n<KeyFrame>\n" (cutstring Objs[i].name DAOAnimVars.CurrentExtension) (if isGOB then "Gob" else "") to:Stream
					format "<![CDATA[\n" to:Stream
					for a = 1 to posArray.count do
					(
						ProgressDialog.BarSmall.value = (a*50/posArray.count)+50
						objposition = posArray[a][2] / DAOTools.WorldScale / DAOAnimSeq.APRScale
						format "% % % %\n" posArray[a][1] (objposition.x) (objposition.y) (objposition.z) to:Stream
					)		
					format "]]>\n" to:Stream
					format "</KeyFrame>\n</AnimationNode>\n" to:Stream	
				)
				--if TrackSetting.Rot != #Off and rotArray.count > 0 then
				if rotArray.count > 0 then
				(
					format "<AnimationNode ExportName=\"%_%Rotation\" SourceType=\"KeyFrame\" Source=\"Quaternion\" Target=\"Quaternion\" Elements=\"4\">\n<KeyFrame>\n" (cutstring Objs[i].name DAOAnimVars.CurrentExtension) (if isGOB then "Gob" else "") to:Stream
					format "<![CDATA[\n" to:Stream
					for a = 1 to rotArray.count do
					(
						ProgressDialog.BarSmall.value = (a*50/rotArray.count)+50
						objrotation = rotArray[a][2]
						format "% % % % %\n" rotArray[a][1]  objrotation.x objrotation.y objrotation.z objrotation.w to:Stream
					)	
					format "]]>\n" to:Stream
					format "</KeyFrame>\n</AnimationNode>\n" to:Stream
				)
			)
			ExportAnimChilds Stream (Objs[i].Children) Progress
		)
	)
	
	fn ExportAnim GOB File SequenceSelected = (
		local objs
		local Stream = undefined
		local CMD = ""
		local SequenceCount
		local FilesToProcess = #()
		
		DAOAnimVars.GOBNode = undefined
		DAOAnimVars.RootNode = undefined
		
		DAOAnimVars.CurrentExtension = GOB.ObjExtension
		DAOAnimVars.GOBNode = getNodeByName (GOB.ObjName+GOB.ObjExtension) exact:true ignoreCase:true all:false
		objs = GetHierarchy #(DAOAnimVars.GOBNode)
		for object in objs do if (EqualsCaseLess (cutstring object.name DAOAnimVars.CurrentExtension) "Root") then DAOAnimVars.RootNode = object
		
		SequenceCount = case DAOAnim.UI_Sequence of
		(
			1: 1											--selected sequence
			2: DAOAnimVars.GOBNode.SequenceName.count		--all sequences
			3: 1											--current animation range
		)
		
		if DAOTools.IsGmax == true then
		(
			MessageBox "GMax users, start YAGG in Snoop mode now before continueing." title:"Ani Export"
			ClearListener()
		)
		
		for i = 1 to SequenceCount do
		(
			if DAOAnim.UI_Sequence == 1 then	--Exporting current sequence
			(
				DAOAnimSeq.SequenceIndex = SequenceSelected
				DAOAnimSeq.HasTrackSettings = true
				DAOAnimSeq.Name = DAOAnimVars.GOBNode.SequenceName[SequenceSelected]
				DAOAnimSeq.FrameStart = DAOAnimVars.GOBNode.SequenceStart[SequenceSelected] as time
				DAOAnimSeq.FrameEnd = DAOAnimVars.GOBNode.SequenceEnd[SequenceSelected] as time
				DAOAnimSeq.GOBOffset = DAOAnimVars.GOBNode.SequencePivot[SequenceSelected]
				DAOAnimSeq.HighQuality = DAOAnimVars.GOBNode.SequenceHQ[SequenceSelected]
				DAOAnimSeq.Additive = DAOAnimVars.GOBNode.SequenceAddv[SequenceSelected]
				DAOAnimSeq.Override = DAOAnimVars.GOBNode.SequenceOverride[SequenceSelected]
				DAOAnimSeq.GADExport = DAOAnimVars.GOBNode.SequenceGAD[SequenceSelected]
				DAOAnimSeq.GADExportEnt = DAOAnimVars.GOBNode.SequenceGADEnt[SequenceSelected]
				DAOAnimSeq.GADExportHeight = DAOAnimVars.GOBNode.SequenceGADFH[SequenceSelected]
				DAOAnimSeq.APRScale = DAOAnimVars.GOBNode.SequenceAPRScale[SequenceSelected]
			)
			if DAOAnim.UI_Sequence == 2 then	--Exporting all sequences
			(
				DAOAnimSeq.SequenceIndex = i
				DAOAnimSeq.HasTrackSettings = true
				DAOAnimSeq.Name = DAOAnimVars.GOBNode.SequenceName[i]
				DAOAnimSeq.FrameStart = DAOAnimVars.GOBNode.SequenceStart[i] as time
				DAOAnimSeq.FrameEnd = DAOAnimVars.GOBNode.SequenceEnd[i] as time
				DAOAnimSeq.GOBOffset = DAOAnimVars.GOBNode.SequencePivot[i]
				DAOAnimSeq.HighQuality = DAOAnimVars.GOBNode.SequenceHQ[i]
				DAOAnimSeq.Additive = DAOAnimVars.GOBNode.SequenceAddv[i]
				DAOAnimSeq.Override = DAOAnimVars.GOBNode.SequenceOverride[i]
				DAOAnimSeq.GADExport = DAOAnimVars.GOBNode.SequenceGAD[i]
				DAOAnimSeq.GADExportEnt = DAOAnimVars.GOBNode.SequenceGADEnt[i]
				DAOAnimSeq.GADExportHeight = DAOAnimVars.GOBNode.SequenceGADFH[i]
				DAOAnimSeq.APRScale = DAOAnimVars.GOBNode.SequenceAPRScale[i]
			)
			if DAOAnim.UI_Sequence == 3 then	--Exporting current range, with UI settings
			(
				DAOAnimSeq.SequenceIndex = 0			--wont set ,because cant get Track Data from this
				DAOAnimSeq.HasTrackSettings = false		--dito
				DAOAnimSeq.Name = File
				DAOAnimSeq.FrameStart = animationrange.start
				DAOAnimSeq.FrameEnd = animationrange.end
				DAOAnimSeq.GOBOffset = DAOAnim.UI_Pivot
				DAOAnimSeq.HighQuality = DAOAnim.UI_HQ
				DAOAnimSeq.Additive = DAOAnim.UI_Addv
				DAOAnimSeq.Override = DAOAnim.UI_Override
				DAOAnimSeq.GADExport = DAOAnim.UI_GAD
				DAOAnimSeq.GADExportEnt = DAOAnim.UI_GADEnt
				DAOAnimSeq.GADExportHeight = DAOAnim.UI_GADHeight
				DAOAnimSeq.APRScale = DAOAnim.UI_APRScale
			)
			
			--***********************************
			--**** Evaluate GAD Appliance *******
			--***********************************
			if DAOAnimSeq.GADExport == true then
			(
				if DAOAnimVars.RootNode == undefined then
				(
					MessageBox ("GAD Export requires \"GOB\" and \"Root\" to be present in the Skeleton.\nSequence:  " + DAOAnimSeq.Name as string + "\n\nAborting..") title:"Ani Export"
					return false
				)
				if DAOAnimVars.RootNode != undefined then
				(
					if DAOAnimVars.Rootnode.parent != undefined then
					(
						if (DAOAnimVars.Rootnode.parent.parent != undefined and (EqualsCaseLess (cutstring DAOAnimVars.Rootnode.parent.parent.name DAOAnimVars.CurrentExtension) "GOB") != true) or \
							   (DAOAnimVars.Rootnode.parent.parent == undefined) then
						(
							if not QueryBox ("GAD Export requires hierarchy GOB--> GOD--> Root, otherwise strange results may occur. (ive not tested this)\n\nContinue anyway?") title:"Ani Export" then return false
						)
					)
				)
			)
			if DAOAnimSeq.GOBOffset == 3 then
			(
				if DAOAnimVars.RootNode == undefined then
				(
					MessageBox ("Using Pivot Current Location requires \"GOB\" and \"Root\" to be present in the Skeleton.\nSequence:  "+DAOAnimSeq.Name as string+"\n\nAborting..") title:"Ani Export"
					return false
				)
			)
			
			--File opening
			--****************************************************
			Stream = DAOTools.FOPen (DAOTools.TempPath + DAOAnimSeq.Name + ".ani.xml")
			if Stream == undefined then
			(
				MessageBox ("Error creating output file.\n"+(DAOTools.TempPath + DAOAnimSeq.Name + ".ani.xml")) title:"Ani Export"
				return false
			)
			CreateDialog ProgressDialog
			ProgressDialog.Label.text = "Creating Output File"
			
			--Outputting File
			--*****************************************************
			format "<?xml version=\"1.0\" ?>\n" to:Stream
			format "<!-- Created with Eshme's DAO Animation Export Tool Version: % -->\n" DAOTools.Version to:Stream
			format "<!-- Visit my Page at http://social.bioware.com/project/2336/  -->\n\n" to:Stream
			format "<Animation Name=\"%\" Length=\"%\"" (DAOAnimSeq.Name + ".ANI") ((DAOAnimSeq.FrameEnd as float - DAOAnimSeq.FrameStart as float)/4800) to:Stream
			if DAOAnimSeq.HighQuality == true then format " QuaternionCompressedSize=\"64\"" to:Stream else format " QuaternionCompressedSize=\"32\"" to:Stream
			if DAOAnimSeq.Additive == true then format " Additive=\"True\"" to:Stream
			if DAOAnimSeq.Override == true then format " Override=\"True\"" to:Stream
			format ">\n" to:Stream
			format "<Meta><CombatRange Value=\"1.0\" /><GOBAnim Value=\"%\" /></Meta>\n" (if DAOAnimSeq.GADExport == true then 1 else 0) to:Stream
			
			--ProgressDialog.Label.text = "Writing Animation Data"
			
			local ProgressText = "Sequence " + i as string + " of " + SequenceCount as string + "  \"" + DAOAnimSeq.Name + "\"   "
			ExportAnimChilds Stream #(DAOAnimVars.GOBNode) #(1,Objs.count,ProgressText)
			
			format "</Animation>\n" to:Stream
			
			--Closing File
			--***************************************************
			if not(DAOTools.isgmax) then flush Stream
			DAOTools.Fclose Stream
			
			append FilesToProcess DAOAnimSeq.Name
		)
		
		--Processing ANI
		--***************************************************
		Stream = DAOTools.FOPen (DAOTools.TempPath + "Donotrun.bat")
		
		format ((Substring DAOTools.OutputPath 1 2)+"\n") to:Stream
		format ("cd \"%\"\n") DAOTools.OutputPath to:Stream
		
		for i = 1 to FilesToProcess.count do
		(
			format ("\""+DAOTools.GamePath + "tools\\ResourceBuild\\Processors\\ANI\\GraphicsProcessorANI.exe\" ") to:Stream
			format ((DAOTools.TempPath + FilesToProcess[i] + ".ani.xml")+" > " + DAOTools.TempPath + "ANIProcessorMSG.txt\n") to:Stream
		)
		--format ("pause") to:stream
		
		DAOTools.Fclose Stream
		CMD = ("\""+DAOTools.TempPath + "Donotrun.bat\"")
		DAOTools.CMD (CMD)
		
		DestroyDialog ProgressDialog	
		MessageBox ("Finished"+(if DAOTools.isgmax then "\n\nGmax Users:\nWait for Yagg to finish up (can take long for big animations)" else "")) title:"Ani Export"
	)
	
	rollout AnimExportDialog "Animation Export" width:232 height:496
	(
		GroupBox grp_gobs "Available Model Root GOBs:" pos:[5,1] width:220 height:45
		dropdownList ddl_gobs "" pos:[15,19] width:182 height:21 --items:#("GOB_Secondary01")
		
		GroupBox grp17 "Game Pivot Control" pos:[5,48] width:220 height:69
		radiobuttons rdo_gobs "" pos:[17,64] width:169 height:48 labels:#("World Center is Game Pivot", "GOB is Game Pivot", "Continue from Models Location")
		
		GroupBox grp_seqs "Sequences" pos:[5,119] width:220 height:122
		dropdownList ddl_seq "" pos:[15,138] width:174 height:21
		radiobuttons rdo_seq "" pos:[17,164] width:173 height:48 labels:#("Export single Sequence", "Export all Sequences", "Export current Animation Range") default:3 columns:1
		edittext edt_filename "" pos:[48,217] width:139 height:17
		label lbl_filename "Name:" pos:[18,219] width:33 height:15
		label lbl_ext ".ANI" pos:[190,219] width:24 height:15
		label lbl_seqext ".ANI" pos:[194,143] width:25 height:15
		
		GroupBox grp5 "APR Scale Correction (use with care)" pos:[5,242] width:220 height:47
		spinner spn_aprX "X:" pos:[25,261] width:51 height:16 range:[0,5,1] type:#float scale:0.01
		spinner spn_aprY "Y:" pos:[91,261] width:52 height:16 range:[0,5,1] type:#float scale:0.01
		spinner spn_aprZ "Z:" pos:[160,261] width:48 height:16 range:[0,5,1] type:#float scale:0.01
		
		GroupBox grp_options "Export Options (per Sequence)" pos:[5,292] width:220 height:140
		checkbox chk_highquality "High  Quality" pos:[17,312] width:117 height:16
		checkbox chk_additive "Additive Animation" pos:[17,332] width:120 height:18
		checkbox chk_override "Override Animation" pos:[17,352] width:133 height:17
		checkbox chk_gad "Export GAD Displacement" pos:[17,372] width:154 height:18
		checkbox chk_gaden "With Entrance GAD" pos:[17,392] width:146 height:18
		checkbox chk_gadheight "GAD follows height" pos:[17,412] width:123 height:18
		label lbl_WS "WorldScale (from Setup):" pos:[16,434] width:131 height:13
		label lbl_Scale "1" pos:[163,435] width:53 height:13
		
		button btn_export "E X P O R T" pos:[4,457] width:223 height:34 --enabled:false
		
		local GOBs = #()
		local SequenceCount
		
		fn UpdateSettings = (
			local CurrentGOB
			local SetEnabled = true
			
			if rdo_seq.state == 1 then SetEnabled = false
			if rdo_seq.state == 2 then SetEnabled = false
			
			if rdo_seq.state == 1 or rdo_seq.state == 2 then
			(
				if GOBs.count > 0 and ddl_gobs.selection > 0 and SequenceCount > 0 then
				(
					CurrentGOB = (getNodeByName (GOBs[ddl_gobs.selection].ObjName+GOBs[ddl_gobs.selection].ObjExtension) exact:true ignoreCase:true all:false)
					if (classof CurrentGOB) == DAO_Gameobject then
					(
						chk_highquality.checked = CurrentGOB.SequenceHQ[ddl_seq.selection]
						chk_additive.checked = CurrentGOB.SequenceAddv[ddl_seq.selection]
						chk_override.checked = CurrentGOB.SequenceOverride[ddl_seq.selection]
						chk_gad.checked = CurrentGOB.SequenceGAD[ddl_seq.selection]
						chk_gaden.checked = CurrentGOB.SequenceGADEnt[ddl_seq.selection]
						chk_gadheight.checked = CurrentGOB.SequenceGADFH[ddl_seq.selection]
						rdo_gobs.state = CurrentGOB.SequencePivot[ddl_seq.selection]
						
						spn_aprX.value = CurrentGOB.SequenceAPRScale[ddl_seq.selection].X
						spn_aprY.value = CurrentGOB.SequenceAPRScale[ddl_seq.selection].Y
						spn_aprZ.value = CurrentGOB.SequenceAPRScale[ddl_seq.selection].Z
					)
				)
			)
			if rdo_seq.state == 3 then
			(
				SetEnabled = true
				chk_highquality.checked = DAOAnim.UI_HQ
				chk_additive.checked = DAOAnim.UI_Addv
				chk_override.checked = DAOAnim.UI_Override
				chk_gad.checked = DAOAnim.UI_GAD
				chk_gaden.checked = DAOAnim.UI_GADEnt
				chk_gadheight.checked = DAOAnim.UI_GADHeight
				rdo_gobs.state = DAOAnim.UI_Pivot
				
				DAOAnim.UI_APRScale.x = spn_aprX.value = 1.0
				DAOAnim.UI_APRScale.y = spn_aprY.value = 1.0
				DAOAnim.UI_APRScale.z = spn_aprZ.value = 1.0
			)
			edt_filename.enabled = lbl_filename.enabled = SetEnabled
			chk_highquality.enabled = SetEnabled
			chk_additive.enabled = SetEnabled
			chk_override.enabled = SetEnabled
			chk_gad.enabled = SetEnabled
			chk_gaden.enabled = SetEnabled; if chk_gad.checked == false then chk_gaden.enabled = false
			chk_gadheight.enabled = SetEnabled; if chk_gad.checked == false then chk_gadheight.enabled = false
			rdo_gobs.enabled = SetEnabled
			spn_aprX.enabled = SetEnabled
			spn_aprY.enabled = SetEnabled
			spn_aprZ.enabled = SetEnabled
		)
		
		fn UpdateSequences = (
			local CurrentGOB
			local SeqsHold = #()
			SequenceCount = 0
			if GOBs.count > 0 and ddl_gobs.selection > 0 and (getNodeByName (GOBs[ddl_gobs.selection].ObjName+GOBs[ddl_gobs.selection].ObjExtension) exact:true ignoreCase:true all:false) != undefined then
			(
				CurrentGOB = (getNodeByName (GOBs[ddl_gobs.selection].ObjName+GOBs[ddl_gobs.selection].ObjExtension) exact:true ignoreCase:true all:false)
				if (classof CurrentGOB) == DAO_Gameobject do
				(
					--unify count. Due to updates on gameobject, added properties may need to be forced in line
					CurrentGOB.SequencePivot.count = CurrentGOB.SequenceStart.count = CurrentGOB.SequenceEnd.count = CurrentGOB.SequenceAPRScale.count = CurrentGOB.SequenceHQ.count = CurrentGOB.SequenceAddv.count = CurrentGOB.SequenceOverride.count = CurrentGOB.SequenceGAD.count = CurrentGOB.SequenceGADEnt.count = CurrentGOB.SequenceGADFH.count = CurrentGOB.SequenceName.count
					for i = 1 to CurrentGOB.SequenceName.count do
					(
						SequenceCount += 1
						append SeqsHold CurrentGOB.SequenceName[i]
					)
				)
				if SeqsHold.count == 0 then append SeqsHold "no Sequences found"
				ddl_seq.items = SeqsHold
			)
			if SequenceCount > 0 then (rdo_seq.state = DAOAnim.UI_Sequence = 1) else (rdo_seq.state = DAOAnim.UI_Sequence = 3)
		)
		
		--********************************************************
		--********* Dialog Initiating ****************************
		--********************************************************
		fn UpdateDialog = (
		
			--Scale display
			lbl_Scale.text = (DAOTools.WorldScale as string)
			
			--GAD buttons preset
			if chk_gad.checked then chk_gaden.enabled = chk_gadheight.enabled = true else chk_gaden.enabled = chk_gadheight.enabled = false
			
			--GOB DDL fill
			GOBs = GetGobs()
			local GOBHold = #()
			for GOB in GOBs where GOB.IsGOB == true do append GOBHold (GOB.ObjName+GOB.ObjExtension)
			ddl_gobs.items = GOBHold
			
			UpdateSequences()
			UpdateSettings()
		)
		
		on AnimExportDialog open do
		(
			UpdateDialog()
			
			if not(DAOTools.GamePathValid == true) or not(DAOTools.OutputPathValid == true) then
			(
				MessageBox "Output Paths are invalid or not set. Please run DAO Setup Tool" title:"Error"
				try (DestroyDialog DAOAnimToolsRollout)catch()
			)
		)	
		
		--********************************************************
		--********* Settings  ************************************
		--********************************************************
		
		on ddl_gobs selected item do (UpdateSequences(); UpdateSettings())
		on ddl_seq selected item do (UpdateSettings())
		
		on edt_filename entered text do edt_filename.text = FixOutputName text #(".Ani",".Ani.xml",".xml")
		on chk_highquality changed state do DAOAnim.UI_HQ = state
		on chk_additive changed state do
		(
			DAOAnim.UI_Addv = state
			if state == true then DAOAnim.UI_Override = DAOAnim.UI_GAD = DAOAnim.UI_GADEnt = DAOAnim.UI_GADHeight = false
			UpdateSettings()
		)
		on chk_override changed state do
		(
			DAOAnim.UI_Override = state
			if state == true then DAOAnim.UI_Addv = DAOAnim.UI_GAD = DAOAnim.UI_GADEnt = DAOAnim.UI_GADHeight = false
			UpdateSettings()
		)
		on chk_gad changed state do
		(
			DAOAnim.UI_GAD = state
			if state == true then DAOAnim.UI_Addv = DAOAnim.UI_Override = false
			if state == false then DAOAnim.UI_GADEnt = DAOAnim.UI_GADHeight = false
			UpdateSettings()
		)
		on chk_gaden changed state do DAOAnim.UI_GADEnt = state
		on chk_gadheight changed state do DAOAnim.UI_GADHeight = state		
		on rdo_gobs changed value do DAOAnim.UI_Pivot = Value
		on rdo_seq changed value do (DAOAnim.UI_Sequence = Value; UpdateSettings())
		
		on spn_aprX changed value do DAOAnim.UI_APRScale.x = value
		on spn_aprY changed value do DAOAnim.UI_APRScale.y = value
		on spn_aprZ changed value do DAOAnim.UI_APRScale.z = value
		
		--********************************************************
		--********* Export Button Pressed ************************
		--********************************************************
		on btn_export pressed do
		(
			local isReady = false
			if GOBs.count > 0 and ddl_gobs.selection > 0 and (getNodeByName (GOBs[ddl_gobs.selection].ObjName+GOBs[ddl_gobs.selection].ObjExtension) exact:true ignoreCase:true all:false) != undefined then
			(
				if DAOAnim.UI_Sequence == 1 then
				(
					if SequenceCount > 0 and ddl_seq.selection > 0 then isReady = true else (MessageBox "No sequence found" title:"Ani Export")
				)
				if DAOAnim.UI_Sequence == 2 then
				(
					if SequenceCount > 0 then isReady = true else (MessageBox "No sequence found" title:"Ani Export")
				)
				if DAOAnim.UI_Sequence == 3 then
				(
					if edt_filename.text != "" then isReady = true else (Messagebox "Please enter a Filename first" title:"Ani Export")
				)
				if isReady then
				(
					ExportAnim GOBs[ddl_gobs.selection] (edt_filename.text) (ddl_seq.selection)
					try(DestroyDialog ProgressDialog)catch()
				)
			)
			else
			(
				MessageBox "No Game Object selected" title:"Ani Export"
				UpdateDialog()
			)
		)
	)
	
	rollout DAOAnimToolsRollout "Dragon Age Animation Exporter" width:244 height:527
	(
		GroupBox grp1 "" pos:[22,-1] width:198 height:45
		label lbl1 "Animation Exporter" pos:[75,9] width:127 height:16
		label lbl2 "Dragon Age: Origins" pos:[71,26] width:95 height:16
		edittext txt_maxversion "" pos:[0,507] width:68 height:16 enabled:false
		subRollout rollouts "" pos:[0,49] width:244 height:454
		label lbl10 "Script by Eshme" pos:[120,507] width:85 height:16 enabled:false
		label lbl11 "" pos:[210,507] width:40 height:16 enabled:false
		
		on DAOAnimToolsRollout open do
		(

			lbl11.text = DAOTools.Version
			local maxver = maxVersion()		
			txt_maxversion.text = "MAX  v"+((maxver[1] / 1000) as string)
			if maxver[1] <= 4200 then txt_maxversion.text = " GMAX"
			Rollouts.height = DAOAnim.UI_Height - 70
			lbl10.pos = [lbl10.pos.x,DAOAnim.UI_Height - 17]
			lbl11.pos = [lbl11.pos.x,DAOAnim.UI_Height - 17]
			txt_maxversion.pos = [txt_maxversion.pos.x,DAOAnim.UI_Height - 20]
		)
		on DAOAnimToolsRollout close do 
		(
			DAOTools.WriteIni "Anim" "UI_Pos_X" DAOAnim.UI_Pos.x
			DAOTools.WriteIni "Anim" "UI_Pos_Y" DAOAnim.UI_Pos.y
			DAOTools.WriteIni "Anim" "UI_Height" DAOAnim.UI_Height
		)
		on DAOAnimToolsRollout resized Size do
		(
			if DAOAnimToolsRollout.open then
			(
				DAOAnim.UI_Height = Size.y
				Rollouts.height = DAOAnim.UI_Height - 70
				lbl10.pos = [lbl10.pos.x,DAOAnim.UI_Height - 17]
				lbl11.pos = [lbl11.pos.x,DAOAnim.UI_Height - 17]
				txt_maxversion.pos = [txt_maxversion.pos.x,DAOAnim.UI_Height - 20]
			)
		)
		on DAOAnimToolsRollout moved pos do DAOAnim.UI_Pos = pos
	)
	
	on execute do with undo off
	(
		if DAOAnim == undefined do Initialize()
		--Initialize()
		--try (DestroyDialog DAOAnimToolsRollout)catch()
		
		CreateDialog DAOAnimToolsRollout style:#(#style_sysmenu,#style_titlebar,#style_minimizebox,#style_resizing) height:DAOAnim.UI_Height lockWidth:true pos:DAOAnim.UI_Pos
		AddSubRollout DAOAnimToolsRollout.Rollouts AnimExportDialog rolledup:false

	)
)