set(SRC
	lustre_lib.c
	lustre_editor.c
	lustre_lua.c
	lustre.h
	)

GET_PROPERTY(_LUSTRE_LIB GLOBAL PROPERTY LUSTRE_LIB)

if( NOT DEFINED _LUSTRE_LIB)
	SET_PROPERTY(GLOBAL PROPERTY LUSTRE_LIB "not set")
	set(_LUSTRE_LIB "not set")
endif() 


if( NOT ${_LUSTRE_LIB} STREQUAL "set")
	SET_PROPERTY(GLOBAL PROPERTY LUSTRE_LIB "set")

add_library(liblustre STATIC ${SRC})

endif() # GUARD
