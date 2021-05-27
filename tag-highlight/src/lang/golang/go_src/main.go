package main

import (
	"fmt"
	"go/ast"
	"go/importer"
	"go/parser"
	"go/token"
	"go/types"
	"log"
	"os"
	"path/filepath"
	"strconv"
	// "golang.org/x/tools/gcexportdata"
	// "github.com/golang/tools/tree/master/go/gcexportdata"
)

var (
	fset *token.FileSet = token.NewFileSet()
	lg   *mylog         = new(mylog)
)

type mylog struct {
	*log.Logger
	File *os.File
}

const (
	TYPE_CONSTANT    = 'c'
	TYPE_FIELD       = 'm'
	TYPE_FUNCTION    = 'f'
	TYPE_INTERFACE   = 'i'
	TYPE_PACKAGENAME = 'p'
	TYPE_STRUCT      = 's'
	TYPE_TYPENAME    = 't'
	TYPE_VARIABLE    = 'v'
	TYPE_METHOD      = 'F'
)

const debug_override = false

//========================================================================================

func main() {
	if len(os.Args) != 8 {
		os.Exit(1)
	}
	var (
		lfile        *os.File
		isdebug      int
		prog_name    string = os.Args[1]
		isdebug_s    string = os.Args[2]
		our_fname    string = os.Args[3]
		our_fpath    string = os.Args[4]
		our_projpath string = os.Args[5]

		sock = connect_socket_c([2]string{os.Args[6], os.Args[7]})
	)

	if err := os.Chdir(our_projpath); err != nil {
		panic(err)
	}

	isdebug, _ = strconv.Atoi(isdebug_s)
	open_log(&lfile, debug_override || isdebug == 1, prog_name)
	defer func() {
		if lfile != nil && lfile != os.Stderr {
			lfile.Close()
		}
	}()

	data := InitialParse(our_fname, our_fpath)

	for {
		data.Update(sock, wait(sock))
	}

	os.Exit(0)
}

// func wait() string {
/* The number 4294967296 (aka UINT32_MAX), which is the largest size a buffer can
 * be (in my app anyway) is 10 characters. The actual size of the buffer will be
 * padded with zeros on the left if necessary. */
//       var (
//             buf   = make([]byte, 10)
//             inlen int
//             n     int
//             err   error
//       )
//
//       // eprintf("Reading 10 bytes (num2read)\n")
//       if n, err = io.ReadFull(os.Stdin, buf); err != nil {
//             panic(err)
//       }
//       // eprintf("Read %d bytes! (num2read)\n", n)
//       if inlen, err = strconv.Atoi(string(buf)); err != nil {
//             panic(err)
//       }
//       // eprintf("Reading %d bytes (buffer)\n", inlen)
//
//       buf = make([]byte, inlen)
//       if n, err = io.ReadFull(os.Stdin, buf); err != nil {
//             panic(err)
//       }
//       // eprintf("Read %d bytes (buffer)!\n", n)
//       _ = n
//
//       return string(buf)
// }

//========================================================================================

type Parsed_Data struct {
	Output    string
	FileName  string
	FilePath  string
	FileSlice []*ast.File
	FileMap   map[string]*ast.File
	Packages  map[string]*ast.Package
	AstFile   *ast.File
	FileToken *token.File
	Pkg       *ast.Package
	Info      *types.Info
	Conf      types.Config
}

func InitialParse(our_fname, our_fpath string) *Parsed_Data {
	var (
		err error

		ret = &Parsed_Data{
			FileName:  our_fname,
			FilePath:  our_fpath,
			FileMap:   nil,
			FileSlice: nil,
			Packages:  nil,
			FileToken: nil,
			AstFile:   nil,
			Pkg:       nil,
			Info:      new(types.Info),
			Conf: types.Config{
				// Importer: importer.ForCompiler(fset, "gc", nil),
				Importer: importer.ForCompiler(fset, "source", nil),
				Error:    func(error) {},

				DisableUnusedImportCheck: true,
			},
		}
	)

	if ret.Packages, err = parse_whole_dir(ret.FilePath); err != nil {
		lg.Printf("parse_files: %v\n", err)
	}

	return ret
}

func parse_whole_dir(path string) (map[string]*ast.Package, error) {
	var (
		astmap map[string]*ast.Package
		err    error
	)
	if astmap, err = parser.ParseDir(fset, path, nil, 0); err != nil {
		lg.Printf("ParseDir: `%v`", err)
	}
	return astmap, err

}

func whyunofind(data *Parsed_Data, fname string) *ast.Package {
	var pack *ast.Package
	if pack = data.Packages[fname]; pack != nil {
		return pack
	}

	bname := filepath.Base(fname)

	if pack = data.Packages[bname]; pack != nil {
		return pack
	}

	if len(data.Packages) == 1 {
		for _, v := range data.Packages {
			if v != nil {
				return v
			}
		}
	} else {
		// eprintf("wtf there are %d packages", len(data.Packages))
	}

	e := fmt.Errorf("Error: Want \"%s\" or \"%s\", but it's not in ( %#+v )", fname, bname, data.Packages)
	panic(e)
}

