#ifndef uifunctiondisplay_h
#define uifunctiondisplay_h

/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Bert
 Date:          Apr 2008
 RCS:           $Id: uifunctiondisplay.h,v 1.18 2009-02-03 08:31:27 cvssatyaki Exp $
________________________________________________________________________

-*/

#include "uigraphicsview.h"
#include "draw.h"
class uiGraphicsScene;
class uiGraphicsItem;
class uiAxisHandler;
class uiPolygonItem;
class uiPolyLineItem;
class uiRectItem;
class uiGraphicsItemGroup;

/*!\brief displays a function of (X,Y) pairs on a canvas - optionally a Y2.

  * No undefined values supportd (yet). X values can't be undef anyway.
  * You cannot change Setup::editable_ after construction.
  * Y2 is just an optional annotation. It can not be edited, it does not
    determine the X axis scale. Also, no markers are drawn.
 
 */

mClass uiFunctionDisplay : public uiGraphicsView
{
public:

    struct Setup
    {
				Setup()
				    : xrg_(mUdf(float),mUdf(float))
				    , yrg_(mUdf(float),mUdf(float))
				    , y2rg_(mUdf(float),mUdf(float))
				    , bgcol_(Color::White())
				    , ycol_(0,0,150)
				    , y2col_(0,200,0)
				    , xmarkcol_(150,0,0)
				    , ymarkcol_(150,0,0)
				    , canvaswidth_(400)
				    , canvasheight_(250)
				    , border_(20,10,20,10)
				    , annotx_(true)
				    , annoty_(true)
				    , annoty2_(true)
				    , noxaxis_(false)
				    , noyaxis_(false)
				    , noy2axis_(false)
				    , noxgridline_(false)
				    , noygridline_(false)
				    , noy2gridline_(false)
				    , pointsz_(0)
				    , ptsnaptol_(0.01)
				    , editable_(false)
				    , curvzval_(1)
				    , fillbelow_(false)
				    , fillbelowy2_(false)
				    , drawscattery1_(false)
				    , drawscattery2_(false)
				    , drawborder_(false)
				    , closepolygon_(true)
				    , drawliney_(true)
				    , drawliney2_(true) {}

	mDefSetupMemb(Interval<float>,xrg)	//!< if fixed start or end
	mDefSetupMemb(Interval<float>,yrg)	//!< if fixed start or end
	mDefSetupMemb(Interval<float>,y2rg)	//!< if fixed start or end
	mDefSetupMemb(Color,bgcol)		//!< Canvas background
	mDefSetupMemb(Color,ycol)
	mDefSetupMemb(Color,y2col)
	mDefSetupMemb(Color,xmarkcol)
	mDefSetupMemb(Color,ymarkcol)
	mDefSetupMemb(int,canvaswidth)
	mDefSetupMemb(int,canvasheight)
	mDefSetupMemb(uiBorder,border)
	mDefSetupMemb(int,curvzval)
	mDefSetupMemb(bool,annotx)
	mDefSetupMemb(bool,annoty)
	mDefSetupMemb(bool,annoty2)
	mDefSetupMemb(bool,noxaxis)
	mDefSetupMemb(bool,noyaxis)
	mDefSetupMemb(bool,noy2axis)
	mDefSetupMemb(bool,noxgridline)
	mDefSetupMemb(bool,noygridline)
	mDefSetupMemb(bool,noy2gridline)
	mDefSetupMemb(int,pointsz)		//!< If > 0, points are drawn
	mDefSetupMemb(bool,editable)		//!< Add/remove/change Y1 pts
	mDefSetupMemb(bool,fillbelow)		//!< Y1 will get fill
	mDefSetupMemb(bool,fillbelowy2)		//!< Y2 will get fill
	mDefSetupMemb(bool,drawliney)		//!< Y1 will be polylines
	mDefSetupMemb(bool,drawliney2)		//!< Y2 will be polylines
	mDefSetupMemb(bool,drawscattery1)		//!< draw Markers
	mDefSetupMemb(bool,drawscattery2)		//!< draw Markers
	mDefSetupMemb(bool,closepolygon)
	mDefSetupMemb(bool,drawborder)
	mDefSetupMemb(float,ptsnaptol)		//!< Snap tol ratio of axis size
    };

				uiFunctionDisplay(uiParent*,const Setup&);
				~uiFunctionDisplay();

    void			setVals(const float* xvals,
	    				const float* yvals,int sz);
    void			setVals(const Interval<float>&,
	    				const float* yvals,int sz);
    void			setY2Vals(const float* xvals,
	    				  const float* yvals,int sz);
    void			setMarkValue(float,bool is_x);

    const TypeSet<float>&	xVals() const	{ return xvals_; }
    const TypeSet<float>&	yVals() const	{ return yvals_; }
    uiAxisHandler*		xAxis()		{ return xax_; }
    uiAxisHandler*		yAxis( bool y2) { return y2 ? y2ax_ : yax_; }
    Setup&			setup()		{ return setup_; }

    int				size() const	{ return xvals_.size(); }

    Notifier<uiFunctionDisplay>	pointSelected;
    Notifier<uiFunctionDisplay>	pointChanged;
    int				selPt() const	{ return selpt_; }

    void			gatherInfo();
    void			draw();

protected:

    Setup			setup_;
    uiAxisHandler*		xax_;
    uiAxisHandler*		yax_;
    uiAxisHandler*		y2ax_;
    uiGraphicsItem*		ypolyitem_;
    uiGraphicsItem*		y2polyitem_;
    uiPolygonItem*		ypolygonitem_;
    uiPolygonItem*		y2polygonitem_;
    uiPolyLineItem*		ypolylineitem_;
    uiPolyLineItem*		y2polylineitem_;
    uiRectItem*			borderrectitem_;
    uiGraphicsItemGroup*	ymarkeritems_;
    uiGraphicsItemGroup*	y2markeritems_;
    TypeSet<float>		xvals_;
    TypeSet<float>		yvals_;
    TypeSet<float>		y2yvals_;
    TypeSet<float>		y2xvals_;
    float			xmarkval_;
    float			ymarkval_;
    int				selpt_;
    bool			mousedown_;


    void			mousePress(CallBacker*);
    void			mouseRelease(CallBacker*);
    void			mouseMove(CallBacker*);
    void			mouseDClick(CallBacker*);

    void			setUpAxis(bool y2);
    void			getPointSet(TypeSet<uiPoint>&,bool y2);
    void			drawYCurve(const TypeSet<uiPoint>&);
    void			drawY2Curve(const TypeSet<uiPoint>&,bool havy2);
    void			drawMarker(const TypeSet<uiPoint>&,
	    				   bool y2=false);
    void			drawBorder();
    bool			setSelPt();
    void			reSized( CallBacker* );
    void			getRanges(const TypeSet<float>&,
	    				  const TypeSet<float>&,
					  const Interval<float>&,
					  const Interval<float>&,
					  StepInterval<float>&,
					  StepInterval<float>&) const;
};


#endif
