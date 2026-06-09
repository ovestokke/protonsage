package steam

import (
	"fmt"
	"os"
	"strings"
	"unicode"
)

// VDFObject is a minimal Valve KeyValues object representation.
type VDFObject map[string]any

type tokenKind int

const (
	tokenEOF tokenKind = iota
	tokenString
	tokenLBrace
	tokenRBrace
)

type token struct {
	kind tokenKind
	text string
}

type scanner struct {
	input []rune
	pos   int
	line  int
	col   int
}

func newScanner(data []byte) *scanner {
	return &scanner{input: []rune(string(data)), line: 1, col: 1}
}

func (s *scanner) nextToken() (token, error) {
	s.skipWhitespaceAndComments()
	if s.pos >= len(s.input) {
		return token{kind: tokenEOF}, nil
	}

	ch := s.peek()
	switch ch {
	case '{':
		s.advance()
		return token{kind: tokenLBrace, text: "{"}, nil
	case '}':
		s.advance()
		return token{kind: tokenRBrace, text: "}"}, nil
	case '"':
		return s.scanQuoted()
	default:
		return s.scanBare(), nil
	}
}

func (s *scanner) skipWhitespaceAndComments() {
	for s.pos < len(s.input) {
		ch := s.peek()
		if unicode.IsSpace(ch) {
			s.advance()
			continue
		}
		if ch == '/' && s.pos+1 < len(s.input) && s.input[s.pos+1] == '/' {
			for s.pos < len(s.input) && s.peek() != '\n' {
				s.advance()
			}
			continue
		}
		break
	}
}

func (s *scanner) scanQuoted() (token, error) {
	startLine, startCol := s.line, s.col
	s.advance() // opening quote
	var b strings.Builder
	for s.pos < len(s.input) {
		ch := s.advance()
		if ch == '"' {
			return token{kind: tokenString, text: b.String()}, nil
		}
		if ch == '\\' && s.pos < len(s.input) {
			next := s.advance()
			switch next {
			case 'n':
				b.WriteRune('\n')
			case 't':
				b.WriteRune('\t')
			case 'r':
				b.WriteRune('\r')
			default:
				b.WriteRune(next)
			}
			continue
		}
		b.WriteRune(ch)
	}
	return token{}, fmt.Errorf("unterminated quoted string at %d:%d", startLine, startCol)
}

func (s *scanner) scanBare() token {
	var b strings.Builder
	for s.pos < len(s.input) {
		ch := s.peek()
		if unicode.IsSpace(ch) || ch == '{' || ch == '}' {
			break
		}
		b.WriteRune(s.advance())
	}
	return token{kind: tokenString, text: b.String()}
}

func (s *scanner) peek() rune {
	return s.input[s.pos]
}

func (s *scanner) advance() rune {
	ch := s.input[s.pos]
	s.pos++
	if ch == '\n' {
		s.line++
		s.col = 1
	} else {
		s.col++
	}
	return ch
}

type vdfParser struct {
	s *scanner
}

// ParseVDF parses the subset of Valve KeyValues used by Steam library and app manifest files.
func ParseVDF(data []byte) (VDFObject, error) {
	p := &vdfParser{s: newScanner(data)}
	obj, err := p.parseObject(false)
	if err != nil {
		return nil, err
	}
	return obj, nil
}

// ParseVDFFile parses a VDF/ACF file from disk. It only reads the file.
func ParseVDFFile(path string) (VDFObject, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	return ParseVDF(data)
}

func (p *vdfParser) parseObject(stopOnRBrace bool) (VDFObject, error) {
	obj := VDFObject{}
	for {
		keyTok, err := p.s.nextToken()
		if err != nil {
			return nil, err
		}

		switch keyTok.kind {
		case tokenEOF:
			if stopOnRBrace {
				return nil, fmt.Errorf("unexpected EOF inside object")
			}
			return obj, nil
		case tokenRBrace:
			if stopOnRBrace {
				return obj, nil
			}
			return nil, fmt.Errorf("unexpected } at top level")
		case tokenLBrace:
			return nil, fmt.Errorf("unexpected { before key")
		case tokenString:
			// Continue below.
		}

		valueTok, err := p.s.nextToken()
		if err != nil {
			return nil, err
		}
		switch valueTok.kind {
		case tokenString:
			obj[keyTok.text] = valueTok.text
		case tokenLBrace:
			child, err := p.parseObject(true)
			if err != nil {
				return nil, err
			}
			obj[keyTok.text] = child
		case tokenEOF:
			return nil, fmt.Errorf("missing value for key %q", keyTok.text)
		case tokenRBrace:
			return nil, fmt.Errorf("missing value for key %q before }", keyTok.text)
		}
	}
}

// AsObject casts a VDF value to an object.
func AsObject(value any) (VDFObject, bool) {
	obj, ok := value.(VDFObject)
	return obj, ok
}

// StringValue returns a string field from a VDF object.
func StringValue(obj VDFObject, key string) string {
	value, ok := obj[key]
	if !ok {
		return ""
	}
	str, _ := value.(string)
	return str
}
