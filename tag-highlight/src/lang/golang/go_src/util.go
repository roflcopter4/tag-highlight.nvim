package main

import (
	"fmt"
	"log"
	"os"

	"github.com/davecgh/go-spew/spew"
)

/*--------------------------------------------------------------------------------------*/

func open_log(lfile **os.File, isdebug bool, our_fname string) {
	if true || isdebug {
		open_log_dbg(lfile, our_fname)
	} else {
		open_log_rel(lfile, our_fname)
	}
}

func open_log_rel(lfile **os.File, prog_name string) {
	var err error
	*lfile, err = os.Open(os.DevNull)
	if err != nil {
		panic(err)
	}
	lg.Logger = log.New(*lfile, prog_name+": ERROR: ", 0)
}

func open_log_dbg(lfile **os.File, prog_name string) {
	// lfile = &os.Stderr
	var err error
	// *lfile, err = os.OpenFile("thl_go.log", os.O_CREATE|os.O_TRUNC|os.O_WRONLY|os.O_SYNC, 0600)
	*lfile, err = os.Create("thl_go.log")
	if err != nil {
		panic(err)
	}
	lg.Logger = log.New(*lfile, "  =====  ", 0)
	lg.File = *lfile
}

func (l *mylog) Spew(things ...interface{}) {
	lg.Println(spew.Sdump(things...))
	lg.File.Sync()
}

//========================================================================================

func init() {
	spew.Config.Indent = "   "
	// unix.Prctl(unix.PR_SET_PDEATHSIG, uintptr(unix.SIGHUP), 0, 0, 0)
	// signal.Notify(sigchan, unix.SIGHUP)
	// go sighandler(sigchan)
}

func eprintln(a ...interface{})               { fmt.Fprintln(errFile, a...); errFile.Sync() }
func eprintf(format string, a ...interface{}) { fmt.Fprintf(errFile, format, a...); errFile.Sync() }

func errx(code int, format string, a ...interface{}) {
	eprintf(format, a...)
	os.Exit(code)
}
