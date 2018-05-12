// directory.cc 
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//	Fixing this is one of the parts to the assignment.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory(int size)
{
    table = new DirectoryEntry[size];
    tableSize = size;
    for (int i = 0; i < tableSize; i++)
	table[i].inUse = FALSE;
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{ 
    delete [] table;
} 

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void
Directory::FetchFrom(OpenFile *file)
{
    (void) file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void
Directory::WriteBack(OpenFile *file)
{
    (void) file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::FindIndex(char *name)
{
    int pos = -1;
    for (int i = strlen(name) - 1; i >= 0; i--)
        if(name[i] == '/'){
            pos = i;
            break;
        }
    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse && !strncmp(table[i].name, name + pos + 1, FileNameMaxLen))
	    return i;
    return -1;		// name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't 
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::Find(char *name)
{
    int i = FindIndex(name);

    if (i != -1)
	return table[i].sector;
    return -1;
}

char
Directory::FindType(char *name)
{
    int i = FindIndex(name);
    if (i != -1)
        return table[i].type;
    return -1;
}
//----------------------------------------------------------------------
// Directory::FindDir
//  Look up directory name in directory, and return the disk sector number
//  where the directory's header is stored. Return -1 if the name isn't 
//  in the directory.
//
//  "name" -- the file name with its directory to look up
//----------------------------------------------------------------------
int
Directory::FindDir(char *name)
{
    int pos = -1, sector, len = strlen(name);
    char *fileName, *dirName;

    fileName = new char[len + 1];
    strncpy(fileName, name, len);
    fileName[len] = 0;
    //the file name would be like DirA/DirB/file, DirA is one of directories in root
    for (int i = len - 1; i >= 0; i--)
        if(fileName[i] == '/') {
            pos = i;
            break;
        }
    // in the root
    if (pos == -1)
        return 1;   //Directory Sector
    fileName[pos] = 0;
    pos = -1;
    for (int i = 0; i < strlen(fileName); i++)
        if(fileName[i] == '/') {
            pos = i;
            break;
        }
    if(pos == -1){
        int i = FindIndex(fileName);
        if (i != -1 && table[i].type == TYPE_DIR)
            return table[i].sector;
        return -1;
    }

    dirName = new char[pos + 1];
    strncpy(dirName, fileName, pos);
    dirName[pos] = 0;

    sector = FindIndex(dirName);
    if(sector == -1 || table[sector].type != TYPE_DIR)
        return -1;
    sector = table[sector].sector;

    Directory *directory = new Directory(tableSize);
    directory->FetchFrom(new OpenFile(sector));
    return directory->FindDir(name + pos + 1);
}
//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------

bool
Directory::Add(char *name, int newSector, char fileType)
{ 
    if (FindIndex(name) != -1)
        return FALSE;
    int pos;
    for (int i = 0; i < tableSize; i++)
        if (!table[i].inUse) {
            table[i].inUse = TRUE;
            printf("add to dir: %s\n", name);
            strncpy(table[i].path, name, FileNameMaxLen << 2);
            pos = -1;
            for (int j = strlen(name) - 1; j >= 0; j--)
                if(name[j] == '/'){
                    pos = j;
                    break;
                }
            strncpy(table[i].name, name + pos + 1, FileNameMaxLen);
            printf("filename: %s\n", table[i].name);
            table[i].sector = newSector;
            table[i].type = fileType;
            return TRUE;
        }
    return FALSE;	// no space.  Fix when we have extensible files.
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory. 
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool
Directory::Remove(char *name)
{ 
    int i = FindIndex(name);

    if (i == -1)
	return FALSE; 		// name not in directory
    table[i].inUse = FALSE;
    return TRUE;	
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory. 
//----------------------------------------------------------------------

void
Directory::List()
{
   for (int i = 0; i < tableSize; i++)
	if (table[i].inUse)
	    printf("%s\n", table[i].name);
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

void
Directory::Print()
{ 
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (int i = 0; i < tableSize; i++)
	if (table[i].inUse) {
	    printf("Name: %s, Sector: %d, Type: %d\n", table[i].name, table[i].sector, table[i].type);
	    hdr->FetchFrom(table[i].sector);
        if (table[i].type == TYPE_FILE)
    	    hdr->Print();
        else if (table[i].type == TYPE_DIR){
            Directory *childDir = new Directory(tableSize);
            childDir->FetchFrom(new OpenFile(table[i].sector));
            childDir->Print();
            delete childDir;
        }
	}
    printf("\n");
    delete hdr;
}

void
Directory::GetNames(char **fileNames, int &fileNum)
{
    if (fileNames)
        delete fileNames;
    fileNum = 0;
    for (int i = 0; i < tableSize; i++)
        if(table[i].inUse)
            fileNum++;
    fileNames = new char *[fileNum];
    for (int i = 0, j = 0; i < tableSize; i++)
        if(table[i].inUse){
            fileNames[j] = new char[FileNameMaxLen + 1];
            strncpy(fileNames[j++], table[i].name, FileNameMaxLen);
        }
}