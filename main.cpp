#include <stdio.h>
#include "libparse/parser.h"
#include <map>

using namespace libparse;


enum NodeType {
  Class, Scope, VariableDeclaration, AssignOp, Constant, BinaryExpression
};
enum ConstantType {
  Integer, String, Character
};
class Node {
public:
  NodeType type;
  Node(NodeType type):type(type) {
    
  }
};

class AssignNode:public Node {
public:
  StringRef dest;
  Node* value;
  AssignNode():Node(AssignOp) {
  }
  
};

class ScopeNode:public Node {
public:
  ScopeNode* parent;
  std::map<StringRef,Node*> tokens;
  ScopeNode():Node(Scope) {
  }
  Node* resolve(const StringRef& name) {
    if(tokens.find(name) == tokens.end()) {
      if(parent) {
	return parent->resolve(name);
      }
      return 0;
    }else {
      return tokens[name];
    }
  }
  bool add(const StringRef& name, Node* value) {
    if(tokens.find(name) == tokens.end()) {
      tokens[name] = value;
      return true;
    }
    return false;
  }
};


class ClassNode:public Node {
public:
  ScopeNode scope;
  StringRef name;
  int align; //Required memory alignment for class (or 0 if undefined)
  int size; //Required size for class (excluding padding) (or 0 if undefined)
  ClassNode():Node(Class) {
    
  }
};

class ConstantNode:public Node {
public:
  ConstantType ctype;
  StringRef value;
ConstantNode():Node(Constant) {
}
};


class BinaryExpressionNode:public Node {
public:
  char op;
  BinaryExpression():Node(BinaryExpression) {
    
  }
};


class VParser:public ParseTree {
public:
  Node* parseExpression(ScopeNode* scope) {
    Node* retval = 0;
    skipWhitespace();
    if(isdigit(*ptr)) {
      int oval;
      StringRef erence;
      if(!parseUnsignedInteger(oval,erence)) {
	return 0;
      }
      ConstantNode* node = new ConstantNode();
      node->ctype = Integer;
      node->value = erence;
      retval = node;
      
    }
    skipWhitespace();
    
    if(*ptr == ';') {
      return retval;
    }
    switch(*ptr) {
      case ''
      default:
	return 0;
    }
  }
  ClassNode* parseClass(ScopeNode* parent) {
    
    skipWhitespace();
    StringRef name;
    int align = 0;
    int size = 0;
    if(!expectToken(name)) {
      return 0;
    }
    skipWhitespace();
    while(*ptr == '.') {
      ptr++;
      StringRef keyword;
      if(!expectToken(keyword)) {
	return 0;
      }
      int wordidx;
      if(!keyword.in(wordidx,"align","size")) {
	return 0;
      }
      skipWhitespace();
      StringRef erence;
      switch(wordidx) {
	case 0:
	{
	  if(!parseUnsignedInteger(align,erence)) {
	    return 0;
	  }
	}
	  break;
	case 1:
	{
	  if(!parseUnsignedInteger(size,erence)) {
	    return 0;
	  }
	}
	  break;
      }
      skipWhitespace();
    }
    
    switch(*ptr) {
      case '{':
      {
	ptr++;
	skipWhitespace();
	if(*ptr == '}') {
	  ptr++;
	  ClassNode* node = new ClassNode();
	  node->align = align;
	  node->name = name;
	  node->size = size;
	  node->scope.parent = parent;
	  if(!parent->add(name,node)) {
	    delete node;
	    return 0;
	  }
	  return node;
	}
      }
	break;
      default:
	return 0;
    }
    
  }
  bool parseUnsignedInteger(int& out, StringRef& seg) {
    skipWhitespace();
    if(!isdigit(*ptr)) {
      return false;
    }
    scan(isdigit,seg);
    
    char* end = (char*)(seg.ptr+seg.count);
    out = strtol(seg.ptr,&end,0);
    ptr--;
    return true;
  }
  bool expectToken(StringRef& out) {
    if(!isalnum(*ptr)) {
      return false;
    }
    scan(isalnum,out);
    return true;
  }
  Node* parse(ScopeNode* scope) {
    skipWhitespace();
    char current = *ptr;
    if(isalpha(current)) {
      //Have token
      StringRef token;
      scan(isalnum,token);
      int keyword;
      std::string cval = token;
      if(token.in(keyword,"class")) {
	switch(keyword) {
	  case 0:
	    return parseClass(scope);
	}
      }else {
	scan(isalnum,token);
	skipWhitespace();
	switch(*ptr) {
	  case '=':
	  {
	    ptr++;
	    skipWhitespace();
	    Node* expression = parseExpression(scope);
	    if(expression) {
	      
	    }
	    
	  }
	    break;
	}
      }
    }
    return 0;
  }
  ScopeNode scope;
  VParser(const char* code):ParseTree(code) {
   while(*ptr) {
    parse(&scope);
   }
  }
};

int main(int argc, char** argv) {
  const char* test = "class int .align 4 .size 4 { }\nclass byte .size 1 { }\nclass long .align 8 .size 8 { }\nint eger = 5;";
  VParser tounge(test);
}