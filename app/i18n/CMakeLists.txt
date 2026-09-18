#
# handle translation using gettext
#
project(i18n C) 

# List of the supported locales.
set(xtrkcad_locale_ids
	cy_GB
	de_DE
	fi
	fr_FR
	pt_BR
	ru
	)

# Misc variables
set(xtc_locale_targets)
set(potfile "${i18n_BINARY_DIR}/${XTRKCAD_PACKAGE}.pot")
set(xgtkeywords
	--keyword=_
	--keyword=p_
	--keyword=N_
)

#
# Find programs, registry is used on Windows to find the installation path

find_program(xtc_xgettext xgettext PATHS [HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\;InstallPath]/bin )
find_program(xtc_msginit msginit PATHS [HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\;InstallPath]/bin )
find_program(xtc_msgmerge msgmerge PATHS [HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\;InstallPath]/bin )
find_program(xtc_msgfmt msgfmt PATHS [HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\;InstallPath]/bin )
set(xtc_translator "" CACHE FILEPATH "Translator program for editing the .po files.")
mark_as_advanced(xtc_xgettext xtc_msginit xtc_msgmerge
                 xtc_msgfmt xtc_translator)

# 
# Extract the translatable strings from text files
#
# Custom string extractor utility
add_executable(stripmsg stripmsg.c)

