//////////////////////////////////////////////////////////////
//                                                          //
// Propeller Spin/PASM Compiler                             //
// Debugging information (openspin -g)                      //
// See end of file for terms of use.                        //
//                                                          //
//////////////////////////////////////////////////////////////
//
// DebugInfo.h
//
// What a debugger needs to show a Spin program by its source: for each
// object, its OBJ entries, its VAR, its methods (index, PUB/PRI, the
// line of the declaration, the parameters, the result's name and the
// locals) and, for the bytecode, the source line of each statement.
// Offsets are bytes from the start of the object (its pbase).  The text
// format is described in DebugInfo.cpp.
//

#ifndef _DEBUG_INFO_H_
#define _DEBUG_INFO_H_

void DebugInfo_Enable(bool bEnable);
bool DebugInfo_Enabled();

// The symbol the compiler has just read (right after BackupSymbol): its
// text as written, for the next Obj, Var, Param, Result or Local (the
// compiler keeps symbols in capitals)
void DebugInfo_Name();

// Compile1: the OBJ section (called again when Compile1 is redone)
void DebugInfo_ObjReset();
void DebugInfo_Obj(const char* pAlias, int count, const char* pFilename);

// Compile2: the VAR section, the methods, the statements
void DebugInfo_BeginObject();
void DebugInfo_Var(const char* pName, int size, int count, int offset);  // offset in its size's group
void DebugInfo_VarSizes(int longBytes, int wordBytes);                  // the groups: longs, then words, then bytes
void DebugInfo_Method(int index, bool bPub, const char* pName, int nameLength, int sourcePos);
void DebugInfo_Param(const char* pName);
void DebugInfo_Result(const char* pName);
void DebugInfo_Local(const char* pName, int count);
void DebugInfo_Line(int objOffset, int sourcePos);                      // code from here on is that source line's
// Code compiled only to be measured, then dropped (SkipBlock): no lines
void DebugInfo_Suspend();
void DebugInfo_Resume();
void DebugInfo_Rewind(int objOffset);                                   // a block is compiled again after here
void DebugInfo_DropMethod();                                            // the method was not used: drop it

// After Compile2: keep the object's information under its file's path
// (replacing what an earlier compile of the same file gave)
void DebugInfo_EndObject(const char* pPath, const char* pSource);

// The whole text, and starting over
const char* DebugInfo_Text();
void DebugInfo_Clear();

#endif // _DEBUG_INFO_H_

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
