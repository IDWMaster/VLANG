/*
Copyright 2018 Brian Bosak

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "tree.h"

class ValidationError {
public:
  std::string msg;
  Node* node = 0;
};
class ExternalCompilerContext {
public:
  virtual bool parse(const char* code,ScopeNode* parent, Node*** nodes, size_t* len) = 0;
  virtual bool verify(ScopeNode* scope,Node** nodes, size_t len, ValidationError** validationMessages, size_t* outlen) = 0;
  virtual Node* resolve(const char* offset) = 0; //Resolves a node at a specific location in text.
  virtual ~ExternalCompilerContext(){};
};


ExternalCompilerContext* compiler_new();
