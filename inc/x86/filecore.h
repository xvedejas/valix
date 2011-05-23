/*  Copyright (C) 2010 Xander Vedejas <xvedejas@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Maintained by:
 *      Xander Vedėjas <xvedejas@gmail.com>
 */
#ifndef __filecore_h__
#define __filecore_h__
#include <main.h>
#include <data.h>

//Set *allTags; // Tag*

//typedef enum
//{
    //local,
    //remote,
    //device
//} FileType;

//typedef struct
//{
    //String name;
    //Set *tags;       // Tag*
    //Map *attributes; // string -> string
//} File;

//typedef struct
//{
    //String name;
    //Set *supertags; // Tag*
    //Map *files;     // string -> File*
//} Tag;

//void fileCoreInstall()
//{
    //allTags = genericSetNewSize(128);       /* replace 128 with a big number */
//}

//File *fileNew(String name)
//{
    //File *file = malloc(sizeof(File));
    //file->name = name;
    //file->tags = setNew();
    //return file;
//}

//Tag *tagNew(String name)
//{
    //Tag *tag = malloc(sizeof(Tag));
    //tag->name = name;
    //tag->supertags = setNew();
    //tag->files = setNew();
    //setAdd(allTags, tag);
    //return tag;
//}

#endif
