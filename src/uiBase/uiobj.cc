/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        A.H. Lammertink
 Date:          25/08/1999
________________________________________________________________________

-*/

#include "uiobj.h"
#include "uiobjbody.h"
#include "uicursor.h"
#include "uimainwin.h"
#include "i_layoutitem.h"
#include "uimain.h"
#include "uigroup.h"
#include "uitreeview.h"

#include "color.h"
#include "settingsaccess.h"
#include "timer.h"
#include "perthreadrepos.h"

#include "q_uiimpl.h"

#include <QEvent>

mUseQtnamespace

static CallBackSet& cmdrecorders_ = *new CallBackSet;

static ObjectSet<const uiBaseObject> cmdrecstopperstack_;
static ObjectSet<const uiBaseObject> cmdrecstopperlist_;
static ObjectSet<const CallBacker> cmdrecstrikeoutlist_;

CmdRecStopper::CmdRecStopper( const uiBaseObject* obj )
{
    cmdrecstopperstack_.allowNull();
    cmdrecstopperstack_.push( obj );

    if ( !cmdrecorders_.isEmpty() && !cmdrecstopperlist_.isPresent(obj) )
	cmdrecstopperlist_ += obj;
}

CmdRecStopper::~CmdRecStopper()
{ cmdrecstopperstack_.pop(); }

void CmdRecStopper::clearStopperList( const CallBacker* cmdrec )
{
    cmdrecstrikeoutlist_ -= cmdrec;
    if ( cmdrecstrikeoutlist_.isEmpty() )
    {
	cmdrecstopperlist_.erase();
	for ( int idx=0; idx<cmdrecorders_.size(); idx++ )
	    cmdrecstrikeoutlist_ += cmdrecorders_[idx].cbObj();
    }
}

bool CmdRecStopper::isInStopperList( const uiBaseObject* obj )
{ return cmdrecstopperlist_.isPresent(obj); }


mDefineEnumUtils(uiRect,Side,"Side") { "Left", "Top", "Right", "Bottom", 0 };

#define mBody_( imp_ )	dynamic_cast<uiObjectBody*>( imp_ )
#define mBody()		mBody_( body() )
#define mConstBody()	mBody_(const_cast<uiObject*>(this)->body())

uiBaseObject::uiBaseObject( const char* nm, uiBody* b )
    : NamedCallBacker(nm)
    , finaliseStart(this)
    , finaliseDone(this)
    , cmdrecrefnr_(0)
    , body_(b)
{}


uiBaseObject::~uiBaseObject()
{ sendDelNotif(); }


void uiBaseObject::finalise()
{ if ( body() ) body()->finalise(); }

void uiBaseObject::clear()
{ if ( body() ) body()->clear(); }

bool uiBaseObject::finalised() const
{ return body() ? body()->finalised() : false; }


int uiBaseObject::beginCmdRecEvent( const char* msg )
{ return beginCmdRecEvent( (od_uint64) 0, msg ); }


int uiBaseObject::beginCmdRecEvent( od_uint64 id, const char* msg )
{
    if ( cmdrecorders_.isEmpty() ||
	 (!id && cmdrecstopperstack_.isPresent(this)) )
	return -1;

    cmdrecrefnr_ = cmdrecrefnr_==INT_MAX ? 1 : cmdrecrefnr_+1;

    BufferString actstr;
    if ( id )
	actstr += toString( id );

    actstr += " Begin "; actstr += cmdrecrefnr_;
    actstr += " "; actstr += msg;
    CBCapsule<const char*> caps( actstr, this );
    cmdrecorders_.doCall( &caps );
    return cmdrecrefnr_;
}


const QWidget* uiBaseObject::getWidget() const
{ return const_cast<uiBaseObject*>(this)->getWidget(); }


void uiBaseObject::endCmdRecEvent( int refnr, const char* msg )
{ endCmdRecEvent( (od_uint64) 0, refnr, msg ); }


