/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edited
9 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
DAO_Helper_Crust.ms
Crust Node Plugin

Adds a new helper object ,that simulates Crust Nodes, (VFX attach points)
************************************************************************************************************
*/
plugin Helper DAO_Crust
name:"DAO VFX Crust Node"
classID:#(0x28c203fa, 0x2733132c)
category:"Dragon Age"
version:1
extends:point
replaceui:true
invisible:true
(
	parameters main rollout:params
	(
		size type:#WorldUnits animatable:false ui:spn_size
		HookID type:#integer animatable:false ui:ddl_hookid default:1
		
		on size set val do
		(
			delegate.size = size
			delegate.centermarker = true
			delegate.axistripod = true
			delegate.cross = true
			delegate.box = false
			delegate.constantscreensize = false
			delegate.drawontop = false
		)
	)
	rollout params "Crust Node Parameters" width:168 height:130
	(
		groupBox grp1 "Display" pos:[7,2] width:150 height:49
		groupBox grp2 "Crust" pos:[7,53] width:150 height:70
		spinner spn_size "Gizmo Size:" type:#worldUnits pos:[28,23] width:107 height:16 range:[0,1e+009,1]
		dropdownList ddl_hookid "Hook ID:" pos:[14,73] width:140 height:40 \
			items:#("1: Default", "2: Right Cast", "3: Left Cast", "4: Root", "5: Head", "6: Neck", "7: Upper Torso", "8: Mid Torso", \
					"9: Lower Torso", "10: Right Clavicle", "11: Right Upper Arm", "12: Right Lower Arm", "13: Right Hand", "14: Left Clavicle", \
					"15: Left Upper Arm", "16: Left Lower Arm", "17: Left Hand", "18: Right Upper Leg", "19: Right Lower Leg", "20: Right Foot", \
					"21: Left Upper Leg", "22: Left Lower Leg", "23: Left Foot", "24: Upper Left Wing", "25: Mid Left Wing", "26: Lower Left Wing", \
					"27: Upper Right Wing", "28: Mid Right Wing", "29: Lower Right Wing", "30: Tail01", "31: Tail02", "32: Tail03", "33: Tail04", \
					"34: Weapon01", "35: Weapon02", "36: Weapon03", "37: Weapon04", "38: Shield01", "39: Shield02", "40: Custom01", "41: Custom02", \
					"42: Custom03", "43: Custom04", "44: Custom05", "45: Custom06", "46: Custom07", "47: Custom08", "48: Custom09", "49: Custom10 ")
		
		on params open do
		(
			if (HookID < 1) or (HookID > 49) then
			(
				MessageBox ("Object: "+$.name+"\nHookID out of bounds: "+HookID as string+"\nResetting to 1 (Default)") title:"DAO VFX Crust Node"
				HookID = 1
				clearselection()
			)
		)
	)
)

plugin Helper DAO_Crust_Create
name:"Crust Node"
category:"Dragon Age"
version:1
(
	local obj
	tool create
	(
		on mousePoint click do
		(
			case click of
			(
				1: (
					in coordsys grid
					(
						obj = DAO_Crust pos:gridpoint
						select obj
						obj.name = uniquename "CrustHook"
						obj.pos = gridpoint
						obj.size = 20
						obj.delegate.size = 20
					)
				)
				2: (
					#stop
				)
			)
		)
	)
)
