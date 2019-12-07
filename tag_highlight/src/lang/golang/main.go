package main

import (
	"fmt"
	"go/ast"
	"go/importer"
	"go/parser"
	"go/token"
	"go/types"
	"io/ioutil"
	"log"
	"os"
	"runtime"
	"strconv"
	"strings"
)

var (
	fset *token.FileSet = token.NewFileSet()
	lg   *log.Logger
)

//========================================================================================

func main() {
	var (
		lfile     *os.File
		isdebug   int
		prog_name string = os.Args[1]
		isdebug_s string = os.Args[2]
		our_fname string = os.Args[3]
		our_fpath string = os.Args[4]
	)

	isdebug, _ = strconv.Atoi(isdebug_s)
	open_log(lfile, false && isdebug == 1, prog_name)
	defer lfile.Close()

	data := do_parse(our_fname, our_fpath)
	data.Highlight()
	os.Exit(0)
}

func open_log(lfile *os.File, isdebug bool, our_fname string) {
	if isdebug {
		open_log_dbg(lfile, our_fname)
	} else {
		open_log_rel(lfile, our_fname)
	}
}

func open_log_rel(lfile *os.File, prog_name string) {
	var err error
	lfile, err = os.Open("/dev/null")
	if err != nil {
		lfile, err = os.Open("NUL")
		if err != nil {
			panic(err)
		}
	}
	lg = log.New(lfile, prog_name+": ERROR: ", 0)
}

func open_log_dbg(lfile *os.File, prog_name string) {
	// lfile = os.Stderr
	var err error
	lfile, err = os.OpenFile("thl_go.log", os.O_WRONLY|os.O_CREATE|os.O_TRUNC|os.O_SYNC, 0644)
	if err != nil {
		panic(err)
	}

	lg = log.New(lfile, "  =====  ", 0)
}

//========================================================================================

type Parsed_Data struct {
	FileName  string
	FilePath  string
	FileSlice []*ast.File
	FileMap   map[string]*ast.File
	Packages  map[string]*ast.Package
	AstFile   *ast.File
	FileToken *token.File
	Pkg       *ast.Package
	Info      *types.Info
}

func do_parse(our_fname, our_fpath string) *Parsed_Data {
	var (
		err error
		buf []byte

		ret = Parsed_Data{
			FileName:  our_fname,
			FilePath:  our_fpath,
			FileMap:   nil,
			FileSlice: nil,
			Packages:  nil,
			FileToken: nil,
			AstFile:   nil,
			Pkg:       nil,
			Info:      nil,
		}
	)

	if buf, err = ioutil.ReadAll(os.Stdin); err != nil {
		panic(err)
	}

	if ret.Packages, err = parse_whole_dir(ret.FilePath); err != nil {
		lg.Printf("parse_files: %v\n", err)
	}
	if ret.AstFile, err = parser.ParseFile(fset, ret.FileName, buf, 0); err != nil {
		lg.Printf("parser.Parsefile: %v\n", err)
	}

	ret.Pkg = ret.Packages[ret.AstFile.Name.String()]
	ret.FileMap = ret.Pkg.Files
	ret.FileMap[ret.FileName] = ret.AstFile
	ret.FileSlice = []*ast.File{}

	for _, f := range ret.FileMap {
		ret.FileSlice = append(ret.FileSlice, f)
	}

	ret.Populate()
	return &ret
}

func parse_whole_dir(path string) (map[string]*ast.Package, error) {
	var (
		astmap map[string]*ast.Package
		err    error
	)
	Eprintf("Reading dir '%s'\n", path)
	if astmap, err = parser.ParseDir(fset, path, nil, 0); err != nil {
		lg.Println(err)
	}
	return astmap, err

}

func (this *Parsed_Data) Populate() error {
	var (
		conf = types.Config{
			// Importer: importer.Default(),
			Importer: importer.ForCompiler(fset, runtime.Compiler, nil),
			// Ignore errors so we can support packages that import "C"
			Error: func(error) {},
		}
	)

	this.Info = &types.Info{
		Defs: make(map[*ast.Ident]types.Object),
		Uses: make(map[*ast.Ident]types.Object),
	}

	fset.Iterate(
		func(f *token.File) bool {
			if f.Name() == this.FileName {
				this.FileToken = f
				return false
			}
			return true
		},
	)
	if this.FileToken == nil {
		Errx(1, "Current file not in fileset.\n")
	}

	if pkg, err := conf.Check("", fset, this.FileSlice, this.Info); err != nil {
		Eprintln(err.Error())
		lg.Println("check: ", err)
		lg.Println(pkg)

		return err
	}

	return nil
}

//========================================================================================

func (this *Parsed_Data) Highlight() {
	for ident, typeinfo := range this.Info.Defs {
		handle_ident(this.FileToken, ident, typeinfo)
	}
	for ident, typeinfo := range this.Info.Uses {
		handle_ident(this.FileToken, ident, typeinfo)
	}
}

func handle_ident(file *token.File, ident *ast.Ident, typeinfo types.Object) {
	if ident == nil {
		return
	}
	var kind rune = identify_kind(ident, typeinfo)

	if kind == 0 {
		return
	}
	if file.Name() != fset.File(ident.Pos()).Name() {
		return
	}
	p := get_range(ident.Pos(), len(ident.Name))
	if p[0].Line <= 0 || p[1].Line <= 0 {
		return
	}

	dump_data(kind, p, ident.Name)
	os.Stdout.Sync()
}

func identify_kind(ident *ast.Ident, typeinfo types.Object) rune {
	switch x := typeinfo.(type) {
	case *types.Const:
		return 'c'
	case *types.Func:
		return 'f'
	case *types.PkgName:
		return 'p'
	case *types.TypeName:
		if x.Type() == nil || x.Type().Underlying() == nil {
			return 0
		}
		switch x.Type().Underlying().(type) {
		case *types.Interface:
			return 'i'
		case *types.Struct:
			return 's'
		default:
			return 't'
		}
	case *types.Var:
		if x.IsField() {
			return 'm'
		}
		scope := typeinfo.Parent()
		if scope != nil && strings.HasPrefix(scope.String(), "package") {
			return 'v'
		}
	}

	return 0
}

func get_range(init_pos token.Pos, length int) [2]token.Position {
	return [2]token.Position{
		fset.Position(init_pos - 1),
		fset.Position(init_pos - 1 + token.Pos(length)),
	}
}

func dump_data(ch rune, p [2]token.Position, ident string) {
	fmt.Printf("%c\t%d\t%d\t%d\t%d\t%d\t%s\n",
		ch, p[0].Line-1, p[0].Column, p[1].Line-1, p[1].Column, len(ident), ident)
}

//========================================================================================

func Eprintln(a ...interface{}) {
	fmt.Fprintln(os.Stderr, a...)
	os.Stderr.Sync()
}
func Eprintf(format string, a ...interface{}) {
	fmt.Fprintf(os.Stderr, format, a...)
	os.Stderr.Sync()
}
func Errx(code int, format string, a ...interface{}) {
	Eprintf(format, a...)
	os.Exit(code)
}
