/**********************************************************************************************************
Dragon Age Origins Tools Setup
Author: Eshme  http://social.bioware.com/project/2336/

last edit:
13 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
*/
struct DAOSetupStruct
(
	UI_Pos,
	UI_Height,
	PresetPath = "C:\\"
)
try (DestroyDialog DAOSetupToolsRollout)catch()
global DAOSetup
global DAOSetupToolsRollout
(
	fn Initialize = (
		DAOSetup = DAOSetupStruct()
		try(DAOSetup.UI_Pos.x = (DAOTools.ReadIni "Setup" "UI_Pos_X") as integer)catch()
		try(DAOSetup.UI_Pos.y = (DAOTools.ReadIni "Setup" "UI_Pos_Y") as integer)catch()
		try(DAOSetup.UI_Height = (DAOTools.ReadIni "Setup" "UI_Height") as integer)catch()
		if DAOSetup.UI_Pos == undefined then
		(
			if ((maxversion())[1] >= 7000) then DAOSetup.UI_Pos = getMAXWindowPos() + (getMAXWindowSize()/4) else DAOSetup.UI_Pos = [100,100]
		)
		if DAOSetup.UI_Height == undefined then DAOSetup.UI_Height = 532
	)
	
	rollout SetupDialog "Setup" width:372 height:431
	(
		GroupBox grp10_WorldScale "WorldScale" pos:[6,2] width:133 height:98
		slider sld_scale "" pos:[15,16] width:40 height:80 range:[0,3,0] type:#integer orient:#vertical ticks:2
		label lbl4 "x1000" pos:[56,22] width:29 height:13
		label lbl5 "x100" pos:[56,38] width:23 height:13
		label lbl6 "x10" pos:[56,56] width:17 height:13
		label lbl7 "x1 (DA Default)" pos:[56,74] width:72 height:13	

		GroupBox grp1_gamepath "Game Path:" pos:[142,8] width:223 height:43
		edittext edt_gamepath "" pos:[147,29] width:161 height:17
		button btn_gamepath "..." pos:[323,29] width:33 height:17
		
		GroupBox grp3_exportpath "Export Path:" pos:[142,57] width:223 height:43
		edittext edt_outputpath "" pos:[147,78] width:161 height:17
		button btn_outputpath "..." pos:[323,78] width:33 height:17

		GroupBox grp7_ImportResources "Import Resource Paths (priority top->bottom)" pos:[6,102] width:360 height:285
		button btn_pickpath "..." pos:[16,329] width:36 height:20
		button btn_addpath "Add" pos:[26,359] width:65 height:20
		button btn_rempath "Remove" pos:[111,359] width:65 height:20
		button btn_moveup "Move Up" pos:[200,359] width:65 height:20
		button btn_movedown "Move Down" pos:[285,359] width:65 height:20
		listbox lbx_importpaths "" pos:[9,124] width:353 height:15
		edittext edt_importpath "" pos:[60,331] width:300 height:17
		
		button btn_ok "OK" pos:[10,397] width:91 height:29
		button btn_cancel "Cancel" pos:[267,397] width:91 height:29
		
		local GamePath = DAOTools.GamePath
		local OutputPath = DAOTools.OutputPath
		local GamePathValid = DAOTools.GamePathValid
		local OutputPathValid = DAOTools.OutputPathValid
		local ImportPaths = #()
		local Scale
		local ExitWarnOnce = false
		
		fn validategame path = (
			GamePath = ""
			if path != "" and path != undefined then
			(
				if DAOTools.ExistFile (path + "tools\\ResourceBuild\\Processors\\ANI\\GraphicsProcessorANI.exe") then GamePath = path \
				else if DAOTools.ExistFile (path + "Dragon Age\\tools\\ResourceBuild\\Processors\\ANI\\GraphicsProcessorANI.exe") then GamePath = path + "Dragon Age\\"
				
				if GamePath == "" then
				(
					MessageBox ("Cannot find correct Path.\nMake sure you select the correct path to Dragon Age Game,\nand that the Toolset is installed.\n\nCannot find:\"Dragon Age\\tools\\ResourceBuild\\Processors\\ANI\\GraphicsProcessorANI.exe\"") title:"Error"
					btn_gamepath.caption = "..."
					edt_gamepath.text = ""
					GamePathValid = false
				)
				if (GamePath != "") then
				(
					btn_gamepath.caption = "OK"
					edt_gamepath.text = GamePath
					GamePathValid = true
				)
			)
			else
			(
				btn_gamepath.caption = "..."
				GamePathValid = false
			)
		)
		
		fn validateoutput path = (
			OutputPath = ""
			
			if path != "" and path != undefined then
			(
				try
				(
					if Path[Path.count] != "\\" then Path += "\\"		--Expecting path to end in a backslash in all scripts
					if (DAOTools.ExistDir Path) then
					(
						local Dummy = CreateFile (path + "aherhetagab.txt")
						if Dummy != undefined then
						(
							close Dummy
							if (DAOTools.ExistFile (path + "aherhetagab.txt")) == true then
							(
								if (deleteFile (path + "aherhetagab.txt")) == true then OutputPath = Path
							)
						)
					)
				)catch ()
				if (DAOTools.isgmax == true) and (DAOTools.ExistDir path) then
				(
					MessageBox ("Gmax Users:\nMake sure the Exportpath is not writeprotected.\nI cannot check") title:"DAO Setup"
					Outputpath = path
				)
				if OutputPath == "" then
				(
					btn_outputpath.caption = "..."
					edt_outputpath.text = ""
					OutputPathValid = false
				)
				if OutputPath != "" then
				(
					btn_outputpath.caption = "OK"
					edt_outputpath.text = path
					OutputPathValid = true
				)
			)
			else
			(
				btn_outputpath.caption = "..."
				OutputPathValid = false
			)
		)
		on SetupDialog open do
		(
			sld_scale.value = case DAOTools.Worldscale of
			(
				1: 0
				10: 1
				100: 2
				1000: 3
				default: (DAOTools.Worldscale = 1; 0)	--reset if off limits
			)
			--Import Settings from global.. keeping local until OK is pressed
			Scale = DAOTools.WorldScale
			ValidateGame DAOTools.GamePath
			ValidateOutput DAOTools.OutputPath
			ImportPaths = DAOTools.ImportPaths
			
			lbx_importpaths.items = for item in DAOTools.ImportPaths collect item
			lbx_importpaths.items = lbx_importpaths.items
		)
		
		on edt_gamepath entered text do validategame text
		on edt_outputpath entered text do validateoutput text
		on btn_gamepath pressed do
		(
			local newpath = getSavePath ()
			if newpath != undefined then
			(
				if not(matchpattern newpath pattern:"*:\\") then newpath = newpath+"\\"
				validategame newpath
			)
		)
		on btn_outputpath pressed do
		(
			local newpath = getSavePath ()
			if newpath != undefined then
			(
				if not(matchpattern newpath pattern:"*:\\") then newpath = newpath+"\\"
				validateoutput newpath
			)
		)
		on sld_scale changed value do
		(
			Scale = case value of
			(
				0: 1
				1: 10
				2: 100
				3: 1000
			)
		)
		
		on btn_addpath pressed do
		(
			if Importpaths.count == DAOTools.MaxImportPaths then Messagebox ("Sorry were full. Max "+DAOTools.MaxImportPaths as string+". Want more, tell me.") title:"DAO Setup"
			if Importpaths.count < DAOTools.MaxImportPaths and edt_importpath.text != "" then
			(
				if (DAOTools.ExistDir edt_importpath.text) then append lbx_importpaths.items edt_importpath.text else \
				MessageBox ("Whoops, path doesnt exist. You can do better than this.") title:"DAO Setup"
			)
			Importpaths = lbx_importpaths.items = lbx_importpaths.items
		)
		on btn_pickpath pressed do
		(
			local newpath = getSavePath initialdir:DAOSetup.PresetPath
			format (newpath as string+"\n")
			if newpath != undefined then
			(
				if matchpattern newpath pattern:"*:\\" then edt_importpath.text = DAOSetup.PresetPath = newpath else edt_importpath.text = DAOSetup.PresetPath = newpath+"\\"
			)
		)
		on btn_rempath pressed do
		(
			if lbx_importpaths.selection != 0 then ImportPaths = deleteitem ImportPaths lbx_importpaths.selection
			lbx_importpaths.items = Importpaths
		)
		on lbx_importpaths selected item do edt_importpath.text = lbx_importpaths.items[item]
		
		on btn_moveup pressed do
		(
			if lbx_importpaths.selection != 0 then
			(
				local olditemindex = lbx_importpaths.selection
				local olditem = lbx_importpaths.selected
				if olditemindex > 1 then
				(
					ImportPaths = deleteitem ImportPaths olditemindex
					insertitem olditem ImportPaths (olditemindex - 1)
					lbx_importpaths.selection = (olditemindex - 1)
				)
			)
			lbx_importpaths.items = Importpaths
		)
		on btn_movedown pressed do
		(
			if lbx_importpaths.selection != 0 then
			(
				local olditemindex = lbx_importpaths.selection
				local olditem = lbx_importpaths.selected
				if olditemindex < ImportPaths.count then
				(
					ImportPaths = deleteitem ImportPaths olditemindex
					insertitem olditem ImportPaths (olditemindex + 1)
					lbx_importpaths.selection = (olditemindex + 1)
				)
			)
			lbx_importpaths.items = Importpaths
		)
		
		on btn_ok pressed do
		(
			DAOTools.GamePath = GamePath
			DAOTools.OutputPath = OutputPath
			DAOTools.GamePathValid = GamePathValid
			DAOTools.OutputPathValid = OutputPathValid
			DAOTools.WorldScale = Scale
			DAOTools.ImportPaths = ImportPaths
			DAOTools.WriteIni "Paths" "GamePath" DAOTools.GamePath
			DAOTools.WriteIni "Paths" "OutputPath" DAOTools.OutputPath
			DAOTools.WriteIni "Paths" "GamePathValid" DAOTools.GamePathValid
			DAOTools.WriteIni "Paths" "OutputPathValid" DAOTools.OutputPathValid
			DAOTools.WriteIni "Global" "WorldScale" DAOTools.WorldScale
			for i = 1 to DAOTools.MaxImportPaths do DAOTools.WriteIni "Paths" ("Import"+i as string) ""
			for i = 1 to ImportPaths.count do DAOTools.WriteIni "Paths" ("Import"+i as string) DAOTools.ImportPaths[i]
			
			ExitWarnOnce = true
			MessageBox "Remember you can restart this Tool by creating a Button or Menuitem for DAOTools-Setup.\n\nSettings saved.." title:"DAO Setup"
			try(DestroyDialog (DAOSetupToolsRollout))catch()
		)
		on btn_cancel pressed do
		(
			ExitWarnOnce = true
			MessageBox "Remember you can restart this Tool by creating a Button or Menuitem for DAOTools-Setup.\n\nCanceled.." title:"DAO Setup"
			try(DestroyDialog (DAOSetupToolsRollout))catch()
		)
		on SetupDialog close do
		(
			if ExitWarnOnce != true then MessageBox "Remember you can restart this Tool by creating a Button or Menuitem for DAOTools-Setup.\n\nCanceled.." title:"DAO Setup"
		)
	)
	
	rollout DAOSetupToolsRollout "Dragon Age Model Importer" width:382 height:227
	(
		GroupBox grp1 "" pos:[100,1] width:198 height:45
		label lbl1 "Setup Tools" pos:[170,10] width:80 height:16
		label lbl2 "Dragon Age: Origins" pos:[153,27] width:95 height:16
		edittext txt_maxversion "" pos:[6,506] width:68 height:16 enabled:false
		subRollout rollouts "" pos:[0,49] width:382 height:454
		label lbl10 "Script by Eshme" pos:[240,507] width:85 height:16 enabled:false
		label lbl11 "" pos:[330,507] width:40 height:16 enabled:false
		
		on DAOSetupToolsRollout open do 
		(

			lbl11.text = DAOTools.Version
			local maxver = maxVersion()		
			txt_maxversion.text = "MAX  v"+((maxver[1] / 1000) as string)
			if maxver[1] <= 4200 then txt_maxversion.text = " GMAX"
			Rollouts.height = DAOSetup.UI_Height - 70
			txt_maxversion.pos = [txt_maxversion.pos.x,DAOSetup.UI_Height - 20]
			lbl10.pos = [lbl10.pos.x,DAOSetup.UI_Height - 17]
			lbl11.pos = [lbl11.pos.x,DAOSetup.UI_Height - 17]
		)
		on DAOSetupToolsRollout close do 
		(
			DAOTools.WriteIni "Setup" "UI_Pos_X" DAOSetup.UI_Pos.x
			DAOTools.WriteIni "Setup" "UI_Pos_Y" DAOSetup.UI_Pos.y
			DAOTools.WriteIni "Setup" "UI_Height" DAOSetup.UI_Height
		)
		on DAOSetupToolsRollout resized Size do
		(
			if DAOSetupToolsRollout.open then
			(
				DAOSetup.UI_Height = Size.y
				Rollouts.height = DAOSetup.UI_Height - 70
				txt_maxversion.pos = [txt_maxversion.pos.x,DAOSetup.UI_Height - 20]
				lbl10.pos = [lbl10.pos.x,DAOSetup.UI_Height - 17]
				lbl11.pos = [lbl11.pos.x,DAOSetup.UI_Height - 17]
			)
		)
		on DAOSetupToolsRollout moved pos do DAOSetup.UI_Pos = pos
	)
	
	on execute do with undo off
	(
		if DAOSetup == undefined do Initialize()
		--Initialize()
		
		CreateDialog DAOSetupToolsRollout style:#(#style_sysmenu,#style_titlebar,#style_minimizebox,#style_resizing) height:DAOSetup.UI_Height lockWidth:true pos:DAOSetup.UI_Pos
		AddSubRollout DAOSetupToolsRollout.Rollouts SetupDialog rolledup:false
	)
	
)