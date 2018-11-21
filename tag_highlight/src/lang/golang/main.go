package main

import (
	"bytes"
	"fmt"
	"github.com/davecgh/go-spew/spew"
	"go/ast"
	"go/importer"
	"go/parser"
	"go/token"
	"go/types"
	"log"
	"os"
	"path/filepath"
	"strings"
)

var (
	fset *token.FileSet = token.NewFileSet()
)

//========================================================================================

func main() {
	var (
		stdin          = QuickRead(os.Stdin).String()
		filename, err1 = filepath.Abs(os.Args[1])
	)
	if err1 != nil {
		panic(err1)
	}
	var (
		pathname       = filepath.Dir(filename)
		astfiles       = parse_files(pathname, filename)
		new_file, err2 = parser.ParseFile(fset, filename, stdin, 0)
	)
	astfiles = append(astfiles, new_file)
	if err2 != nil {
		// eprintf("%v\n", err2)
	}

	highlight(filename, astfiles)
}

//========================================================================================

func parse_whole_dir(path string) *ast.Package {
	astmap, err := parser.ParseDir(fset, path, nil, 0)
	if err != nil {
		panic(err)
	}
	var ret *ast.Package
	for _, file := range astmap {
		ret = file
		break
	}

	return ret
}

func parse_files(path, skip string) []*ast.File {
	var (
		fnames       = get_files(path)
		parsed_files = make([]*ast.File, 0, len(fnames))
	)
	for _, file := range fnames {
		if file == skip {
			continue
		}
		tmp, err := parser.ParseFile(fset, file, nil, 0)
		if err != nil {
			panic(err)
		}
		parsed_files = append(parsed_files, tmp)
	}
	return parsed_files
}

func get_files(path string) []string {
	var (
		dir    *os.File
		st     os.FileInfo
		fnames []string
		err    error
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
		conf = types.Config{Importer: importer.Default()}
		info = &types.Info{
			Types:     make(map[ast.Expr]types.TypeAndValue),
			Implicits: make(map[ast.Node]types.Object),
			Defs:      make(map[*ast.Ident]types.Object),
			Uses:      make(map[*ast.Ident]types.Object),
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
		eprintf("Current file not in fileset.\n")
		os.Exit(1)
	}
	if _, err := conf.Check("", fset, ast_files, info); err != nil {
	}

	for ident, typeinfo := range info.Defs {
		handle_ident(file, ident, typeinfo)
	}
	for ident, typeinfo := range info.Uses {
		handle_ident(file, ident, typeinfo)
	}
}

//========================================================================================

func handle_ident(file *token.File, ident *ast.Ident, typeinfo types.Object) {
	if ident == nil {
		return
	}
	var (
		pos       = ident.Pos()
		name      = ident.Name
		kind rune = identify_kind(ident, typeinfo)
	)
	if kind == 0 || file != fset.File(pos) {
		return
	}
	p := get_range(pos, len(name))
	if p[0].Line <= 0 || p[1].Line <= 0 {
		return
	}

	dump_data(kind, p)
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

//========================================================================================

func get_range(init_pos token.Pos, length int) [2]token.Position {
	pos := [2]token.Pos{init_pos, init_pos + token.Pos(length)}
	return resolve(pos)
}

func resolve(pos [2]token.Pos) [2]token.Position {
	var p [2]token.Position
	p[0] = fset.Position(pos[0] - 1)
	p[1] = fset.Position(pos[1] - 1)
	return p
}

func dump_data(ch rune, p [2]token.Position) {
	fmt.Printf("%c\t%d\t%d\t%d\t%d\n",
		ch, p[0].Line-1, p[0].Column, p[1].Line-1, p[1].Column)
}

func eprintf(format string, a ...interface{}) {
	fmt.Fprintf(os.Stderr, format, a...)
}

func QuickRead(input interface{}) *bytes.Buffer {
	var (
		e   error
		f   *os.File
		buf bytes.Buffer
	)

	switch x := input.(type) {
	case string:
		f, e = os.Open(x)
		if e != nil {
			panic("fail")
		}
		defer f.Close()
	case *os.File:
		f = x
	default:
		panic("Invalid")
	}

	buf.ReadFrom(f)
	return &buf
}

func dummy() {
	spew.Dump(nil)
}