void uiBaseObject::endCmdRecEvent( od_uint64 id, int refnr, const char* msg )
{
    if ( cmdrecorders_.isEmpty() ||
	 (!id && cmdrecstopperstack_.isPresent(this)) )
	return;

    BufferString actstr;
    if ( id )
	actstr += toString( id );

    actstr += " End "; actstr += refnr;
    actstr += " "; actstr += msg;
    CBCapsule<const char*> caps( actstr, this );
    cmdrecorders_.doCall( &caps );
}


void uiBaseObject::removeCmdRecorder( const CallBack& cb )
{
    cmdrecorders_ -= cb;
    CmdRecStopper::clearStopperList( cb.cbObj() );
}


void uiBaseObject::addCmdRecorder( const CallBack& cb )
{
    cmdrecorders_ += cb;
    cmdrecstrikeoutlist_ += cb.cbObj();
}


uiParent::uiParent( const char* nm, uiParentBody* b )
    : uiBaseObject( nm, b )
{}


void uiParent::addChild( uiBaseObject& child )
{
    mDynamicCastGet(uiBaseObject*,thisuiobj,this);
    if ( thisuiobj && &child == thisuiobj ) return;
    if ( !body() )
	{ pErrMsg("uiParent has no body!"); return; }

    uiParentBody* b = dynamic_cast<uiParentBody*>( body() );
    if ( !b )
	{ pErrMsg("uiParent has a body, but it's no uiParentBody"); return; }

    b->addChild( child );
}


void uiParent::manageChld( uiBaseObject& child, uiObjectBody& bdy )
{
    if ( &child == static_cast<uiBaseObject*>(this) ) return;

    uiParentBody* b = dynamic_cast<uiParentBody*>( body() );
    if ( !b )	return;

    b->manageChld( child, bdy );
}


void uiParent::attachChild ( constraintType tp, uiObject* child,
			     uiObject* other, int margin, bool reciprocal )
{
    if ( child == static_cast<uiBaseObject*>(this) ) return;
    if ( !body() )		{ pErrMsg("uiParent has no body!"); return; }

    uiParentBody* b = dynamic_cast<uiParentBody*>( body() );
    if ( !b )
	{ pErrMsg("uiParent has a body, but it's no uiParentBody"); return; }

    b->attachChild ( tp, child, other, margin, reciprocal );
}


const ObjectSet<uiBaseObject>* uiParent::childList() const
{
    uiParentBody* uipb =
	    dynamic_cast<uiParentBody*>( const_cast<uiParent*>(this)->body() );
    return uipb ? uipb->childList(): 0;
}


OD::Color uiObject::roBackgroundColor() const
{
    return backgroundColor().lighter( 2.5f );
}


OD::Color uiParent::backgroundColor() const
{
    return mainObject() ? mainObject()->backgroundColor()
			: uiMain::theMain().windowColor();
}


void uiParent::translateText()
{
    uiBaseObject::translateText();

    if ( !childList() )
	return;

    for ( int idx=0; idx<childList()->size(); idx++ )
    {
	uiBaseObject* child = const_cast<uiBaseObject*>((*childList())[idx]);

	//Workaround for missing function on uiGroupObj
	mDynamicCastGet( uiGroupObj*, groupobj, child );
	if ( groupobj && groupobj->group() )
	    groupobj->group()->translateText();

	child->translateText();
    }
}


// uiParentBody
uiParentBody* uiParent::pbody()
{
    return dynamic_cast<uiParentBody*>( body() );
}


void uiParentBody::finaliseChildren()
{
    if ( !finalised_ )
    {
	finalised_= true;
	for ( int idx=0; idx<children_.size(); idx++ )
	    children_[idx]->finalise();
    }
}


void uiParentBody::clearChildren()
{
    for ( int idx=0; idx<children_.size(); idx++ )
	children_[idx]->clear();
}



