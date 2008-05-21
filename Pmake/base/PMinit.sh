#---------------------------------------------------------
# Author: dGB Earth Sciences ( A.H.Bril )
# Pmake Environment bash initialization shell script
# $Id: PMinit.sh,v 1.7 2008-05-21 13:40:15 cvsbert Exp $
#---------------------------------------------------------

if [ "$HDIR" = "" ]; then
    
    HDIR=`uname`
    if [ "$HDIR" == "SunOS" ]; then
	HDIR=sun5
    fi
    if [ "$HDIR" == "Linux" ]; then
	HDIR=lux
    fi
    if [ "$HDIR" == "Darwin" ]; then
	HDIR=mac
    elif [ "$COMSPEC" == "" ]; then
	HDIR=unknown
    else
	HDIR=win
    fi
    export HDIR
fi

if [ "$PLFSUBDIR" = "" ]; then

    PLFSUBDIR=$HDIR
    if [ "$HDIR" = sun5 ]; then
	PLFSUBDIR=sol32
    fi
    if [ "$HDIR" = lux ]; then
	if [ `uname -a | grep _64 | wc -l` = 1 ]; then
	    PLFSUBDIR=lux64
	else
	    PLFSUBDIR=lux32
	fi
    fi
    export PLFSUBDIR
fi

if [ "$PMAKE" = "" ]; then
    if [ "$WORK" = "" ]; then
	PMAKE=/users/appman/pmake
    else
	PMAKE=$WORK/Pmake/base
    fi
    export PMAKE
fi

if [ "$GNUMAKE" = "" ]; then
    GNUMAKE="gmake"; export GNUMAKE
fi

PMAKECOMMAND='$GNUMAKE -I$WORK/Pmake -I$PMAKE'
export PMAKECOMMAND
alias make="$PMAKECOMMAND"

alias wdir='export WORK=`pwd`'
alias cdw='cd $WORK'

_pmake_dbg_dir_=G
if [ "$DEBUG" = "no" ]; then
    _pmake_dbg_dir_=O
fi

alias cdb='cd $WORK/bin/$PLFSUBDIR/'$_pmake_dbg_dir_
alias cdl='cd $WORK/lib/$PLFSUBDIR/'$_pmake_dbg_dir_
