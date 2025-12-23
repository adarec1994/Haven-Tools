/**********************************************************************************************************
Dragon Age Origins Tools
Author: Eshme  http://social.bioware.com/project/2336/

latest edit:
13 February 2011

Copyright notes:

This Script was created in my free time as a modding hobby, and is free of charge. 
Share it with your friends, but do not claim it as your own, nor sell it. 

Copyright © 2011 Eshme

************************************************************************************************************
Include Script
DAOGFFFormat.ms
GFF Parsing functions
************************************************************************************************************
*/
	struct GFFHeaderStruct (
		MagicNumber,
		Version,
		Platform,
		FileType,
		FileVersion,
		StructCount,
		DataOffset
	)
	
	struct GFFStructStruct (
		StructType,
		FieldCount,
		FieldOffset,
		StructSize,
		Fields = #()
	)
	
	struct GFFFieldStruct (
		Label,
		TypeID,
		Flags,
		DataOffset
	)
	
	struct GFFFileStruct (
		--**********************************************************
		--This is how the GFF is accessed
		--This struct 	|->Header ...
		--			  	|->Structs->|->1	|->Type
		--									|->FieldCount
		--									|->FieldOffset
		--									|->StructSize
		--									|->Fields->		|1| ->	|->Label
		--															|->TypeID
		--															|->Flags
		--															|->DataOffset
		--													|2| -> ...
		--							|->2 ....
		--**********************************************************
		Header = GFFHeaderStruct(),
		Structs = #()
	)
	
	struct GFFIDStruct (
		GFF_TAG							= 1, --| object tag - should be unique
		GFF_NAME						= 2, -- | the name of the object - as it shows up in 
		GFF_TEMPLATERESREF				= 3, -- | The template file for this object
		GFF_POSITION					= 4, -- | The position of this object
		GFF_ORIENTATION					= 5, -- | The orientation of this object
		GFF_UINT8_LIST					= 6, -- | UINT8 List
		GFF_INT8_LIST					= 7, -- | INT8 List
		GFF_UINT16_LIST					= 8, -- | UINT16 List
		GFF_INT16_LIST					= 9, -- | INT16 List
		GFF_UINT32_LIST					= 10, -- | UINT32 List
		GFF_INT32_LIST					= 11, -- | INT32 List
		GFF_UINT64_LIST					= 12, -- | UINT64 List
		GFF_INT64_LIST					= 13, -- | INT64 List
		GFF_FLOAT32_LIST				= 14, -- | FLOAT32 List
		GFF_FLOAT64_LIST				= 15, -- | FLOAT64 List
		GFF_VECTOR3F_LIST				= 16, -- | Vector3f List
		GFF_VECTOR4F_LIST				= 17, -- | Vector4f List
		GFF_QUATERNIONF_LIST			= 18, -- | Quaternionf List
		GFF_ECSTRING_LIST				= 19, -- | ECString List
		GFF_COLOR4F_LIST				= 20, -- | Color4f List
		GFF_NAME_HASH					= 21, -- | the hash value of the object's name
		GFF_TEXT						= 22, -- | Localizable text
		GFF_OBJECT_ID					= 23 -- | The object id for this object.	
	)
	
	struct VertexTypeStruct (
		DECLTYPE_FLOAT1 = 0,		--1 float
		DECLTYPE_FLOAT2 = 1,		--2 floats
		DECLTYPE_FLOAT3 = 2,		--3 floats
		DECLTYPE_FLOAT4 = 3,		--4 floats
		DECLTYPE_COLOR = 4,			--4-byte color
		DECLTYPE_UBYTE4 = 5,		--4 unsigned bytes
		DECLTYPE_SHORT2 = 6,		--2 shorts
		DECLTYPE_SHORT4 = 7,		--4 shorts
		DECLTYPE_UBYTE4N = 8,		--4 normalized bytes
		DECLTYPE_SHORT2N = 9,		--2 normalized shorts
		DECLTYPE_SHORT4N = 10,		--4 normalized shorts
		DECLTYPE_USHORT2N = 11,		--2 normalized unsigned shorts
		DECLTYPE_USHORT4N = 12,		--4 normalized unsigned shorts
		DECLTYPE_UDEC3 = 13,		--3d unsigned 10/10/10 format
		DECLTYPE_DEC3N = 14,		--3d unsigned 10/10/10 normalized
		DECLTYPE_FLOAT16_2 = 15,	--2 16-bit floats
		DECLTYPE_FLOAT16_4 = 16,	--4 16-bit unsigned floats
		--DECLTYPE_UNUSED = 0xffffffff

		DECLUSAGE_POSITION = 0,		--Position
		DECLUSAGE_BLENDWEIGHT = 1,	--Blend weights
		DECLUSAGE_BLENDINDICES = 2,	--Blend indices
		DECLUSAGE_NORMAL = 3,		--Normal
		DECLUSAGE_PSIZE = 4,		--Point Size
		DECLUSAGE_TEXCOORD = 5,		--Texture coordinates
		DECLUSAGE_TANGENT = 6,		--Tangent vector
		DECLUSAGE_BINORMAL = 7,		--binormal vector
		DECLUSAGE_TESSFACTOR = 8,	--tessellation factor
		DECLUSAGE_POSITIONT = 9,	--PositionT
		DECLUSAGE_COLOR = 10,		--color channel
		DECLUSAGE_FOG = 11,			--fog value
		DECLUSAGE_DEPTH = 12,		--depth
		DECLUSAGE_SAMPLE = 13		--sample
		--DECLUSAGE_UNUSED = 0xffffffff	--error/other/unset

	)
	local GFFFile = GFFFileStruct()
	local VertexType = VertexTypeStruct()
	local GFFID = GFFIDStruct()
	
	fn LoadGFF Stream = (
		--*****************************************************************************************
		--GFF Import. Loading the Definition into GFFFile accessed by all Import Functions
		--Only 1 Definition present at a time, so reading 2 GFFs at once requires a load inbetween
		--Returns true on success, false if not
		--*****************************************************************************************
		local out = false
		GFFFile = GFFFileStruct() --reset it
		
		--***************
		--Getting Header
		--***************
		fseek Stream 0 #seek_set
		GFFFile.Header.MagicNumber = readlong Stream		--All GFF files will start with the hexadecimal value 0x47464620, which is the ASCII value for “GFF “.
		GFFFile.Header.Version = readlong Stream			--4 bytes representing the version of the underlying GFF format. This should be “V4.0” or 0x56342E30 for all files using this format.
		GFFFile.Header.Platform = readlong Stream			--4-byte field indicating the intended target platform for this file. 
		GFFFile.Header.FileType = readlong Stream			--4-byte field used to identify this file type. By convention it should be the three letter file extension followed by a space.
		GFFFile.Header.FileVersion = readlong Stream		--4-byte version of the FileType. By convention it should be “Vx.x” or “xx.x” where X is a digit.
		GFFFile.Header.StructCount = readlong Stream		--4-byte unsigned number of elements in the Struct Array.
		GFFFile.Header.DataOffset = readlong Stream			--4-byte unsigned offset from the beginning of the file to the Raw Data Block.
		
		if GFFFile.Header.MagicNumber == 541476423 then			--If is a GFF File at all
		(
			--*************
			--Get Structs
			--*************
			for i = 1 to GFFFile.Header.StructCount do
			(
				local GFFStruct = GFFStructStruct()
				local name = ""
				for j = 1 to 4 do name += (bit.intaschar (readbyte Stream))
				GFFStruct.StructType	= name							--4-byte programmer defined ID
				GFFStruct.FieldCount	= readlong Stream				--4-byte number of fields in the struct
				GFFStruct.FieldOffset	= readlong Stream #unsigned		--4-byte unsigned offset from the beginning of the file to the first field in the struct
				GFFStruct.StructSize	= readlong Stream #unsigned		--4-byte unsigned size of the chunk of data representing the struct
				append GFFFile.Structs GFFStruct
			)
			for i = 1 to GFFFile.Header.StructCount do
			(
				fseek Stream (GFFFile.Structs[i].FieldOffset) #seek_set
				for j = 1 to GFFFile.Structs[i].FieldCount do
				(
					local GFFField		= GFFFieldStruct()
					GFFField.Label		= readlong Stream				--4-byte label used to look up the field
					GFFField.TypeID		= readshort Stream #unsigned	--A 2-byte unsigned number indicating the type
					GFFField.Flags		= readshort Stream #unsigned	--2-bytes of bit flags
					GFFField.DataOffset	= readlong Stream #unsigned		--4-byte unsigned offset to the location of the data
					append GFFFile.Structs[i].Fields GFFField
				)
			)
			out = true
		)
		out
	)
	
	fn IsMMH = (if GFFFile.Header.FileType == 541609293 then true else false)	-- MMH file type (lol)
	
	fn IsAni = (if GFFFile.Header.FileType == 541675073 then true else false)	-- Couldnt bother better checks
	
	fn IsGAD = (if GFFFile.Header.FileType == 541344071 then true else false)
	
	fn IsPHY = (if GFFFile.Header.FileType == 542722128 then true else false)
	
	fn IsMSH = (if GFFFile.Header.FileType == 1213416781 then true else false)
	
	fn isbitset thebit value = (
		local Bitint = 0
		Bitint = case thebit of
		(
			1: 32768			--List Flag
			2: 16384			--Struct Flag, confusing
			3: 8192				--Ref Flag ???
		)
		if (bit.or Bitint value) == value then true else false
	)
	
	fn ReadData Stream Field Offset = (			--Returns Array of [1]=Type(has no big meaning should be != "") [2]=Data
		local out = #("",("No type yet: "+ Field.TypeID as string))	--Default	--was a remain of my own test parser
		local isStruct = isbitset 2 Field.Flags
		local isList = isbitset 1 Field.Flags
		local isRef = isbitset 3 Field.Flags
		local ListArray = #()

		fseek Stream (GFFFile.Header.DataOffset + Field.DataOffset + Offset) #seek_set		--seek DataBlockOffset+ SingledataOffset +ChildStructOffset

		if isList then		--General differences seem to be, either List or no List. Everything else depends on it.
		(
			ref = readlong Stream
			if ref >= 0 then
			(
				fseek Stream (GFFFile.Header.DataOffset + ref) #seek_set	--??? huh ?? Think this is the ref to the list.. or Bit 1 set is a reference again
				if isStruct and not isRef then
				(
					StructOffset = ref + 4
					StructRef = Field.TypeID	--When structbit is set, TypeID is the index of Struct, or so the Wiki sings
					ListCount = readlong Stream
					for i = 1 to ListCount do
					(
						append ListArray #(StructRef+1,StructOffset)	--Added +1 because Structs Array begins count at [1] not at 0 --ref+4 is just the offset after the struct count
						StructOffset += GFFFile.Structs[StructRef+1].StructSize
					)
					out = #("Struct List", ListArray)
				)				
				if isRef and not isStruct then
				(
					ListCount = readlong Stream
					for i = 1 to ListCount do
					(
						StructRef = readshort stream
						FieldFlags = readshort stream
						FieldOffset = readlong stream
						
						append ListArray #(StructRef+1, FieldFlags ,FieldOffset)	--Added +1 because Structs Array begins count at [1] not at 0
					)
					out = #("Generic List",ListArray)
				)
				if isStruct and isRef then		--is everyting! 
				(
					ListCount = readlong stream
					for i = 1 to ListCount do
					(
						StructRef = Field.TypeID
						StructOffset = readlong Stream
						append ListArray #(StructRef+1,StructOffset)
					)
					out = #("Ref Struct List", ListArray)		--If you are unsure which flag is set, just call it Ref-Struct-List i suppose.
				)
				if not isStruct and not isRef then
				(
					if Field.TypeID == 0 then		--UINT8 List	--Seems to be only used in huge binary datablocks so far. Thus i avoid reading all the bytes here.
					(											--Returning only the offset. Functions need to deal with the rest.
						ListCount = readlong Stream
						append ListArray ref
						--for i = 1 to ListCount do
						--(
						--	append ListArray #(readbyte Stream #unsigned)
						--)
						out = #("Type 0 List", ListArray)
					)
					if Field.TypeID == 4 then		--UINT32 list
					(
						ListCount = readlong Stream
						for i = 1 to ListCount do
						(
							append ListArray #(readlong Stream #unsigned)
						)
						out = #("Type 4 List", ListArray)
					)
					if Field.TypeID == 8 then		--Float32 List
					(
						ListCount = readlong Stream
						for i = 1 to ListCount do
						(
							append ListArray #(readfloat stream)
						)
						out = #("Type 8 List", ListArray)
					)
					if Field.TypeID == 14 then
					(
						ListCount = readlong Stream
						out = #("Type 14 List", Listcount)
					)
				)
			)
			else out = #("**","Null Reference")
		)
		
		if not isList then
		(
			if isRef then
			(
				StructRef = readshort stream
				FieldFlags = readshort stream
				FieldOffset = readlong stream
				append ListArray #(StructRef+1, FieldFlags ,FieldOffset)
				out = #("Generic List", ListArray)
			)
			if isStruct then
			(
				StructRef = Field.TypeID
				ListCount = 1
				append ListArray #(StructRef+1,Offset)
				out = #("Struct List", ListArray)
			)
			if not isRef and not isStruct then
			(
				if Field.TypeID == 0 then out = #("Type 0",readbyte Stream #unsigned)	--UINT8
				if Field.TypeID == 1 then out = #("Type 1",readbyte Stream #signed)		--INT8
				if Field.TypeID == 2 then out = #("Type 2",readshort Stream #unsigned)	--UINT16
				if Field.TypeID == 3 then out = #("Type 3",readshort Stream #signed)	--INT16
				if Field.TypeID == 4 then out = #("Type 4",readlong Stream #unsigned)	--UINT32
				if Field.TypeID == 5 then out = #("Type 5",readlong Stream #signed)		--INT32
				if Field.TypeID == 6 then out = #("Type 6",#(readlong Stream #unsigned,readlong Stream #unsigned)) 	--UINT64
				if Field.TypeID == 8 then out = #("Type 8",readfloat Stream)			--Float 32
				if Field.TypeID == 10 then out = #("Type 10",[readfloat Stream,readfloat Stream,readfloat Stream] as point3)		--Vector3f
				if Field.TypeID == 12 then out = #("Type 12",[readfloat Stream,readfloat Stream,readfloat Stream] as point3)		--Vector4f (didnt include the 4th cause of gmax)
				if Field.TypeID == 13 then out = #("Type 13",(quat (readfloat Stream) (readfloat Stream) (readfloat Stream) (readfloat Stream)) as quat)	--Quaternionf
				if Field.TypeID	== 14 then --ECString
				(
					out = ""
					ECoffset = readlong Stream							--Read EC
					if ECoffset >= 0 then								-- -1 is a null reference
					(	--Reading String
						fseek Stream (GFFFile.Header.DataOffset + ECoffset) #seek_set	--DataBlockOffset+ SingledataOffset (EC is offset again)
						for j = 1 to (readlong Stream) do				--For number of chars
						(
							out += (bit.intaschar (readbyte Stream))	--Append a letter
							dummychar = readbyte Stream					--but skip empty spaces (wchar)
						)
						out = #("Type 14", out)
					)
					else out = #("Type 14" ,"Null Reference")
				)
				if Field.TypeID	== 15 then out = #("Type 15",#(readfloat Stream,readfloat Stream,readfloat Stream,readfloat Stream))	--Color4f
			)
		)

		out		--Return Data (any type)
	)
	
	fn ReadDataByID Stream StructDef ID Offset = (
		--****************************************************************************************************
		--Searching Data by Field label (numerical ID) inside of all Fields in a Struct.
		--returns the Data in [2] (Filters out type string which is [1]), (remains of test scripts)
		--****************************************************************************************************
		local done = false
		local Data
		local out = undefined
		for i = 1 to StructDef.Fields.count where not done do
		(
			if (StructDef.Fields[i].label) == ID then
			(
				Data = (ReadData Stream StructDef.Fields[i] Offset)
				if Data[1] != "" then
				(
					out = Data[2]
					done = true
				)
			)
		)
		out	
	)
	
	fn ReadDataByIDInStructs Stream DataStructs ID = (
		--*******************************************************************************
		--Finds Data in Fields across multiple Structs and returns it
		--....in the form of #(#(6, 16384, 400), #(6, 16384, 456), #(9, 16384, 512)...
		--********************************************************************************
		local out = undefined
		for i = 1 to DataStructs.count do
		(
			local DataOffset	= DataStructs[i][3]
			local DataStructID	= DataStructs[i][1]
			local DataFields	= GFFFile.Structs[DataStructID]
			if (ReadDataByID Stream DataFields ID DataOffset) != undefined then out = (ReadDataByID Stream DataFields ID DataOffset)
		)
		out
	)
	
	fn ReadDeclType Stream Offset Type = (
		local out = #(0,0,0,0)
		fseek Stream Offset #seek_set
		if Type == 0 then out = #(readfloat Stream)
		if Type == 1 then out = #(readfloat Stream,readfloat Stream)
		if Type == 2 then out = #(readfloat Stream,readfloat Stream,readfloat Stream)
		if Type == 3 then out = #(readfloat Stream,readfloat Stream,readfloat Stream,readfloat Stream)
		if Type == 4 then out = #(readbyte Stream #unsigned,readbyte Stream #unsigned,readbyte Stream #unsigned,readbyte Stream #unsigned)
		if Type == 5 then out = #(readbyte Stream #unsigned,readbyte Stream #unsigned,readbyte Stream #unsigned,readbyte Stream #unsigned)
		if Type == 6 then out = #(readshort Stream,readshort Stream)
		if Type == 7 then out = #(readshort Stream,readshort Stream,readshort Stream,readshort Stream)
		if Type > 7 and Type < 15 then MessageBox ("Vertex Declarator is in unsupported Format.\nFormat: "+Type as string+"\nStream:  "+Stream as string+"\nNotify Eshme") title:"MSH Import"
		if Type == 15 then out = #(Float16to32(readshort Stream),Float16to32(readshort Stream))
		if Type == 16 then out = #(Float16to32(readshort Stream),Float16to32(readshort Stream),Float16to32(readshort Stream),Float16to32(readshort Stream))
		out
	)
