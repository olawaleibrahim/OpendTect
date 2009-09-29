#ifndef uiwelltiesavedatadlg_h
#define uiwelltiesavedatadlg_h

/*+
________________________________________________________________________

(C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
Author:        Bruno
Date:          January 2009
RCS:           $Id: uiwellwelltiesavedatadlg.h,v 1.1 2009-09-19 13:02:33 cvsbruno Exp
$
________________________________________________________________________

-*/

#include "uidialog.h"
#include "uigroup.h"
#include "bufstringset.h"
#include "welltieunitfactors.h"

class CtxtIOObj;
class IOObj;
class IOObjSel;
class uiGenInput;
class uiListBox;
class uiIOObjSel;
class uiLabel;
class uiCheckBox;
class Wavelet;

namespace Well
{
    class Log;
    class LogSet;
}

namespace WellTie
{
    class DataHolder;
    class DataWriter;
    class Log;

mClass uiSaveDataGroup : public uiGroup
{
public:

      mClass Setup
      {
	      public:
				Setup()
				    : nrtimes_(0)
				    , colnm_("Log")   
				    , saveaslog_(true)   
				    {}	  
			
        mDefSetupMemb(BufferString,colnm)
        mDefSetupMemb(BufferStringSet,itemnames)
        mDefSetupMemb(int,nrtimes)
        mDefSetupMemb(bool,saveaslog)
      };

    				uiSaveDataGroup(uiParent*,CtxtIOObj&,
						const Setup&);
    				~uiSaveDataGroup(){};

    bool 			getNamesToBeSaved(BufferStringSet&);
    const int			indexOf( const char* nm ) const
				{ return names_.indexOf(nm); }
    void 			changeLogUIOutput(CallBacker*);

protected:

    CtxtIOObj&          	ctio_;
    ObjectSet<uiGroup> 		objgrps_;
    ObjectSet<uiLabel> 		titlelblflds_;
    ObjectSet<uiLabel> 		lblflds_;
    ObjectSet<uiGenInput> 	nameflds_;
    ObjectSet<uiCheckBox> 	boxflds_;
    ObjectSet<uiIOObjSel>  	ioobjselflds_;

    const BufferStringSet	names_;
    int 			nrtimessaved_;
    bool 			saveaslog_;

    void			init();
};


mClass uiSaveDataDlg : public uiDialog
{
public: 
				uiSaveDataDlg(uiParent*,WellTie::DataHolder*);
				~uiSaveDataDlg();

protected :

    CtxtIOObj&          	seisctio_;
    CtxtIOObj&          	wvltctio_; 

    int 			nrtimessaved_;
    uiSaveDataGroup* 		savelogsfld_;
    uiSaveDataGroup* 		savewvltsfld_;
    uiGenInput* 		saveasfld_;
    const WellTie::DataHolder* 	dataholder_;
    WellTie::DataWriter*	datawriter_;

    bool 			acceptOK(CallBacker*);
};

}; //namespace WellTie

#endif