/*--------------------------------------------------------------------------------------*/

func (this *Parsed_Data) Update(sock *sockets, buf string) {
	var err error

	this.AstFile, err = parser.ParseFile(fset, this.FileName, buf, 0)
	if err != nil {
		lg.Printf("parser.Parsefile: %v\n", err)
	}

	this.Pkg = whyunofind(this, this.AstFile.Name.String())
	this.FileMap = this.Pkg.Files
	this.FileMap[this.FileName] = this.AstFile

	this.Output = ""
	this.FileSlice = []*ast.File{}
	this.Info = &types.Info{
		Defs: make(map[*ast.Ident]types.Object),
		Uses: make(map[*ast.Ident]types.Object),
	}

	for _, f := range this.FileMap {
		this.FileSlice = append(this.FileSlice, f)
	}

	if err = this.Populate(); err != nil {
		// eprintf("Error: %v", err)
		// return
	}
	this.Highlight()
	this.WriteOutput(sock)
}

func (this *Parsed_Data) Populate() error {
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
		errx(1, "Current file not in fileset.\n")
	}

	pkg, err := this.Conf.Check(this.FilePath, fset, this.FileSlice, this.Info)
	if err != nil {
		lg.Println("check: ", err)
		lg.Println(pkg)
		return err
	}

	return nil
}

func (this *Parsed_Data) Highlight() {
	for ident, typeinfo := range this.Info.Defs {
		this.handle_ident(ident, typeinfo)
	}
	for ident, typeinfo := range this.Info.Uses {
		this.handle_ident(ident, typeinfo)
	}
}

//----------------------------------------------------------------------------------------

//func (this *Parsed_Data) WriteOutput() {
//	var (
//		err    error
//		n      int
//		s      = []byte(this.Output)
//		lenstr = []byte(fmt.Sprintf("%010d", len(s)))
//	)
//
//	if len(lenstr) != 10 {
//		panic(fmt.Sprintf("Invalid string output! %s", lenstr))
//	}
//	if n, err = os.Stdout.Write(lenstr); err != nil {
//		panic(err)
//	}
//	if n != len(lenstr) {
//		panic(fmt.Sprintf("Undersized write (%d != %d)", n, len(lenstr)))
//	}
//	if n, err = os.Stdout.Write(s); err != nil {
//		panic(err)
//	}
//	if n != len(s) {
//		panic(fmt.Sprintf("Undersized write (%d != %d)", n, len(s)))
//	}
//}

//========================================================================================

func (this *Parsed_Data) handle_ident(ident *ast.Ident, typeinfo types.Object) {
	if ident == nil {
		return
	}
	var kind int = identify_kind(ident, typeinfo)

	if kind == 0 {
		return
	}
	if this.FileToken.Name() != fset.File(ident.Pos()).Name() {
		return
	}
	p := get_range(ident.Pos(), len(ident.Name))
	if p[0].Line <= 0 || p[1].Line <= 0 {
		return
	}

	this.Output += fmt.Sprintf(
		"%c\t%d\t%d\t%d\t%d\t%d\t%s\n",
		kind, p[0].Line-1, p[0].Column, p[1].Line-1, p[1].Column,
		len(ident.Name), ident.Name)
}

func identify_kind(ident *ast.Ident, typeinfo types.Object) int {
	var ret int = 0

	switch x := typeinfo.(type) {
	case *types.Const:
		ret = TYPE_CONSTANT
	case *types.Func:
		if x.Parent() == nil {
			ret = TYPE_METHOD
		} else {
			ret = TYPE_FUNCTION
		}
	case *types.PkgName:
		ret = TYPE_PACKAGENAME
	case *types.TypeName:
		if x.Type() != nil || x.Type().Underlying() != nil {
			switch x.Type().Underlying().(type) {
			case *types.Interface:
				ret = TYPE_INTERFACE
			case *types.Struct:
				ret = TYPE_STRUCT
			default:
				ret = TYPE_TYPENAME
			}
		}
	case *types.Var:
		if x.Type() != nil && x.Type().Underlying() != nil {
			switch x.Type().Underlying().(type) {
			case *types.Signature:
				ret = TYPE_FUNCTION
			}
		}
		if x.IsField() {
			ret = TYPE_FIELD
		} else if typeinfo.Parent() != nil && typeinfo.Parent().Parent() == types.Universe {
			ret = TYPE_VARIABLE
		}
	}

	return ret
}

func get_range(init_pos token.Pos, length int) [2]token.Position {
	return [2]token.Position{
		fset.Position(init_pos - 1),
		fset.Position(init_pos - 1 + token.Pos(length)),
	}
}