// uiCentralWidgetBody
uiCentralWidgetBody::uiCentralWidgetBody( const char* nm )
    : uiParentBody(nm)
    , initing_(true)
    , centralwidget_(nullptr)
{}


uiCentralWidgetBody::~uiCentralWidgetBody()
{}


void uiCentralWidgetBody::addChild( uiBaseObject& child )
{
    if ( !initing_ && centralwidget_ )
	centralwidget_->addChild( child );
    else
	uiParentBody::addChild( child );
}


void uiCentralWidgetBody::manageChld_( uiBaseObject& o, uiObjectBody& b )
{
    if ( !initing_ && centralwidget_ )
	centralwidget_->manageChld( o, b );
}


void uiCentralWidgetBody::attachChild ( constraintType tp,
					uiObject* child,
					uiObject* other, int margin,
					bool reciprocal )
{
    if ( !centralwidget_ || !child || initing_ )
	return;

    centralwidget_->attachChild( tp, child, other, margin, reciprocal);
}


const QWidget* uiCentralWidgetBody::managewidg_() const
{
    return initing_ ? qwidget_() : centralwidget_->pbody()->managewidg();
}



// uiObjEventFilter
class uiObjEventFilter : public QObject
{
public:
			uiObjEventFilter( uiObject& uiobj )
			    : uiobject_( uiobj )
			{}
protected:
    bool		eventFilter(QObject*,QEvent*);
    uiObject&		uiobject_;
};


bool uiObjEventFilter::eventFilter( QObject* obj, QEvent* ev )
{
    if ( ev && ev->type() == mUsrEvLongTabletPress )
    {
	uiobject_.handleLongTabletPress();
	return true;
    }

    return false;
}


static ObjectSet<uiObject> uiobjectlist_;


static BufferString getCleanName( const char* nm )
{
    QString qstr( nm );
    qstr.remove( QChar('&') );
    return BufferString( qstr );
}



static int iconsz_ = -1;
static int fldsz_ = -1;

uiObject::uiObject( uiParent* p, const char* nm )
    : uiBaseObject( getCleanName(nm), 0 )
    , setGeometry(this)
    , closed(this)
    , parent_( p )
{
    if ( p ) p->addChild( *this );
    uiobjectlist_ += this;
    updateToolTip();

    uiobjeventfilter_ = new uiObjEventFilter( *this );
    if ( body() && body()->qwidget() )
	body()->qwidget()->installEventFilter( uiobjeventfilter_ );
}


uiObject::uiObject( uiParent* p, const char* nm, uiObjectBody& b )
    : uiBaseObject( getCleanName(nm), &b )
    , setGeometry(this)
    , closed(this)
    , parent_( p )
{
    if ( p ) p->manageChld( *this, b );
    uiobjectlist_ += this;
    updateToolTip();

    uiobjeventfilter_ = new uiObjEventFilter( *this );
    if ( body() && body()->qwidget() )
	body()->qwidget()->installEventFilter( uiobjeventfilter_ );
}


uiObject::~uiObject()
{
    delete uiobjeventfilter_;

    closed.trigger();
    uiobjectlist_ -= this;
}


void uiObject::setHSzPol( SzPolicy p )
    { mBody()->setHSzPol(p); }

void uiObject::setVSzPol( SzPolicy p )
    { mBody()->setVSzPol(p); }

uiObject::SzPolicy uiObject::szPol(bool hor) const
    { return mConstBody()->szPol(hor); }


void uiObject::setName( const char* nm )
{
    uiBaseObject::setName( getCleanName(nm) );
    updateToolTip();
}


const uiString& uiObject::toolTip() const
{
    return tooltip_;
}


void uiObject::setToolTip( const uiString& txt )
{
    tooltip_ = txt;
    updateToolTip();
}


