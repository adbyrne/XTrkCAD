#
# handle translation using gettext
#
project(i18n)

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
	COMMAND stripmsg ${xtc_custom_msg_files} > ${CMAKE_CURRENT_BINARY_DIR}/custmsg.h
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
set(SOURCEFILES 
	"${xtrkcad-lib_SOURCE_DIR}/misc.c"
	"${wlib_SOURCE_DIR}/mswlib/mswmisc.c"
	"${messagefile_BINARY_DIR}/messages.h"
	"${CMAKE_CURRENT_BINARY_DIR}/custmsg.h"
	"${XTrkCAD_BINARY_DIR}/app/bin/bllnhlp.c"
)

foreach( file ${SOURCEFILES})
	cmake_path(RELATIVE_PATH 
		file 
		BASE_DIRECTORY ${i18n_SOURCE_DIR}
	)
	list(APPEND xgt_source_files ${file})
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
	DEPENDS custmsg
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

	file(MAKE_DIRECTORY ${mofile_dir})

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
		${i18n_BINARY_DIR}/${localeid}/LC_MESSAGES/xtrkcad.mo
		PARENT_SCOPE
   	)	
endfunction()

#
# Loop through the list of the supported locales

foreach(localeid ${xtrkcad_locale_ids})
   updatepofile(${localeid})   
endforeach(localeid)


# xTrackCAD must be built before this to
# make sure that the dynamically generated files messages.h and bllnhlp.c are
# up to date.
add_custom_target(messages 
	DEPENDS xtrkcad ${potfile}
)

set_target_properties(
   messages
   PROPERTIES FOLDER i18n
)

# Utility target for building all translations 
add_custom_target(alltranslations ALL
	DEPENDS ${xtc_locale_targets}
)

