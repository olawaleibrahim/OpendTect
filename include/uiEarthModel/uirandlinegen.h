#ifndef uirandlinegen_h
#define uirandlinegen_h

/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Bert
 Date:          Nov 2007
 RCS:           $Id: uirandlinegen.h,v 1.5 2008-10-03 12:22:42 cvsjaap Exp $
________________________________________________________________________

-*/

#include "uidialog.h"

class CtxtIOObj;
class uiGenInput;
class uiIOObjSel;
class uiCheckBox;
class uiSelZRange;
class uiLabeledSpinBox;
class uiSpinBox;
class uiLabel;


/*! \brief Generate random lines from contours of a horizon */

class uiGenRanLinesByContour : public uiDialog
{
public:
			uiGenRanLinesByContour(uiParent*);
			~uiGenRanLinesByContour();

    const char*		getNewSetID() const;
    bool		dispOnCreation();

protected:

    CtxtIOObj&		horctio_;
    CtxtIOObj&		polyctio_;
    CtxtIOObj&		rlsctio_;

    uiIOObjSel*		infld_;
    uiIOObjSel*		polyfld_;
    uiIOObjSel*		outfld_;
    uiGenInput*		contzrgfld_;
    uiGenInput*		relzrgfld_;
    uiGenInput*		abszrgfld_;
    uiCheckBox*		isrelfld_;
    uiCheckBox*		dispfld_;
    uiLabeledSpinBox*	vtxthreshfld_;
    uiCheckBox*		largestfld_;
    uiSpinBox*		nrlargestfld_;
    uiLabel*		largestendfld_;

    void		largestOnlyChg(CallBacker*);
    void		isrelChg(CallBacker*);

    bool		acceptOK(CallBacker*);

};


/*! \brief Generate random lines by shifting an existing */

class uiGenRanLinesByShift : public uiDialog
{
public:
			uiGenRanLinesByShift(uiParent*);
			~uiGenRanLinesByShift();

    const char*		getNewSetID() const;
    bool		dispOnCreation();

protected:

    CtxtIOObj&		inctio_;
    CtxtIOObj&		outctio_;

    uiIOObjSel*		infld_;
    uiIOObjSel*		outfld_;
    uiGenInput*		distfld_;
    uiGenInput*		sidefld_;
    uiCheckBox*		dispfld_;

    bool		acceptOK(CallBacker*);

};


/*! \brief Generate random line from polygon */

class uiGenRanLineFromPolygon : public uiDialog
{
public:
			uiGenRanLineFromPolygon(uiParent*);
			~uiGenRanLineFromPolygon();

    const char*		getNewSetID() const;
    bool		dispOnCreation();

protected:

    CtxtIOObj&		inctio_;
    CtxtIOObj&		outctio_;

    uiIOObjSel*		infld_;
    uiIOObjSel*		outfld_;
    uiSelZRange*	zrgfld_;
    uiCheckBox*		dispfld_;

    bool		acceptOK(CallBacker*);

};


#endif