void uiObject::updateToolTip(CallBacker*)
{
    mEnsureExecutedInMainThread( uiObject::updateToolTip );
    if ( !qwidget() ) return;

    if ( uiMain::isNameToolTipUsed() && !name().isEmpty() )
    {
	BufferString namestr = name().buf();
	uiMain::formatNameToolTipString( namestr );
	qwidget()->setToolTip( namestr.buf() );
    }
    else
	qwidget()->setToolTip( toQString(tooltip_) );
}


void uiObject::translateText()
{
    uiBaseObject::translateText();
    updateToolTip();
}


void uiObject::display( bool yn, bool shrink, bool maximize )
{
    finalise();
    mBody()->display(yn,shrink,maximize);
}

void uiObject::setFocus()
{ mBody()->uisetFocus(); }

bool uiObject::hasFocus() const
{ return mConstBody()->uihasFocus(); }

void uiObject::disabFocus()
{
    if ( qwidget() )
	qwidget()->setFocusPolicy( Qt::NoFocus );
}


void uiObject::setCursor( const MouseCursor& cursor )
{
    QCursor qcursor;
    uiCursorManager::fillQCursor( cursor, qcursor );
    body()->qwidget()->setCursor( qcursor );
}


bool uiObject::isCursorInside() const
{
    const uiPoint cursorpos = uiCursorManager::cursorPos();
    const QPoint objpos = mConstBody()->qwidget()->mapToGlobal( QPoint(0,0) );
    return cursorpos.x>=objpos.x() && cursorpos.x<objpos.x()+width() &&
	   cursorpos.y>=objpos.y() && cursorpos.y<objpos.y()+height();
}


void uiObject::setStyleSheet( const char* qss )
{
    body()->qwidget()->setStyleSheet( qss );
}


OD::Color uiObject::backgroundColor() const
    { return mConstBody()->uibackgroundColor(); }


void uiObject::setBackgroundColor(const OD::Color& col)
    { mBody()->uisetBackgroundColor(col); }


void uiObject::setBackgroundPixmap( const uiPixmap& pm )
    { mBody()->uisetBackgroundPixmap( pm ); }


void uiObject::setTextColor(const OD::Color& col)
    { mBody()->uisetTextColor(col); }


void uiObject::setSensitive(bool yn)
    { mBody()->uisetSensitive(yn); }

bool uiObject::sensitive() const
    { return mConstBody()->uisensitive(); }


bool uiObject::visible() const
    { return mConstBody()->uivisible(); }

bool uiObject::isDisplayed() const
    { return mConstBody()->isDisplayed(); }

int uiObject::prefHNrPics() const
    { return mConstBody()->prefHNrPics(); }


void uiObject::setPrefWidth( int w )
    { mBody()->setPrefWidth(w); }


void uiObject::setPrefWidthInChar( int w )
    { mBody()->setPrefWidthInChar( (float)w ); }

void uiObject::setPrefWidthInChar( float w )
     { mBody()->setPrefWidthInChar(w); }

void uiObject::setMinimumWidth( int w )
    { mBody()->setMinimumWidth(w); }

void uiObject::setMinimumHeight( int h )
    { mBody()->setMinimumHeight(h); }

void uiObject::setMaximumWidth( int w )
    { mBody()->setMaximumWidth(w); }

void uiObject::setMaximumHeight( int h )
    { mBody()->setMaximumHeight(h); }

int uiObject::prefVNrPics() const
    { return mConstBody()->prefVNrPics(); }

void uiObject::setPrefHeight( int h )
    { mBody()->setPrefHeight(h); }

void uiObject::setPrefHeightInChar( int h )
    { mBody()->setPrefHeightInChar( (float)h ); }

void uiObject::setPrefHeightInChar( float h )
     {mBody()->setPrefHeightInChar(h);}

void uiObject::setStretch( int hor, int ver )
     {mBody()->setStretch(hor,ver); }

int uiObject::stretch( bool hor ) const
    { return mConstBody()->stretch(hor,false); }

