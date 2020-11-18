/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : A.H. Bril
 * DATE     : Oct 1995
 * FUNCTION : Stream Provider functions
-*/

static const char* rcsID mUsedVar = "$Id$";

#include "oscommand.h"

#include "commandlineparser.h"
#include "envvars.h"
#include "file.h"
#include "filepath.h"
#include "genc.h"
#include "od_iostream.h"
#include "oddirs.h"
#include "perthreadrepos.h"
#include "pythonaccess.h"
#include "separstr.h"
#include "uistrings.h"

#ifndef OD_NO_QT
# include "qstreambuf.h"
# include <QProcess>
#endif

#include <iostream>

#ifdef __win__
# include "shellapi.h"
# include "winutils.h"
# include <windows.h>
# include <stdlib.h>
#endif

BufferString OS::MachineCommand::defremexec_( "ssh" );
static const char* sODProgressViewerProgName = "od_ProgressViewer";
static const char* sKeyExecPars = "ExecPars";
static const char* sKeyMonitor = "Monitor";
static const char* sKeyProgType = "ProgramType";
static const char* sKeyPriorityLevel = "PriorityLevel";
static const char* sKeyWorkDir = "WorkingDirectory";

//
class QProcessManager
{
public:
		~QProcessManager()
		{
		    deleteProcesses();
		}
    void	takeOver( QProcess* p )
		{
		    processes_ += p;
		}
    void	deleteProcesses()
		{
#ifndef OD_NO_QT
		    mObjectSetApplyToAll( processes_, processes_[idx]->close());
		    deepErase( processes_ );
#endif
		}


    static Threads::Lock	lock_;

private:
    ObjectSet<QProcess>		processes_;
};

static PtrMan<QProcessManager> processmanager = 0;
Threads::Lock QProcessManager::lock_( true );

void DeleteProcesses()
{
    Threads::Locker locker( QProcessManager::lock_ );
    if ( processmanager )
	processmanager->deleteProcesses();
}

void OS::CommandLauncher::manageQProcess( QProcess* p )
{
    Threads::Locker locker( QProcessManager::lock_ );

    if ( !processmanager )
    {
	processmanager = new QProcessManager;
	NotifyExitProgram( DeleteProcesses );
    }

    processmanager->takeOver( p );
}



OS::CommandExecPars::CommandExecPars( bool isbatchprog )
    : CommandExecPars( isbatchprog ? RunInBG : Wait4Finish )
{}


void OS::CommandExecPars::usePar( const IOPar& iop )
{
    PtrMan<IOPar> subpar = iop.subselect( sKeyExecPars );
    if ( !subpar || subpar->isEmpty() )
	return;

    FileMultiString fms;
    subpar->get( sKeyMonitor, fms );
    int sz = fms.size();
    if ( sz > 0 )
    {
	needmonitor_ = toBool( fms[0] );
	monitorfnm_ = fms[1];
    }

    fms.setEmpty();
    subpar->get( sKeyProgType, fms );
    sz = fms.size();
    if ( sz > 0 )
    {
	launchtype_ = *fms[0] == 'W' ? Wait4Finish
		    : (fms[0] == "BG" ? RunInBG
				      : (fms[0] == "BW" ? BatchWait : Batch) );
	isconsoleuiprog_ = *fms[0] == 'C';
    }

    subpar->get( sKeyPriorityLevel, prioritylevel_ );

    BufferString workdir;
    if ( subpar->get(sKeyWorkDir,workdir) && !workdir.isEmpty() )
	workingdir_.set( workdir );
}


void OS::CommandExecPars::fillPar( IOPar& iop ) const
{
    iop.removeSubSelection( sKeyExecPars );
    IOPar subiop;

    FileMultiString fms;
    fms += needmonitor_ ? "Yes" : "No";
    fms += monitorfnm_;
    subiop.set( sKeyMonitor, fms );

    fms = launchtype_ == Wait4Finish ? "Wait"
	   : (launchtype_ == RunInBG ? "BG"
	       : (launchtype_ == Batch ? "Batch" : "BW") );
    fms += isconsoleuiprog_ ? "ConsoleUI" : "";
    subiop.set( sKeyProgType, fms );

    subiop.set( sKeyPriorityLevel, prioritylevel_ );
    if ( !workingdir_.isEmpty() )
	subiop.set( sKeyWorkDir, workingdir_ );

    iop.mergeComp( subiop, sKeyExecPars );
}


