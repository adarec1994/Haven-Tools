/**********************************************************************************************************
Dragon Age Origins Tools
Author: Eshme  http://social.bioware.com/project/2336/

latest edit:
3 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Include Script
DAOImportHelper.ms
Import functions
************************************************************************************************************
*/

	
	fn SearchFile Paths Alternative Filename Ending = (
		--*******************************************************************************
		--Looking for file, or prompting user. Paths be normally "Resource Paths" from Setup
		--Paths(Path Array) Alternative(guessed single path) Filename(***.***) Ending(***)
		--returns proper resource filename, or undefined if none
		--*******************************************************************************
		local outfile = undefined
		
		if (DAOTools.ExistFile (Alternative+Filename)) then outfile = (Alternative+Filename)
		for i = 1 to Paths.count where outfile == undefined do if (DAOTools.ExistFile (Paths[i]+Filename)) then outfile = Paths[i]+Filename
		
		if outfile == undefined then
		(
			if QueryBox ("Oops.\n\nNo "+Ending+" file was found in:\n\""+(Alternative+Filename) as string+"\"\n\nChoose one manually?") title:(Ending+" Import") then
			(
				outfile = getOpenFileName caption:("Select a "+Ending+" File") \
					types:(Ending+" files (*."+Ending+")|*."+Ending+"|All Files (*.*)|*.*|") Filename:(Alternative+Filename)
				--if (outfile != undefined and not (DAOTools.ExistFile outfile)) then outfile = undefined
			)
		)
		outfile
	)
	
	fn CreateTemplate Create = (	-- Creates a temporary Template, which is later used to align Bones to. Option true=create false=delete
		for i = 1 to DAOImportVars.BoneArray.count do
		(
			local Bone = getNodeByName (DAOImportVars.BoneArray[i].Name+DAOImportVars.Extension+DAOImportVars.TemplateExtension) exact:true ignoreCase:false all:false
			if Bone == undefined and Create == true then
			(
				local pivot = DAOImportVars.BoneArray[i].Pos*DAOTools.WorldScale 
				Mydummy = Dummy position:pivot
				Mydummy.name = DAOImportVars.BoneArray[i].Name+DAOImportVars.Extension+DAOImportVars.TemplateExtension
				if DAOImportVars.BoneArray[i].Parent != undefined then Mydummy.parent = (getNodeByName (DAOImportVars.BoneArray[i].Parent+DAOImportVars.Extension+DAOImportVars.TemplateExtension) exact:true ignoreCase:false all:false)
				in coordsys parent Mydummy.rotation = DAOImportVars.BoneArray[i].Rot
				in coordsys parent Mydummy.pos = pivot
			)
			if Bone != undefined and Create == false then
			(
				delete Bone
			)
		)
	)
	
	fn CreateNode Index = (
		
		local pivot
		local MyDummy
		local Bone = getNodeByName (DAOImportVars.BoneArray[Index].Name+DAOImportVars.Extension) exact:true ignoreCase:false all:false
		local TemplateNode = (getNodeByName (DAOImportVars.BoneArray[Index].Name+DAOImportVars.Extension+DAOImportVars.TemplateExtension) exact:true ignoreCase:false all:false)
		
		if Bone == undefined then 
		(		
			pivot = DAOImportVars.BoneArray[Index].Pos * DAOTools.WorldScale
			
			if DAOImportVars.BoneArray[Index].Parent == undefined and DAO_GameObject != undefined then	--GOB
			(
				Mydummy = DAO_GameObject position:pivot
				Mydummy.name = DAOImportVars.BoneArray[Index].Name + DAOImportVars.Extension
				Mydummy.size = 0.06 * DAOTools.WorldScale
				Mydummy.transform = TemplateNode.transform
				
				if (finditem DAOImportVars.MeshNodeNames "Mesh1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 1)
				else if (finditem DAOImportVars.MeshNodeNames "ArmorM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 2)
				else if (finditem DAOImportVars.MeshNodeNames "BootsM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 3)
				else if (finditem DAOImportVars.MeshNodeNames "GlovesM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 4)
				else if (finditem DAOImportVars.MeshNodeNames "HelmetM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 5)
				else if (finditem DAOImportVars.MeshNodeNames "ClothesM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 6)
				else if (finditem DAOImportVars.MeshNodeNames "RobeM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 7)
				else if (finditem DAOImportVars.MeshNodeNames "NeckM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 8)
				else if (finditem DAOImportVars.MeshNodeNames "FaceM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 9)
				else if (finditem DAOImportVars.MeshNodeNames "EyesM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 10)
				else if (finditem DAOImportVars.MeshNodeNames "HairM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 11)
				else if (finditem DAOImportVars.MeshNodeNames "LashesM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 12)
				else if (finditem DAOImportVars.MeshNodeNames "BeardM1") > 0 then (Mydummy.modeltype = 1; mydummy.meshtype = 13)
				else
				(
					Mydummy.modeltype = 2
					Mydummy.meshtype = 1
				)
				Mydummy.FXActorName = DAOImportVars.FXActorName
			)
			else if DAOImportVars.BoneArray[Index].BoneIndex == undefined and DAOImportVars.BoneArray[Index].HookID == undefined then --Helper Node
			(
				Mydummy = Dummy position:pivot
				Mydummy.name = DAOImportVars.BoneArray[Index].Name + DAOImportVars.Extension
				if DAOImportVars.BoneArray[Index].Name == "Root" then	--make root bigger
				(
					Mydummy.boxsize = [0.06 * DAOTools.WorldScale , 0.06 * DAOTools.WorldScale , 0.06 * DAOTools.WorldScale]
				)
				else
				(
					Mydummy.boxsize = [0.01 * DAOTools.WorldScale , 0.01 * DAOTools.WorldScale , 0.01 * DAOTools.WorldScale]
				)
				Mydummy.transform = TemplateNode.transform
				if DAOImportVars.BoneArray[Index].Parent != undefined then Mydummy.parent = (getNodeByName (DAOImportVars.BoneArray[Index].Parent+DAOImportVars.Extension) exact:true ignoreCase:false all:false)
			)
			else if DAOImportVars.BoneArray[Index].BoneIndex != undefined then --Bone
			(
				pivot = TemplateNode.Transform.translationpart
				--Get Lenght roughly
				local Childs = TemplateNode.children
				local End
				if Childs.count > 0 then End = Childs[1].Transform.translationpart else End = pivot + [0,(0.05 * DAOTools.WorldScale),0]
				local lenght = distance pivot end
				End = pivot + [lenght,0,0]
				if DAOImportVars.BoneArray[Index].Lenght != undefined then
				(
					end = pivot + [(DAOImportVars.BoneArray[Index].Lenght * DAOTools.WorldScale),0,0] --Lenght Override when set >0
				)
				--Create Bone
				local DABone = (BoneSys.createBone pivot end [0,0,1])
				DABone.width = DABone.height = length(end-pivot)/6
				DABone.name = DAOImportVars.BoneArray[Index].Name + DAOImportVars.Extension
			
				DABone.setBoneEnable false 0 --disable for making adjustments
 				if DAOImportVars.BoneArray[Index].Parent != undefined then DABone.parent = (getNodeByName (DAOImportVars.BoneArray[Index].Parent+DAOImportVars.Extension) exact:true ignoreCase:false all:false)
			
				--Adjust Bone
				in coordsys parent DABone.rotation = DAOImportVars.BoneArray[Index].Rot
				in coordsys parent DABone.pos = DAOImportVars.BoneArray[Index].Pos * DAOTools.WorldScale
			)
			else if DAOImportVars.BoneArray[Index].HookID != undefined and DAO_Crust != undefined then --Crust	Legacy
			(
				Mydummy = DAO_Crust position:pivot
				Mydummy.name = DAOImportVars.BoneArray[Index].Name + DAOImportVars.Extension
				Mydummy.size = 0.04 * DAOTools.WorldScale
				Mydummy.transform = TemplateNode.transform
				Mydummy.HookID = DAOImportVars.BoneArray[Index].HookID
				if DAOImportVars.BoneArray[Index].Parent != undefined then Mydummy.parent = (getNodeByName (DAOImportVars.BoneArray[Index].Parent+DAOImportVars.Extension) exact:true ignoreCase:false all:false)
			)
			false  --Returns Error (no error)
		)
		else	--When it exists, move to position.
		(
			animate on
			(
				pivot = DAOImportVars.BoneArray[Index].Pos * DAOTools.WorldScale
				if DAOImportVars.BoneArray[Index].BoneIndex == undefined and DAOImportVars.BoneArray[Index].HookID == undefined then --Helper
				(
					at time 0 Bone.transform = TemplateNode.transform
				)
				if DAOImportVars.BoneArray[Index].BoneIndex != undefined then --Bone
				(
					animate off (Bone.setBoneEnable false 0)
					pivot = TemplateNode.Transform.translationpart
					at time 0 in coordsys parent Bone.rotation = DAOImportVars.BoneArray[Index].Rot
					at time 0 in coordsys parent Bone.pos = DAOImportVars.BoneArray[Index].Pos * DAOTools.WorldScale				
				)
				if DAOImportVars.BoneArray[Index].HookID != undefined then --Crust	Legacy
				(
					at time 0 Bone.transform = TemplateNode.transform
				)
			)
			true	--Returns Error (Error)
		)
	)

	fn ActivateBones = (	--Activates bone property (Bone On) for only Bones in the Array

		for i = 1 to DAOImportVars.BoneArray.count do
		(
			if DAOImportVars.BoneArray[i].BoneIndex != undefined then
			(
				local Bone = getNodeByName (DAOImportVars.BoneArray[i].Name+DAOImportVars.Extension) exact:true ignoreCase:false all:false
				Bone.setBoneEnable true 0
			)
		)
	)

	fn AddBoneAttributes = (	--Adds the custom attributes to all Nodes ,which correlate to those in the DA Exporter
		
		--Adding Attributes to the Bone
		--*****************************************
		for i = 1 to DAOImportVars.BoneArray.count do
		(
			if (Bone = (getNodeByName (DAOImportVars.BoneArray[i].Name+DAOImportVars.Extension) exact:true ignoreCase:false all:false)) != undefined and \
			   (classof Bone) != DAO_GameObject and (classof Bone) != DAO_Crust then
			(
				If (GetBoneParams Bone) > 0 then custAttributes.delete Bone (GetBoneParams Bone)
	
				custAttributes.add Bone customBoneParams
				
				Bone.custBoneParams.genuine = true	--accuracy
				if DAOImportVars.BoneArray[i].BoneIndex != undefined then
				(
					Bone.custBoneParams.BoneParamIndex = DAOImportVars.BoneArray[i].BoneIndex
				)
	  		)	
		)
	)

	fn CreateSystem = ( --Initial create skeleton function, distributes jobs one at a time
		local warning = false
		local error = false
		
		CreateTemplate true
		for i = 1 to DAOImportVars.BoneArray.count do
		(
			error = CreateNode i
			if error == true then warning = true
		)
		CreateTemplate false
		ActivateBones ()
		AddBoneAttributes ()
		DAOImportVars.BoneArray = #()
		Warning
	)
	
	fn MakeBoneArray Name Parent Pos Rot Index = (
		local BoneArraySingle
		
		BoneArraySingle = DAONodeStruct Name:Name Parent:Parent Pos:Pos Rot:Rot BoneParamPos:false BoneParamRot:false BoneParamAdd:false	--default
		
		if Index >= 0 then BoneArraySingle.BoneIndex = Index			--Leave undefined when no index!
		
		if Name == "GOB" or Name == "GOD" or Name == "Root" then		--these get pos + rot animation enabled
		(																--..just a basic setting not more.
			BoneArraySingle.BoneParamPos = true
			BoneArraySingle.BoneParamRot = true
		)
		
		if matchpattern Name pattern:"*Pelvis*" \
		or matchpattern Name pattern:"*Lowerback*" \
		or matchpattern Name pattern:"*Chest*" \
		or matchpattern Name pattern:"*Collar*" \
		or matchpattern Name pattern:"*Shoulder*" \
		or matchpattern Name pattern:"*Elbow*" \
		or matchpattern Name pattern:"*Hip*" \
		or matchpattern Name pattern:"*Knee*" \
		or matchpattern Name pattern:"*Ankle*" \
		or matchpattern Name pattern:"*Toe*" \
		or matchpattern Name pattern:"*Head*" \
		or matchpattern Name pattern:"*Neck*" then BoneArraySingle.BoneParamRot = true	--these get rot animation enabled, just a basic setting not more.
		
		append DAOImportVars.BoneArray BoneArraySingle
	)