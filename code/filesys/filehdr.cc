// filehdr.cc
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector,
//
//      Unlike in a real system, we do not keep track of file permissions,
//	ownership, last modification date, etc., in the file header.
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	   return FALSE;		// not enough space

    // 直接索引即可
    if (numSectors <= FirstDirect)
        for (int i = 0; i < numSectors; i++)
            dataSectors[i] = freeMap->Find();
    // 部分间接索引
    else {
        for (int i = 0; i < FirstDirect; i++)
            dataSectors[i] = freeMap->Find();
        //int secondNum = divRoundUp(numSectors - FirstDirect, SecondSize);
        int sectorContent[SecondSize];
        for (int i = 0, left = numSectors - FirstDirect; left > 0; i++, left -= SecondSize){
            dataSectors[FirstDirect + i] = freeMap->Find();
            memset(sectorContent, 0, sizeof sectorContent);
            for (int j = 0; j < (left < SecondSize ? left:SecondSize); j++)
                sectorContent[j] = freeMap->Find();
            synchDisk->WriteSector(dataSectors[FirstDirect + i], (char *)sectorContent);
        }
    }
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void
FileHeader::Deallocate(BitMap *freeMap)
{
    if (numSectors <= FirstDirect)
        for (int i = 0; i < numSectors; i++) {
            ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
            freeMap->Clear((int) dataSectors[i]);
        }
    else {
        for (int i = 0; i < FirstDirect; i++) {
            ASSERT(freeMap->Test((int) dataSectors[i]));
            freeMap->Clear((int) dataSectors[i]);
        }
        int sectorContent[SecondSize];
        for (int i = 0, left = numSectors - FirstDirect; left > 0; i++, left -= SecondSize) {
            ASSERT(freeMap->Test((int) dataSectors[FirstDirect + i]));
            synchDisk->ReadSector(dataSectors[FirstDirect + i], (char *)sectorContent);
            for (int j = 0; j < (left < SecondSize ? left:SecondSize); j++){
                ASSERT(freeMap->Test((int) sectorContent[j]));
                freeMap->Clear((int) sectorContent[j]);                
            }
            freeMap->Clear((int) dataSectors[FirstDirect + i]);
        }
    }
}