void OS::CommandExecPars::removeFromPar( IOPar& iop ) const
{
    iop.removeWithKeyPattern( BufferString(sKeyExecPars,".*") );
}


const StepInterval<int> OS::CommandExecPars::cMachineUserPriorityRange(
					     bool iswin )
{ return iswin ? StepInterval<int>( 6, 8, 1 ) :StepInterval<int>( 0, 19, 1 ); }


int OS::CommandExecPars::getMachinePriority( float priority, bool iswin )
{
    const StepInterval<int> machpriorg( cMachineUserPriorityRange(iswin) );
    const float scale = iswin ? 1.f : -1.f;

    int machprio = mCast(int, mNINT32(scale * priority * machpriorg.width()) );

    return machprio += iswin ? machpriorg.stop : machpriorg.start;
}



OS::MachineCommand::MachineCommand( const char* prognm, const char* arg1,
       const char* arg2,const char* arg3, const char* arg4,const char* arg5 )
    : MachineCommand(prognm)
{
    addArg( arg1 ).addArg( arg2 ).addArg( arg3 ).addArg( arg4 ).addArg( arg5 );
}


OS::MachineCommand::MachineCommand( const char* prognm, bool isolated )
{
    if ( isolated )
	setIsolated( prognm );
    else
	*this = MachineCommand( prognm );
}


OS::MachineCommand::MachineCommand( const MachineCommand& oth, bool isolated )
{
    if ( &oth == this )
	return;

    if ( isolated )
    {
	setIsolated( oth.program() );
	addArgs( oth.args() );
    }
    else
	*this = MachineCommand( oth );
}


OS::MachineCommand& OS::MachineCommand::addArg( const char* str )
{
    if ( str && *str )
	args_.add( str );
    return *this;
}


OS::MachineCommand& OS::MachineCommand::addFileRedirect( const char* fnm,
						int stdcode, bool append )
{
    BufferString redirect;
    if ( stdcode == 1 )
	redirect.add( 1 );
    else if ( stdcode == 2 )
	redirect.add( 2 );
    redirect.add( ">" );
    if ( append )
	redirect.add( ">" );

    return addArg( redirect ).addArg( fnm );
}


OS::MachineCommand& OS::MachineCommand::addArgs( const BufferStringSet& toadd )
{
    args_.append( toadd );
    return *this;
}


OS::MachineCommand& OS::MachineCommand::addKeyedArg( const char* ky,
			 const char* str, KeyStyle ks )
{
    if ( isOldStyle(ks) )
	addArg( BufferString( "-", ky ) );
    else
	addArg( CommandLineParser::createKey(ky) );
    addArg( str );
    return *this;
}


void OS::MachineCommand::setIsolated( const char* prognm )
{
    BufferString scriptcmd( GetODExternalScript() );
    prognm_.set( scriptcmd );
    if ( prognm && *prognm )
	args_.insertAt( new BufferString(prognm), 0 );
    const BufferString pathed( GetEnvVarDirListWoOD("PATH") );
    if ( !pathed.isEmpty() )
	SetEnvVar( "OD_INTERNAL_CLEANPATH", pathed.buf() );

#ifdef __unix__
    if ( GetEnvVar("OD_SYSTEM_LIBRARY_PATH") )
	return;

    const BufferString ldlibpathed( GetEnvVarDirListWoOD("LD_LIBRARY_PATH") );
    if ( !ldlibpathed.isEmpty() )
	SetEnvVar( "OD_SYSTEM_LIBRARY_PATH", ldlibpathed.buf() );
#endif
}


#ifdef __win__

