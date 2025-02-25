#pragma once

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		Oct 2010
________________________________________________________________________


-*/

#include "stratmod.h"
#include "objectset.h"
#include "multiid.h"
#include "propertyref.h"
#include "iopar.h"
#include "od_iosfwd.h"
#include "uistring.h"

namespace Strat
{
class RefTree;
class LayerSequence;
class LayerGenerator;


/*!\brief Collection of LayerGenerator's that can generate a full LayerSequence.

  The 'modpos' that generate() wants needs to be  0 for the first, and 1 for
  the last model to be generated (linear in between). For one model only,
  specify 0.5.

 */

mExpClass(Strat) LayerSequenceGenDesc : public ObjectSet<LayerGenerator>
{ mODTextTranslationClass(LayerSequenceGenDesc);
public:
			LayerSequenceGenDesc(const RefTree&);
			LayerSequenceGenDesc(const LayerSequenceGenDesc&);
			~LayerSequenceGenDesc();

    const RefTree&	refTree() const		{ return rt_; }
    IOPar&		getWorkBenchParams()	{ return workbenchparams_; }

    const PropertyRefSelection& propSelection() const	{ return propsel_; }
    void		setPropSelection(const PropertyRefSelection&);
    float		startDepth() const	{ return startdepth_; }
    void		setStartDepth( float z)	{ startdepth_ = z; }
    const MultiID&	elasticPropSel() const;
    void		setElasticPropSel(const MultiID&);

    bool		getFrom(od_istream&);
    bool		putTo(od_ostream&) const;

    bool		prepareGenerate() const;
    bool		generate(LayerSequence&,float modpos) const;

    uiString		errMsg() const			{ return errmsg_; }

    const char*		userIdentification(int) const;
    int			indexFromUserIdentification(const char*) const;
    LayerSequenceGenDesc& operator=(const LayerSequenceGenDesc&);

    void		erase() override		{ deepErase(*this); }

protected:

    IOPar		workbenchparams_;

    const RefTree&	rt_;
    PropertyRefSelection propsel_;
    MultiID		elasticpropselmid_;
    float		startdepth_ = 0.f;

    static const char*	sKeyWorkBenchParams();
    mutable uiString	errmsg_;

};


}; // namespace Strat

