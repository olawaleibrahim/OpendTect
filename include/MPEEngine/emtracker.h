#ifndef emtracker_h
#define emtracker_h
                                                                                
/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        A.H. Bril
 Date:          23-10-1996
 RCS:           $Id: emtracker.h,v 1.1 2005-01-06 09:25:55 kristofer Exp $
________________________________________________________________________

-*/

#include "sets.h"
#include "emposid.h"


namespace Geometry { class Element; };
namespace EM { class EMObject; };


namespace MPE
{


class SectionTracker;
class TrackPlane;

class EMTracker
{
public:
    			EMTracker();
    virtual		~EMTracker();

    virtual const char*	objectName() const				= 0;
    virtual EM::ObjectID objectID() const				= 0;

    virtual bool	isEnabled() const { return isenabled; }
    virtual void	enable(bool yn) { isenabled=yn; }
    virtual bool	setSeeds( const ObjectSet<Geometry::Element>&,
	    			  const char* name )			= 0;
    virtual bool	trackSections( const TrackPlane& );
    virtual bool	trackIntersections( const TrackPlane& );

    const char*		errMsg() const;

protected:
    bool				isenabled;
    ObjectSet<SectionTracker>		sectiontrackers;
    BufferString			errmsg;
};


typedef EMTracker*(*EMTrackerCreationFunc)(EM::EMObject*);


class TrackerFactory
{
public:
			TrackerFactory( const char* emtype,
					EMTrackerCreationFunc );
    const char*		emObjectType() const;
    EMTracker*		create( EM::EMObject* emobj = 0 ) const;

protected:
    EMTrackerCreationFunc	createfunc;
    const char*			type;

};

}; //Namespace

#endif