static BufferString getUsableWinCmd( const char* fnm, BufferStringSet& args )
{
    BufferString ret( fnm );

    BufferString execnm( fnm );
    char* ptr = execnm.find( ':' );
    if ( !ptr )
	return ret;

    char* argsptr = nullptr;

    // if only one char before the ':', it must be a drive letter.
    if ( ptr == execnm.buf() + 1 )
    {
	ptr = firstOcc( ptr , ' ' );
	if ( ptr ) { *ptr = '\0'; argsptr = ptr+1; }
    }
    else if ( ptr == execnm.buf()+2)
    {
	char sep = *execnm.buf();
	if ( sep == '\"' || sep == '\'' )
	{
	    execnm=fnm+1;
	    ptr = execnm.find( sep );
	    if ( ptr ) { *ptr = '\0'; argsptr = ptr+1; }
	}
    }
    else
	return ret;

    if ( execnm.contains(".exe") || execnm.contains(".EXE")
       || execnm.contains(".bat") || execnm.contains(".BAT")
       || execnm.contains(".com") || execnm.contains(".COM") )
	return ret;

    const char* interp = 0;

    if ( execnm.contains(".csh") || execnm.contains(".CSH") )
	interp = "tcsh.exe";
    else if ( execnm.contains(".sh") || execnm.contains(".SH") ||
	      execnm.contains(".bash") || execnm.contains(".BASH") )
	interp = "sh.exe";
    else if ( execnm.contains(".awk") || execnm.contains(".AWK") )
	interp = "awk.exe";
    else if ( execnm.contains(".sed") || execnm.contains(".SED") )
	interp = "sed.exe";
    else if ( File::exists( execnm ) )
    {
	// We have a full path to a file with no known extension,
	// but it exists. Let's peek inside.

	od_istream strm( execnm );
	if ( !strm.isOK() )
	    return ret;

	char buf[41];
	strm.getC( buf, 40 );
	BufferString line( buf );

	if ( !line.contains("#!") && !line.contains("# !") )
	    return ret;

	if ( line.contains("csh") )
	    interp = "tcsh.exe";
	else if ( line.contains("awk") )
	    interp = "awk.exe";
	else if ( line.contains("sh") )
	    interp = "sh.exe";
    }

    if ( !interp )
	return ret;

    FilePath interpfp;
    const char* cygdir = getCygDir();
    if ( cygdir && *cygdir )
    {
	interpfp.set( cygdir );
	interpfp.add( "bin" ).add( interp );
    }

    if ( !File::exists( interpfp.fullPath() ) )
    {
	interpfp.set( GetSoftwareDir(true) );
	interpfp.add("bin").add("win").add("sys").add(interp);
    }

    ret.set( interpfp.fullPath() );
    if ( argsptr && *argsptr )
	args.unCat( argsptr, " " );
    args.insertAt( new BufferString(
		FilePath(execnm).fullPath(FilePath::Unix)), 0 );

    return ret;
}

#endif


static BufferString getUsableUnixCmd( const char* fnm, BufferStringSet& )
{
    BufferString ret( fnm );
    if ( FilePath(fnm).isAbsolute() &&
	 File::exists(fnm) && File::isFile(fnm) )
	return ret;

    ret = GetShellScript( fnm );
    if ( File::exists(ret) && File::isFile(fnm) )
	return ret;

    ret = GetPythonScript( fnm );
    if ( File::exists(ret) && File::isFile(fnm) )
	return ret;

    return BufferString( fnm );
}


static BufferString getUsableCmd( const char* fnm, BufferStringSet& args )
{
#ifdef __win__
    return getUsableWinCmd( fnm, args );
#else
    return getUsableUnixCmd( fnm, args );
#endif
}


OS::MachineCommand OS::MachineCommand::getExecCommand(
					const CommandExecPars* pars ) const
{
    MachineCommand ret;

    if ( pars && pars->isconsoleuiprog_ )
    {
#ifdef __unix__
	const BufferString str(
	    FilePath( GetSoftwareDir(true), "bin",
		    "od_exec_consoleui.scr" ).fullPath() );
	ret.setProgram( FilePath( GetSoftwareDir(true), "bin",
				    "od_exec_consoleui.scr" ).fullPath() );
#endif
    }

    BufferStringSet mcargs;
    const BufferString prognm = getUsableCmd( prognm_, mcargs );
    const BufferString localhostnm( GetLocalHostName() );
    if ( remexec_.isEmpty() || hname_.isEmpty() || hname_ == localhostnm )
    {
	if ( ret.isBad() )
	    ret.setProgram( prognm );
	else
	    ret.addArg( prognm );
    }
    else
    {
	if ( ret.isBad() )
	    ret.setProgram( remexec_ );
	else
	    ret.addArg( remexec_ );
	if ( remexec_ == odRemExecCmd() )
	{
	    ret.addKeyedArg( sKeyRemoteHost(), hname_ )
	       .addFlag( sKeyRemoteCmd() );
	}
	else
	{
	    ret.addArg( hname_.str() );
	    if ( prognm.startsWith("od_") )
		ret.addArg( FilePath(GetShellScript("exec_prog")).fullPath() );
	}
	ret.addArg( prognm );
    }

    ret.addArgs( mcargs );
    ret.addArgs( args_ );

    if ( pars && pars->launchtype_ != Wait4Finish &&
	 !mIsZero(pars->prioritylevel_,1e-2f) )
	ret.addKeyedArg( CommandExecPars::sKeyPriority(),pars->prioritylevel_);
    ret.addShellIfNeeded();

    return ret;
}


