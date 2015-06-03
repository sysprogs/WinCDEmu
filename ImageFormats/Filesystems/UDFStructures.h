#pragma once

namespace UDFStructures
{
#pragma region Integral types
	typedef unsigned short Uint16;
	typedef signed short Int16;

	typedef unsigned int Uint32;
	typedef signed int Int32;

	typedef unsigned __int64 Uint64;
	typedef signed __int64 Int64;

	typedef unsigned char byte, Uint8, dstring;
	typedef signed char Int8;
#pragma endregion

#pragma pack(push, 1)
	enum UDFTagIdentifier : Uint16
	{
		dtPrimaryVolumeDescriptor = 1,
		dtAnchorVolumeDescriptorPointer = 2,
		dtVolumeDescriptorPointer = 3,
		dtImplementationUseVolumeDescriptor = 4,
		dtPartitionDescriptor = 5,
		dtLogicalVolumeDescriptor = 6,
		dtUnallocatedSpaceDescriptor = 7,
		dtTerminatingDescriptor = 8,

		dtFileSet = 0x100,				//=256
		dtFileEntry = 0x105,			//=261
		dtExtendedFileEntry = 0x10A,	//=266
	};

	struct Tag { /* ECMA 167 3/7.2 */ 
		UDFTagIdentifier TagIdentifier; 
		Uint16 DescriptorVersion; 
		Uint8 TagChecksum; 
		byte Reserved; 
		Uint16  TagSerialNumber; 
		Uint16 DescriptorCRC; 
		Uint16  DescriptorCRCLength; 
		Uint32 TagLocation; 
	};

	C_ASSERT(sizeof(Tag) == 16);


	struct extent_ad {
		Uint32		Length;
		Uint32		Location;
	};

	C_ASSERT(sizeof(extent_ad) == 8);

	struct long_ad  {
		unsigned Length;
		unsigned LBA;
		unsigned short PartitionNumber;
		unsigned char Reserved[6];
	};
	C_ASSERT(sizeof(long_ad) == 16);

	struct charspec { /* ECMA 167 1/7.2.1 */ 
		Uint8  CharacterSetType; 
		byte  CharacterSetInfo[63]; 
	};
	C_ASSERT(sizeof(charspec) == 64);


	struct EntityID { /* ECMA 167 1/7.4 */ 
		Uint8  Flags; 
		char  Identifier[23]; 
		char  IdentifierSuffix[8]; 
	};
	C_ASSERT(sizeof(EntityID) == 32);

	struct timestamp { /* ECMA 167 1/7.3 */ 
		Uint16  TypeAndTimezone; 
		Uint16 Year; 
		Uint8 Month; 
		Uint8 Day; 
		Uint8 Hour; 
		Uint8 Minute; 
		Uint8 Second; 
		Uint8 Centiseconds; 
		Uint8 HundredsofMicroseconds; 
		Uint8 Microseconds; 
	};
	C_ASSERT(sizeof(timestamp) == 12);


	struct AnchorVolumeDescriptorPointer { /* ECMA 167 3/10.2 */ 
		struct Tag DescriptorTag; 
		struct extent_ad  MainVolumeDescriptorSequenceExtent; 
		struct extent_ad  ReserveVolumeDescriptorSequenceExtent; 
		byte Reserved[480]; 
	};
	C_ASSERT(sizeof(AnchorVolumeDescriptorPointer) == 512);

