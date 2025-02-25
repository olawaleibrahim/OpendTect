#pragma once

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert
 Date:          Feb 2010
________________________________________________________________________

-*/

#include "uiiomod.h"
#include "uigroup.h"
#include "uidialog.h"

class uiComboBox;
class uiFlatViewMainWin;
class uiGenInput;
class uiListBox;
class uiPDF1DViewWin;
class uiPushButton;
class uiTable;
class uiTabStack;
class uiToolButton;
class uiUnitSel;
class Gaussian1DProbDenFunc;
class Gaussian2DProbDenFunc;
class GaussianNDProbDenFunc;
class MnemonicSelection;
class ProbDenFunc;
class UnitOfMeasure;


/*!\brief Base class for edit probability density function editors. */

mExpClass(uiIo) uiEditProbDenFunc : public uiGroup
{ mODTextTranslationClass(uiEditProbDenFunc);
public:

    virtual		~uiEditProbDenFunc();

    virtual bool	commitChanges()			= 0;
    inline bool		isChanged() const		{ return chgd_; }

    static void		getPars(const MnemonicSelection*,
				const BufferStringSet* varnms,int idx,
				BufferString& varnm,Interval<float>& rg,
				const UnitOfMeasure*&);

    static const UnitOfMeasure* guessUnit(const ProbDenFunc&,int idim);

protected:
			uiEditProbDenFunc(uiParent*,ProbDenFunc&,bool editable);

    const UnitOfMeasure* getUnit(int idim);

    ProbDenFunc&	pdf_;
    const ProbDenFunc&	inpdf_;
    const int		nrdims_;
    const bool		editable_;
    bool		chgd_;

};


/*!\brief Dialog to edit probability density functions. */

mExpClass(uiIo) uiEditProbDenFuncDlg : public uiDialog
{ mODTextTranslationClass(uiEditProbDenFuncDlg);
public:
			uiEditProbDenFuncDlg(uiParent*,ProbDenFunc&,
					 bool edit,bool isnew=false,
					 const MnemonicSelection* =nullptr,
					 const BufferStringSet* varnms=nullptr);

    bool		isChanged() const	{ return edfld_->isChanged(); }

protected:

    uiEditProbDenFunc*	edfld_;

    bool		acceptOK(CallBacker*);

};


/*!\brief Group to edit SampledProbDenFunc's. */

mExpClass(uiIo) uiEditSampledProbDenFunc : public uiEditProbDenFunc
{ mODTextTranslationClass(uiEditSampledProbDenFunc);
public:
			uiEditSampledProbDenFunc(uiParent*,ProbDenFunc&,bool);
			~uiEditSampledProbDenFunc();

    bool		commitChanges() override;

protected:

    int			curdim2_ = 0;

    uiTabStack*		tabstack_;
    ObjectSet<uiGenInput> nmflds_;
    ObjectSet<uiUnitSel> unflds_;
    uiTable*		tbl_ = nullptr;
    uiFlatViewMainWin*	vwwinnd_ = nullptr;
    uiPDF1DViewWin*	vwwin1d_ = nullptr;

    void		mkTable(uiGroup*);

    bool		getNamesFromScreen();
    void		getUnitsFromScreen();
    void		putValsToScreen();
    bool		getValsFromScreen(bool* chg=nullptr);
    void		setToolTips();
    void		updateUI();

    void		viewPDF(CallBacker*);
    void		vwWinClose(CallBacker*);
    void		tabChg(CallBacker*);
    void		smoothReq(CallBacker*);
    void		dimNext(CallBacker*);
    void		dimPrev(CallBacker*);

};


/*!\brief Group to edit Gaussian PPDF's. */


class uiEditGaussianProbDenFunc : public uiEditProbDenFunc
{ mODTextTranslationClass(uiEditGaussianProbDenFunc);
public:

			uiEditGaussianProbDenFunc(uiParent*,ProbDenFunc&,
				    bool editable,bool isnew=false,
				    const MnemonicSelection* = nullptr,
				    const BufferStringSet* varnms = nullptr);

			~uiEditGaussianProbDenFunc();

    bool		commitChanges() override;

protected:

    Gaussian1DProbDenFunc* pdf1d_ = nullptr;
    Gaussian2DProbDenFunc* pdf2d_ = nullptr;
    GaussianNDProbDenFunc* pdfnd_ = nullptr;

    uiTabStack*		tabstack_ = nullptr;
    uiGenInput*		ccfld_ = nullptr;
    ObjectSet<uiGenInput> nmflds_;
    ObjectSet<uiGenInput> expflds_;
    ObjectSet<uiGenInput> stdflds_;
    ObjectSet<uiUnitSel> unflds_;
    uiComboBox*		var1fld_ = nullptr;
    uiComboBox*		var2fld_ = nullptr;
    uiPushButton*	addsetbut_ = nullptr;
    uiListBox*		defcorrsfld_;
    uiToolButton*	rmbut_ = nullptr;

    float		getCC() const;
    void		mkCorrTabFlds(uiGroup*);
    int			findCorr() const;
    void		updateCorrList(int);

    void		initGrp(CallBacker*);
    void		tabChg(CallBacker*);
    void		unitChgCB(CallBacker*);
    void		corrSel(CallBacker*);
    void		varSel(CallBacker*);
    void		addSetPush(CallBacker*);
    void		rmPush(CallBacker*);

};



