#ifndef uiwelltietoseismicdlg_h
#define uiwelltietoseismicdlg_h

/*+
________________________________________________________________________

(C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
Author:        Bruno
Date:          January 2009
RCS:           $Id: uiwellwelltietoseismicdlg.h,v 1.1 2009-01-19 13:02:33 cvsbruno Exp
$
________________________________________________________________________

-*/

#include "uidialog.h"
#include "uiflatviewmainwin.h"
#include "bufstringset.h"

class uiGroup;
class uiToolBar;
class uiGenInput;
class uiPushButton;
class uiLabel;
class uiLabeledComboBox;
class uiCheckBox;
class uiWellLogDisplay;

namespace Well	 { class Data; }

namespace WellTie
{
    class DispParams;
    class Setup;
    class Server;
    class EventStretch;
    class uiControlView;
    class uiCrossCorrView;
    class uiInfoDlg;
    class uiTieView;
    class uiWaveletView;

mClass uiTieWin : public uiFlatViewMainWin
{
public:

				uiTieWin(uiParent*,const WellTie::Setup&);
				~uiTieWin();

    const WellTie::Setup&	Setup()		{ return setup_; }
	
protected:

    const WellTie::Setup& 	setup_;
    Server&			server_;
    EventStretch&		stretcher_;
    DispParams&			params_;
    
    uiCheckBox* 		cscorrfld_;
    uiCheckBox* 		csdispfld_;
    uiCheckBox* 		markerfld_;
    uiCheckBox* 		zinftfld_;
    uiCheckBox* 		zintimefld_;
    uiGroup*            	vwrgrp_;
    uiLabeledComboBox*		eventtypefld_;
    uiPushButton*		infobut_;
    uiPushButton*		applybut_;
    uiPushButton*		undobut_;
    uiPushButton*		clearpicksbut_;
    uiPushButton*		clearlastpicksbut_;
    uiPushButton*		matchhormrksbut_;
    uiToolBar*          	toolbar_;

    uiControlView* 		controlview_;
    uiInfoDlg* 			infodlg_; 
    uiTieView*			drawer_;
    
    void			addControls();
    void 			addToolBarTools();
    void			createViewerTaskFields(uiGroup*);
    void			createDispPropFields(uiGroup*);
    void 			drawData();
    void 			drawFields();
    void 			getDispParams();
    void 			initAll();
    void 			putDispParams();
    void			resetInfoDlg();

    bool			acceptOK(CallBacker*);
    void 			applyPushed(CallBacker*);
    void 			applyShiftPushed(CallBacker*);
    bool			compute(CallBacker*);
    void			checkIfPick(CallBacker*);
    void			checkShotChg(CallBacker*);
    void			checkShotDisp(CallBacker*);
    void 			csCorrChanged(CallBacker*);
    void			clearLastPick(CallBacker*);
    void			clearPicks(CallBacker*);
    void 			dispParPushed(CallBacker*);
    void 			dispPropChg(CallBacker*);
    void			dispInfoMsg(CallBacker*);
    void 			displayUserMsg(CallBacker*);
    void 			doWork(CallBacker*);
    void			drawUserPick(CallBacker*);
    void 			editD2TPushed(CallBacker*);
    void			eventTypeChg(CallBacker*);
    void 			infoPushed(CallBacker*);
    bool 			matchHorMrks(CallBacker*);
    void 			provideWinHelp(CallBacker*);
    void			reDrawSeisViewer(CallBacker*);
    bool			rejectOK(CallBacker*);
    void 			setView(CallBacker*);
    bool 			saveDataPushed(CallBacker*);
    void 			timeChanged(CallBacker*);
    bool 			undoPushed(CallBacker*);
    void			userDepthsChanged(CallBacker*);
};



mClass uiInfoDlg : public uiDialog
{
public:		
    		
				uiInfoDlg(uiParent*,Server&);
				~uiInfoDlg();

    Notifier<uiInfoDlg>  	redrawNeeded;

    void 			drawData();
    bool 			getMarkerDepths(Interval<float>& zrg );
    void 			propChanged(CallBacker*);

protected:
   
    BufferStringSet             markernames_;

    Server&			server_;
    ObjectSet<uiGenInput>	zrangeflds_;
    ObjectSet<uiLabel>		zlabelflds_;
    uiGenInput*                 choicefld_;
    uiGenInput*                 estwvltlengthfld_;
    uiCrossCorrView*      	crosscorr_;
    uiWaveletView*     		wvltdraw_;

    Interval<float>		zrg_;
    int				estwvltsz_;

    void    			computeData();
    void 			applyMarkerPushed(CallBacker*);
    void 			wvltChanged(CallBacker*);
};

}; //namespace WellTie

#endif

