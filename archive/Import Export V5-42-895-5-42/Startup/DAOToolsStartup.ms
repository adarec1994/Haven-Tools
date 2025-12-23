/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edited:
15 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Startup Script
*********************************************
*/


struct DAOToolsStruct	--These must be available to all Scripts. Global startup
(
	--Variables
	TempPath,
	INIFile,
	ScriptsPath,
	GamePath,
	GamePathValid,
	OutputPath,
	OutputPathValid,
	ImportPaths = #(),
	MaxImportPaths = 30,
	WorldScale,
	IsGmax,
	Version = "5.42",
	
	--Functions
	ExistFile,ExistDir,mkdir,ReadIni,WriteIni,CMD,Fopen,Fclose,
	Yagg_ShellExec
)
global DAOTools = DAOToolsStruct()
global DAOWelcomeDialog
(
	fn YaggString Input = (		--Fixes strings to work in yagg.
		local Output = "\""
		for i = 1 to Input.count do
		(
			if Input[i] == "\"" or Input[i] == "\\" then Output += "\\"
			Output += Input[i]
		)
		if Input.count > 0 then Output += "\""
		Output
	)

	fn Yagg_Fopen File = (
		format ("\x1b[fopen file="+(YaggString File)+"\n") -- newline=\"~\"\n")
	)
	
	fn Yagg_Fclose = (
		format "\x1b[fclose\n"
	)
	
	fn Yagg_cmd CMD = (

		format ("\x1b[cmd cmd="+(YaggString CMD)+"\n")
	)
	
	fn Yagg_ShellExec = (
	)
	
	fn Yagg_setINISetting File Section Key Value = (
		format ("\x1b[setINISetting file="+(YaggString File)+" sec="+(YaggString Section)+" key="+(YaggString Key)+" val="+(YaggString (Value as string))+"\n")
	)

	DAOTools.ExistFile = fn ExistFile File = (	--accomodates for gmax still finding a file with "doesfileexist" when not even the path exists
		local out = false
		try
		(
			getFileAttribute File #readOnly
			out = true
		)
		catch ()
		out
	)
	
	DAOTools.ExistDir = fn ExistDir Dir = (
		if Dir[Dir.count] == "\\" then Dir = Replace Dir Dir.count 1 ""
		((getDirectories Dir).count != 0)
	)
	
	DAOTools.mkdir = fn mkdir dir = (
		if DAOTools.isgmax == true then
		(
			local CMD = ("mkdir "+dir)
			DAOTools.CMD CMD
		)
		else
		(
			try(makeDir dir)catch()

		)
	)
	
	DAOTools.ReadIni = fn ReadIni Section Key = (
		local out
		out = GetINISetting DAOTools.INIFile Section Key
		if out == "" then undefined else out
	)
	
	DAOTools.WriteIni = fn WriteIni Section Key Value = (
		if DAOTools.IsGmax then
		(
			Yagg_setINISetting DAOTools.INIFile Section Key (Value as string)
		)
		else
		(
			setINISetting DAOTools.INIFile Section Key (Value as string)
		)
	)
	
	DAOTools.Fopen = fn DAOToolsFopen File = (
		local Stream
		if DAOTools.isGmax == true then
		(
			Stream = Listener
			Yagg_Fopen (File)		
		)
		else
		(
			if (DAOTools.ExistFile File) then Stream = openFile (File) mode:"wt"
			if not(DAOTools.ExistFile File) then Stream = CreateFile (File)
		)
		Stream
	)
	
	DAOTools.Fclose = fn DAOToolsFclose Stream = (
		if DAOTools.IsGmax == true then
		(
			Yagg_Fclose ()
		)
		else
		(
			close Stream
		)
	)
	
	DAOTools.CMD = fn CMD CMDo = (
		if DAOTools.isGmax then
		(
			sleep 3
			Yagg_cmd (CMDo)
		)
		else
		(
			sleep 1
			Doscommand (CMDo)
		)
	)
	
	fn InitializeVars = (
		
		DAOTools.IsGmax = ((maxversion())[1] <= 4200)
		DAOTools.TempPath = (sysinfo.tempdir + "DAOTools\\")
		if not (ExistDir DAOTools.TempPath) then DAOTools.mkDir DAOTools.TempPath
		DAOTools.INIFile = (DAOTools.TempPath + "DAOTools.INI")
		
		DAOTools.GamePath = ReadIni "Paths" "GamePath"
		if DAOTools.GamePath == undefined then DAOTools.GamePath = ""
		
		DAOTools.OutputPath = ReadIni "Paths" "OutputPath"
		if DAOTools.OutputPath == undefined then DAOTools.OutputPath = ""
		
		DAOTools.GamePathValid = ReadIni "Paths" "GamePathValid"
		if DAOTools.GamePathValid == "true" then DAOTools.GamePathValid = true
		
		DAOTools.OutputPathValid = ReadIni "Paths" "OutputPathValid"
		if DAOTools.OutputPathValid == "true" then DAOTools.OutputpathValid = true
		
		try(DAOTools.Worldscale = (ReadIni "Global" "WorldScale") as integer)catch()
		if DAOTools.WorldScale == undefined then DAOTools.WorldScale = 1
		
		for i = 1 to DAOTools.MaxImportPaths do try
		(
			local ImpPath = (ReadIni "Paths" ("Import"+i as string))
			if ImpPath != undefined and ImpPath != "undefined" and ImpPath != "" then
			(
				append DAOTools.ImportPaths (ReadIni "Paths" ("Import"+i as string))
			)
		)
		catch()
		
		DAOTools.ScriptsPath = ReadIni "Paths" "ScriptsPath"
		if DAOTools.ScriptsPath == undefined then
		(
			try				--Searching for Scripts
			(
				if (DAOTools.ExistDir "$Scripts\\DAOTools") and (DAOTools.ExistFile "$Scripts\\DAOTools\\DAOToolsSetup.ms") then DAOTools.ScriptsPath = (getdir #scripts) + "\\DAOTools\\"
			)catch()
			try
			(
				if (DAOTools.ExistDir "$userScripts\\DAOTools") and (DAOTools.ExistFile "$userScripts\\DAOTools\\DAOToolsSetup.ms") then DAOTools.ScriptsPath = (getdir #userscripts) + "\\DAOTools\\"
			)catch()
		)
	)
	
	fn Welcome = (
	
		if not(ExistFile DAOTools.INIFile) or (DAOTools.ScriptsPath == undefined) then
		(
			rollout DAOWelcomeDialog "Welcome" width:281 height:155
			(
				GroupBox grp2 "" pos:[44,0] width:186 height:29
				label lbl1 "Welcome to DAO Animation Export" pos:[52,10] width:172 height:17
				label lbl2 "Remember to add the macroscripts to the Toolbar." pos:[13,31] width:240 height:16
				label lbl3 "(see readme for instructions)" pos:[13,47] width:153 height:17
				label lbl4 "DAOTools scripts have not been found." pos:[14,70] width:219 height:30
				edittext edt1 "" pos:[11,100] width:211 height:17 enabled:false
				--button btn2 "..." pos:[239,100] width:29 height:17 enabled:false
				--label lbl5 "OK!" pos:[241,83] width:27 height:16
				button btn_cont "continue.." pos:[96,124] width:89 height:20 enabled:false
				
				on DAOWelcomeDialog open do
				(
					if DAOTools.ScriptsPath == undefined then
					(
						btn_cont.enabled = false
					)
					else
					(
						lbl4.caption = "Found scripts location."
						edt1.text = DAOTools.ScriptsPath
						btn_cont.enabled = true
					)
				)
				on DAOWelcomeDialog close do
				(
					if DAOTools.ScriptsPath == undefined then
					(
						MessageBox "The DAOTools Scripts havent been found in neither Scripts\\DAOTools or UserScrips\\DAOTools.\n\nYou need to check the installation. See (readme) for instructions." title:"DAO Tools"
					)
					else
					(
						Messagebox "Running Setup for the first time..." title:"Setup"
						if DAOTools.ExistFile (DAOTools.ScriptsPath + "DAOToolsSetup.ms") then
						(
							FileIn (DAOTools.ScriptsPath + "DAOToolsSetup.ms")
						)
						else MessageBox ("Script Execution failed (not found). Please check installation\n\nScript: "+(DAOTools.ScriptsPath + "DAOToolsSetup.ms \n\n..is missing!")) title:"Error"
					)
				)		
				on btn_cont pressed do
				(
					DestroyDialog (DAOWelcomeDialog)
				)
			)
			CreateDialog (DAOWelcomeDialog)
		)
	)
	
	fn InstallMacros = (
		local Macro1 = ""
		Macro1 = "macroScript AnimationExport\n"
		Macro1 += "category:\"DAOTools\"\n"
		Macro1 += "internalCategory:\"DAOTools\"\n"
		Macro1 += "tooltip:\"Dragon Age: Origins Animation Exporter\"\n"
		Macro1 += "icon:#(\"DAOToolsAnimExport\",1)"
		Macro1 += "\n(\n    filein (DAOTools.ScriptsPath+ \"DAOAnimExport.ms\")\n)"
		execute (Macro1)

		Macro1 = "macroScript ModelExport\n"
		Macro1 += "category:\"DAOTools\"\n"
		Macro1 += "internalCategory:\"DAOTools\"\n"
		Macro1 += "tooltip:\"Dragon Age: Origins Model Exporter\"\n"
		Macro1 += "icon:#(\"DAOToolsModelExport\",1)"
		Macro1 += "\n(\n    filein (DAOTools.ScriptsPath+ \"DAOModelExport.ms\")\n)"
		execute (Macro1)
		
		Macro1 = "macroScript DAOToolsSetup\n"
		Macro1 += "category:\"DAOTools\"\n"
		Macro1 += "internalCategory:\"DAOTools\"\n"
		Macro1 += "tooltip:\"Dragon Age: Origins Tools Setup\"\n"
		Macro1 += "icon:#(\"DAOToolsSetup\",1)"
		Macro1 += "\n(\n    filein (DAOTools.ScriptsPath+ \"DAOToolsSetup.ms\")\n)"
		execute (Macro1)
		
		Macro1 = "macroScript ModelManager\n"
		Macro1 += "category:\"DAOTools\"\n"
		Macro1 += "internalCategory:\"DAOTools\"\n"
		Macro1 += "tooltip:\"Dragon Age: Origins Model Manager\"\n"
		Macro1 += "icon:#(\"DAOToolsModelManager\",1)"
		Macro1 += "\n(\n    filein (DAOTools.ScriptsPath+ \"DAOModelManager.ms\")\n)"
		execute (Macro1)
		
		Macro1 = "macroScript Importer\n"
		Macro1 += "category:\"DAOTools\"\n"
		Macro1 += "internalCategory:\"DAOTools\"\n"
		Macro1 += "tooltip:\"Dragon Age: Origins Importer\"\n"
		Macro1 += "icon:#(\"DAOToolsImport\",1)"
		Macro1 += "\n(\n    filein (DAOTools.ScriptsPath+ \"DAOImport.ms\")\n)"
		execute (Macro1)
	)
	
	fn CheckMacroInstall = (
		try
		(
			if (ExistFile "$userMacros\\DAOTools-AnimationExport.mcr") == true and \
			   (ExistFile "$userMacros\\DAOTools-ModelExport.mcr") == true and \
			   (ExistFile "$userMacros\\DAOTools-ModelManager.mcr") == true and \
			   (ExistFile "$userMacros\\DAOTools-Importer.mcr") == true and \
			   (ExistFile "$userMacros\\DAOTools-DAOToolsSetup.mcr") then return true
		)catch()
		try
		(
			if (ExistFile "$Macroscripts\\DAOTools-AnimationExport.mcr") == true and \
			   (ExistFile "$Macroscripts\\DAOTools-ModelExport.mcr") == true and \
			   (ExistFile "$Macroscripts\\DAOTools-ModelManager.mcr") == true and \
			   (ExistFile "$Macroscripts\\DAOTools-Importer.mcr") == true and \
			   (ExistFile "$Macroscripts\\DAOTools-DAOToolsSetup.mcr") then return true
		)catch()
		
		false
	)
	
	on execute do with undo off
	(
		local IsNewInstall = not (CheckMacroInstall ())
		InstallMacros()		--Evaluate Macroscript all the time
		if IsNewInstall then
		(
			MessageBox "Dragon Age Macroscripts have been registered.\nPlease restart 3DS." title:"Dragon Age Export"
		)
		
		if not IsNewInstall then
		(
			InitializeVars()
			if DAOTools.ScriptsPath != undefined then
			(
				FileIn (DAOTools.ScriptsPath + "plugs\DAO_Material_Plugin.ms")
				FileIn (DAOTools.ScriptsPath + "plugs\DAO_Helper_GameObject.ms")
				FileIn (DAOTools.ScriptsPath + "plugs\DAO_Helper_Crust.ms")
				FileIn (DAOTools.ScriptsPath + "plugs\DAO_Helper_UserPoint.ms")
				FileIn (DAOTools.ScriptsPath + "plugs\DAO_Helper_SnapPoint.ms")
				FileIn (DAOTools.ScriptsPath + "plugs\DAO_Mod_Cloth.ms")
				if ((maxversion())[1] <= 4200) then
				(
					FileIn (DAOTools.ScriptsPath + "plugs\DAO_Helper_Weapontrail_Gmax.ms")
				)
				else
				(
					FileIn (DAOTools.ScriptsPath + "plugs\DAO_Helper_Weapontrail.ms")
				)
			)
			Welcome()
		)
	)
)
