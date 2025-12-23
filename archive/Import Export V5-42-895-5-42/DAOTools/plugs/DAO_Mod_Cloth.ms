/**********************************************************************************************************
DA:O Model and Animation Import Export
Author: Eshme  http://social.bioware.com/project/2336/

last edited
15 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
DAO_Mod_Cloth.ms
Cloth Modifier Plugin
************************************************************************************************************
*/

plugin simplemod DAO_Mod_Cloth
name:"DAO Cloth"
classID:#(0x28c203fd, 0x2733132f)
--category:"Dragon Age"
version:1
--extends:Bend
--replaceui:true
(
	parameters ClothShapeParams rollout:DAOClothShapeRoll
	(
		ClothNodeID							type:#string	default:""
		ClothCastRuntimeShadow				type:#boolean	animatable:false	default:false	ui:chk_CastRuntimeShadow
		ClothCastBakedShadow				type:#boolean	animatable:false	default:false	ui:chk_CastBakedShadow
		ClothReceiveRuntimeShadow			type:#boolean	animatable:false	default:false	ui:chk_ReceiveRuntimeShadow
		ClothReceiveBakedShadow				type:#boolean	animatable:false	default:false	ui:chk_ReceiveBakedShadow
		ClothCutAway						type:#boolean	animatable:false	default:false	ui:chk_CutAway
		ClothPunchThrough					type:#boolean	animatable:false	default:false	ui:chk_PunchThrough
		
		ClothShapeCollMaskAll				type:#boolean	animatable:false	default:false	--not used yet
		ClothShapeCollMaskNone				type:#boolean	animatable:false	default:false
		ClothShapeCollMaskItems				type:#boolean	animatable:false	default:false
		ClothShapeCollMaskCreatures			type:#boolean	animatable:false	default:false
		ClothShapeCollMaskPlaceables		type:#boolean	animatable:false	default:false
		ClothShapeCollMaskTriggers			type:#boolean	animatable:false	default:false
		ClothShapeCollMaskStatic			type:#boolean	animatable:false	default:false
		ClothShapeCollMaskEffects			type:#boolean	animatable:false	default:false
		ClothShapeCollMaskWaypoints			type:#boolean	animatable:false	default:false
		ClothShapeCollMaskWater				type:#boolean	animatable:false	default:false
		ClothShapeCollMaskTerrainWall		type:#boolean	animatable:false	default:false
		ClothShapeCollMaskWalkable			type:#boolean	animatable:false	default:false
		ClothShapeCollMaskNonWalkable		type:#boolean	animatable:false	default:false
	)
	parameters Clothparams rollout:DAOClothPropRoll
	(
		ClothThickness						type:#float		animatable:false	default:0.1		ui:spn_thickness
		ClothDensity						type:#float		animatable:false	default:1.0		ui:spn_density
		ClothBendingStiffness				type:#float		animatable:false	default:0.3		ui:spn_bending_stiffness
		ClothStretchingStiffness			type:#float		animatable:false	default:1.0		ui:spn_stretching_stiffness
		ClothDampingCoefficient				type:#float		animatable:false	default:0.05	ui:spn_damping_coefficient
		ClothFriction						type:#float		animatable:false	default:0.0		ui:spn_friction
		ClothPressure						type:#float		animatable:false	default:1.0		ui:spn_pressure
		ClothTearFactor						type:#float		animatable:false	default:1.5		ui:spn_tearfactor
		ClothCollisionResponseCoefficient	type:#float		animatable:false	default:0.2		ui:spn_collision_response_coefficient
		ClothAttachmentResponseCoefficient	type:#float		animatable:false	default:0.2		ui:spn_attachment_response
		ClothAttachmentTearFactor			type:#float		animatable:false	default:1.5		ui:spn_attachment_tearfactor
		ClothSolverIterations				type:#integer	animatable:false	default:5		ui:spn_solver_iterations
		ClothExternalAcceleration			type:#point3	animatable:false	default:[0,0,0]
		ClothWakeUpCounter					type:#float		animatable:false	default:10.0	ui:spn_wakeup_counter
		ClothSleepLinearVelocity			type:#float		animatable:false	default:0.01	ui:spn_sleep_linear_velocity
		ClothFadeable						type:#boolean	animatable:false	default:false	--not used yet
		
		ClothFlagPressure				type:#boolean	animatable:false	default:false		ui:chk_pressure
		ClothFlagStatic					type:#boolean	animatable:false	default:false		ui:chk_static
		ClothFlagDisableCollision		type:#boolean	animatable:false	default:true		ui:chk_disable_coll
		ClothFlagSelfCollision			type:#boolean	animatable:false	default:false		ui:chk_self_coll
		ClothFlagVisualization			type:#boolean	animatable:false	default:false		ui:chk_visualization
		ClothFlagGravity				type:#boolean	animatable:false	default:true		ui:chk_gravity
		ClothFlagBending				type:#boolean	animatable:false	default:true		ui:chk_bending
		ClothFlagBendingOrtho			type:#boolean	animatable:false	default:false		ui:chk_ortho_bending
		ClothFlagDamping				type:#boolean	animatable:false	default:false		ui:chk_damping
		ClothFlagCollisionTwoWay		type:#boolean	animatable:false	default:false		ui:chk_twoway_coll
		ClothFlagTriangleCollision		type:#boolean	animatable:false	default:false		ui:chk_triangle_coll
		ClothFlagTearable				type:#boolean	animatable:false	default:false		ui:chk_tearable
		ClothFlagHardware				type:#boolean	animatable:false	default:false		ui:chk_hardware
		ClothFlagCOMDamping				type:#boolean	animatable:false	default:false		ui:chk_com_damping
		
		ClothWindEnabled				type:#integer	animatable:false	default:1				--not used yet
		ClothWindSpace					type:#integer	animatable:false	default:1	--"world"	--not used yet
		ClothWindDirection				type:#point3	animatable:false	default:[1.0,0.0,0.0]	--not used yet
		ClothWindResponse				type:#float		animatable:false	default:30.0			--not used yet
		ClothWindResponseLimit			type:#float		animatable:false	default:30.0			--not used yet
		ClothWindStrength				type:#float		animatable:false	default:2.0				--not used yet
		ClothGustingMinStrength			type:#float		animatable:false	default:10.0			--not used yet
		ClothGustingMaxStrength			type:#float		animatable:false	default:25.0			--not used yet
		ClothGustingMinDuration			type:#float		animatable:false	default:0.1				--not used yet
		ClothGustingMaxDuration			type:#float		animatable:false	default:2.0				--not used yet
		ClothGustingMinInterval			type:#float		animatable:false	default:0.0				--not used yet
		ClothGustingMaxInterval			type:#float		animatable:false	default:1.5				--not used yet
		ClothGustingDirectionChange		type:#float		animatable:false	default:0.4				--not used yet
		ClothGustingDirectionAxisRatio	type:#point3	animatable:false	default:[1.0,1.0,0.2]	--not used yet
		ClothSpeedTreeWindUpdateTime	type:#float		animatable:false	default:10000.0			--not used yet
		ClothSpeedTreeWindStrength		type:#integer	animatable:false	default:1				--not used yet
		ClothSpeedTreeWindDirection		type:#integer	animatable:false	default:1				--not used yet
		ClothSpeedTreeWindParams		type:#integer	animatable:false	default:1				--not used yet
		
		ClothAttachmentType				type:#integer	animatable:false	default:2			ui:drp_attachment_type	--1:"none" 2:"CollidingShapes", 3:"Shape", 4:"Vertex" 1-based!. MMH is 0 based
		ClothFlagTwoWayAttachment		type:#boolean	animatable:false	default:true		ui:chk_twoway_attachment
		ClothFlagTearableAttachment		type:#boolean	animatable:false	default:false		ui:chk_tearable_attachment
	)
	rollout DAOClothShapeRoll "Mesh Physical Properties" width:165 height:350 rolledup:true
	(
		--only cast runtime shadow seems to work at all. needs more tests
		checkbox chk_CastRuntimeShadow "Cast Runtime Shadow" pos:[13,8] width:129 height:15
		checkbox chk_OnlyHereToAvoidGmaxError "dummy" pos:[1345,1]	--some strange glitch ,makes beeps
		checkbox chk_CastBakedShadow "Cast Baked Shadow" pos:[13,28] width:121 height:15 enabled:false
		checkbox chk_ReceiveRuntimeShadow "Receive Runtime Shadow" pos:[13,48] width:148 height:15 enabled:false
		checkbox chk_ReceiveBakedShadow "Receive Baked Shadow" pos:[13,68] width:140 height:15 enabled:false
		checkbox chk_CutAway "Cut Away" pos:[13,88] width:69 height:15 enabled:false
		checkbox chk_PunchThrough "Punch Through" pos:[13,108] width:98 height:15 enabled:false
			
		dropdownlist ddl_walkable "Walkable State:" items:#("Walkable","NonWalkable") enabled:false
		dropdownlist ddl_collision_mask "Collision Mask:" items:#("None","Item","Creature","Placeable","Static Geometry","Trigger","Terrain Wall","Water","Effects","WayPoint") enabled:false
		
		on ddl_collision_mask selected sel do
		(
			ClothShapeCollMaskNone			= (if sel == 1 then true else false)
			ClothShapeCollMaskItems			= (if sel == 2 then true else false)
			ClothShapeCollMaskCreatures		= (if sel == 3 then true else false)
			ClothShapeCollMaskPlaceables	= (if sel == 4 then true else false)
			ClothShapeCollMaskStatic		= (if sel == 5 then true else false)
			ClothShapeCollMaskTriggers		= (if sel == 6 then true else false)
			ClothShapeCollMaskTerrainWall	= (if sel == 7 then true else false)
			ClothShapeCollMaskWater			= (if sel == 8 then true else false)
			ClothShapeCollMaskEffects		= (if sel == 9 then true else false)
			ClothShapeCollMaskWaypoints		= (if sel == 10 then true else false)
		)
		
		on ddl_walkable selected sel do
		(
			ClothShapeCollMaskWalkable		= (if sel == 1 then true else false)
			ClothShapeCollMaskNonWalkable	= (if sel == 2 then true else false)
		)
		
		on DAOClothShapeRoll open do
		(
			--if ClothShapeCollMaskAll
			if ClothShapeCollMaskNone == true			then ddl_collision_mask.selection = 1
			if ClothShapeCollMaskItems == true			then ddl_collision_mask.selection = 2
			if ClothShapeCollMaskCreatures == true		then ddl_collision_mask.selection = 3
			if ClothShapeCollMaskPlaceables == true		then ddl_collision_mask.selection = 4
			if ClothShapeCollMaskStatic == true			then ddl_collision_mask.selection = 5
			if ClothShapeCollMaskTriggers == true		then ddl_collision_mask.selection = 6
			if ClothShapeCollMaskTerrainWall == true	then ddl_collision_mask.selection = 7
			if ClothShapeCollMaskWater == true			then ddl_collision_mask.selection = 8
			if ClothShapeCollMaskEffects == true		then ddl_collision_mask.selection = 9
			if ClothShapeCollMaskWaypoints == true		then ddl_collision_mask.selection = 10
			if ClothShapeCollMaskWalkable == true		then ddl_walkable.selection = 1
			if ClothShapeCollMaskNonWalkable == true	then ddl_walkable.selection = 2
			
		)
	)
	
	rollout DAOClothPropRoll "Cloth Properties" width:165 height:500 rolledup:false
	(
		groupbox grp1 "Cloth Flags:" pos:[2,3] width:162 height:154
		checkbox chk_pressure "Pressure" pos:[6,22] width:65 height:15
		checkbox chk_static "Static" pos:[82,22] width:51 height:15
		checkbox chk_disable_coll "Disable Coll" pos:[6,40] width:75 height:15
		checkbox chk_self_coll "Self Collision" pos:[82,40] width:77 height:15
		checkbox chk_visualization "Visualization" pos:[6,58] width:75 height:15
		checkbox chk_gravity "Gravity" pos:[82,58] width:57 height:15
		checkbox chk_bending "Bending" pos:[6,76] width:63 height:15
		checkbox chk_ortho_bending "Ortho Bend" pos:[82,76] width:73 height:15
		checkbox chk_damping "Damping" pos:[6,94] width:66 height:15
		checkbox chk_twoway_coll "2Way Coll" pos:[6,112] width:72 height:15
		checkbox chk_triangle_coll "Triangle Coll" pos:[82,112] width:80 height:15
		checkbox chk_tearable "Tearable" pos:[6,130] width:66 height:15
		checkbox chk_hardware "Hardware" pos:[82,130] width:70 height:15
		checkbox chk_com_damping "COM Damp" pos:[82,94] width:80 height:15
		
		groupbox grp2 "Cloth Properties:" pos:[2,158] width:162 height:387
		label lbl_thickness "Thickness" pos:[46,180];			spinner spn_thickness "" pos:[100,180] width:60 height:16 range:[0,100,0.01]
		label lbl_density "Density" pos:[60,200];				spinner spn_density "" pos:[100,200] width:60 height:16 range:[0,100,1.0]
		label lbl_ben_stiff "Bending Stiffness" pos:[14,220];	spinner spn_bending_stiffness "" pos:[100,220] width:60 height:16 range:[0,1,1.0]
		label lbl_str_stiff "Stretching Stiffness" pos:[6,240];	spinner spn_stretching_stiffness "" pos:[100,240] width:60 height:16 range:[0,1,1.0]
		label lbl_damp_coef "Damping Coefficient" pos:[6,260];	spinner spn_damping_coefficient "" pos:[100,260] width:60 height:16 range:[0,1,0.5]
		label lbl_friction "Friction" pos:[60,280];				spinner spn_friction "" pos:[100,280] width:60 height:16 range:[0,1,0.5]
		label lbl_pressure "Pressure" pos:[54,300];				spinner spn_pressure "" pos:[100,300] width:60 height:16 range:[1,100,1.0]
		label lbl_tearfactor "Tearfactor" pos:[46,320];			spinner spn_tearfactor "" pos:[100,320] width:60 height:16 range:[1,100,1.5]
		label lbl_collision_resp "Collision Response" pos:[6,340];	spinner spn_collision_response_coefficient "" pos:[100,340] width:60 height:16 range:[1,100,0.2]
		label lbl_attach_resp "Attachm. Response" pos:[6,360];	spinner spn_attachment_response "" pos:[100,360] width:60 height:16 range:[1,100,0.2]
		label lbl_attach_tf "Attachm. Tearfactor" pos:[6,380];	spinner spn_attachment_tearfactor "" pos:[100,380] width:60 height:16
		label lbl_siteration "Solver Iterations" pos:[19,400];	spinner spn_solver_iterations "" pos:[100,400] width:60 height:16 range:[1,100,5] type:#integer scale:1
		label lbl_wakeup "Wake Up Counter" pos:[10,420];		spinner spn_wakeup_counter "" pos:[100,420] width:60 height:16
		label lbl_slinvelo "Sleep Lin. Velocity" pos:[8,440];	spinner spn_sleep_linear_velocity "" pos:[100,440] width:60 height:16
		
		label lbl_clothextaccell "Cloth External Acceleration:" pos:[29,463]
		label lbl_clothextaccellX "X" pos:[80,480];		spinner spn_cloth_ext_accelX pos:[100,480] width:60 height:16 range:[0,100,0.0]
		label lbl_clothextaccellY "Y" pos:[80,500];		spinner spn_cloth_ext_accelY pos:[100,500] width:60 height:16 range:[0,100,0.0]
		label lbl_clothextaccellZ "Z" pos:[80,520];		spinner spn_cloth_ext_accelZ pos:[100,520] width:60 height:16 range:[0,100,0.0]

		groupbox grp3 "Cloth Attachments:" pos:[2,547] width:162 height:106
		dropdownlist drp_attachment_type "Attachment Type" pos:[10,566] items:#("Dont attach","Auto attach to Coll. shapes","Shape??","Vertex??")
		checkbox chk_twoway_attachment "Two Way Attachment" pos:[10,610]
		checkbox chk_tearable_attachment "Tearable" pos:[10,630]
		
		on spn_cloth_ext_accelX changed val do
		(
			ClothExternalAcceleration.X = val
		)
		on spn_cloth_ext_accelY changed val do
		(
			ClothExternalAcceleration.Y = val
		)
		on spn_cloth_ext_accelZ changed val do
		(
			ClothExternalAcceleration.Z = val
		)
		
		on DAOClothPropRoll open do
		(
			spn_cloth_ext_accelX.value = ClothExternalAcceleration.X
			spn_cloth_ext_accelY.value = ClothExternalAcceleration.Y
			spn_cloth_ext_accelZ.value = ClothExternalAcceleration.Z
		)
	)
	
	rollout DAOClothAboutRoll "About" width:165 height:74 rolledup:true
	(
		label lbl1 "This Modifier is Part of:" pos:[3,8] width:137 height:15
		label lbl2 "DAO Import/Export Script" pos:[3,31] width:134 height:15
		label lbl3 "Written by Eshme" pos:[3,50] width:123 height:15
	)
	
	on map i p do	--must remain i think (default handler for simplemod)
	(
	)
)