void OS::MachineCommand::addShellIfNeeded()
{
    bool needsshell = prognm_.startsWith( "echo", CaseInsensitive );
    if ( !needsshell )
    {
	for ( int idx=0; idx<args_.size(); idx++ )
	{
	    const auto* arg = args_[idx];
	    if ( arg->find(">") || arg->find("<") || arg->find("|") )
	    {
		needsshell = true;
		break;
	    }
	}
    }

    if ( !needsshell )
	return;

    args_.insertAt( new BufferString(prognm_), 0 );
#ifdef __win__
    args_.insertAt( new BufferString("/c"), 0 );
    prognm_.set( "cmd" );
#else
    prognm_.set( "/bin/sh" );
    for ( int idx=0; idx<args_.size(); idx++ )
    {
	auto* arg = args_[idx];
	if ( arg->find(' ') && arg->firstChar() != '\'' )
	    arg->quote();
    }
    const BufferString cmdstr( args_.cat(" ") );
    args_.setEmpty();
    args_.add( "-c" ).add( cmdstr.buf() );
    // The whole command as one arguments. Quotes will be added automatically
#endif
}


BufferString OS::MachineCommand::toString( const OS::CommandExecPars* pars
									) const
{
    const MachineCommand mc = getExecCommand( pars );
    BufferString ret( mc.program() );
    if ( !mc.args().isEmpty() )
	ret.addSpace().add( mc.args().cat( " " ) );
    return ret;
}


bool OS::MachineCommand::execute( LaunchType lt, const char* workdir )
{
    return CommandLauncher(*this).execute( lt, workdir );
}


bool OS::MachineCommand::execute( BufferString& out, BufferString* err,
				  const char* workdir )
{
    return CommandLauncher(*this).execute( out, err, workdir );
}


bool OS::MachineCommand::execute( const CommandExecPars& execpars )
{
    return CommandLauncher(*this).execute( execpars );
}



BufferString OS::MachineCommand::runAndCollectOutput( BufferString* errmsg )
{
    BufferString ret;
    if ( !CommandLauncher(*this).execute(ret,errmsg) )
	ret.setEmpty();
    return ret;
}


// OS::CommandLauncher

OS::CommandLauncher::CommandLauncher( const OS::MachineCommand& mc )
    : odprogressviewer_(FilePath(GetExecPlfDir(),sODProgressViewerProgName)
			    .fullPath())
    , process_( 0 )
    , stderror_( 0 )
    , stdoutput_( 0 )
    , stdinput_( 0 )
    , stderrorbuf_( 0 )
    , stdoutputbuf_( 0 )
    , stdinputbuf_( 0 )
    , pid_( 0 )
    , machcmd_(mc)
{
    reset();
}


OS::CommandLauncher::~CommandLauncher()
{
#ifndef OD_NO_QT
    reset();
#endif
}


PID_Type OS::CommandLauncher::processID() const
{
#ifndef OD_NO_QT
    if ( !process_ )
	return pid_;
# ifdef __win__
    const PROCESS_INFORMATION* pi = (PROCESS_INFORMATION*) process_->pid();
    return pi->dwProcessId;
# else
    return process_->pid();
# endif
#else
    return 0;
#endif
}


void OS::CommandLauncher::reset()
{
    deleteAndZeroPtr( stderror_ );
    deleteAndZeroPtr( stdoutput_ );
    deleteAndZeroPtr( stdinput_ );

    stderrorbuf_ = 0;
    stdoutputbuf_ = 0;
    stdinputbuf_ = 0;

#ifndef OD_NO_QT
    if ( process_ && process_->state()!=QProcess::NotRunning )
    {
	manageQProcess( process_ );
	process_ = 0;
    }
    deleteAndZeroPtr( process_ );
#endif
    errmsg_.setEmpty();
    monitorfnm_.setEmpty();
    progvwrcmd_.setEmpty();
    redirectoutput_ = false;
}


