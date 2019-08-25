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
	"path/filepath"
	"strings"
)

var (
	fset   *token.FileSet = token.NewFileSet()
	lfile  *os.File
	errlog *log.Logger
)

type QuickReadInfo struct {
	Filename      string
	Fp            *os.File
	Bytes_To_Read int
}

//========================================================================================

func main() {
	open_log_rel()

	var (
		buf, err1      = ioutil.ReadAll(os.Stdin)
		filename, err2 = filepath.Abs(os.Args[1])
	)
	if err1 != nil {
		panic(err1)
	}
	if err2 != nil {
		panic(err2)
	}
	var (
		pathname       = filepath.Dir(filename)
		astfiles       = parse_files(pathname, filename)
		new_file, err3 = parser.ParseFile(fset, filename, buf, 0)
	)
	if err3 != nil {
		if errlog != nil {
			errlog.Printf("%v\n", err3)
		}
	}

	astfiles = append(astfiles, new_file)
	highlight(filename, astfiles)
	eprintf("Done!\n")
	os.Exit(0)
}

func open_log_rel() {
	var err error
	lfile, err = os.Open("/dev/null")
	if err != nil {
		lfile, err = os.Open("NUL")
		if err != nil {
			panic(err)
		}
	}
	errlog = log.New(lfile, "ERROR: ", 0)
}

func open_log_dbg() {
	lfile = os.Stderr
	errlog = log.New(lfile, "ERROR: ", 0)
}

//========================================================================================

func parse_whole_dir(path string) *ast.Package {
	var (
		astmap map[string]*ast.Package
		err    error
	)
	if astmap, err = parser.ParseDir(fset, path, nil, 0); err != nil {
		panic(err)
	}
	for _, file := range astmap {
		return file
	}

	return nil
}

func parse_files(path, skip string) []*ast.File {
	var (
		fnames       = get_files(path)
		parsed_files = make([]*ast.File, 0, len(fnames))
	)
	for _, file := range fnames {
		if file != skip {
			tmp, err := parser.ParseFile(fset, file, nil, 0)
			if err != nil {
				panic(err)
			}
			parsed_files = append(parsed_files, tmp)
		}
	}
	return parsed_files
}

func get_files(path string) []string {
	var (
		dir    *os.File
		err    error
		fnames []string
		st     os.FileInfo
	)
	if dir, err = os.Open(path); err != nil {
		panic(err)
	}
	if st, err = dir.Stat(); err != nil {
		panic(err)
	}
	if !st.IsDir() {
		log.Fatalln("Invalid path")
	}
	if fnames, err = dir.Readdirnames(0); err != nil {
		panic(err)
	}

	ret := make([]string, 0, len(fnames))
	for _, name := range fnames {
		name = filepath.Join(path, name)
		if strings.HasSuffix(name, ".go") {
			ret = append(ret, name)
		}
	}

	return ret
}

//========================================================================================

func highlight(filename string, ast_files []*ast.File) {
	var (
		file *token.File
		conf = types.Config{
			Importer: importer.Default(),
			Error:    func(error) {}, // Ignore errors so we can support packages that import "C"
		}
		info = &types.Info{
			Defs: make(map[*ast.Ident]types.Object),
			Uses: make(map[*ast.Ident]types.Object),
		}
	)

	fset.Iterate(func(f *token.File) bool {
		if f.Name() == filename {
			file = f
			return false
		}
		return true
	})

	if file == nil {
		errx(1, "Current file not in fileset.\n")
	}
	if _, err := conf.Check("", fset, ast_files, info); err != nil {
		if errlog != nil {
			errlog.Printf("%v\n", err)
		}
	}

	for ident, typeinfo := range info.Defs {
		handle_ident(file, ident, typeinfo)
	}
	for ident, typeinfo := range info.Uses {
		handle_ident(file, ident, typeinfo)
	}
}

func handle_ident(file *token.File, ident *ast.Ident, typeinfo types.Object) {
	if ident == nil {
		return
	}
	kind := identify_kind(ident, typeinfo)

	if kind == 0 || file != fset.File(ident.Pos()) {
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
// Util

func eprintf(format string, a ...interface{}) {
	fmt.Fprintf(os.Stderr, format, a...)
	os.Stderr.Sync()
}
func errx(code int, format string, a ...interface{}) {
	eprintf(format, a...)
	os.Exit(code)
}
