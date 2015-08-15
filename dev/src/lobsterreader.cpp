// Copyright 2014 Wouter van Oortmerssen. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdafx.h"

#include "vmdata.h"
#include "natreg.h"

#include "ttypes.h"
#include "lex.h"

using namespace lobster;

struct ValueParser
{
    vector<string> filenames;
    vector<RefObj *> allocated;
    Lex lex;

    ValueParser(char *_src) : lex("string", filenames, _src)
    {
    }

    ~ValueParser()
    {
        for (auto lo : allocated)
            Value(lo).DECRT();
    }

    Value Parse(type_elem_t typeoff)
    {
        Value v = ParseFactor(typeoff);
        Gobble(T_LINEFEED);
        Expect(T_ENDOFFILE);
        return v;
    }

    Value ParseElems(TType end, type_elem_t typeoff, int numelems = -1)
    {
        Gobble(T_LINEFEED);
        vector<Value> elems;
        auto ti = g_vm->TypeInfo(typeoff);

        if (lex.token == end) lex.Next();
        else
        {
            for (int i = 0;; i++)
            {
                auto x = ParseFactor(ti[0] == V_VECTOR ? ti[1] : ti[i + 2]);  // FIXME: error checking :)
                if ((int)elems.size() == numelems) x.DEC();
                else elems.push_back(x);
                bool haslf = lex.token == T_LINEFEED;
                if (haslf) lex.Next();
                if (lex.token == end) break;
                if (!haslf) Expect(T_COMMA);
            }
            lex.Next();
        }

        // FIXME: improve error.
        // There's no default value possible for non-nillables, so we can't provide a default value here.
        if (numelems >= 0 && (int)elems.size() < numelems)
            lex.Error("not enough constructor initializers");

        auto vec = g_vm->NewVector(elems.size(), typeoff);
        allocated.push_back(vec);
        for (auto &e : elems) vec->push(e.INC());
        return Value(vec);
    }

    Value ParseFactor(type_elem_t typeoff)
    {
        switch (lex.token)
        {
            case T_INT:   { int i    = atoi(lex.sattr.c_str()); lex.Next(); return Value(i); }
            case T_FLOAT: { double f = atof(lex.sattr.c_str()); lex.Next(); return Value((float)f); }
            case T_STR:   { string s = lex.sattr;               lex.Next(); auto str = g_vm->NewString(s);
                                                                            allocated.push_back(str);
                                                                            return Value(str); }     
            case T_NIL:   {                                     lex.Next(); return Value(0, V_NIL); }

            case T_MINUS:
            {
                lex.Next(); 
                Value v = ParseFactor(typeoff);
                switch (v.type)
                {
                    case V_INT:   v.ival() *= -1; break;
                    case V_FLOAT: v.fval() *= -1; break;
                    default: lex.Error("numeric value expected");
                }
                return v;
            }

            case T_LEFTBRACKET:
            {
                lex.Next();
                return ParseElems(T_RIGHTBRACKET, typeoff);
            }

            case T_IDENT:
            {
                string sname = lex.sattr;
                lex.Next();
                Expect(T_LEFTCURLY);
                int reqargs = 0;
                auto ti = g_vm->StructTypeInfo(sname, reqargs);
                if (ti < 0) lex.Error("unknown type: " + sname);
                return ParseElems(T_RIGHTCURLY, ti, reqargs);
            }

            default:
                lex.Error("illegal start of expression: " + lex.TokStr());
                return Value();
        }
    }

    void Expect(TType t)
    {
        if (lex.token != t)
            lex.Error(lex.TokStr(t) + " expected, found: " + lex.TokStr());

        lex.Next();
    }

    void Gobble(TType t)
    {
        if (lex.token == t) lex.Next();
    }
};

static Value ParseData(type_elem_t typeoff, char *inp)
{
    try
    {
        ValueParser parser(inp);
        g_vm->Push(parser.Parse(typeoff).INC());
        return Value(0, V_NIL);
    }
    catch (string &s)
    {
        g_vm->Push(Value(0, V_NIL));
        return Value(g_vm->NewString(s));
    }
}

void AddReaderOps()
{
    STARTDECL(parse_data) (Value &type, Value &ins)
    {
        Value v = ParseData((type_elem_t)type.ival(), ins.sval()->str());
        ins.DECRT();
        return v;
    }
    ENDDECL2(parse_data, "typeid,stringdata", "TS", "AS?",
        "parses a string containing a data structure in lobster syntax (what you get if you convert an arbitrary data"
        " structure to a string) back into a data structure. supports int/float/string/vector and structs."
        " structs will be forced to be compatible with their current definitions, i.e. too many elements will be"
        " truncated, missing elements will be set to nil, and unknown type means downgrade to vector."
        " useful for simple file formats. returns the value and an error string as second return value"
        " (or nil if no error)");
}

AutoRegister __aro("parsedata", AddReaderOps);