void OS::CommandLauncher::adaptForV7( const OS::MachineCommand& mc )
{
    if ( !FixedString(mc.program()).startsWith("od_deeplearn_apply") )
	return;

    const FilePath v7binfp( GetSoftwareDir(true), "v7", "bin" );
    const FilePath extscriptfp( GetODExternalScript() );
    const FilePath extscript7fp( v7binfp, extscriptfp.fileName() );
    if ( !extscript7fp.exists() )
	return;

    const BufferString execnm( mc.program() );
    FilePath scriptfp( v7binfp );
#ifdef __win__
    scriptfp.add( GetPlfSubDir() ).add( GetBinSubDir() ).add( execnm );
    scriptfp.setExtension( "exe" );
#else
    scriptfp.add( "exec_prog" );
    if ( !scriptfp.exists() )
    {
	scriptfp.set( scriptfp.dirUpTo(scriptfp.nrLevels()-2) );
	scriptfp.add( GetPlfSubDir() ).add( GetBinSubDir() ).add( execnm );
    }
#endif
    if ( !scriptfp.exists() )
	return;

    machcmd_ = OS::MachineCommand( extscript7fp.fullPath(), true );
    machcmd_.addArg( scriptfp.fullPath() );
#ifdef __unix__
    if ( scriptfp.fileName() == "exec_prog" )
	machcmd_.addArg( execnm );
#endif
    machcmd_.addArgs( mc.args() );
}


void OS::CommandLauncher::set( const OS::MachineCommand& cmd )
{
    adaptForV7( cmd );
    reset();
}


bool OS::CommandLauncher::execute( OS::LaunchType lt, const char* workdir )
{
    CommandExecPars execpars( lt );
    if ( workdir && *workdir )
	execpars.workingdir( workdir );
    return execute( execpars );
}


bool OS::CommandLauncher::execute( BufferString& out, BufferString* err,
				   const char* workdir )
{
    CommandExecPars execpars( Wait4Finish );
    execpars.createstreams( true );
    if ( workdir && *workdir )
	execpars.workingdir( workdir );

    if ( !execute(execpars) )
	return false;

    getStdOutput()->getAll( out );
    if ( err )
	getStdError()->getAll( *err );
    return true;
}


bool OS::CommandLauncher::execute( const OS::CommandExecPars& pars )
{
    reset();
    if ( machcmd_.isBad() )
	{ errmsg_ = toUiString("Command is invalid"); return false; }

    if ( FixedString(machcmd_.program()).contains("python") )
    {
	const FilePath pythfp( machcmd_.program() );
	if ( pythfp.nrLevels() < 2 ||
	    (pythfp.exists() && pythfp.fileName().contains("python") ) )
	    pErrMsg("Python commands should be run using OD::PythA().execute");
    }

    const MachineCommand mcmd = machcmd_.getExecCommand( &pars );
    if ( mcmd.isBad() )
	{ errmsg_ = toUiString("Empty command to execute"); return false; }

    if ( pars.needmonitor_ && !pars.isconsoleuiprog_ )
    {
	monitorfnm_ = pars.monitorfnm_;
	if ( monitorfnm_.isEmpty() )
	{
	    monitorfnm_ = FilePath::getTempFullPath( "mon", "txt" );
	    redirectoutput_ = true;
	}

	if ( File::exists(monitorfnm_) && !File::remove(monitorfnm_) )
	    return false;
    }

    const bool ret = doExecute( mcmd, pars );
    uiString cannotlaunchstr = toUiString( "Cannot launch '%1'" );
    if ( pars.isconsoleuiprog_ )
    {
	if ( errmsg_.isEmpty() )
	    errmsg_ = cannotlaunchstr.arg( mcmd.toString(&pars) );
	return ret;
    }

    if ( !ret )
    {
	if ( errmsg_.isEmpty() )
	    errmsg_ = cannotlaunchstr.arg( mcmd.toString(&pars) );
	return false;
    }

    if ( pars.launchtype_ != Wait4Finish )
	startMonitor();

    return ret;
}


