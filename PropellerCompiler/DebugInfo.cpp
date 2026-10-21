//////////////////////////////////////////////////////////////
//                                                          //
// Propeller Spin/PASM Compiler                             //
// Debugging information (openspin -g)                      //
// See end of file for terms of use.                        //
//                                                          //
//////////////////////////////////////////////////////////////
//
// DebugInfo.cpp
//
// The text, one object after another, each object once (the last
// compile of its file):
//
//   OpenSpin debug 1
//   object <path of the source>
//   obj <count> <alias> <file name as in the OBJ section>
//   var <size: 1, 2, 4> <count> <offset from vbase> <name>
//   method <index from 1> pub|pri <line> <name>
//   param <name>
//   result <name>
//   local <count> <name>
//   line <offset from pbase> <line>
//   end
//
// obj lines come in OBJ order (an array is one line, its count the
// number of entries it takes in the object table); param, result and
// local lines follow their method, in order (locals are longs, a count
// above 1 is an array); line entries are in address order and give the
// source line of the code from that offset up to the next entry (the
// last one: up to the end of the method's code).  Lines count from 1.
//

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include "DebugInfo.h"
#include "PropellerCompilerInternal.h"

namespace
{
    struct Local { std::string name; int count; };
    struct Method
    {
        int index;
        bool bPub;
        int sourcePos;
        std::string name, result;
        std::vector<std::string> params;
        std::vector<Local> locals;
    };
    struct Var { std::string name; int size, count, offset; };
    struct Obj { std::string alias, file; int count; };
    struct Line { int offset, sourcePos; };
    struct Object { std::string path, text; };

    bool s_bEnabled = false;
    std::vector<Obj> s_objs;
    std::vector<Var> s_vars;
    int s_longBytes = 0, s_wordBytes = 0;
    std::vector<Method> s_methods;
    std::vector<Line> s_lines;
    std::vector<Object> s_objects;
    std::string s_text;
    std::string s_name;         // DebugInfo_Name's
    int s_suspended = 0;        // DebugInfo_Suspend's, nested

    // The name to give: the source's spelling, if it is the same symbol
    std::string Name(const char* pName)
    {
        std::string n = s_name;
        s_name.clear();
        if (n.size() == strlen(pName))
        {
            bool bSame = true;
            for (size_t i = 0; i < n.size(); i++)
            {
                bSame &= toupper((unsigned char)n[i]) == toupper((unsigned char)pName[i]);
            }
            if (bSame)
            {
                return n;
            }
        }
        return pName;
    }
}

void DebugInfo_Name()
{
    if (s_bEnabled)
    {
        int start = g_pCompilerData->source_start, finish = g_pCompilerData->source_finish;
        s_name.assign(&g_pCompilerData->source[start], finish > start ? finish - start : 0);
    }
}

void DebugInfo_Enable(bool bEnable)
{
    s_bEnabled = bEnable;
}

bool DebugInfo_Enabled()
{
    return s_bEnabled;
}

void DebugInfo_ObjReset()
{
    s_objs.clear();
}

void DebugInfo_Obj(const char* pAlias, int count, const char* pFilename)
{
    if (!s_bEnabled)
    {
        return;
    }
    Obj o;
    o.alias = Name(pAlias);
    o.file = pFilename;
    o.count = count;
    s_objs.push_back(o);
}

void DebugInfo_BeginObject()
{
    s_suspended = 0;
    s_vars.clear();
    s_methods.clear();
    s_lines.clear();
    s_longBytes = s_wordBytes = 0;
}

void DebugInfo_Var(const char* pName, int size, int count, int offset)
{
    if (!s_bEnabled)
    {
        return;
    }
    Var v;
    v.name = Name(pName);
    v.size = size;
    v.count = count;
    v.offset = offset;
    s_vars.push_back(v);
}

void DebugInfo_VarSizes(int longBytes, int wordBytes)
{
    s_longBytes = longBytes;
    s_wordBytes = wordBytes;
}

void DebugInfo_Method(int index, bool bPub, const char* pName, int nameLength, int sourcePos)
{
    if (!s_bEnabled)
    {
        return;
    }
    Method m;
    m.index = index;
    m.bPub = bPub;
    m.sourcePos = sourcePos;
    m.name.assign(pName, nameLength);
    m.result = "result";
    s_methods.push_back(m);
}

void DebugInfo_Param(const char* pName)
{
    if (s_bEnabled && !s_methods.empty())
    {
        s_methods.back().params.push_back(Name(pName));
    }
}

void DebugInfo_Result(const char* pName)
{
    if (s_bEnabled && !s_methods.empty())
    {
        s_methods.back().result = Name(pName);
    }
}

void DebugInfo_Local(const char* pName, int count)
{
    if (s_bEnabled && !s_methods.empty())
    {
        Local l;
        l.name = Name(pName);
        l.count = count;
        s_methods.back().locals.push_back(l);
    }
}

void DebugInfo_DropMethod()
{
    if (s_bEnabled && !s_methods.empty())
    {
        s_methods.pop_back();
    }
}

// Drop the entries from 'objOffset' on (or after it)
static void Truncate(int objOffset, bool bInclusive)
{
    while (!s_lines.empty() && (s_lines.back().offset > objOffset || (bInclusive && s_lines.back().offset == objOffset)))
    {
        s_lines.pop_back();
    }
}

void DebugInfo_Rewind(int objOffset)
{
    // The block's own statement, at objOffset, stays
    Truncate(objOffset, false);
}

