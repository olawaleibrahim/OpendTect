#_______________________Pmake___________________________________________________
#
#	Makefile : 	uiGrav plugin
# 	March 2012	Nageswara
#_______________________________________________________________________________

SET(OD_MODULE_DEPS uiODMain)
SET(OD_USECOIN 1)
SET(OD_IS_PLUGIN yes)
SET(OD_MODULE_SOURCES
	grav.cc
	gravhorcalc.cc
	uigravhorcalc.cc
	uigravpi.cc)
SET( OD_PLUGIN_ALO_EXEC ${OD_MAIN_EXEC} )
SET( OD_NO_ALO_ENTRY 1 )
OD_INIT_MODULE()
