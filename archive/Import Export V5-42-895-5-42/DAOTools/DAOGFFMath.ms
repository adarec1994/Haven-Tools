/**********************************************************************************************************
Dragon Age Origins Tools
Author: Eshme  http://social.bioware.com/project/2336/

latest edit:
24 May 2010

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2010 Eshme

************************************************************************************************************
Include Script
DAOGFFMath.ms
GFF Decode functions
************************************************************************************************************
*/

	fn Float16to32 Float16 = (
		--*************************************
		--Converting IEE Float16 into Float32
		--*************************************
	
		local out = 0 as float
		S	= (bit.and Float16 32768)/32768				--Sign Bit 1 ,shifting 15 to the right
		E	= (bit.and Float16 31744)/1024				--Exp Bit 5	,shifting 10 to the right
		M	= bit.and Float16 1023						--Mantisse Bit 10
		F	= M/1024 as float							--the fraction
		
		if E==31 and M!= 0 then out = 0  --"NaN"
		if E==31 and M== 0 and S == 1 then out = 0 --"-Infinity"
		if E==31 and M== 0 and S == 0 then out = 0 --"Infinity"
		if E>0 and E<31 then out = (-1 as float)^S * (2 as float)^(E-15) * (1+F)
		if E==0 and M!=0 then out = (-1 as float)^S * (2 as float)^(-14)* F
		if E==0 and M==0 and S==1 then out =-0
		if E==0 and M==0 and S==0 then out =0
		
		out as float
	)
	
	fn DecompressQuat Quat32 Quat64 Quat48 Quality = (
		--*****************************************************************
		--Compressed Quaternions
		--Quality 2 Quat32 			= 1 x 32bit Int
		--Quality 4 Quat32+64 		= 1 x 64bit Int
		--Quality 3 Quat32+64+48 	= 3 x 16bit Int (rare but added it lastly)
		--*******************************************************************
	
		local out = (quat 0 0 0 1)
		local quat0
		local quat1
		local quat2
		local quat3
		local order
		local bits1
		
		if Quality == 2 then
		(
			if (maxversion())[1] > 4200 then
			(
				quat1 = bit.shift (Bit.and Quat32 4290772992) -22		--First 10bits, shifted 22 to the right
				quat2 = bit.shift (Bit.and Quat32 4190208) -12			--next 10bits, shifted 12 to the right
				quat3 = bit.shift (Bit.and Quat32 4092) -2				--next 10bits, shifted 2 to the right
			)
			else	--gmax cant shift to the right.. thus
			(
				bits1 = Bit.and Quat32 4290772992
				quat1 = (bit.set bits1 32 false) / 4194304		--veery hacky,sign bit removal
				if (bit.and bits1 -2147483648) == -2147483648 then quat1 += 512
				quat2 = (Bit.and Quat32 4190208) / 4096
				quat3 = (Bit.and Quat32 4092) / 4
			)
			quat1 = (quat1-512) * (1/(sqrt(2)*511)) as float		--Full 10bit range 0-1024, convert into ranges -0.707 to 0.707
			quat2 = (quat2-512) * (1/(sqrt(2)*511)) as float		--1/511 because that seems more accurate than 512. This is the scale factor
			quat3 = (quat3-512) * (1/(sqrt(2)*511)) as float
			order = (Bit.and Quat32 3) 								--last 2 bits, order, biggest quaternion
		)
		if Quality == 4 then
		(
			if (maxversion())[1] > 4200 then
			(
				quat1 = bit.shift (Bit.and Quat32 4294965248) -11			--First 21 bits
				quat2 = bit.shift (Bit.and Quat32 2047) 10					--last 10bits and
				quat2 =(bit.shift (Bit.and Quat64 4290772992) -22) + quat2	--first 11bits makes up quat2
				quat3 = bit.shift (Bit.and Quat64 4194300) -2				--20bits, poor quat3 only has 20bits
			)
			else --gmaxes craziness comes here
			(
				bits1 = Bit.and Quat32 4294965248
				quat1 = (bit.set bits1 32 false) / 2048
				if (bit.and bits1 -2147483648) == -2147483648 then quat1 += 1048576
				quat2 = bit.shift (Bit.and Quat32 2047) 10
				bits1 = Bit.and Quat64 4290772992
				quat2 = (bit.set bits1 32 false) / 4194304 + quat2
				if (bit.and bits1 -2147483648) == -2147483648 then quat2 += 1024
				quat3 = (Bit.and Quat64 4194300) / 4			
			)
			quat1 = (quat1-1048576) * (1/(sqrt(2)*1048575)) as float		--Also reduced the Scale factor by 1 here, because that seems point accurate
			quat2 = (quat2-1048576) * (1/(sqrt(2)*1048575)) as float
			quat3 = (quat3-524288) * (1/(sqrt(2)*524287)) as float
			order = (Bit.and Quat64 3) 									--last 2 bits, order, biggest quaternion
		)
		if Quality == 3 then
		(
			quat1 = (Bit.and Quat32 65534) /2	--Shift 1 to the right
			quat2 = (Bit.and Quat64 65534) /2
			quat3 = (Bit.and Quat48 65534) /2
			quat1 = (quat1-16384) * (1/(sqrt(2)*16383)) as float
			quat2 = (quat2-16384) * (1/(sqrt(2)*16383)) as float
			quat3 = (quat3-16384) * (1/(sqrt(2)*16383)) as float
			Order = ((Bit.and Quat32 1)*2)+(Bit.and Quat64 1)	--littlest2 bits of first2 ints make up the order i guess
		)
		quat0 = sqrt(1-quat1^2-quat2^2-quat3^2)
		if Order == 0 then out = (quat (quat0) (quat1) (quat2) (quat3))
		if Order == 1 then out = (quat (quat1) (quat0) (quat2) (quat3))
		if Order == 2 then out = (quat (quat1) (quat2) (quat0) (quat3))
		if Order == 3 then out = (quat (quat1) (quat2) (quat3) (quat0))
		out
	)
