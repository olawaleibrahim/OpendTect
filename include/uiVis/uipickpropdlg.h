#ifndef uipickpropdlg_h
#define uipickpropdlg_h
/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        N. Hemstra
 Date:          May 2002
 RCS:           $Id: uipickpropdlg.h,v 1.8 2010-04-06 17:34:50 cvsyuancheng Exp $
________________________________________________________________________

-*/

#include "uimarkerstyledlg.h"

class uiCheckBox;

namespace Pick { class Set; };
namespace visSurvey { class PickSetDisplay; };


mClass uiPickPropDlg : public uiMarkerStyleDlg
{
public:
				uiPickPropDlg(uiParent* p,
					      Pick::Set& set, 
					      visSurvey::PickSetDisplay* psd);

protected:

    void			doFinalise(CallBacker*);
    void			sliderMove(CallBacker*);
    void			typeSel(CallBacker*);
    void			colSel(CallBacker*);
    bool			acceptOK(CallBacker*);
    void			drawStyleCB(CallBacker*);
    void			drawSel(CallBacker*);
    
    uiGenInput*         	drawstylefld_;			
    uiCheckBox*         	usedrawstylefld_;			

    Pick::Set&			set_;
    visSurvey::PickSetDisplay*	psd_;
};

#endif