bool OS::CommandLauncher::startServer( bool ispyth, double waittm )
{
    CommandExecPars execpars( RunInBG );
    execpars.createstreams_ = true;
	// this has to be done otherwise we cannot pick up any error messages
    pid_ = -1;
    if ( ispyth )
    {
	if ( !OD::PythA().execute(machcmd_,execpars,&pid_,&errmsg_) )
	    pid_ = -1;
    }
    else
    {
	if ( !execute(execpars) )
	    pid_ = -1;
    }

    if ( pid_ < 1 )
    {
	if ( errmsg_.isEmpty() )
	    errmsg_ = uiStrings::phrCannotStart(
				::toUiString(machcmd_.toString(&execpars) ) );
	return false;
    }

    bool wasalive = false;
    const double waitstepsec = 0.1;
    while ( waittm > 0 )
    {
	if ( isProcessAlive(pid_) )
	{
	    wasalive = true;
	    break;
	}
	waittm -= waitstepsec;
	sleepSeconds( waitstepsec );
    }

    if ( !wasalive && !isProcessAlive(pid_) )
    {
	if ( ispyth )
	    errmsg_ = toUiString(OD::PythA().lastOutput(true,nullptr));

	if ( errmsg_.isEmpty() )
	    errmsg_ = tr("Server process (%1) exited early")
			    .arg( machcmd_.toString(&execpars) );
	return false;
    }

    return true;
}


void OS::CommandLauncher::startMonitor()
{
    if ( monitorfnm_.isEmpty() )
	return;

    MachineCommand progvwrcmd( odprogressviewer_ );
    progvwrcmd.addKeyedArg( "inpfile", monitorfnm_ );
    progvwrcmd.addKeyedArg( "pid", processID() );

    OS::CommandLauncher progvwrcl( progvwrcmd );
    if ( !progvwrcl.execute(RunInBG) )
    {
	// sad ... but the process has been launched anyway
	ErrMsg( BufferString( "[Monitoring does not start] ",
				 progvwrcl.errorMsg() ) );
    }
}


bool OS::CommandLauncher::doExecute( const MachineCommand& mc,
				     const CommandExecPars& pars )
{
    if ( mc.isBad() )
	{ errmsg_ = tr("Command is empty"); return false; }

#ifdef __win__
    if ( pars.runasadmin_ )
    {
        BufferString argsstr;
        for ( int idx=0; idx<mc.args().size(); idx++ )
        {
            BufferString arg( mc.args().get(idx) );
            if ( arg.find(" ") )
                arg.quote('\"');
            if ( !argsstr.isEmpty() )
                argsstr.addSpace();
            argsstr.add( arg );
        }
        const HINSTANCE res = ShellExecuteA( NULL, "runas", mc.program(),
            argsstr, pars.workingdir_, SW_SHOW );
        return static_cast<int>(reinterpret_cast<uintptr_t>(res)) >
							    HINSTANCE_ERROR;
    }
#endif

    if ( process_ )
    {
	errmsg_ = tr("Process is already running ('%1')")
				.arg( mc.toString(&pars) );
	return false;
    }

    DBG::message( DBG_DBG,
        BufferString("About to execute: ",mc.toString(&pars)) );

    const bool wt4finish = pars.launchtype_ == Wait4Finish;
    const bool createstreams = pars.createstreams_;
#ifndef OD_NO_QT
    process_ = wt4finish || createstreams ? new QProcess : 0;

    if ( createstreams )
    {
	if ( !monitorfnm_.isEmpty() )
	    process_->setStandardOutputFile( monitorfnm_.buf() );
	else
	{
	    stdinputbuf_ = new qstreambuf( *process_, false, false );
	    stdinput_ = new od_ostream( new oqstream( stdinputbuf_ ) );

	    stdoutputbuf_ = new qstreambuf( *process_, false, false  );
	    stdoutput_ = new od_istream( new iqstream( stdoutputbuf_ ) );

	    stderrorbuf_ = new qstreambuf( *process_, true, false  );
	    stderror_ = new od_istream( new iqstream( stderrorbuf_ ) );
	}
    }
    BufferString str = mc.toString();
    const BufferString& workingdir = pars.workingdir_;
    if ( process_ )
    {
	if ( !workingdir.isEmpty() )
	{
	    const QString qworkdir( workingdir );
	    process_->setWorkingDirectory( qworkdir );
	}
	//TODO: use inconsole on Windows ?
	const QString qprog( mc.program() );
	QStringList qargs;
	mc.args().fill( qargs );
	process_->start( qprog, qargs, QIODevice::ReadWrite );
    }
    else
    {
	const bool res = startDetached( mc, pars.isconsoleuiprog_,
					workingdir );
	return res;
    }

    if ( !process_->waitForStarted(10000) ) //Timeout of 10 secs
    {
	return !catchError();
    }

    if ( wt4finish )
    {
	startMonitor();
	if ( process_->state()==QProcess::Running )
	    process_->waitForFinished(-1);

	const bool res = process_->exitStatus()==QProcess::NormalExit;

	if ( createstreams )
	{
	    stderrorbuf_->detachDevice( true );
	    stdoutputbuf_->detachDevice( true );
	    stdinputbuf_->detachDevice( false );
	}

	deleteAndZeroPtr( process_ );

	return res;
    }
#endif

    return true;
}