	struct PrimaryVolumeDescriptor { /* ECMA 167 3/10.1 */ 
		struct Tag DescriptorTag; 
		Uint32 VolumeDescriptorSequenceNumber; 
		Uint32 PrimaryVolumeDescriptorNumber; 
		dstring VolumeIdentifier[32]; 
		Uint16 VolumeSequenceNumber; 
		Uint16 MaximumVolumeSequenceNumber; 
		Uint16  InterchangeLevel; 
		Uint16  MaximumInterchangeLevel; 
		Uint32  CharacterSetList; 
		Uint32  MaximumCharacterSetList; 
		dstring  VolumeSetIdentifier[128]; 
		struct charspec  DescriptorCharacterSet; 
		struct charspec  ExplanatoryCharacterSet; 
		struct extent_ad VolumeAbstract; 
		struct extent_ad VolumeCopyrightNotice; 
		struct EntityID  ApplicationIdentifier; 
		struct timestamp RecordingDateandTime; 
		struct EntityID  ImplementationIdentifier; 
		byte ImplementationUse[64]; 
		Uint32 PredecessorVolumeDescriptorSequenceLocation; 
		Uint16 Flags; 
		byte Reserved[22]; 
	};
	C_ASSERT(sizeof(PrimaryVolumeDescriptor) == 512);


	struct LogicalVolumeDescriptor { /* ECMA 167 3/10.6 */ 
		struct Tag DescriptorTag; 
		Uint32 VolumeDescriptorSequenceNumber; 
		struct charspec  DescriptorCharacterSet; 
		dstring LogicalVolumeIdentifier[128]; 
		Uint32  LogicalBlockSize;
		EntityID  DomainIdentifier; 
		union
		{
			byte  LogicalVolumeContentsUse[16]; 
			long_ad FileSetDescriptorSequence;
		};
		Uint32 MapTableLength; 
		Uint32 NumberofPartitionMaps; 
		EntityID  ImplementationIdentifier; 
		byte ImplementationUse[128]; 
		extent_ad  IntegritySequenceExtent;
#pragma warning( push )
#pragma warning( disable : 4200 )
		byte  PartitionMaps[]; 
#pragma warning( pop )
	};
	C_ASSERT(sizeof(LogicalVolumeDescriptor) == 440);


	struct PartitionDescriptor {    /* ECMA 167 3/10.5 */ 
		struct Tag  DescriptorTag; 
		Uint32   VolumeDescriptorSequenceNumber; 
		Uint16   PartitionFlags; 
		Uint16   PartitionNumber; 
		struct EntityID  PartitionContents; 
		byte    PartitionContentsUse[128]; 
		Uint32    AccessType; 
		Uint32    PartitionStartingLocation; 
		Uint32    PartitionLength; 
		struct EntityID   ImplementationIdentifier; 
		byte   ImplementationUse[128]; 
		byte   Reserved[156]; 
	};
	C_ASSERT(sizeof(PartitionDescriptor) == 512);

	namespace PartitionMapEntries
	{
		struct Type1
		{
			byte PartitionMapType;
			byte PartitionMapLength;
			Uint16 VolumeSequenceNumber;
			Uint16 PartitionNumber;
		};

		C_ASSERT(sizeof(Type1) == 6);

		struct Type2
		{
			byte PartitionMapType;
			byte PartitionMapLength;
			byte Reserved1[2];
			EntityID PartitionTypeIdentifier;
			Uint16 VolumeSequenceNumber;
			Uint16 PartitionNumber;
			
			union
			{
				struct
				{
					byte Reserved[24];
				} VirtualPartition;

				struct
				{
					Uint16 PacketLength;
					Uint8 NumberOfSparingTables;
					UINT8 Reserved2;
					Uint32 SizeOfEachSparingTable;
					Uint32 LocationsOfSparingTables[4];
				} SparingPartition;

				struct  
				{
					Uint32 MetadataFileLocation;
					Uint32 MetadataMirrorFileLocation;
					Uint32 MetadataBitmapFileLocation;
					Uint32 AllocationUnitSize;
					Uint16 AlignmentUnitSize;
					Uint8 Flags;
					byte Reserved[5];
				} MetadataPartition;

				byte Data[24];
			};
		};
		C_ASSERT(sizeof(Type2) == 64);
	}

