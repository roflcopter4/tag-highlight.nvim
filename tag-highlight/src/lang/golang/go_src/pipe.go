package main

import (
	"fmt"
	"io"
	"os"
	"unsafe"
)

//========================================================================================

func Wait() string {
	/* The number 4294967295 (aka UINT32_MAX), which is the largest size a buffer can
	 * be (in my app anyway) is 10 characters. The actual size of the buffer will be
	 * padded with zeros on the left if necessary. */
	var (
		buf   = make([]byte, 8)
		inlen int
		n     int
		err   error
	)

	if n, err = io.ReadFull(os.Stdin, buf); err != nil {
		panic(err)
	}

	// eprintf("Read %d bytes! (num2read)\n", n)

	inlen = int(mkuint64(buf))

	// eprintf("Reading %d bytes (buffer)\n", inlen)

	buf = make([]byte, inlen)

	if n, err = io.ReadFull(os.Stdin, buf); err != nil {
		panic(err)
	}

	// eprintf("Read %d bytes (buffer)!\n", n)
	_ = n

	return string(buf)
}

func mkuint64(input []byte) uint64 {
	//var uint64 n = 0
	//n |= (uint64(input[0]) << 000)
	//n |= (uint64(input[1]) << 010)
	//n |= (uint64(input[2]) << 020)
	//n |= (uint64(input[3]) << 030)
	//n |= (uint64(input[4]) << 040)
	//n |= (uint64(input[5]) << 050)
	//n |= (uint64(input[6]) << 060)
	//n |= (uint64(input[7]) << 070)
	//return n

	// By god is this ever EVIL
	return *((*uint64)(unsafe.Pointer(&input[0])))
}

//----------------------------------------------------------------------------------------

func (this *Parsed_Data) WriteOutput() {
	var (
		err    error
		n      int
		s      = []byte(this.Output)
		lenstr = []byte(fmt.Sprintf("%010d", len(s)))
	)

	if len(lenstr) != 10 {
		panic(fmt.Sprintf("Invalid string output! %s", lenstr))
	}
	if n, err = os.Stdout.Write(lenstr); err != nil {
		panic(err)
	}
	if n != len(lenstr) {
		panic(fmt.Sprintf("Undersized write (%d != %d)", n, len(lenstr)))
	}
	if n, err = os.Stdout.Write(s); err != nil {
		panic(err)
	}
	if n != len(s) {
		panic(fmt.Sprintf("Undersized write (%d != %d)", n, len(s)))
	}
}
