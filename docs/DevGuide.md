# **XTrkCad Developer's Guide**

**Table of Contents**

- [Introduction](#Introduction)

- [Source Files](#SourceFiles)

    - [Naming Convention](#SourceNaming)

    - [Source Layout](#SourceLayout)

- [Text Files](#TextFiles)

- [Parameter File Maintenance](#ParamFile)

    - [Independence of Parameters in Layout files](#Independence)

    - [CONTENTS Label](#Contents)

    - [Adding or Deleting Parameter files](#AddDelete)

    - [Changing CONTENTS Label and/or FileName](#ChangeContents)

- [WLib Interaction](#WlibInteraction)

- [Pretty Printing](#PrettyPrint)

- [Casts](#Casts)

    - [Downcast Numeric values](#Downcast)

    - [Generic Values](#Generic)

    - [Wlib wControl\_t conversion](#wControl-t)

- [GetTrkExtraData](#GetTrkExtraData)

- [Error Handling](#ErrorHandling)

    - [User Input Errors](#UserErrors)

    - [File Input Errors](#FileErrors)

    - [Logic errors](#LogicErrors)

    - [System/Evironmental errors](#SystemErrors)

- [Undo Processing](#UndoProcess)

- [Draw Model](#DrawModel)

- [Headers](#Headers)

    - [\#ifdef WINDOWS](#IfdefWindows)

- [VERSION handling](#VersionHandle)



<a name='Introduction' />

# Introduction

XTrkCad is a large program and like most large program, it has developed an infrastructure of how things are done and a base level of functionality. Some are general to any program (Error Handling, Source Formatting) and some particular to XtrkCad (Undo Processing, Parameter Files).

This document describes a fairly random set of features. It is expected to evolve as needs be.

If you figure out some interesting, tricky or obscure things, please add them to this document.

You'll see a number of place holders (pages with just a header). These will be addressed in the future.


<a name='SourceFiles' />

# Source Files



<a name='SourceNaming' />

## Naming Convention

Source files use the following naming convention, somewhat loosely followed as not all of these are appropriate for all objects:
- c\<object\>.c: This would be a class file in c++. It contains the code for creating, manipulating and deleting the object.
- d\<object\>.c: This code handles the dialog(s) for creating, changing and deleting the object. 
- t\<object\h.c: This code handle the drawing of the object. 
- c\<object\>.h: Definitions for the methods of the object that need to be visible outside the object class.

For example, there are files ccruve.c, ccurve.h and tcurve.c. There is no dialog for curved track. 


<a name='SourceLayout' />

## Source Layout


<a name='TextFiles' />

# Text Files

All text files (Params, Demos, Layout, etc.) use the UTF-8 character set and Unix-style line terminators (Single New Line, no Carriage Return).


<a name='ParamFile' />

# Parameter File Maintenance

<a name='Independence' />

## Independence of Parameters in Layout files

When a Turnout or Structure is place on the layout, all of its information is copied to layout file. There is no dependency on the Parameter file.

If a change is made to Parameter file, such as adding art-work to a Turnout d0efinition, the existing copies of that definition can be updated by the _Manage/Update Turnouts and Structures_ command.

<a name='Contents' />

## CONTENTS Label

Each Parameter file must have a least one CONTENTS line. The first is used as the CONTENTS Label. Any others as used as described below.

Parameter files are distinquished by their CONTENTS Label. A map from CONTENTS Label to file name is maintained. This map is implemented by Preferences entries:

    Parameter File Map.Kato Unitrack HO Scale: ../params/HO-Kato.xtp

If the current XTrkCad library directory has changed, then these entries are updated to the new location.

These entries accumulate, they are never purged. But unreferenced CONTENTS Labels are ignoreed.

The active Parameter file list is implemented by Preferences entries:

    Parameter File Names.File1: Kato Unitrack HO Scale

    Parameter File Names.File2:

The list is terminated by an empty value

<a name='AddDelete' />

## Adding / Deleting Parameter files

A new Parameter file is added to the _parms/_ directory and is available when the user accesses the Parameter library.

A deleted Paramter is removed from the params/ directory. If the user had loaded that Parameter file, then an error will be raised saying the Parameter file can not be found and will be moved from the Parameter file list.

<a name='ChangeContents' />

## Changing CONTENTS Label and/or FileName

Occansionally you may need to rename a parameter file or change its CONTENTS Label. This can lead to missing file errors.

For example, we have a paramter file **XYZ.xtp** with CONTENTS Label

    CONTENTS XYZ Parameters

We want to rename this file to **HO-XYZ.xtp** and change its CONTENTS Label

    CONTENTS HO XYZ Parameters

You need to do 2 things when you submit an updated file;

1. Add the old CONTENTS line to the new param file (HO-XYZ.xtp). Note: the first line is the new Contents Label

    CONTENTS HO XYZ Parameters

    CONTENTS XYZ Parameters

2. Update xtrkcad.upd with the current date and append the name of the new param file

    ...
    20220108
    ...

    HO-XYZ.xtp

If you are just updating the Contents Label, you only need to do step 1

If you are just changing the file name, you only need to do step 2.


<a name='WlibInteraction' />

# WLib Interaction

**Wlib** packages the interface between the core XTrkCad code and the underlying windowing tool kit (MS-Windows or GTK2). There is currently work to migrate to GTK3, which will be the same toolkiit for all platforms.

1. WLib calls mostly create and interact with a variety window objects (wButton, wString, wDraw, collectively known as wControl). Many of these objects notify the core code when certain events occur (button push, text entry,...) via Callback function pointers specified when the object is created.
2. The Callback also is passed a Context value which was specified at creation. This Context value is a void **\*** value, but sometimes the creator of the object needs a integral valiue. A pair of defines **I2VP()** and **VP2I()** handle the conversion of integral value to the void \* Context. Typicall I2VP() is used when creating window object and VP2L in callbacks.
3. Sometimes we use a **void \* context** in other places as a generic value(eg paramData\_t) and use L2VP()/VP2L() pairs to access integral values.


<a name='PrettyPrint' />

# Pretty Printing

Code may be formatted (pretty-printed) using the [astyle](http://astyle.sourceforge.net/) tool with the config file _astylerc_ or _AStyle.cfg_ found in _$SRCDIR/app/lib_.

If a file is pretty-printed, that must be to only change made for that commit. Do not mix non-formatting changes with pretty-printing.

<a name='Casts' />


# Casts

Casts are generally to be avoided. There is no need to cast to or from **void\***. There are a few situations where casts are required:

<a name='Downcast' />

## Downcast Numeric values

Down casting a wide numeric to a narrower numeric (including floating point to integral)

<a name='Generic' />

## Generic Values

Casting a generic call-back value from or to a (void\*) using **I2VP()** and **VP2L**

<a name='wControl-t' />

## Wlib wControl_t conversion

Converting a wlib callback.

<a name='GetTrkExtraData' />

## GetTrkExtraData


<a name='ErrorHandling' />

# Error Handling

  There are four types of errors:


<a name='UserErrors' />

## 1. User Input Errors

This includes invalid entries on dialogs, invalid object selection / manipulation.

The user is directed to correct the error or reset to a valid state. Invalid dialog values are highlighted and balloon help identifies the problem (typically value out of range),

Otherwise ErrorMessage() can explain the problem. If it is complex, NoticeWindow() can provide details. The user should be returned to a state where they can try again. The message should explain, in user terms, what the problem is and it's fix. &quot;Ptr == NULL&quot; is not a useful User Input error.

Most messages are in _help/messages.in_. These entries provide a user centric description of the problem and its resolution. The errors are added to the 'Help/Recent Messages' list which can direct the user to additional help. If your error does not fit this model (user-centric description and resolution) then you should consider alternatives: InfoMessage(), Logic Error or LOG().

<a name='FileErrors' />

## 2. File Input Errors

These could be corrupt files or new files with unsupported features

Generally we try to recover by skipping past the problem after a error message. InputError() is called which offers the option of terminating processing the file.

Layout files are versioned so new feature that are not backwards compatible will be caught. If the format of the file has been changed and is not backwards comatible, the file VERSION number has to be updated (See **VERSION handling** )

The user is not expected to understand these errors. The corrupt files must be fixed.

<a name='LogicErrors' />

## 3. Logic errors

Null pointer checks, array index range, parameter assertions, invalid case/default, 'Can not happen' conditions, unable to recover.

_ASSERT(cond)_ is called which, if cond is false, displays an Error Message (wNoticeEx), with the condition and location. _ASSERTEX(cond, (mesg) )_ includes (mesg) in the report, if extra information is required After the user has the chance of saving their layout, XTrkCad aborts.

The user is not expected to understand this error, but report it to the developers. In most cases the condition and location is sufficient.

Most likely, these errors indicate a bug in the code.

<a name='SystemErrors' />

## 4. System/Evironmental errors

Typically, Out-Of-Memory. We exit ASAP.

<a name='UndoProcess' />

# Undo Processing

<a name='DrawModel' />

# Draw Model

<a name='Headers' />

# Headers

<a name='IfdefWindows' />

## #ifdef WINDOWS

<a name='VersionHandle' />

# VERSION handling
