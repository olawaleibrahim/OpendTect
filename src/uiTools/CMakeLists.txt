#_______________________Pmake___________________________________________________
#
#	Makefile : 	General uiToolkits
# 	Jan 2012	K. Tingdahl
#_______________________________________________________________________________

set( OD_MODULE_DEPS uiBase MMProc )
set( OD_MODULE_SOURCES
	inituitools.cc
	annotbuffill.cc
	uiamplspectrum.cc
	uiapplserv.cc
	uiarray1dinterpol.cc
	uiarray2dchg.cc
	uiarray2dinterpol.cc
	uiaxisdata.cc
	uiaxishandler.cc
	uibasemap.cc
	uibatchjob.cc
	uibinidtable.cc
	uibuildlistfromlist.cc
	uibuttonstateedit.cc
	uichecklist.cc
	uicolortable.cc
	uicoltabimport.cc
	uicoltabman.cc
	uicoltabmarker.cc
	uicoltabtools.cc
	uicompparsel.cc
	uiconstvel.cc
	uicoordsystem.cc
	uicrssystem.cc
	uidatapackmon.cc
	uidirectionalplot.cc
	uidlggroup.cc
	uifileinput.cc
	uifillpattern.cc
	uifontsel.cc
	uifreqfilter.cc
	uifunctiondisplay.cc
	uigeninput.cc
	uigeninput_impl.cc
	uigeninputdlg.cc
	uigraphicssaveimagedlg.cc
	uigraphicsscalebar.cc
	uigraphicssceneaxismgr.cc
	uigraphicsview.cc
	uigridder2d.cc
	uigvfreehandareaselectiontool.cc
	uihandledlsitefail.cc
	uihistogramdisplay.cc
	uihistogramsel.cc
	uihostiddlg.cc
	uiinterpollayermodel.cc
	uiiosel.cc
	uiissuereporter.cc
	uilatlong.cc
	uilistboxfilter.cc
	uilocalhostgrp.cc
	uimapperrangeeditor.cc
	uimapperrangeeditordlg.cc
	uimarkerstyle.cc
	uimathexpression.cc
	uimathexpressionvariable.cc
	uimeasuredlg.cc
	uimenuhandler.cc
	uimnemonicsel.cc
	uinotsaveddlg.cc
	uiobjectitemview.cc
	uiobjectitemviewwin.cc
	uiofferinfo.cc
	uipathsel.cc
	uipluginman.cc
	uipluginsel.cc
	uipolardiagram.cc
	uipositiontable.cc
	uipropvalfld.cc
	uiproxydlg.cc
	uiraytrace1d.cc
	uirgbarraycanvas.cc
	uirockphysform.cc
	uisaveimagedlg.cc
	uiscaler.cc
	uisellinest.cc
	uiselsimple.cc
	uiserviceservermgr.cc
	uisettings.cc
	uishortcuts.cc
	uisizesel.cc
	uislicepos.cc
	uislicesel.cc
	uistatsdisplay.cc
	uistepoutsel.cc
	uistratlvlsel.cc
	uitaskrunner.cc
	uitblimpexpdatasel.cc
	uitextfile.cc
	uitime2depthzaxistrans.cc
	uitoolbarcmded.cc
	uitreeitemmanager.cc
	uiunitsel.cc
	uivalseriestracker.cc
	uivariogram.cc
	uivarwizard.cc
	uiwindowfuncseldlg.cc
	uiwindowfunctionsel.cc
	uiwindowgrabber.cc
	uiwizard.cc
	uiworld2ui.cc
	uizaxistransform.cc
	uizrangeinput.cc
)

set( OD_MODULE_GUI_PROGS
	od_FileBrowser.cc
	od_ImageViewer.cc
	od_ProgressViewer.cc
	od_uiReportIssue.cc
)

if( WIN32 )
    list( APPEND OD_ELEVATED_PERMISSIONS_GUI_PROGS od_runinst.cc )
    set( OD_od_runinst_RESOURCE resources/od_runinst.ico )
endif( WIN32 )

if ( OD_ENABLE_BREAKPAD )
    list ( APPEND OD_MODULE_TESTPROGS test_breakpad.cc )
endif()


OD_INIT_MODULE()