void DebugInfo_Suspend()
{
    s_suspended++;
}

void DebugInfo_Resume()
{
    if (s_suspended > 0)
    {
        s_suspended--;
    }
}

void DebugInfo_Line(int objOffset, int sourcePos)
{
    if (!s_bEnabled || s_suspended)
    {
        return;
    }
    // Code compiled again (an optimised block) replaces what was there
    Truncate(objOffset, true);
    Line l;
    l.offset = objOffset;
    l.sourcePos = sourcePos;
    s_lines.push_back(l);
}

// The line numbers of source positions, in one pass over the source
// (lines end with 13, as the compiler's errors count them)
static void LineNumbers(const char* pSource, const std::vector<int>& positions, std::vector<int>& lines)
{
    std::vector<std::pair<int, int> > order;
    for (size_t i = 0; i < positions.size(); i++)
    {
        order.push_back(std::make_pair(positions[i], (int)i));
    }
    std::sort(order.begin(), order.end());
    lines.assign(positions.size(), 1);
    int line = 1, pos = 0;
    for (size_t i = 0; i < order.size(); i++)
    {
        while (pos < order[i].first && pSource[pos] != 0)
        {
            if (pSource[pos] == 13)
            {
                line++;
            }
            pos++;
        }
        lines[order[i].second] = line;
    }
}

// The absolute path of a file (the compiler has it as found: maybe
// relative to the current folder, through a -I folder)
static std::string FullPath(const char* pPath)
{
#ifdef _WIN32
    char full[_MAX_PATH];
    if (_fullpath(full, pPath, sizeof(full)))
    {
        return full;
    }
#else
    char full[PATH_MAX];
    if (realpath(pPath, full))
    {
        return full;
    }
#endif
    return pPath;
}

void DebugInfo_EndObject(const char* pFilePath, const char* pSource)
{
    if (!s_bEnabled)
    {
        return;
    }
    std::string path = FullPath(pFilePath);
    const char* pPath = path.c_str();
    std::vector<int> positions, lines;
    for (size_t i = 0; i < s_methods.size(); i++)
    {
        positions.push_back(s_methods[i].sourcePos);
    }
    for (size_t i = 0; i < s_lines.size(); i++)
    {
        positions.push_back(s_lines[i].sourcePos);
    }
    LineNumbers(pSource, positions, lines);

    std::string t;
    char buf[64];
    t += "object ";
    t += pPath;
    t += "\n";
    for (size_t i = 0; i < s_objs.size(); i++)
    {
        snprintf(buf, sizeof(buf), "obj %d ", s_objs[i].count);
        t += buf + s_objs[i].alias + " " + s_objs[i].file + "\n";
    }
    for (size_t i = 0; i < s_vars.size(); i++)
    {
        const Var& v = s_vars[i];
        int offset = v.offset + (v.size < 4 ? s_longBytes : 0) + (v.size == 1 ? s_wordBytes : 0);
        snprintf(buf, sizeof(buf), "var %d %d %d ", v.size, v.count, offset);
        t += buf + v.name + "\n";
    }
    for (size_t i = 0; i < s_methods.size(); i++)
    {
        const Method& m = s_methods[i];
        snprintf(buf, sizeof(buf), "method %d %s %d ", m.index, m.bPub ? "pub" : "pri", lines[i]);
        t += buf + m.name + "\n";
        for (size_t k = 0; k < m.params.size(); k++)
        {
            t += "param " + m.params[k] + "\n";
        }
        t += "result " + m.result + "\n";
        for (size_t k = 0; k < m.locals.size(); k++)
        {
            snprintf(buf, sizeof(buf), "local %d ", m.locals[k].count);
            t += buf + m.locals[k].name + "\n";
        }
    }
    int lastLine = -1;
    for (size_t i = 0; i < s_lines.size(); i++)
    {
        int line = lines[s_methods.size() + i];
        if (line == lastLine)
        {
            continue;               // the same line goes on
        }
        snprintf(buf, sizeof(buf), "line %d %d\n", s_lines[i].offset, line);
        t += buf;
        lastLine = line;
    }
    t += "end\n";

    for (size_t i = 0; i < s_objects.size(); i++)
    {
        if (s_objects[i].path == pPath)
        {
            s_objects[i].text = t;
            return;
        }
    }
    Object o;
    o.path = pPath;
    o.text = t;
    s_objects.push_back(o);
}

const char* DebugInfo_Text()
{
    s_text = "OpenSpin debug 1\n";
    for (size_t i = 0; i < s_objects.size(); i++)
    {
        s_text += s_objects[i].text;
    }
    return s_text.c_str();
}

void DebugInfo_Clear()
{
    s_objs.clear();
    DebugInfo_BeginObject();
    s_objects.clear();
    s_text.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////
//                           TERMS OF USE: MIT License                                   //
///////////////////////////////////////////////////////////////////////////////////////////
// Permission is hereby granted, free of charge, to any person obtaining a copy of this  //
// software and associated documentation files (the "Software"), to deal in the Software //
// without restriction, including without limitation the rights to use, copy, modify,    //
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    //
// permit persons to whom the Software is furnished to do so, subject to the following   //
// conditions:                                                                           //
//                                                                                       //
// The above copyright notice and this permission notice shall be included in all copies //
// or substantial portions of the Software.                                              //
//                                                                                       //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   //
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         //
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    //
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION     //
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE        //
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                //
///////////////////////////////////////////////////////////////////////////////////////////