#ifndef OD_NO_QT
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)

namespace OS {

static bool startDetachedLegacy( const OS::MachineCommand& mc,
	    bool inconsole, const char* workdir, PID_Type& pid )
{
#ifdef __win__
    BufferString comm( mc.program() );
    if ( !mc.args().isEmpty() )
    {
	BufferStringSet args( mc.args() );
	for ( int idx=0; idx<args_.size(); idx++ )
	{
	    auto* arg = args_[idx];
	    if ( arg->find(" ") && !arg->startsWith("\"") &&
		!arg->startsWith("'") )
		arg->quote('\"');
	    comm.addSpace().add( arg->str() );
	}
    }

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(STARTUPINFO) );
    ZeroMemory( &pi, sizeof(pi) );
    si.cb = sizeof( STARTUPINFO );

    LPCSTR curdir = workdir && *workdir ? workdir : nullptr;
    const int wincrflg = inconsole ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;
    const bool res = CreateProcess( NULL,
	comm.getCStr(),
	NULL,	// Process handle not inheritable.
	NULL,	// Thread handle not inheritable.
	FALSE,	// Set handle inheritance.
	wincrflg,   // Creation flags.
	NULL,	// Use parent's environment block.
	curdir,   // Use parent's starting directory.
	&si, &pi );

    if ( res )
    {
	pid = (PID_Type)pi.dwProcessId;
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
    }

    return res;
#else
    const QString qprog( mc.program() );
    QStringList qargs;
    mc.args().fill( qargs );
    const QString qworkdir( workdir );
    qint64 qpid = 0;

    if ( !QProcess::startDetached(qprog,qargs,qworkdir,&qpid) )
	return false;

    pid = (PID_Type)qpid;

    return true;

#endif
}

} // namespace OS

#endif
#endif


bool OS::CommandLauncher::startDetached( const OS::MachineCommand& mc,
					 bool inconsole, const char* workdir )
{
    if ( mc.isBad() )
	return false;

#ifndef OD_NO_QT

#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)

    const QString qprog( mc.program() );
    QStringList qargs;
    mc.args().fill( qargs );
    const QString qworkdir( workdir );
    qint64 qpid = 0;

    QProcess qproc;
    qproc.setProgram( qprog );
    qproc.setArguments( qargs );
    if ( !qworkdir.isEmpty() )
	qproc.setWorkingDirectory( qworkdir );

#ifdef __win__
    if ( inconsole )
    {
	qproc.setCreateProcessArgumentsModifier(
	    [] (QProcess::CreateProcessArguments *args )
	{
	    args->flags |= CREATE_NEW_CONSOLE;
	    args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
	    args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
	    args->startupInfo->hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	    args->startupInfo->wShowWindow = SW_SHOW;
	});
    }
#endif

    if ( !qproc.startDetached(&qpid) )
	return false;

    pid_ = (PID_Type)qpid;
    return true;

#else

    return startDetachedLegacy( mc, inconsole, workdir, pid_ );

#endif

#else

    return false;

#endif
}


int OS::CommandLauncher::catchError()
{
    if ( !process_ )
	return 0;

    if ( errmsg_.isSet() )
	return 1;

#ifndef OD_NO_QT
    switch ( process_->error() )
    {
	case QProcess::FailedToStart :
	    errmsg_ = tr("Cannot start process %1.");
	    break;
	case QProcess::Crashed :
	    errmsg_ = tr("%1 crashed.");
	    break;
	case QProcess::Timedout :
	    errmsg_ = tr("%1 timeout");
	    break;
	case QProcess::ReadError :
	    errmsg_ = tr("Read error from process %1");
	    break;
	case QProcess::WriteError :
	    errmsg_ = tr("Write error from process %1");
	    break;
	default :
	    break;
    }

    if ( !errmsg_.isEmpty() )
    {
	BufferString argstr( machcmd_.program() );
	if ( machcmd_.hasHostName() )
	    argstr.add( " @ " ).add( machcmd_.hostName() );
	errmsg_.arg( argstr );
	return 1;
    }
    return process_->exitCode();
#else

    return 0;
#endif
}


