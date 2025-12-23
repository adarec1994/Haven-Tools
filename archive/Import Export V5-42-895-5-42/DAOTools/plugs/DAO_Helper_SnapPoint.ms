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
DAO_Helper_SnapPoint.ms
SnapPoint Node Plugin

Adds a new helper object ,that simulates "SnapPoints"
************************************************************************************************************
*/
plugin Helper DAO_SnapPoint
name:"DAO SnapPoint Node"
classID:#(0x28c203fc, 0x2733132e)
category:"Dragon Age"
version:1
extends:point
replaceui:true
invisible:true
(
	parameters main rollout:params
	(
		size type:#WorldUnits animatable:false ui:spn_size
		on size set val do
		(
			delegate.size = size
			delegate.centermarker = true
			delegate.axistripod = true
			delegate.cross = true
			delegate.box = true
			delegate.constantscreensize = false
			delegate.drawontop = false
		)
	)
	rollout params "SnapPoint Parameters" width:168 height:130
	(
		groupBox grp1 "Display" pos:[7,2] width:150 height:49
		spinner spn_size "Gizmo Size:" type:#worldUnits pos:[28,23] width:107 height:16 range:[0,1e+009,1]
		
		on params open do
		(
		)
	)
)

plugin Helper DAO_SnapPoint_Create
name:"Snap Point"
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
						obj = DAO_SnapPoint pos:gridpoint
						select obj
						obj.name = uniquename "SnaptoPoint"
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
