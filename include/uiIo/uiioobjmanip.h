#ifndef uiioobjmanip_h
#define uiioobjmanip_h

/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        A.H. Bril
 Date:          May 2003
 RCS:           $Id: uiioobjmanip.h,v 1.6 2004-10-18 15:10:51 nanne Exp $
________________________________________________________________________

-*/

#include "uibuttongroup.h"
class IODirEntryList;
class IOObj;
class IOStream;
class MultiID;
class Translator;
class ioPixmap;
class uiListBox;
class uiToolButton;


class uiManipButGrp : public uiButtonGroup
{
public:
    			uiManipButGrp(uiParent* p)
			    : uiButtonGroup(p,"")	{}

    enum Type		{ FileLocation, Rename, Remove, ReadOnly };

    uiToolButton*	addButton(Type,const CallBack&,const char* tip);
    uiToolButton*	addButton(const ioPixmap&,const CallBack&,const char*);
};


/*! \brief Buttongroup to manipulate an IODirEntryList. */

class uiIOObjManipGroup : public uiManipButGrp
{
public:
			uiIOObjManipGroup(uiListBox*,IODirEntryList&,
				      const char* default_extension);
			~uiIOObjManipGroup();

    void		selChg(CallBacker*);
    void		refreshList(const MultiID& selkey);

    Notifier<uiIOObjManipGroup>	preRelocation;
    Notifier<uiIOObjManipGroup>	postRelocation;
    const char*		curRelocationMsg() const	{ return relocmsg; }

protected:

    IODirEntryList&	entries;
    IOObj*		ioobj;
    BufferString	defext;
    BufferString	relocmsg;

    uiListBox*		box;
    uiToolButton*	locbut;
    uiToolButton*	robut;
    uiToolButton*	renbut;
    uiToolButton*	rembut;

    bool		gtIOObj();
    void		tbPush(CallBacker*);
    void		relocCB(CallBacker*);

    bool		rmEntry(bool);
    bool		renameEntry(Translator*);
    bool		relocEntry(Translator*);
    bool		readonlyEntry(Translator*);

    bool		doReloc(Translator*,IOStream&,IOStream&);

};


//! Removes the underlying file(s) that an IOObj describes, with warnings
//!< if exist_lbl set to true, " existing " is added to message.
bool uiRmIOObjImpl(IOObj&,bool exist_lbl=false);


#endif
