file(GLOB_RECURSE all_files *)

if(all_files)
	file(REMOVE ${all_files})
endif(all_files)