	struct icbtag { /* ECMA 167 4/14.6 */ 
		Uint32 PriorRecordedNumberofDirectEntries; 
		Uint16  StrategyType; 
		byte StrategyParameter[2]; 
		Uint16 MaximumNumberofEntries; 
		byte Reserved; 
		Uint8  FileType; 
		char ParentICBLocation[6]; 
		Uint16  Flags; 
	} ;

	struct regid {
		byte flags;
		byte		ident[23];
		byte		identSuffix[8];
	};

	struct FileEntry { /* ECMA 167 4/14.9 */ 
		Tag DescriptorTag; 
		icbtag ICBTag; 
		Uint32 Uid; 
		Uint32 Gid; 
		Uint32 Permissions; 
		Uint16  FileLinkCount; 
		Uint8  RecordFormat; 
		Uint8  RecordDisplayAttributes; 
		Uint32  RecordLength; 
		Uint64  InformationLength; 
		Uint64  LogicalBlocksRecorded; 
		timestamp AccessTime; 
		timestamp ModificationTime; 
		timestamp AttributeTime; 
		Uint32 Checkpoint; 
		long_ad ExtendedAttributeICB; 
		EntityID  ImplementationIdentifier; 
		Uint64  UniqueID;
		Uint32 LengthofExtendedAttributes; 
		Uint32 LengthofAllocationDescriptors; 
	} ;
	C_ASSERT(sizeof(FileEntry) == 176);

	struct ExtendedFileEntry {
		Tag DescriptorTag; 
		icbtag ICBTag; 
		Uint32 Uid;
		Uint32 Gid;
		Uint32 Permissions;
		Uint16 FileLinkCount;
		Uint8 RecordFormat;
		Uint8 RecordDisplayAttr;
		Uint32 RecordLength;
		Uint64 InformationLength;
		Uint64 ObjectSize;
		Uint64 LogicalBlocksRecorded;
		timestamp AccessTime;
		timestamp ModificationTime;
		timestamp CeateTime;
		timestamp AttrTime;
		Uint32 Checkpoint;
		Uint32 Reserved;
		long_ad ExtendedAttrICB;
		long_ad StreamDirectoryICB;
		regid ImpIdent;
		Uint64 UniqueID;
		Uint32 LengthofExtendedAttributes;
		Uint32 LengthofAllocationDescriptors;
	} ;

	struct short_ad
	{
		Uint32 Length; 
		Uint32 Position;
	};
	C_ASSERT(sizeof(short_ad) == 8);

	struct FileSetDescriptor { /* ECMA 167 4/14.1 */ 
		struct Tag DescriptorTag; 
		struct timestamp RecordingDateandTime; 
		Uint16  InterchangeLevel; 
		Uint16  MaximumInterchangeLevel; 
		Uint32  CharacterSetList; 
		Uint32  MaximumCharacterSetList; 
		Uint32 FileSetNumber; 
		Uint32 FileSetDescriptorNumber; 
		struct charspec  LogicalVolumeIdentifierCharacterSet;
		dstring LogicalVolumeIdentifier[128]; 
		struct charspec  FileSetCharacterSet; 
		dstring FileSetIdentifier[32]; 
		dstring CopyrightFileIdentifier[32]; 
		dstring AbstractFileIdentifier[32]; 
		struct long_ad RootDirectoryICB; 
		struct EntityID  DomainIdentifier; 
		struct long_ad NextExtent; 
		struct long_ad SystemStreamDirectoryICB; 
		byte Reserved[32]; 
	};

	struct FileIdentifierDescriptor { /* ECMA 167 4/14.4 */ 
		struct Tag DescriptorTag; 
		Uint16  FileVersionNumber; 
		Uint8  FileCharacteristics; 
		Uint8 LengthofFileIdentifier; 
		struct long_ad  ICB; 
		Uint16  LengthOfImplementationUse; 
	};

	C_ASSERT(sizeof(FileIdentifierDescriptor) == 38);


#pragma pack(pop)
}