# Create custom message files list
file(GLOB_RECURSE xtc_custom_msg_files
	${XTrkCAD_SOURCE_DIR}/*.xtq
	${XTrkCAD_SOURCE_DIR}/*.xtr
	${XTrkCAD_SOURCE_DIR}/*.tip)

# Extract translatable strings from custom message files to one temporary file
add_custom_target(
	custmsg
	COMMAND ${CMAKE_COMMAND}
		-DEXE=$<TARGET_FILE:stripmsg>
		-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/custmsg.h
		-P ${CMAKE_CURRENT_SOURCE_DIR}/run_stripmsg.cmake
		${xtc_custom_msg_files}
	DEPENDS stripmsg ${xtc_custom_msg_files}
)

set_target_properties(
   stripmsg custmsg
   PROPERTIES FOLDER i18n
)

#
# Create the list of the original source files.

get_target_property(sourcefiles xtrkcad-lib SOURCES)
foreach( file ${sourcefiles})
	#
	# do not scan dependent objects / libraries for translatable strings
	string(FIND "${file}" "TARGET_OBJECTS:" result)
	if(result EQUAL -1)
	#
	# bllnhlp is generated in the build directory and has to be special cased
		string(COMPARE NOTEQUAL ${file} "bllnhlp.c" RESULT)
		if(RESULT)
			cmake_path(ABSOLUTE_PATH file BASE_DIRECTORY ${xtrkcad-lib_SOURCE_DIR} )
			cmake_path(RELATIVE_PATH file BASE_DIRECTORY ${i18n_SOURCE_DIR})
			list(APPEND xgt_source_files ${file})
		endif()
	endif()
endforeach()

#
# add file with translatable strings
#
# misc.c and tooltiptext.c are sources of the executable target, not of
# xtrkcad-lib, so the loop above misses them. tooltiptext.c (the generated
# balloon-help table, full of N_() strings) was called bllnhlp.c before the
# Halibut->Doxygen help conversion; the old name here referred to a file
# that is never generated any more, which aborted the whole .pot build.
set(SOURCEFILES
	"${xtrkcad-lib_SOURCE_DIR}/misc.c"
	"${messagefile_BINARY_DIR}/messages.h"
	"${CMAKE_CURRENT_BINARY_DIR}/custmsg.h"
	"${messagefile_BINARY_DIR}/tooltiptext.c"
)

foreach( file ${SOURCEFILES})
	cmake_path(RELATIVE_PATH
		file
		BASE_DIRECTORY ${i18n_SOURCE_DIR}
	)
	list(APPEND xgt_source_files ${file})
endforeach()

#
# Add the GtkBuilder .ui files. xgettext recognises them as GtkBuilder XML
# by extension and extracts every translatable="yes" property, emitting a
# "#: <file>:<line>" source reference for each so translators can see which
# dialog a string belongs to. The list is taken straight from the wlib
# gresource manifest, so it always matches the .ui files actually built
# into the program - add one there and it is picked up here, with no second
# list to keep in sync (this also skips orphaned .ui files like about.ui /
# linkedit.ui that are not shipped).
file(READ
	"${XTrkCAD_SOURCE_DIR}/app/wlib/gtk3lib/ui/wlib.gresource.xml.in"
	ui_gresource_manifest)
string(REGEX MATCHALL "<file[^>]*>[^<]+\\.ui</file>"
	ui_gresource_entries "${ui_gresource_manifest}")
set(ui_source_files)
foreach(entry ${ui_gresource_entries})
	string(REGEX REPLACE "^<file[^>]*>([^<]+)</file>$" "\\1" ui_name "${entry}")
	set(ui_abs "${XTrkCAD_SOURCE_DIR}/app/wlib/gtk3lib/ui/${ui_name}")
	list(APPEND ui_source_files "${ui_abs}")
	cmake_path(RELATIVE_PATH ui_abs
		BASE_DIRECTORY ${i18n_SOURCE_DIR}
		OUTPUT_VARIABLE ui_rel)
	list(APPEND xgt_source_files "${ui_rel}")
endforeach()

# Extract strings and create xtrkcad.pot
add_custom_command(
	OUTPUT ${potfile}
	COMMAND ${xtc_xgettext}
			${xgtkeywords}
			--add-comments=i18n
			-d ${GETTEXT_PACKAGE}
			-F
			-o ${potfile}
			--from-code=ISO-8859-15 ${xgt_source_files}
	DEPENDS custmsg msgfiles tooltips ${ui_source_files}
	WORKING_DIRECTORY ${i18n_SOURCE_DIR}
)


#
# function to create and update the po-file for a locale

function(updatepofile locale)
	set(pofile ${i18n_SOURCE_DIR}/${locale}.po)
	set(mofile_dir ${i18n_BINARY_DIR}/${locale}/LC_MESSAGES)
	set(mofile ${mofile_dir}/${XTRKCAD_PACKAGE}.mo)

	# Merge .pot file changes to .po file
	add_custom_target(msgmerge-${locale} 
		COMMAND ${xtc_msgmerge} -U --backup=none ${pofile} ${potfile}
    	COMMAND ${xtc_msgfmt} -c -v -o ${mofile} ${pofile}
		DEPENDS messages ${pofile} 
	)

	# If new locale id was added, add .po file creation routine
	if(NOT EXISTS ${pofile})
		add_custom_command(
			OUTPUT ${pofile}
			COMMAND ${xtc_msginit} -l ${locale} -o ${pofile} -i ${potfile}
			DEPENDS ${potfile}
			)
	endif()

	file(MAKE_DIRECTORY ${mofile_dir} )
	# If translator program was given, add target to edit .po files with it
	if(xtc_translator)
		add_custom_target(msgtranslate-${locale}
			COMMAND ${xtc_translator} ${pofile}
		)
	endif()
	
	# Compile .po file to binary format .mo file
	add_custom_command(
		OUTPUT ${mofile}
		COMMAND ${xtc_msgfmt} -c -v -o ${mofile} ${pofile}
		DEPENDS ${pofile}
	)

	# Install all .mo files
	install(
		FILES ${mofile}
		DESTINATION ${XTRKCAD_LOCALE_INSTALL_DIR}/${locale}/LC_MESSAGES
		)

    set_target_properties(
	    msgmerge-${locale} 
 	    PROPERTIES FOLDER i18n
    )

    set(xtc_locale_targets
		${xtc_locale_targets}
		${mofile}
		PARENT_SCOPE
   	)
endfunction()

#
# Loop through the list of the supported locales

foreach(localeid ${xtrkcad_locale_ids})
   updatepofile(${localeid})   
endforeach(localeid)


# Regenerate xtrkcad.pot. The .pot command above depends on custmsg (the
# .xtq/.xtr/.tip extraction) and on the msgfiles / tooltips targets that
# generate messages.h and tooltiptext.c, so the generated inputs are
# current -- this used to say "DEPENDS xtrkcad", a target name that only
# exists when XTRKCAD_BIN is the bare string "xtrkcad" (unversioned flatpak
# builds); in every other configuration it was an unbuildable file
# dependency that broke `ninja messages` and `ninja msgmerge-<locale>`.
add_custom_target(messages
	DEPENDS ${potfile}
)

set_target_properties(
   messages
   PROPERTIES FOLDER i18n
)

# Utility target for building all translations 
add_custom_target(alltranslations ALL
	DEPENDS ${xtc_locale_targets}
)

