set(SRC
	lustre_lib.c
	lustre_editor.c
	lustre_lua.c
	lustre.h
	)

add_library(liblustre STATIC ${SRC})