void OD::DisplayErrorMessage( const char* msg )
{
    OS::MachineCommand machcomm( "od_DispMsg" );
    machcomm.addKeyedArg( "err", msg );
    machcomm.execute( OS::RunInBG );
}


bool OS::MachineCommand::setFromSingleStringRep( const char* inp,
						 bool ignorehostname )
{
    return false;
}



const char* OS::MachineCommand::getSingleStringRep() const
{
    mDeclStaticString( ret );
    ret.set( toString() );
    return ret.buf();
}

/*
const char* OS::MachineCommand::extractHostName( const char* str,
						 BufferString& hnm )
{
    hnm.setEmpty();
    if ( !str )
	return str;

    mSkipBlanks( str );
    BufferString inp( str );
    char* ptr = inp.getCStr();
    const char* rest = str;

#ifdef __win__

    if ( *ptr == '\\' && *(ptr+1) == '\\' )
    {
	ptr += 2;
	char* phnend = firstOcc( ptr, '\\' );
	if ( phnend ) *phnend = '\0';
	hnm = ptr;
	rest += hnm.size() + 2;
    }

#else

    while ( *ptr && !iswspace(*ptr) && *ptr != ':' )
	ptr++;

    if ( *ptr == ':' )
    {
	if ( *(ptr+1) == '/' && *(ptr+2) == '/' )
	{
	    inp.add( "\nlooks like a URL. Not supported (yet)" );
	    ErrMsg( inp ); rest += FixedString(str).size();
	}
	else
	{
	    *ptr = '\0';
	    hnm = inp;
	    rest += hnm.size() + 1;
	}
    }

#endif

    return rest;
}
*/


mStartAllowDeprecatedSection

void OS::CommandLauncher::addQuotesIfNeeded( BufferString& cmd )
{
    if ( !cmd.find(' ' ) )
	return;

    if ( cmd[0]=='"' )
	return;

    const char* quote = "\"";

    cmd.insertAt( 0, quote );
    cmd.add( quote );
}


void OS::CommandLauncher::addShellIfNeeded( BufferString& cmd )
{
    bool needsshell = cmd.find('|') || cmd.find('<') || cmd.find( '>' );
#ifdef __win__
    //Check if command starts with echo
    if ( !needsshell )
	needsshell = !strncasecmp( cmd.buf(), "echo", 4 );
#endif

    if ( needsshell )
    {
	if ( cmd.find( "\"" ) )
	{
	    pFreeFnErrMsg("Commands with quote-signs not supported");
	}

	const BufferString comm = cmd;
#ifdef __msvc__
	cmd = "cmd /c \"";
#else
	cmd = "sh -c \"";
#endif
	cmd.add( comm );
	cmd.add( "\"" );
    }
}


static bool doExecOSCmd( const char* cmd, OS::LaunchType ltyp, bool isbatchprog,
			 BufferString* stdoutput, BufferString* stderror )
{
    const OS::MachineCommand mc( cmd );
    OS::CommandLauncher cl( mc );
    OS::CommandExecPars cp( isbatchprog );
    cp.launchtype( ltyp ).createstreams( stdoutput || stderror );
    const bool ret = cl.execute( cp );
    if ( stdoutput )
	cl.getStdOutput()->getAll( *stdoutput );

    if ( stderror )
	cl.getStdError()->getAll( *stderror );

    return ret;
}


bool OS::ExecCommand( const char* cmd, OS::LaunchType ltyp, BufferString* out,
		      BufferString* err )
{
    if ( ltyp!=Wait4Finish )
    {
	out = 0;
	err = 0;
    }

    return doExecOSCmd( cmd, ltyp, false, out, err );
}


bool ExecODProgram( const char* prognm, const char* args, OS::LaunchType ltyp )
{
    BufferString cmd = prognm;
    OS::CommandLauncher::addQuotesIfNeeded( cmd );

    if ( args )
	cmd.addSpace().add( args );

    return doExecOSCmd( cmd, ltyp, true, 0, 0 );
}

mStopAllowDeprecatedSection
