//go:build ignore

// Reads the supplied static analysis bundle; never executes the target program.
package main

import (
	"bufio"
	"bytes"
	"compress/gzip"
	"crypto/sha256"
	"debug/pe"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

type nativeRange struct {
	Begin        string     `json:"begin_va"`
	End          string     `json:"end_va_exclusive"`
	Offset       uint64     `json:"file_offset"`
	Section      string     `json:"section"`
	Instructions [][]string `json:"instructions"`
}

func number(s string) uint64 {
	n, e := strconv.ParseUint(strings.TrimPrefix(s, "0x"), 16, 64)
	if e != nil {
		panic(e)
	}
	return n
}
func scan(path string, visit func([]byte) bool) {
	f, e := os.Open(path)
	if e != nil {
		panic(e)
	}
	defer f.Close()
	g, e := gzip.NewReader(f)
	if e != nil {
		panic(e)
	}
	defer g.Close()
	s := bufio.NewScanner(g)
	s.Buffer(make([]byte, 65536), 16<<20)
	for s.Scan() {
		if !visit(s.Bytes()) {
			break
		}
	}
	if s.Err() != nil {
		panic(s.Err())
	}
}
func main() {
	root := flag.String("analysis", `D:\mgsv-analysis\mgsv-analysis`, "static analysis directory")
	address := flag.String("asm", "", "comma-separated preferred addresses")
	find := flag.String("find", "", "instruction operand regex")
	xref := flag.String("xref", "", "reference to a preferred VA, resolving RIP-relative operands")
	filter := flag.String("range", "", "range start regex")
	query := flag.String("strings", "", "regex against string inventory rows")
	refs := flag.String("refs", "", "regex against verified string reference rows")
	pointer := flag.String("ptr", "", "find this absolute pointer in the snapshot")
	immediate := flag.String("imm", "", "find this little-endian 64-bit immediate in executable sections")
	rawXref := flag.String("raw-xref", "", "find relative 32-bit references including leaf functions; inspect instruction boundaries separately")
	dump := flag.String("dump", "", "dump pointer slots at this VA")
	count := flag.Int("count", 32, "maximum matching rows or pointer slots")
	targets := flag.String("targets", "", "write Ghidra target plan for -asm addresses")
	flag.Parse()
	if *query != "" || *refs != "" {
		path := filepath.Join(*root, "native_inventory", "mgsvtpp.exe", "strings.jsonl.gz")
		pattern := *query
		if *refs != "" {
			path = filepath.Join(*root, "native_inventory", "mgsvtpp.exe", "rip_lea_string_refs_verified.jsonl.gz")
			pattern = *refs
		}
		re := regexp.MustCompile(pattern)
		n := 0
		scan(path, func(line []byte) bool {
			if re.Match(line) {
				fmt.Println(string(line))
				n++
			}
			return n < *count
		})
		return
	}
	if *pointer != "" || *dump != "" || *immediate != "" || *rawXref != "" {
		path := filepath.Join(*root, "native_targeted_decompilation", "input", "mgsvtpp.exe")
		data, e := os.ReadFile(path)
		if e != nil {
			panic(e)
		}
		exe, e := pe.NewFile(bytes.NewReader(data))
		if e != nil {
			panic(e)
		}
		base := exe.OptionalHeader.(*pe.OptionalHeader64).ImageBase
		if *rawXref != "" {
			target := number(*rawXref)
			for _, section := range exe.Sections {
				if section.Characteristics&0x20000000 == 0 { continue }
				body := data[section.Offset:section.Offset+section.Size]
				for pos:=0; pos+4<=len(body); pos++ {
					va:=base+uint64(section.VirtualAddress)+uint64(pos)
					if uint64(int64(va+4)+int64(int32(binary.LittleEndian.Uint32(body[pos:]))))==target {
						fmt.Printf("disp32 at 0x%x bytes %x\n",va,body[max(0,pos-8):min(len(body),pos+10)])
					}
				}
			}
			return
		}
		if *immediate != "" {
			var needle [8]byte
			binary.LittleEndian.PutUint64(needle[:], number(*immediate))
			n := 0
			for _, section := range exe.Sections {
				if section.Characteristics&0x20000000 == 0 {
					continue
				}
				body := data[section.Offset : section.Offset+section.Size]
				for start := 0; start < len(body); {
					pos := bytes.Index(body[start:], needle[:])
					if pos < 0 {
						break
					}
					pos += start
					fmt.Printf("0x%x\n", base+uint64(section.VirtualAddress)+uint64(pos))
					n++
					if n >= *count {
						return
					}
					start = pos + 1
				}
			}
			return
		}
		if *pointer != "" {
			needle := number(*pointer)
			n := 0
			for _, section := range exe.Sections {
				for off := uint32(0); off+8 <= section.Size; off += 8 {
					p := uint64(section.Offset) + uint64(off)
					if p+8 <= uint64(len(data)) && binary.LittleEndian.Uint64(data[p:p+8]) == needle {
						fmt.Printf("%s 0x%x\n", section.Name, base+uint64(section.VirtualAddress)+uint64(off))
						n++
						if n >= *count {
							return
						}
					}
				}
			}
			return
		}
		start := number(*dump)
		for _, section := range exe.Sections {
			if start >= base+uint64(section.VirtualAddress) && start < base+uint64(section.VirtualAddress)+uint64(section.Size) {
				off := uint64(section.Offset) + start - base - uint64(section.VirtualAddress)
				for i := 0; i < *count && off+uint64(i*8)+8 <= uint64(len(data)); i++ {
					p := off + uint64(i*8)
					fmt.Printf("0x%x +0x%x 0x%x\n", start+uint64(i*8), i*8, binary.LittleEndian.Uint64(data[p:p+8]))
				}
				return
			}
		}
		panic("VA outside file-backed sections")
	}
	var requested []uint64
	for _, a := range strings.Split(*address, ",") {
		if a != "" {
			requested = append(requested, number(a))
		}
	}
	var re, rangeRE *regexp.Regexp
	if *find != "" {
		re = regexp.MustCompile(*find)
	}
	if *filter != "" {
		rangeRE = regexp.MustCompile(*filter)
	}
	ripRE := regexp.MustCompile(`rip ([+-]) (0x[0-9a-f]+)`)
	var selected []nativeRange
	n := 0
	scan(filepath.Join(*root, "native_disassembly", "mgsvtpp.exe.ranges.jsonl.gz"), func(line []byte) bool {
		if rangeRE != nil && !rangeRE.Match(line[:min(len(line), 100)]) {
			return true
		}
		var r nativeRange
		if e := json.Unmarshal(line, &r); e != nil {
			panic(e)
		}
		match := false
		for _, a := range requested {
			if a >= number(r.Begin) && a < number(r.End) {
				match = true
			}
		}
		if match {
			selected = append(selected, r)
			fmt.Printf("%s..%s %s\n", r.Begin, r.End, r.Section)
			if *targets == "" {
				for _, i := range r.Instructions {
					fmt.Println(strings.Join(i, "  "))
				}
			}
			n++
		} else if re != nil || *xref != "" {
			for j, i := range r.Instructions {
				line := strings.Join(i, " ")
				hit := re != nil && re.MatchString(line)
				if *xref != "" {
					if parts := ripRE.FindStringSubmatch(i[3]); len(parts) > 0 {
						value := number(i[0]) + uint64(len(i[1])/2)
						delta := number(parts[2])
						if parts[1] == "+" {
							value += delta
						} else {
							value -= delta
						}
						if value == number(*xref) {
							hit = true
						}
					} else if i[3] == *xref {
						hit = true
					}
				}
				if hit {
					fmt.Println("range", r.Begin, r.End)
					for k := max(0, j-3); k < min(len(r.Instructions), j+4); k++ {
						fmt.Println(strings.Join(r.Instructions[k], "  "))
					}
					n++
					if n >= *count {
						return false
					}
				}
			}
		}
		return re != nil || *xref != "" || len(selected) < len(requested)
	})
	if *targets != "" {
		data, e := os.ReadFile(filepath.Join(*root, "native_targeted_decompilation", "input", "mgsvtpp.exe"))
		if e != nil {
			panic(e)
		}
		hash := sha256.Sum256(data)
		var out strings.Builder
		out.WriteString("id\tname\taddress\tend_exclusive\tspan_bytes\tbound_kind\tsection\tfile_offset\tsource_bytes_sha256\ttarget_sha256\tname_provenance\tsource\teligible\tskip_reason\tlinear_instruction_starts\texpected_instruction_count\n")
		for i, r := range selected {
			span := number(r.End) - number(r.Begin)
			h := sha256.Sum256(data[r.Offset : r.Offset+span])
			fmt.Fprintf(&out, "%04d\tBuddyCandidate_%s\t%s\t%s\t%d\texact_unwind_range\t%s\t%d\t%s\t%s\tstatic candidate; behavior unverified\tuser-supplied decompilation bundle\t1\t\t\t\n", i+1, r.Begin, r.Begin, r.End, span, r.Section, r.Offset, hex.EncodeToString(h[:]), hex.EncodeToString(hash[:]))
		}
		if e := os.WriteFile(*targets, []byte(out.String()), 0644); e != nil {
			panic(e)
		}
	}
}
