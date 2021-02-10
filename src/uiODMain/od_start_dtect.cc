/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Ranojay Sen
 Date:          Mar 2012
________________________________________________________________________

-*/

#include "prog.h"

#include "oscommand.h"



int mProgMainFnName( int argc, char** argv )
{
    mInitProg( OD::BatchProgCtxt )
    SetProgramArgs( argc, argv );
    OS::MachineCommand machcomm( "od_main" );
    for ( int iarg=1; iarg<argc; iarg++ )
	machcomm.addArg( iarg );
    return machcomm.execute( OS::RunInBG ) ? 0 : 1;
}