void uiObject::attach( constraintType tp, int margin )
    { mBody()->attach(tp, (uiObject*)0, margin); }

void uiObject::attach( constraintType tp, uiObject* other, int margin,
			bool reciprocal )
    { mBody()->attach(tp, other, margin, reciprocal); }

void uiObject::attach( constraintType tp, uiParent* other, int margin,
			bool reciprocal )
    { mBody()->attach(tp, other, margin, reciprocal); }

/*!
    Moves the \a second widget around the ring of focus widgets so
    that keyboard focus moves from the \a first widget to the \a
    second widget when the Tab key is pressed.

    Note that since the tab order of the \a second widget is changed,
    you should order a chain like this:

    \code
	setTabOrder( a, b ); // a to b
	setTabOrder( b, c ); // a to b to c
	setTabOrder( c, d ); // a to b to c to d
    \endcode

    \e not like this:

    \code
	setTabOrder( c, d ); // c to d   WRONG
	setTabOrder( a, b ); // a to b AND c to d
	setTabOrder( b, c ); // a to b to c, but not c to d
    \endcode

    If \a first or \a second has a focus proxy, setTabOrder()
    correctly substitutes the proxy.
*/
void uiObject::setTabOrder( uiObject* first, uiObject* second )
{
    QWidget::setTabOrder( first->body()->qwidget(), second->body()->qwidget() );
}



void uiObject::setFont( const uiFont& f )
    { mBody()->uisetFont(f); }


const uiFont* uiObject::font() const
    { return mConstBody()->uifont(); }


uiSize uiObject::actualSize( bool include_border ) const
    { return mConstBody()->actualSize( include_border ); }


void uiObject::setCaption( const uiString& c )
    { mBody()->uisetCaption(c); }


void uiObject::triggerSetGeometry( const i_LayoutItem* mylayout, uiRect& geom )
{
    if ( mBody() && mylayout == mBody()->layoutItem() )
	setGeometry.trigger(geom);
}


void uiObject::reDraw( bool deep )
    { mBody()->reDraw( deep ); }


uiMainWin* uiObject::mainwin()
{
    uiParent* par = parent();
    if ( !par )
    {
	mDynamicCastGet(uiMainWin*,mw,this)
	return mw;
    }

    return par->mainwin();
}


QWidget* uiObject::qwidget()
{ return body() ? body()->qwidget() : 0 ; }


void uiObject::close()
{
    if ( body() && body()->qwidget() )
	body()->qwidget()->close();
}


int uiObject::width() const
{
    return qwidget() ? qwidget()->width() : 1;
}


int uiObject::height() const
{
    return qwidget() ? qwidget()->height() : 1;
}


int uiObject::iconSize()
{
    if ( iconsz_ < 0 )
    {
	const BufferString key =
	    IOPar::compKey( SettingsAccess::sKeyIcons(), "size" );
	iconsz_ = 24;
	Settings::common().get( key, iconsz_ );
    }

    return iconsz_;
}


int uiObject::baseFldSize()
{
    if ( fldsz_ < 0 )
    {
	fldsz_ = 10;
	Settings::common().get( "dTect.Field.size", fldsz_ );
    }

    return fldsz_;
}


void uiObject::updateToolTips()
{
    for ( int idx=uiobjectlist_.size()-1; idx>=0; idx-- )
	uiobjectlist_[idx]->updateToolTip();
}


void uiObject::reParent( uiParent* p )
{
    if ( !p || !p->pbody() ) return;
    qwidget()->setParent( p->pbody()->managewidg() );
    uiParentBody* b = dynamic_cast<uiParentBody*>( p->body() );
    if ( !b ) return;
    mBody()->reParent( b );
    p->manageChld( *this, *mBody() );
}


bool uiObject::handleLongTabletPress()
{
    if ( !parent() || !parent()->mainObject() || parent()->mainObject()==this )
	return false;

    return parent()->mainObject()->handleLongTabletPress();
}
