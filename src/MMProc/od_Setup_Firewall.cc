/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Prajjaval Singh
 * DATE     : May 2019
-*/

#include "mmprocmod.h"

#include "commandlineparser.h"
#include "filepath.h"
#include "keystrs.h"
#include "oddirs.h"
#include "oscommand.h"
#include "pythonaccess.h"
#include "settings.h"
#include "winutils.h"

#include "prog.h"

static const int cProtocolNr = 1;

static const char* sAddStr	= "add";
static const char* sRemoveStr	= "remove";
static const char* sPythonStr	= "py";
static const char* sODStr	= "od";

/*
Command --add --py <procnm1.exe> <procnm2.exe> : Python related
Command --add --od <procnm1.exe> <procnm2.exe> :  OpendTect related
*/

class SetUpFirewallServerTool
{
public:
			SetUpFirewallServerTool()
			{ createDirPaths(); }
    bool		handleProcess(BufferString&, bool);
    bool		ispyproc_;
    void		updateDirPath(FilePath*);
protected:
    void		createDirPaths();

    FilePath		pypath_;
    FilePath		odpath_;

};


void SetUpFirewallServerTool::createDirPaths()
{
    odpath_ = GetExecPlfDir();

    BufferString strcheck = odpath_.fullPath();

    BufferString pythonstr( sKey::Python() ); pythonstr.toLower();
    const IOPar& pythonsetts = Settings::fetch( pythonstr );
    OD::PythonSource source;
    BufferString pythonloc;
    const bool pythonsource = OD::PythonSourceDef().parse( pythonsetts,
	OD::PythonAccess::sKeyPythonSrc(), source );

    if ( !pythonsource ||
	!pythonsetts.get(OD::PythonAccess::sKeyEnviron(), pythonloc) )
	return;

    pypath_ = pythonloc;
    pypath_.add( "envs" );
}


void SetUpFirewallServerTool::updateDirPath( FilePath* fp )
{
    if ( !fp )
	return;

    if ( ispyproc_ )
    {
	pypath_ = FilePath::getFullLongPath( *fp );
	pypath_.add( "envs" );
    }
    else
	odpath_ = FilePath::getFullLongPath( *fp );
}


bool SetUpFirewallServerTool::handleProcess( BufferString& procnm, bool toadd )
{
    BufferString exenm = procnm;
    FilePath fp( ispyproc_ ? pypath_ : odpath_ );

    if ( fp.isEmpty() )
	return false;

    if ( ispyproc_ )
	fp.add( procnm ).add( "python.exe" );
    else
    {
	exenm.add( ".exe" );
	fp.add( exenm );
    }

    OS::MachineCommand mc( "netsh", "advfirewall", "firewall" );
    mc.addArg( toadd ? "add" : "delete" )
       .addArg( "rule" )
       .addArg( BufferString("name=\"",procnm,"\"") );
    char shortpath[1024];
    GetShortPathName( fp.fullPath().buf(), shortpath, 1024 );
    mc.addArg( BufferString("program=\"",shortpath,"\"") );
    if ( toadd )
    {
	mc.addArg( "enable=yes" );
	mc.addArg( "dir=in" );
	mc.addArg( "action=allow" );
    }

    OS::CommandExecPars pars( OS::RunInBG );
    pars.runasadmin( !WinUtils::IsUserAnAdmin() );
    return mc.execute( pars );
}


int mProgMainFnName( int argc, char** argv )
{
    mInitProg( OD::BatchProgCtxt )
    SetProgramArgs( argc, argv );
    CommandLineParser parser;
    SetUpFirewallServerTool progtool;
    BufferStringSet procnms;
    parser.getNormalArguments( procnms );
    const bool ispyproc = parser.hasKey( sODStr ) ? false : true;
    progtool.ispyproc_ = ispyproc;
    const bool toadd = parser.hasKey( sAddStr );
    if ( !toadd && !parser.hasKey(sRemoveStr) )
	return 1;

    for ( int procidx=0; procidx<procnms.size(); procidx++ )
    {
	if ( procidx==0 ) // index 0 stores path
	{
	    progtool.updateDirPath( new FilePath(procnms.get(procidx)) );
	    continue;
	}

	progtool.handleProcess( procnms.get(procidx), toadd );
    }

    return 0;
}
