#
# handle translation using gettext
#
project(i18n)

# List of the supported locales.
set(XTRKCAD_LOCALE_IDS
	cy_GB
	de_DE
	fi
	fr_FR
	pt_BR
	ru
	)

# Misc variables
set(XTC_LOCALE_TARGETS )
set(POTFILE "${i18n_BINARY_DIR}/${XTRKCAD_PACKAGE}.pot")
set(XGTKEYWORDS
	--keyword=_
	--keyword=p_
	--keyword=N_
)

#
# Find programs, registry is used on Windows to find the installation path

find_program(XTC_XGETTEXT xgettext PATHS [HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\;InstallPath]/bin )
find_program(XTC_MSGINIT msginit PATHS [HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\;InstallPath]/bin )
find_program(XTC_MSGMERGE msgmerge PATHS [HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\;InstallPath]/bin )
find_program(XTC_MSGFMT msgfmt PATHS [HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\;InstallPath]/bin )
set(XTC_TRANSLATOR "" CACHE FILEPATH "Translator program for editing the .po files.")
mark_as_advanced(XTC_XGETTEXT XTC_MSGINIT XTC_MSGMERGE
                 XTC_MSGFMT XTC_TRANSLATOR)

# 
# Extract the translatable strings from text files
#
# Custom string extractor utility
add_executable(stripmsg stripmsg.c)

set_target_properties(
   stripmsg
   PROPERTIES FOLDER i18n
)

# Create custom message files list
file(GLOB_RECURSE XTC_CUSTOM_MSG_FILES
	${XTrkCAD_SOURCE_DIR}/*.xtq
	${XTrkCAD_SOURCE_DIR}/*.xtr
	${XTrkCAD_SOURCE_DIR}/*.tip)

# Extract translatable strings from custom message files to one temporary file
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/custmsg.h
	COMMAND stripmsg ${XTC_CUSTOM_MSG_FILES} > ${CMAKE_CURRENT_BINARY_DIR}/custmsg.h
	DEPENDS stripmsg ${XTC_CUSTOM_MSG_FILES}
	)

#
# Create the list of the original source files.

get_target_property(SOURCEFILES xtrkcad-lib SOURCES)
foreach( sourcefile ${SOURCEFILES})
	#
	# do not scan dependent objects / libraries for translatable strings
	string(FIND "${sourcefile}" "TARGET_OBJECTS:" result)
	if(result EQUAL -1)
	#
	# bllnhlp is generated in the build directory and has to be special cased
		string(COMPARE NOTEQUAL ${sourcefile} "bllnhlp.c" RESULT)
		if(RESULT)
			cmake_path(ABSOLUTE_PATH sourcefile BASE_DIRECTORY ${xtrkcad-lib_SOURCE_DIR} )
			cmake_path(RELATIVE_PATH sourcefile BASE_DIRECTORY ${i18n_SOURCE_DIR})
			list(APPEND XGT_SOURCE_FILES ${sourcefile})
		endif()
	endif()
endforeach()

#
# add file with translatable strings 
set(SOURCEFILES 
	"${xtrkcad-lib_SOURCE_DIR}/misc.c"
	"${wlib_SOURCE_DIR}/mswlib/mswmisc.c"
	"${help_BINARY_DIR}/messages.h"
	"${CMAKE_CURRENT_BINARY_DIR}/custmsg.h"
	"${XTrkCAD_BINARY_DIR}/app/bin/bllnhlp.c"
)

foreach( FILENAME ${SOURCEFILES})
	cmake_path(RELATIVE_PATH 
		FILENAME 
		BASE_DIRECTORY ${i18n_SOURCE_DIR}
	)
	list(APPEND XGT_SOURCE_FILES ${FILENAME})
endforeach()

# Extract strings and create xtrkcad.pot
add_custom_command(
	OUTPUT ${POTFILE}
	COMMAND ${XTC_XGETTEXT} 
			${XGTKEYWORDS} 
			--add-comments=i18n 
			-d ${GETTEXT_PACKAGE} 
			-F 
			-o ${POTFILE} 
			--from-code=ISO-8859-15 ${XGT_SOURCE_FILES}
	DEPENDS xtrkcad ${CMAKE_CURRENT_BINARY_DIR}/custmsg.h
	WORKING_DIRECTORY ${i18n_SOURCE_DIR}
)

# Target to be executed with "make all". Xtrkcad must be built before this to
# make sure that the dynamically generated files messages.h and bllnhlp.c are
# up to date.
add_custom_target(messages ALL
	DEPENDS xtrkcad ${POTFILE}
)

set_target_properties(
   messages
   PROPERTIES FOLDER i18n
)

#
# function to create and update the po-file for a locale

function(updatepofile locale)
	set(pofile ${i18n_SOURCE_DIR}/${locale}.po)
	set(mofile_dir ${i18n_BINARY_DIR}/${locale}/LC_MESSAGES)
	set(mofile ${mofile_dir}/${XTRKCAD_PACKAGE}.mo)

	# Merge .pot file changes to .po file
	add_custom_target(msgmerge-${locale}
		COMMAND ${XTC_MSGMERGE} -U --backup=none ${pofile} ${POTFILE}
      COMMAND ${XTC_MSGFMT} -c -v -o ${mofile} ${pofile}
		DEPENDS ${POTFILE} ${pofile}
		)

	# If new locale id was added, add .po file creation routine
	if(NOT EXISTS ${pofile})
		add_custom_command(
			OUTPUT ${pofile}
			COMMAND ${XTC_MSGINIT} -l ${locale} -o ${pofile} -i ${POTFILE}
			DEPENDS ${POTFILE}
			)
	endif()

	file(MAKE_DIRECTORY ${mofile_dir})

	# If translator program was given, add target to edit .po files with it
	if(xtc_translator)
		add_custom_target(msgtranslate-${locale}
			COMMAND ${XTC_TRANSLATOR} ${pofile}
		)
	endif()

	# Install all .mo files
	install(
		FILES ${mofile}
		DESTINATION ${XTRKCAD_LOCALE_INSTALL_DIR}/${locale}/LC_MESSAGES
		)

   set_target_properties(
	   msgmerge-${locale} 
 	   PROPERTIES FOLDER i18n
   )
endfunction()

#
# Loop through the list of the supported locales

foreach(localeid ${XTRKCAD_LOCALE_IDS})
   updatepofile(${localeid})   
endforeach(localeid)