//----------------------------------------------------------------------
// FileHeader::Reallocate
//  Change the file size and reallocate for that
//
//  "freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Reallocate(BitMap *freeMap, int fileSize)
{
    int oldSize = numBytes, oldNumSectors = numSectors;

    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);

    if (fileSize > oldSize) {
        // no need to allocate more
        if (numSectors == oldNumSectors)
            return TRUE;
        // need more in first direct
        if (freeMap->NumClear() < numSectors - oldNumSectors){
            numBytes = oldSize;
            numSectors = oldNumSectors; 
            return FALSE;        // not enough space  
        }
        // 直接索引即可
        if (numSectors <= FirstDirect)
            for (int i = oldNumSectors; i < numSectors; i++)
                dataSectors[i] = freeMap->Find();
        // 部分间接索引
        else {
            for (int i = oldNumSectors; i < FirstDirect; i++)
                dataSectors[i] = freeMap->Find();
            int sectorContent[SecondSize];
            // 原来只用到直接索引
            if (oldNumSectors <= FirstDirect)
                for (int i = 0, left = numSectors - FirstDirect; left > 0; i++, left -= SecondSize){
                    dataSectors[FirstDirect + i] = freeMap->Find();
                    memset(sectorContent, 0, sizeof sectorContent);
                    for (int j = 0; j < (left < SecondSize ? left:SecondSize); j++)
                        sectorContent[j] = freeMap->Find();
                    synchDisk->WriteSector(dataSectors[FirstDirect + i], (char *)sectorContent);
                }                
            else {
                int secSector = oldNumSectors - FirstDirect - 1;
                //printf("dataSectors[%d]: %d\n", FirstDirect + secSector / SecondSize, dataSectors[FirstDirect + secSector / SecondSize]);
                // 原先的二级索引部分可用
                if ((secSector + 1) % SecondSize != 0){
                    synchDisk->ReadSector(dataSectors[FirstDirect + secSector / SecondSize], (char *)sectorContent);
                    for (int j = (secSector + 1) % SecondSize, left = numSectors - oldNumSectors; j < SecondSize && left > 0; j++, left--)
                        sectorContent[j] = freeMap->Find();
                    synchDisk->WriteSector(dataSectors[FirstDirect + secSector / SecondSize], (char *)sectorContent);
                }
                // 需要新的二级索引
                for (int i = 0, left = numSectors - FirstDirect - (secSector / SecondSize + 1) * SecondSize; left > 0; i++, left -= SecondSize){
                    dataSectors[FirstDirect + (secSector / SecondSize + 1) + i] = freeMap->Find();
                    memset(sectorContent, 0, sizeof sectorContent);
                    for (int j = 0; j < (left < SecondSize ? left:SecondSize); j++)
                        sectorContent[j] = freeMap->Find();
                    synchDisk->WriteSector(dataSectors[FirstDirect + (secSector / SecondSize + 1) + i], (char *)sectorContent);
                }
            }
        }
    }
    else {
        if (numSectors == oldNumSectors)
            return TRUE;
        if (oldNumSectors <= FirstDirect)
            for (int i = numSectors; i < oldNumSectors; i++) {
                ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
                freeMap->Clear((int) dataSectors[i]);
            }
        else {
            for (int i = numSectors; i < FirstDirect; i++) {
                ASSERT(freeMap->Test((int) dataSectors[i]));
                freeMap->Clear((int) dataSectors[i]);
            }
            int sectorContent[SecondSize];
            // 减小大小后只用到直接索引
            if (numSectors <= FirstDirect)
                for (int i = 0, left = oldNumSectors - FirstDirect; left > 0; i++, left -= SecondSize) {
                    ASSERT(freeMap->Test((int) dataSectors[FirstDirect + i]));
                    synchDisk->ReadSector(dataSectors[FirstDirect + i], (char *)sectorContent);
                    for (int j = 0; j < (left < SecondSize ? left:SecondSize); j++){
                        ASSERT(freeMap->Test((int) sectorContent[j]));
                        freeMap->Clear((int) sectorContent[j]);                
                    }
                    freeMap->Clear((int) dataSectors[FirstDirect + i]);
                }
            // 减小大小后仍要用到二级索引
            else
                for (int i = 0, left = oldNumSectors - numSectors; left > 0; i++, left -= SecondSize) {
                    ASSERT(freeMap->Test((int) dataSectors[numSectors + i]));
                    synchDisk->ReadSector(dataSectors[numSectors + i], (char *)sectorContent);
                    for (int j = 0; j < (left < SecondSize ? left:SecondSize); j++){
                        ASSERT(freeMap->Test((int) sectorContent[j]));
                        freeMap->Clear((int) sectorContent[j]);                
                    }
                    freeMap->Clear((int) dataSectors[numSectors + i]);
                }
        }        
    }
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk.
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk.
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    int index = offset / SectorSize;
    if (index < FirstDirect)
        return dataSectors[index];
    int secOffset = index - FirstDirect;
    int sectorContent[SecondSize];
    //printf("offset: %d, dataSectors[%d], secOffset[%d]\n", offset, FirstDirect + secOffset / SecondSize, secOffset % SecondSize);
    synchDisk->ReadSector(dataSectors[FirstDirect + secOffset / SecondSize], (char *)sectorContent);
    return sectorContent[secOffset % SecondSize];
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.\n", numBytes);
    printf("File created at:\t%s\n", createTime);
    printf("File last opened at:\t%s\n", openTime);
    printf("File last modified at:\t%s\n", modifyTime);
    printf("File blocks:\n");
    for (i = 0; i < numSectors; i++)
        printf("%d ", ByteToSector(i * SectorSize));
        printf("\nFile contents:\n");
        for (i = k = 0; i < numSectors; i++) {
            synchDisk->ReadSector(ByteToSector(i * SectorSize), data);
            for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
        printf("\n");
    }
    delete [] data;
}

void
FileHeader::setCreateTime(){
    setTime(createTime);
    printf("created: %s\n", createTime);
}

void
FileHeader::setOpenTime(){
    setTime(openTime);
}

void
FileHeader::setModifyTime(){
    setTime(modifyTime);
}

void
FileHeader::setTime(char *target)
{
    time_t t;
    time(&t);
    strncpy(target, ctime(&t), DATETIMELEN);
    target[DATETIMELEN] = 0;
}
