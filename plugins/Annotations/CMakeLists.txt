#_______________________Pmake___________________________________________________
#
#	Makefile : 	Annotations plugin
# 	Jan 2012	Nageswara
#_______________________________________________________________________________

SET( OD_MODULE_DEPS uiODMain)
SET( OD_IS_PLUGIN yes)
SET( OD_USECOIN 1)
SET( OD_MODULE_SOURCES
	annotationspi.cc
	treeitem.cc
	uiarrowdlg.cc
	uiscalebaritem.cc
	visannotimage.cc
	visarrow.cc
	viscallout.cc
	visscalebar.cc )
SET( OD_PLUGIN_ALO_EXEC ${OD_MAIN_EXEC} )
OD_INIT_MODULE()
